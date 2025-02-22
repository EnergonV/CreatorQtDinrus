// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

/* A debug dispatcher for Windows that can be registered for calls with crashed
 * processes. It offers debugging using either Qt Creator or
 * the previously registered default debugger.
 * See usage() on how to install/use.
 * Installs itself in the bin directory of Qt Creator. */

#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QTextStream>
#include <QFileInfo>
#include <QByteArray>
#include <QElapsedTimer>
#include <QSysInfo>
#include <QString>
#include <QDir>
#include <QTime>
#include <QProcess>
#include <QPushButton>

#include <registryaccess.h>

#include <windows.h>
#include <psapi.h>

#include <app/app_version.h>

using namespace RegistryAccess;

enum { debug = 0 };

static const char titleC[] = "Qt Creator Debugger";

// Optional
static const WCHAR debuggerWow32RegistryKeyC[] = L"Software\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug";

static const WCHAR debuggerRegistryDefaultValueNameC[] = L"Debugger.Default";

static const char linkC[] = "http://msdn.microsoft.com/en-us/library/cc266343.aspx";
static const char creatorBinaryC[] = "qtcreator.exe";

#ifdef __GNUC__
#define RRF_RT_ANY             0x0000ffff  // no type restriction
#endif


enum Mode { HelpMode, RegisterMode, UnregisterMode, PromptMode, ForceCreatorMode, ForceDefaultMode };

Mode optMode = PromptMode;
// WOW: Indicates registry key access mode:
// - Accessing 32bit using a 64bit built Qt Creator or,
// - Accessing 64bit using a 32bit built Qt Creator on 64bit Windows
bool optIsWow = false;
bool noguiMode = false;
unsigned long argProcessId = 0;
quint64 argWinCrashEvent = 0;

static bool parseArguments(const QStringList &args, QString *errorMessage)
{
    int argNumber = 0;
    const int count = args.size();
    const QChar dash = QLatin1Char('-');
    const QChar slash = QLatin1Char('/');
    for (int i = 1; i < count; i++) {
        QString arg = args.at(i);
        if (arg.startsWith(dash) || arg.startsWith(slash)) { // option
            arg.remove(0, 1);
            if (arg == QLatin1String("help") || arg == QLatin1String("?")) {
                optMode = HelpMode;
            } else if (arg == QLatin1String("qtcreator")) {
                optMode = ForceCreatorMode;
            } else if (arg == QLatin1String("default")) {
                optMode = ForceDefaultMode;
            } else if (arg == QLatin1String("register")) {
                optMode = RegisterMode;
            } else if (arg == QLatin1String("unregister")) {
                optMode = UnregisterMode;
            } else if (arg == QLatin1String("wow")) {
                optIsWow = true;
            } else if (arg == QLatin1String("nogui")) {
                noguiMode = true;
            } else if (arg == QLatin1String("p")) {
                // Ignore, see QTCREATORBUG-18194.
            } else {
                *errorMessage = QString::fromLatin1("Unexpected option: %1").arg(arg);
                return false;
            }
            } else { // argument
                bool ok = false;
                switch (argNumber++) {
                case 0:
                    argProcessId = arg.toULong(&ok);
                    break;
                case 1:
                    argWinCrashEvent = arg.toULongLong(&ok);
                    break;
                }
                if (!ok) {
                    *errorMessage = QString::fromLatin1("Invalid argument: %1").arg(arg);
                    return false;
                }
            }
        }
    switch (optMode) {
    case HelpMode:
    case RegisterMode:
    case UnregisterMode:
        break;
    default:
        if (argProcessId == 0) {
            *errorMessage = QString::fromLatin1("Please specify the process-id.");
            return false;
        }
    }
    return true;
}

static bool readDebugger(const wchar_t *key, QString *debugger,
                         QString *errorMessage)
{
    bool success = false;
    HKEY handle;
    const RegistryAccess::AccessMode accessMode = optIsWow
#ifdef Q_OS_WIN64
        ? RegistryAccess::Registry32Mode
#else
        ? RegistryAccess::Registry64Mode
#endif
        : RegistryAccess::DefaultAccessMode;

    if (openRegistryKey(HKEY_LOCAL_MACHINE, debuggerRegistryKeyC, false, &handle, accessMode, errorMessage)) {
        success = registryReadStringKey(handle, key, debugger, errorMessage);
        RegCloseKey(handle);
    }
    return success;
}

static void usage(const QString &binary, const QString &message = QString())
{
    QString msg;
    QTextStream str(&msg);
    str << "<html><body>";
    if (message.isEmpty()) {
        str << "<h1>" <<  titleC << "</h1>"
            << "<p>Dispatcher that launches the desired debugger for a crashed process"
            " according to <a href=\"" << linkC << "\">Enabling Postmortem Debugging</a>.</p>";
    } else {
        str << "<b>" << message << "</b>";
    }
    str << "<pre>"
        << "Usage: " << QFileInfo(binary).baseName() << "[-wow] [-help|-?|qtcreator|default|register|unregister] &lt;process-id> &lt;event-id>\n"
        << "Options: -help, -?   Display this help\n"
        << "         -qtcreator  Launch Qt Creator without prompting\n"
        << "         -default    Launch Default handler without prompting\n"
        << "         -register   Register as post mortem debugger (requires administrative privileges)\n"
        << "         -unregister Unregister as post mortem debugger (requires administrative privileges)\n"
        << "         -wow        Indicates Wow32 call\n"
        << "         -nogui      Do not show error messages in popup windows\n"
        << "</pre>"
        << "<p>To install, modify the registry key <i>HKEY_LOCAL_MACHINE\\" << wCharToQString(debuggerRegistryKeyC)
        << "</i>:</p><ul>"
        << "<li>Create a copy of the string value <i>" << wCharToQString(debuggerRegistryValueNameC)
        << "</i> as <i>" << wCharToQString(debuggerRegistryDefaultValueNameC) << "</i>"
        << "<li>Change the value of <i>" << wCharToQString(debuggerRegistryValueNameC) << "</i> to "
        << "<pre>\"" << QDir::toNativeSeparators(binary) << "\" %ld %ld</pre>"
        << "</ul>"
        << "<p>On 64-bit systems, do the same for the key <i>HKEY_LOCAL_MACHINE\\" << wCharToQString(debuggerWow32RegistryKeyC) << "</i>, "
        << "setting the new value to <pre>\"" << QDir::toNativeSeparators(binary) << "\" -wow %ld %ld</pre></p>"
        << "<p>How to run a command with administrative privileges:</p>"
        << "<pre>runas /env /noprofile /user:Administrator \"command arguments\"</pre>";
    QString currentDebugger;
    QString errorMessage;
    if (readDebugger(debuggerRegistryValueNameC, &currentDebugger, &errorMessage))
       str << "<p>Currently registered debugger:</p><pre>" << currentDebugger << "</pre>";
    str << "<p>Qt " << QT_VERSION_STR << ", " << QSysInfo::WordSize
        << "bit</p></body></html>";

    QMessageBox msgBox(QMessageBox::Information, QLatin1String(titleC), msg, QMessageBox::Ok);
    msgBox.setTextInteractionFlags(Qt::TextBrowserInteraction);
    msgBox.exec();
}

#ifndef Q_OS_WIN64
static bool is64BitWindowsSystem() // Courtesy utils library
{
    SYSTEM_INFO systemInfo;
    GetNativeSystemInfo(&systemInfo);
    return systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64
        || systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64;
}
#endif

// ------- Registry helpers

static inline bool registryWriteBinaryKey(HKEY handle,
                                          const WCHAR *valueName,
                                          DWORD type,
                                          const BYTE *data,
                                          DWORD size,
                                          QString *errorMessage)
{
    const LONG rc = RegSetValueEx(handle, valueName, 0, type, data, size);
    if (rc != ERROR_SUCCESS) {
        *errorMessage = msgRegistryOperationFailed("write", valueName, msgFunctionFailed("RegSetValueEx", rc));
        return false;
    }
    return true;
}

static inline bool registryWriteStringKey(HKEY handle, // HKEY_LOCAL_MACHINE, etc.
                                          const WCHAR *key,
                                          const QString &s,
                                          QString *errorMessage)
{
    const BYTE *data = reinterpret_cast<const BYTE *>(s.utf16());
    const DWORD size = 2 * s.size(); // excluding 0
    return registryWriteBinaryKey(handle, key, REG_SZ, data, size, errorMessage);
}

static inline bool registryReplaceStringKey(HKEY rootHandle, // HKEY_LOCAL_MACHINE, etc.
                                       const WCHAR *key,
                                       const WCHAR *valueName,
                                       const QString &newValue,
                                       QString *oldValue,
                                       QString *errorMessage)
{
    HKEY handle = 0;
    bool rc = false;
    do {
        if (!openRegistryKey(rootHandle, key, true, &handle, errorMessage))
            break;
        if (!registryReadStringKey(handle, valueName, oldValue, errorMessage))
            break;
        if (*oldValue != newValue) {
            if (!registryWriteStringKey(handle, valueName, newValue, errorMessage))
                break;
        }
        rc = true;
    } while (false);
    if (handle)
        RegCloseKey(handle);
    return rc;
}

static inline bool registryDeleteValue(HKEY handle,
                                       const WCHAR *valueName,
                                       QString *errorMessage)
{
    const LONG rc = RegDeleteValue(handle, valueName);
    if (rc != ERROR_SUCCESS) {
        *errorMessage = msgFunctionFailed("RegDeleteValue", rc);
        return false;
    }
    return true;
}

static QString getProcessBaseName(DWORD pid)
{
    QString rc;
    const HANDLE  handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (handle != NULL) {
        WCHAR buffer[MAX_PATH];
        if (GetModuleBaseName(handle, 0, buffer, MAX_PATH))
            rc = wCharToQString(buffer);
        CloseHandle(handle);
    }
    return rc;
}

// ------- main modes

static bool waitForProcess(DWORD pid)
{
    HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION|READ_CONTROL|SYNCHRONIZE, false, pid);
    if (handle == NULL)
        return false;
    const DWORD waitResult = WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);
    return waitResult == WAIT_OBJECT_0;
}

bool startCreatorAsDebugger(bool asClient, QString *errorMessage)
{
    const QString dir = QApplication::applicationDirPath();
    const QString binary = dir + QLatin1Char('/') + QLatin1String(creatorBinaryC);
    QStringList args;
    // Send to running Creator: Unstable with directly linked CDB engine.
    if (asClient)
        args << QLatin1String("-client");
    if (argWinCrashEvent != 0) {
        args << QLatin1String("-wincrashevent")
            << QString::fromLatin1("%1:%2").arg(argWinCrashEvent).arg(argProcessId);
    } else {
        args << QLatin1String("-debug")
            << QString::fromLatin1("%1").arg(argProcessId);
    }
    if (debug)
        qDebug() << binary << args;
    QProcess p;
    p.setWorkingDirectory(dir);
    QElapsedTimer executionTime;
    executionTime.start();
    p.start(binary, args, QIODevice::NotOpen);
    if (!p.waitForStarted()) {
        *errorMessage = QString::fromLatin1("Unable to start %1!").arg(binary);
        return false;
    }
    // Short execution time: indicates that -client was passed on attach to
    // another running instance of Qt Creator. Keep alive as long as user
    // does not close the process. If that fails, try to launch 2nd instance.
    const bool waitResult = p.waitForFinished(-1);
    const bool ranAsClient = asClient && (executionTime.elapsed() < 10000);
    if (waitResult && p.exitStatus() == QProcess::NormalExit && ranAsClient) {
        if (p.exitCode() == 0) {
            waitForProcess(argProcessId);
        } else {
            errorMessage->clear();
            return startCreatorAsDebugger(false, errorMessage);
        }
    }
    return true;
}

bool readDefaultDebugger(QString *defaultDebugger,
                         QString *errorMessage)
{
   return readDebugger(debuggerRegistryDefaultValueNameC, defaultDebugger, errorMessage);
}

bool startDefaultDebugger(QString *errorMessage)
{
    QString defaultDebugger;
    if (!readDefaultDebugger(&defaultDebugger, errorMessage))
        return false;
    // binary, replace placeholders by pid/event id
    if (debug)
        qDebug() << "Default" << defaultDebugger;
    const QString placeHolder = QLatin1String("%ld");
    const int pidPlaceHolderPos = defaultDebugger.indexOf(placeHolder);
    if (pidPlaceHolderPos == -1)
        return true; // was empty or sth
    defaultDebugger.replace(pidPlaceHolderPos, placeHolder.size(), QString::number(argProcessId));
    const int evtPlaceHolderPos = defaultDebugger.indexOf(placeHolder);
    if (evtPlaceHolderPos != -1)
        defaultDebugger.replace(evtPlaceHolderPos, placeHolder.size(), QString::number(argWinCrashEvent));
    if (debug)
        qDebug() << "Default" << defaultDebugger;
    QProcess p;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QStringList arguments = QProcess::splitCommand(defaultDebugger);
    const QString executable = arguments.takeFirst();
    p.start(executable, arguments, QIODevice::NotOpen);
#else
    p.start(defaultDebugger, QIODevice::NotOpen);
#endif
    if (!p.waitForStarted()) {
        *errorMessage = QString::fromLatin1("Unable to start %1!").arg(defaultDebugger);
        return false;
    }
    p.waitForFinished(-1);
    return true;
}

bool chooseDebugger(QString *errorMessage)
{
    QString defaultDebugger;
    const QString processName = getProcessBaseName(argProcessId);
    const QString msg = QString::fromLatin1("The application \"%1\" (process id %2)  crashed. Would you like to debug it?").arg(processName).arg(argProcessId);
    QMessageBox msgBox(QMessageBox::Information, QLatin1String(titleC), msg, QMessageBox::Cancel);
    QPushButton *creatorButton = msgBox.addButton(QLatin1String("Debug with Qt Creator"), QMessageBox::AcceptRole);
    QPushButton *defaultButton = msgBox.addButton(QLatin1String("Debug with default debugger"), QMessageBox::AcceptRole);
    defaultButton->setEnabled(readDefaultDebugger(&defaultDebugger, errorMessage)
                              && !defaultDebugger.isEmpty());
    msgBox.exec();
    if (msgBox.clickedButton() == creatorButton) {
        // Just in case, default to standard. Do not run as client in the unlikely case
        // Creator crashed
        // TODO: pass asClient=true for new CDB engine.
        const bool canRunAsClient = !processName.contains(QLatin1String(creatorBinaryC), Qt::CaseInsensitive);
        Q_UNUSED(canRunAsClient)
        if (startCreatorAsDebugger(false, errorMessage))
            return true;
        return startDefaultDebugger(errorMessage);
    }
    if (msgBox.clickedButton() == defaultButton)
        return startDefaultDebugger(errorMessage);
    return true;
}

// registration helper: Register ourselves in a debugger registry key.
// Make a copy of the old value as "Debugger.Default" and have the
// "Debug" key point to us.

static bool registerDebuggerKey(const WCHAR *key,
                                const QString &call,
                                RegistryAccess::AccessMode access,
                                QString *errorMessage)
{
    HKEY handle = 0;
    bool success = false;
    do {
        if (!openRegistryKey(HKEY_LOCAL_MACHINE, key, true, &handle, access, errorMessage))
            break;

        // Make sure to automatically open the qtcdebugger dialog on a crash
        QString autoVal;
        registryReadStringKey(handle, autoRegistryValueNameC, &autoVal, errorMessage);
        if (autoVal != "1") {
            if (!registryWriteStringKey(handle, autoRegistryValueNameC, "1", errorMessage))
                break;
        }

        // Save old key, which might be missing
        QString oldDebugger;
        if (isRegistered(handle, call, errorMessage, &oldDebugger)) {
            *errorMessage = QLatin1String("The program is already registered as post mortem debugger.");
            break;
        }
        if (!(oldDebugger.contains(QLatin1String(debuggerApplicationFileC), Qt::CaseInsensitive)
              || registryWriteStringKey(handle, debuggerRegistryDefaultValueNameC, oldDebugger, errorMessage)))
            break;
        if (debug)
            qDebug() << "registering self as " << call;
        if (!registryWriteStringKey(handle, debuggerRegistryValueNameC, call, errorMessage))
            break;
        success = true;
    } while (false);
    if (handle)
        RegCloseKey(handle);
    return success;
}

bool install(QString *errorMessage)
{
    if (!registerDebuggerKey(debuggerRegistryKeyC, debuggerCall(), RegistryAccess::DefaultAccessMode, errorMessage))
        return false;
#ifdef Q_OS_WIN64
    if (!registerDebuggerKey(debuggerRegistryKeyC, debuggerCall(QLatin1String("-wow")), RegistryAccess::Registry32Mode, errorMessage))
        return false;
#else
    if (is64BitWindowsSystem()) {
        if (!registerDebuggerKey(debuggerRegistryKeyC, debuggerCall(QLatin1String("-wow")), RegistryAccess::Registry64Mode, errorMessage))
            return false;
    }
#endif
    return true;
}

// Unregister helper: Restore the original debugger key
static bool unregisterDebuggerKey(const WCHAR *key,
                                  const QString &call,
                                  RegistryAccess::AccessMode access,
                                  QString *errorMessage)
{
    HKEY handle = 0;
    bool success = false;
    do {
        if (!openRegistryKey(HKEY_LOCAL_MACHINE, key, true, &handle, access, errorMessage))
            break;
        QString debugger;
        if (!isRegistered(handle, call, errorMessage, &debugger) && !debugger.isEmpty()) {
            *errorMessage = QLatin1String("The program is not registered as post mortem debugger.");
            break;
        }
        QString oldDebugger;
        registryReadStringKey(handle, debuggerRegistryDefaultValueNameC, &oldDebugger, errorMessage);
        // Re-register old debugger or delete key if it was empty.
        if (oldDebugger.isEmpty()) {
            if (!registryDeleteValue(handle, debuggerRegistryValueNameC, errorMessage))
                break;
        } else {
            if (!registryWriteStringKey(handle, debuggerRegistryValueNameC, oldDebugger, errorMessage))
                break;
        }
        if (!registryDeleteValue(handle, debuggerRegistryDefaultValueNameC, errorMessage))
            break;
        success = true;
    } while (false);
    if (handle)
        RegCloseKey(handle);
    return success;
}


bool uninstall(QString *errorMessage)
{
    if (!unregisterDebuggerKey(debuggerRegistryKeyC, debuggerCall(), RegistryAccess::DefaultAccessMode, errorMessage))
        return false;
#ifdef Q_OS_WIN64
    if (!unregisterDebuggerKey(debuggerRegistryKeyC, debuggerCall(QLatin1String("-wow")), RegistryAccess::Registry32Mode, errorMessage))
        return false;
#else
    if (is64BitWindowsSystem()) {
        if (!unregisterDebuggerKey(debuggerRegistryKeyC, debuggerCall(QLatin1String("-wow")), RegistryAccess::Registry64Mode, errorMessage))
            return false;
    }
#endif

    return true;
}


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName(QLatin1String(titleC));
    QApplication::setOrganizationName(QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR));
    QString errorMessage;

    if (!parseArguments(QApplication::arguments(), &errorMessage)) {
        qWarning("%s\n", qPrintable(errorMessage));
        usage(QCoreApplication::applicationFilePath(), errorMessage);
        return -1;
    }
    if (debug)
        qDebug() << "Mode=" << optMode << " PID=" << argProcessId << " Evt=" << argWinCrashEvent;
    bool ex = 0;
    switch (optMode) {
    case HelpMode:
        usage(QCoreApplication::applicationFilePath(), errorMessage);
        break;
    case ForceCreatorMode:
        // TODO: pass asClient=true for new CDB engine.
        ex = startCreatorAsDebugger(false, &errorMessage) ? 0 : -1;
        break;
    case ForceDefaultMode:
        ex = startDefaultDebugger(&errorMessage) ? 0 : -1;
        break;
    case PromptMode:
        ex = chooseDebugger(&errorMessage) ? 0 : -1;
        break;
    case RegisterMode:
        ex = install(&errorMessage) ? 0 : -1;
        break;
    case UnregisterMode:
        ex = uninstall(&errorMessage) ? 0 : -1;
        break;
    }
    if (ex && !errorMessage.isEmpty()) {
        if (noguiMode)
            qWarning("%s\n", qPrintable(errorMessage));
        else
            QMessageBox::warning(0, QLatin1String(titleC), errorMessage, QMessageBox::Ok);
    }
    return ex;
}
