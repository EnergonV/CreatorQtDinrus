// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "cdbparsehelpers.h"

#include "stringinputstream.h"

#include <debugger/breakhandler.h>
#include <debugger/debuggerprotocol.h>
#include <debugger/disassemblerlines.h>
#include <debugger/shared/hostutils.h>
#include <debugger/threaddata.h>

#include <utils/fileutils.h>
#include <utils/qtcassert.h>

#include <QDir>
#include <QTextStream>
#include <QDebug>

#include <cctype>

enum { debugDisAsm = 0 };

namespace Debugger {
namespace Internal {

// Perform mapping on parts of the source tree as reported by/passed to debugger
// in case the user has specified such mappings in the global settings.
// That is, when debugging an executable built from 'X:\buildsrv\foo.cpp' and using a local
// source tree under 'c:\src', the user would specify a mapping 'X:\buildsrv'->'c:\src'
// and file names passed to breakpoints and reported stack traces can be converted.
QString cdbSourcePathMapping(QString fileName,
                             const QList<QPair<QString, QString> > &sourcePathMapping,
                             SourcePathMode mode)
{
    using SourcePathMapping = QPair<QString, QString>;

    if (fileName.isEmpty() || sourcePathMapping.isEmpty())
        return fileName;
    for (const SourcePathMapping &m : sourcePathMapping) {
        const QString &source = mode == DebuggerToSource ? m.first : m.second;
        const int sourceSize = source.size();
        // Map parts of the path and ensure a slash follows.
        if (fileName.size() > sourceSize && fileName.startsWith(source, Qt::CaseInsensitive)) {
            const QChar next = fileName.at(sourceSize);
            if (next == '\\' || next == '/') {
                const QString &target = mode == DebuggerToSource ? m.second: m.first;
                fileName.replace(0, sourceSize, target);
                return fileName;
            }
        }
    }
    return fileName;
}

// Determine file name to be used for breakpoints. Convert to native and, unless short path
// is set, perform reverse lookup in the source path mappings.
static inline QString cdbBreakPointFileName(const BreakpointParameters &params,
                                            const QList<QPair<QString, QString> > &sourcePathMapping)
{
    if (params.fileName.isEmpty())
        return {};
    if (params.pathUsage == BreakpointUseShortPath)
        return params.fileName.fileName();
    return cdbSourcePathMapping(params.fileName.toUserOutput(), sourcePathMapping, SourceToDebugger);
}

static BreakpointParameters fixWinMSVCBreakpoint(const BreakpointParameters &p)
{
    switch (p.type) {
    case UnknownBreakpointType:
    case LastBreakpointType:
    case BreakpointByFileAndLine:
    case BreakpointByFunction:
    case BreakpointByAddress:
    case BreakpointAtFork:
    case WatchpointAtExpression:
    case BreakpointAtSysCall:
    case WatchpointAtAddress:
    case BreakpointOnQmlSignalEmit:
    case BreakpointAtJavaScriptThrow:
        break;
    case BreakpointAtExec: { // Emulate by breaking on CreateProcessW().
        BreakpointParameters rc(BreakpointByFunction);
        rc.module = "kernel32";
        rc.functionName = "CreateProcessW";
        return rc;
    }
    case BreakpointAtThrow: {
        BreakpointParameters rc(BreakpointByFunction);
        rc.functionName = "CxxThrowException"; // MSVC runtime. Potentially ambiguous.
        return rc;
    }
    case BreakpointAtCatch: {
        BreakpointParameters rc(BreakpointByFunction);
        rc.functionName = "__CxxCallCatchBlock"; // MSVC runtime. Potentially ambiguous.
        return rc;
    }
    case BreakpointAtMain: {
        BreakpointParameters rc(BreakpointByFunction);
        rc.functionName = "main";
        rc.module = p.module;
        rc.oneShot = true;
        return rc;
    }
    } // switch
    return p;
}

QString breakPointCdbId(const Breakpoint &bp)
{
    static int bpId = 1;
    if (!bp->responseId().isEmpty())
        return bp->responseId();
    return QString::number(cdbBreakPointStartId + (bpId++) * cdbBreakPointIdMinorPart);
}

QString cdbAddBreakpointCommand(const BreakpointParameters &bpIn,
                                const QList<QPair<QString, QString> > &sourcePathMapping,
                                const QString &responseId)
{
    const BreakpointParameters params = fixWinMSVCBreakpoint(bpIn);
    QString rc;
    StringInputStream str(rc);

    if (params.threadSpec >= 0)
        str << '~' << params.threadSpec << ' ';

    // Currently use 'bu' so that the offset expression (including file name)
    // is kept when reporting back breakpoints (which is otherwise discarded
    // when resolving).
    str << (params.type == WatchpointAtAddress ? "ba" : "bu")
        << responseId
        << ' ';
    if (params.oneShot)
        str << "/1 ";
    switch (params.type) {
    case BreakpointAtFork:
    case BreakpointAtExec:
    case WatchpointAtExpression:
    case BreakpointAtSysCall:
    case BreakpointAtCatch:
    case BreakpointAtThrow:
    case BreakpointAtMain:
    case BreakpointOnQmlSignalEmit:
    case BreakpointAtJavaScriptThrow:
    case UnknownBreakpointType:
    case LastBreakpointType:
        QTC_ASSERT(false, return QString());
        break;
    case BreakpointByAddress:
        str << hex << hexPrefixOn << params.address << hexPrefixOff << dec;
        break;
    case BreakpointByFunction:
        if (!params.module.isEmpty())
            str << params.module << '!';
        str << params.functionName;
        break;
    case BreakpointByFileAndLine:
        str << '`';
        if (!params.module.isEmpty())
            str << params.module << '!';
        str << cdbBreakPointFileName(params, sourcePathMapping) << ':' << params.lineNumber << '`';
        break;
    case WatchpointAtAddress: { // Read/write, no space here
        const unsigned size = params.size ? params.size : 1;
        str << 'r' << size << ' ' << hex << hexPrefixOn << params.address << hexPrefixOff << dec;
    }
        break;
    }
    if (params.ignoreCount)
        str << " 0n" << (params.ignoreCount + 1);
    // Condition currently unsupported.
    if (!params.command.isEmpty())
        str << " \"" << params.command << '"';
    return rc;
}

QString cdbClearBreakpointCommand(const Breakpoint &bp)
{
// FIME: Check
//    const int firstBreakPoint = breakPointCdbId(id);
//    if (id.isMinor())
//        return "bc " + QString::number(firstBreakPoint);
    // If this is a major break point we also want to delete all sub break points
    const int firstBreakPoint = bp->responseId().toInt();
    const int lastBreakPoint = firstBreakPoint + cdbBreakPointIdMinorPart - 1;
    return "bc " + QString::number(firstBreakPoint) + '-' + QString::number(lastBreakPoint);
}

// Helper to retrieve an int child from GDBMI
static inline bool gdbmiChildToInt(const GdbMi &parent, const char *childName, int *target)
{
    const GdbMi childBA = parent[childName];
    if (childBA.isValid()) {
        bool ok;
        const int v = childBA.data().toInt(&ok);
        if (ok) {
            *target = v;
            return  true;
        }
    }
    return false;
}

// Helper to retrieve an bool child from GDBMI
static inline bool gdbmiChildToBool(const GdbMi &parent, const char *childName, bool *target)
{
    const GdbMi childBA = parent[childName];
    if (childBA.isValid()) {
        *target = childBA.data() == "true";
        return true;
    }
    return false;
}

// Parse extension command listing breakpoints.
// Note that not all fields are returned, since file, line, function are encoded
// in the expression (that is in addition deleted on resolving for a bp-type breakpoint).
void parseBreakPoint(const GdbMi &gdbmi, BreakpointParameters *r,
                     QString *expression /*  = 0 */)
{
    gdbmiChildToBool(gdbmi, "enabled", &(r->enabled));
    gdbmiChildToBool(gdbmi, "deferred", &(r->pending));
    const GdbMi moduleG = gdbmi["module"];
    if (moduleG.isValid())
        r->module = moduleG.data();
    const GdbMi sourceFileName = gdbmi["srcfile"];
    if (sourceFileName.isValid()) {
        r->fileName = Utils::FilePath::fromUserInput(
            Utils::FileUtils::normalizedPathName(sourceFileName.data()));
        const GdbMi lineNumber = gdbmi["srcline"];
        if (lineNumber.isValid())
            r->lineNumber = lineNumber.data().toULongLong(nullptr, 0);
    }
    if (expression) {
        const GdbMi expressionG = gdbmi["expression"];
        if (expressionG.isValid())
            *expression = expressionG.data();
    }
    const GdbMi addressG = gdbmi["address"];
    if (addressG.isValid())
        r->address = addressG.data().toULongLong(nullptr, 0);
    if (gdbmiChildToInt(gdbmi, "passcount", &(r->ignoreCount)))
        r->ignoreCount--;
    gdbmiChildToInt(gdbmi, "thread", &(r->threadSpec));
}

QString cdbWriteMemoryCommand(quint64 addr, const QByteArray &data)
{
    QString cmd;
    StringInputStream str(cmd);
    str.setIntegerBase(16);
    str << "f " << addr << " L" << data.size();
    const int count = data.size();
    for (int i = 0 ; i < count ; i++ ) {
        const unsigned char uc = (unsigned char)data.at(i);
        str << ' ' << unsigned(uc);
    }
    return cmd;
}

QString debugByteArray(const QByteArray &a)
{
    QString rc;
    const int size = a.size();
    rc.reserve(size * 2);
    QTextStream str(&rc);
    for (int i = 0; i < size; i++) {
        const unsigned char uc = (unsigned char)(a.at(i));
        switch (uc) {
        case 0:
            str << "\\0";
            break;
        case '\n':
            str << "\\n";
            break;
        case '\t':
            str << "\\t";
            break;
        case '\r':
            str << "\\r";
            break;
        default:
            if (uc >=32 && uc < 128)
                str << a.at(i);
            else
                str << '<' << unsigned(uc) << '>';
            break;
        }
    }
    return rc;
}

WinException::WinException() = default;

void WinException::fromGdbMI(const GdbMi &gdbmi)
{
    exceptionCode = gdbmi["exceptionCode"].data().toUInt();
    exceptionFlags = gdbmi["exceptionFlags"].data().toUInt();
    exceptionAddress = gdbmi["exceptionAddress"].data().toULongLong();
    firstChance = gdbmi["firstChance"].data() != "0";
    const GdbMi ginfo1 = gdbmi["exceptionInformation0"];
    if (ginfo1.isValid()) {
        info1 = ginfo1.data().toULongLong();
        const GdbMi ginfo2  = gdbmi["exceptionInformation1"];
        if (ginfo2.isValid())
            info2 = ginfo2.data().toULongLong();
    }
    const GdbMi gLineNumber = gdbmi["exceptionLine"];
    if (gLineNumber.isValid()) {
        lineNumber = gLineNumber.toInt();
        file = gdbmi["exceptionFile"].data();
    }
    function = gdbmi["exceptionFunction"].data();
}

QString WinException::toString(bool includeLocation) const
{
    QString rc;
    QTextStream str(&rc);
    formatWindowsException(exceptionCode, exceptionAddress,
                           exceptionFlags, info1, info2, str);
    if (firstChance)
        str << " (first chance)";
    if (includeLocation) {
        if (lineNumber) {
            str << " at " << file << ':' << lineNumber;
        } else {
            if (!function.isEmpty())
                str << " in " << function;
        }
    }
    return rc;
}

QDebug operator<<(QDebug s, const WinException &e)
{
    QDebug nsp = s.nospace();
    nsp << "code=" << e.exceptionCode << ",flags=" << e.exceptionFlags
        << ",address=0x" << QString::number(e.exceptionAddress, 16)
        << ",firstChance=" << e.firstChance;
    return s;
}

/*!
    \fn DisassemblerLines Debugger::Internal::parseCdbDisassembler(const QList<QByteArray> &a)

    Parses CDB disassembler output into DisassemblerLines (with helpers).

    Expected options (prepend source file line):
    \code
    .asm source_line
    .lines
    \endcode

    should cause the 'u' command to produce:

    \code
gitgui!Foo::MainWindow::on_actionPtrs_triggered+0x1f9 [c:\qt\projects\gitgui\app\mainwindow.cpp @ 758]:
  225 00000001`3fcebfe9 488b842410050000 mov     rax,qword ptr [rsp+510h]
  225 00000001`3fcebff1 8b4030          mov     eax,dword ptr [rax+30h]
  226 00000001`3fcebff4 ffc0            inc     eax
      00000001`3fcebff6 488b8c2410050000 mov     rcx,qword ptr [rsp+510h]
...
QtCored4!QTextStreamPrivate::putString+0x34:
   10 00000000`6e5e7f64 90              nop
...
\endcode

    The algorithm checks for a function line and grabs the function name, offset and (optional)
    source file from it.
    Instruction lines are checked for address and source line number.
    When the source line changes, the source instruction is inserted.
*/

// Parse a function header line: Match: 'nsp::foo+0x<offset> [<file> @ <line>]:'
// or 'nsp::foo+0x<offset>:', 'nsp::foo [<file> @ <line>]:'
// Do not use regexp here as it is hard for functions like operator+, operator[].
bool parseCdbDisassemblerFunctionLine(const QString &l,
                                      QString *currentFunction, quint64 *functionOffset,
                                      QString *sourceFile)
{
    if (l.isEmpty() || !l.endsWith(':') || l.at(0).isDigit() || l.at(0).isSpace())
        return false;
    int functionEnd = l.indexOf(' ');
    if (functionEnd < 0)
        functionEnd = l.size() - 1; // Nothing at all, just ':'
    const int offsetPos = l.indexOf("+0x");
    if (offsetPos > 0) {
        *currentFunction = l.left(offsetPos);
        *functionOffset = l.mid(offsetPos + 3, functionEnd - offsetPos - 3).trimmed().toULongLong(nullptr, 16);
    } else { // No offset, directly at beginning.
        *currentFunction = l.left(functionEnd);
        *functionOffset = 0;
    }
    sourceFile->clear();
    // Parse file and line.
    const int filePos = l.indexOf('[', functionEnd);
    if (filePos == -1)
        return true; // No file
    const int linePos = l.indexOf(" @ ", filePos + 1);
    if (linePos == -1)
        return false;
    *sourceFile = l.mid(filePos + 1, linePos - filePos - 1).trimmed();
    if (debugDisAsm)
        qDebug() << "Function with source: " << l << currentFunction
                 << functionOffset << sourceFile;
    return true;
}

/* Parse an instruction line, CDB 6.12:
 *  0123456
 * '   21 00000001`3fcebff1 8b4030          mov     eax,dword ptr [rax+30h]'
 * or CDB 6.11 (source line and address joined, 725 being the source line number):
 *  0123456
 * '  725078bb291 8bec            mov     ebp,esp
 * '<source_line>[ ]?<address> <raw data> <instruction> */

bool parseCdbDisassemblerLine(const QString &line, DisassemblerLine *dLine, uint *sourceLine)
{
    *sourceLine = 0;
    if (line.size() < 6)
        return false;
    const QChar blank = ' ';
    int addressPos = 0;
    // Check for joined source and address in 6.11
    const bool hasV611SourceLine = line.at(5).isDigit();
    const bool hasV612SourceLine = !hasV611SourceLine && line.at(4).isDigit();
    if (hasV611SourceLine) {
        // v6.11: Fixed 5 source line columns, joined
        *sourceLine = line.left(5).trimmed().toUInt();
        addressPos = 5;
    } else if (hasV612SourceLine) {
        // v6.12: Free format columns
        const int sourceLineEnd = line.indexOf(blank, 4);
        if (sourceLineEnd == -1)
              return false;
        *sourceLine = line.left(sourceLineEnd).trimmed().toUInt();
        addressPos = sourceLineEnd + 1;
    } else {
        // Skip source line column.
        const int size = line.size();
        for ( ; addressPos < size && line.at(addressPos).isSpace(); ++addressPos) ;
        if (addressPos == size)
            return false;
    }
    // Find positions of address/raw data/instruction
    const int addressEnd = line.indexOf(blank, addressPos + 1);
    if (addressEnd < 0)
        return false;
    const int rawDataPos = addressEnd + 1;
    const int rawDataEnd = line.indexOf(blank, rawDataPos + 1);
    if (rawDataEnd < 0)
        return false;
    const int instructionPos = rawDataEnd + 1;
    bool ok;
    QString addressS = line.mid(addressPos, addressEnd - addressPos);
    if (addressS.size() > 9 && addressS.at(8) == '`')
        addressS.remove(8, 1);
    dLine->address = addressS.toULongLong(&ok, 16);
    if (!ok)
        return false;
    dLine->rawData = QByteArray::fromHex(line.mid(rawDataPos, rawDataEnd - rawDataPos).toLatin1());
    dLine->data = line.right(line.size() - instructionPos).trimmed();
    return true;
}

DisassemblerLines parseCdbDisassembler(const QString &a)
{
    DisassemblerLines result;
    quint64 functionAddress = 0;
    uint lastSourceLine = 0;
    QString currentFunction;
    quint64 functionOffset = 0;
    QString sourceFile;

    const QStringList lines = a.split('\n');
    for (const QString &line : lines) {
        // New function. Append as comment line.
        if (parseCdbDisassemblerFunctionLine(line, &currentFunction, &functionOffset, &sourceFile)) {
            functionAddress = 0;
            DisassemblerLine commentLine;
            commentLine.data = line;
            result.appendLine(commentLine);
        } else {
            DisassemblerLine disassemblyLine;
            uint sourceLine;
            if (parseCdbDisassemblerLine(line, &disassemblyLine, &sourceLine)) {
                // New source line: Add source code if available.
                if (sourceLine && sourceLine != lastSourceLine) {
                    lastSourceLine = sourceLine;
                    result.appendSourceLine(sourceFile, sourceLine);
                }
            } else {
                qWarning("Unable to parse assembly line '%s'", qPrintable(line));
                disassemblyLine.fromString(line);
            }
            // Determine address of function from the first assembler line after a
            // function header line.
            if (!functionAddress && disassemblyLine.address)
                functionAddress = disassemblyLine.address - functionOffset;
            if (functionAddress && disassemblyLine.address)
                disassemblyLine.offset = disassemblyLine.address - functionAddress;
            disassemblyLine.function = currentFunction;
            result.appendLine(disassemblyLine);
        }
    }
    return result;
}

} // namespace Internal
} // namespace Debugger
