// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "cppsourceprocessor.h"

#include "cppmodelmanager.h"
#include "cpptoolsreuse.h"

#include <coreplugin/editormanager/editormanager.h>

#include <utils/fileutils.h>
#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>
#include <utils/textfileformat.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QLoggingCategory>
#include <QTextCodec>

/*!
 * \class CppEditor::Internal::CppSourceProcessor
 * \brief The CppSourceProcessor class updates set of indexed C++ files.
 *
 * Working copy ensures that documents with most recent copy placed in memory will be parsed
 * correctly.
 *
 * \sa CPlusPlus::Document
 * \sa CppEditor::WorkingCopy
 */

using namespace CPlusPlus;

using Message = Document::DiagnosticMessage;

namespace CppEditor::Internal {

static Q_LOGGING_CATEGORY(log, "qtc.cppeditor.sourceprocessor", QtWarningMsg)

namespace {

inline QByteArray generateFingerPrint(const QList<CPlusPlus::Macro> &definedMacros,
                                      const QByteArray &code)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(code);
    for (const CPlusPlus::Macro &macro : definedMacros) {
        if (macro.isHidden()) {
            static const QByteArray undef("#undef ");
            hash.addData(undef);
            hash.addData(macro.name());
        } else {
            static const QByteArray def("#define ");
            hash.addData(macro.name());
            hash.addData(" ", 1);
            hash.addData(def);
            hash.addData(macro.definitionText());
        }
        hash.addData("\n", 1);
    }
    return hash.result();
}

inline Message messageNoSuchFile(Document::Ptr &document, const QString &fileName, unsigned line)
{
    const QString text = QCoreApplication::translate(
        "CppSourceProcessor", "%1: No such file or directory").arg(fileName);
    return Message(Message::Warning, document->fileName(), line, /*column =*/ 0, text);
}

inline Message messageNoFileContents(Document::Ptr &document, const QString &fileName,
                                     unsigned line)
{
    const QString text = QCoreApplication::translate(
        "CppSourceProcessor", "%1: Could not get file contents").arg(fileName);
    return Message(Message::Warning, document->fileName(), line, /*column =*/ 0, text);
}

inline const CPlusPlus::Macro revision(const WorkingCopy &workingCopy,
                                       const CPlusPlus::Macro &macro)
{
    CPlusPlus::Macro newMacro(macro);
    newMacro.setFileRevision(workingCopy.get(macro.fileName()).second);
    return newMacro;
}

} // anonymous namespace

CppSourceProcessor::CppSourceProcessor(const Snapshot &snapshot, DocumentCallback documentFinished)
    : m_snapshot(snapshot),
      m_documentFinished(documentFinished),
      m_preprocess(this, &m_env),
      m_languageFeatures(LanguageFeatures::defaultFeatures()),
      m_defaultCodec(Core::EditorManager::defaultTextCodec())
{
    m_preprocess.setKeepComments(true);
}

CppSourceProcessor::~CppSourceProcessor() = default;

void CppSourceProcessor::setCancelChecker(const CppSourceProcessor::CancelChecker &cancelChecker)
{
    m_preprocess.setCancelChecker(cancelChecker);
}

void CppSourceProcessor::setWorkingCopy(const WorkingCopy &workingCopy)
{ m_workingCopy = workingCopy; }

void CppSourceProcessor::setHeaderPaths(const ProjectExplorer::HeaderPaths &headerPaths)
{
    using ProjectExplorer::HeaderPathType;
    m_headerPaths.clear();

    for (const auto &path : headerPaths) {
         if (path.type == HeaderPathType::Framework )
            addFrameworkPath(path);
        else
            m_headerPaths.append({cleanPath(path.path), path.type});
    }
}

void CppSourceProcessor::setLanguageFeatures(const LanguageFeatures languageFeatures)
{
    m_languageFeatures = languageFeatures;
}

// Add the given framework path, and expand private frameworks.
//
// Example:
//  <framework-path>/ApplicationServices.framework
// has private frameworks in:
//  <framework-path>/ApplicationServices.framework/Frameworks
// if the "Frameworks" folder exists inside the top level framework.
void CppSourceProcessor::addFrameworkPath(const ProjectExplorer::HeaderPath &frameworkPath)
{
    QTC_ASSERT(frameworkPath.type == ProjectExplorer::HeaderPathType::Framework, return);

    // The algorithm below is a bit too eager, but that's because we're not getting
    // in the frameworks we're linking against. If we would have that, then we could
    // add only those private frameworks.
    const auto cleanFrameworkPath = ProjectExplorer::HeaderPath::makeFramework(
                cleanPath(frameworkPath.path));
    if (!m_headerPaths.contains(cleanFrameworkPath))
        m_headerPaths.append(cleanFrameworkPath);

    const QDir frameworkDir(cleanFrameworkPath.path);
    const QStringList filter = QStringList("*.framework");
    const QList<QFileInfo> frameworks = frameworkDir.entryInfoList(filter);
    for (const QFileInfo &framework : frameworks) {
        if (!framework.isDir())
            continue;
        const QFileInfo privateFrameworks(framework.absoluteFilePath(),
                                          QLatin1String("Frameworks"));
        if (privateFrameworks.exists() && privateFrameworks.isDir())
            addFrameworkPath(ProjectExplorer::HeaderPath::makeFramework(
                                 privateFrameworks.absoluteFilePath()));
    }
}

void CppSourceProcessor::setTodo(const QSet<QString> &files)
{
    m_todo = files;
}

void CppSourceProcessor::run(const QString &fileName,
                             const QStringList &initialIncludes)
{
    sourceNeeded(0, fileName, IncludeGlobal, initialIncludes);
}

void CppSourceProcessor::removeFromCache(const QString &fileName)
{
    m_snapshot.remove(fileName);
}

void CppSourceProcessor::resetEnvironment()
{
    m_env.reset();
    m_processed.clear();
    m_included.clear();
}

bool CppSourceProcessor::getFileContents(const QString &absoluteFilePath,
                                         QByteArray *contents,
                                         unsigned *revision) const
{
    if (absoluteFilePath.isEmpty() || !contents || !revision)
        return false;

    // Get from working copy
    if (m_workingCopy.contains(absoluteFilePath)) {
        const QPair<QByteArray, unsigned> entry = m_workingCopy.get(absoluteFilePath);
        *contents = entry.first;
        *revision = entry.second;
        return true;
    }

    // Get from file
    *revision = 0;
    QString error;
    if (Utils::TextFileFormat::readFileUTF8(Utils::FilePath::fromString(absoluteFilePath),
                                            m_defaultCodec,
                                            contents,
                                            &error)
        != Utils::TextFileFormat::ReadSuccess) {
        qWarning("Error reading file \"%s\": \"%s\".", qPrintable(absoluteFilePath),
                 qPrintable(error));
        return false;
    }
    contents->replace("\r\n", "\n");
    return true;
}

bool CppSourceProcessor::checkFile(const QString &absoluteFilePath) const
{
    if (absoluteFilePath.isEmpty()
            || m_included.contains(absoluteFilePath)
            || m_workingCopy.contains(absoluteFilePath)) {
        return true;
    }

    const QFileInfo fileInfo(absoluteFilePath);
    return fileInfo.isFile() && fileInfo.isReadable();
}

QString CppSourceProcessor::cleanPath(const QString &path)
{
    QString result = QDir::cleanPath(path);
    const QChar slash(QLatin1Char('/'));
    if (!result.endsWith(slash))
        result.append(slash);
    return result;
}

/// Resolve the given file name to its absolute path w.r.t. the include type.
QString CppSourceProcessor::resolveFile(const QString &fileName, IncludeType type)
{
    if (isInjectedFile(fileName))
        return fileName;

    if (QFileInfo(fileName).isAbsolute())
        return checkFile(fileName) ? fileName : QString();

    if (m_currentDoc) {
        if (type == IncludeLocal) {
            const QFileInfo currentFileInfo(m_currentDoc->fileName());
            const QString path = cleanPath(currentFileInfo.absolutePath()) + fileName;
            if (checkFile(path))
                return path;
            // Fall through! "16.2 Source file inclusion" from the standard states to continue
            // searching as if this would be a global include.

        } else if (type == IncludeNext) {
            const QFileInfo currentFileInfo(m_currentDoc->fileName());
            const QString currentDirPath = cleanPath(currentFileInfo.dir().path());
            auto headerPathsEnd = m_headerPaths.end();
            auto headerPathsIt = m_headerPaths.begin();
            for (; headerPathsIt != headerPathsEnd; ++headerPathsIt) {
                if (headerPathsIt->path == currentDirPath) {
                    ++headerPathsIt;
                    return resolveFile_helper(fileName, headerPathsIt);
                }
            }
        }
    }

    QHash<QString, QString>::ConstIterator it = m_fileNameCache.constFind(fileName);
    if (it != m_fileNameCache.constEnd())
        return it.value();
    const QString fn = resolveFile_helper(fileName, m_headerPaths.begin());
    if (!fn.isEmpty())
        m_fileNameCache.insert(fileName, fn);
    return fn;
}

QString CppSourceProcessor::resolveFile_helper(const QString &fileName,
                                               ProjectExplorer::HeaderPaths::Iterator headerPathsIt)
{
    auto headerPathsEnd = m_headerPaths.end();
    const int index = fileName.indexOf(QLatin1Char('/'));
    for (; headerPathsIt != headerPathsEnd; ++headerPathsIt) {
        if (!headerPathsIt->path.isNull()) {
            QString path;
            if (headerPathsIt->type == ProjectExplorer::HeaderPathType::Framework) {
                if (index == -1)
                    continue;
                path = headerPathsIt->path + fileName.left(index)
                       + QLatin1String(".framework/Headers/") + fileName.mid(index + 1);
            } else {
                path = headerPathsIt->path + fileName;
            }
            if (m_workingCopy.contains(path) || checkFile(path))
                return path;
        }
    }

    return QString();
}

void CppSourceProcessor::macroAdded(const CPlusPlus::Macro &macro)
{
    if (!m_currentDoc)
        return;

    m_currentDoc->appendMacro(macro);
}

void CppSourceProcessor::passedMacroDefinitionCheck(int bytesOffset, int utf16charsOffset,
                                                    int line, const CPlusPlus::Macro &macro)
{
    if (!m_currentDoc)
        return;

    m_currentDoc->addMacroUse(revision(m_workingCopy, macro),
                              bytesOffset, macro.name().length(),
                              utf16charsOffset, macro.nameToQString().size(),
                              line, QVector<MacroArgumentReference>());
}

void CppSourceProcessor::failedMacroDefinitionCheck(int bytesOffset, int utf16charOffset,
                                                    const ByteArrayRef &name)
{
    if (!m_currentDoc)
        return;

    m_currentDoc->addUndefinedMacroUse(QByteArray(name.start(), name.size()),
                                       bytesOffset, utf16charOffset);
}

void CppSourceProcessor::notifyMacroReference(int bytesOffset, int utf16charOffset,
                                              int line, const CPlusPlus::Macro &macro)
{
    if (!m_currentDoc)
        return;

    m_currentDoc->addMacroUse(revision(m_workingCopy, macro),
                              bytesOffset, macro.name().length(),
                              utf16charOffset, macro.nameToQString().size(),
                              line, QVector<MacroArgumentReference>());
}

void CppSourceProcessor::startExpandingMacro(int bytesOffset, int utf16charOffset,
                                             int line, const CPlusPlus::Macro &macro,
                                             const QVector<MacroArgumentReference> &actuals)
{
    if (!m_currentDoc)
        return;

    m_currentDoc->addMacroUse(revision(m_workingCopy, macro),
                              bytesOffset, macro.name().length(),
                              utf16charOffset, macro.nameToQString().size(),
                              line, actuals);
}

void CppSourceProcessor::stopExpandingMacro(int, const CPlusPlus::Macro &)
{
    if (!m_currentDoc)
        return;
}

void CppSourceProcessor::markAsIncludeGuard(const QByteArray &macroName)
{
    if (!m_currentDoc)
        return;

    m_currentDoc->setIncludeGuardMacroName(macroName);
}

void CppSourceProcessor::mergeEnvironment(Document::Ptr doc)
{
    if (!doc)
        return;

    const QString fn = doc->fileName();

    if (m_processed.contains(fn))
        return;

    m_processed.insert(fn);

    const QList<Document::Include> includes = doc->resolvedIncludes();
    for (const Document::Include &incl : includes) {
        const QString includedFile = incl.resolvedFileName();

        if (Document::Ptr includedDoc = m_snapshot.document(includedFile))
            mergeEnvironment(includedDoc);
        else if (!m_included.contains(includedFile))
            run(includedFile);
    }

    m_env.addMacros(doc->definedMacros());
}

void CppSourceProcessor::startSkippingBlocks(int utf16charsOffset)
{
    if (m_currentDoc)
        m_currentDoc->startSkippingBlocks(utf16charsOffset);
}

void CppSourceProcessor::stopSkippingBlocks(int utf16charsOffset)
{
    if (m_currentDoc)
        m_currentDoc->stopSkippingBlocks(utf16charsOffset);
}

void CppSourceProcessor::sourceNeeded(int line, const QString &fileName, IncludeType type,
                                      const QStringList &initialIncludes)
{
    if (fileName.isEmpty())
        return;

    QString absoluteFileName = resolveFile(fileName, type);
    absoluteFileName = QDir::cleanPath(absoluteFileName);
    if (m_currentDoc) {
        m_currentDoc->addIncludeFile(Document::Include(fileName, absoluteFileName, line, type));
        if (absoluteFileName.isEmpty()) {
            m_currentDoc->addDiagnosticMessage(messageNoSuchFile(m_currentDoc, fileName, line));
            return;
        }
    }
    if (m_included.contains(absoluteFileName))
        return; // We've already seen this file.
    if (!isInjectedFile(absoluteFileName))
        m_included.insert(absoluteFileName);

    // Already in snapshot? Use it!
    if (Document::Ptr document = m_snapshot.document(absoluteFileName)) {
        mergeEnvironment(document);
        return;
    }

    const QFileInfo info(absoluteFileName);
    if (fileSizeExceedsLimit(info, m_fileSizeLimitInMb))
        return; // TODO: Add diagnostic message

    // Otherwise get file contents
    unsigned editorRevision = 0;
    QByteArray contents;
    const bool gotFileContents = getFileContents(absoluteFileName, &contents, &editorRevision);
    if (m_currentDoc && !gotFileContents) {
        m_currentDoc->addDiagnosticMessage(messageNoFileContents(m_currentDoc, fileName, line));
        return;
    }

    qCDebug(log) << "Parsing:" << absoluteFileName << "contents:" << contents.size() << "bytes";

    Document::Ptr document = Document::create(absoluteFileName);
    document->setEditorRevision(editorRevision);
    document->setLanguageFeatures(m_languageFeatures);
    for (const QString &include : initialIncludes) {
        m_included.insert(include);
        Document::Include inc(include, include, 0, IncludeLocal);
        document->addIncludeFile(inc);
    }
    if (info.exists())
        document->setLastModified(info.lastModified());

    const Document::Ptr previousDocument = switchCurrentDocument(document);
    const QByteArray preprocessedCode = m_preprocess.run(absoluteFileName, contents);
//    {
//        QByteArray b(preprocessedCode); b.replace("\n", "<<<\n");
//        qDebug("Preprocessed code for \"%s\": [[%s]]", fileName.toUtf8().constData(), b.constData());
//    }
    document->setFingerprint(generateFingerPrint(document->definedMacros(), preprocessedCode));

    // Re-use document from global snapshot if possible
    Document::Ptr globalDocument = m_globalSnapshot.document(absoluteFileName);
    if (globalDocument && globalDocument->fingerprint() == document->fingerprint()) {
        switchCurrentDocument(previousDocument);
        mergeEnvironment(globalDocument);
        m_snapshot.insert(globalDocument);
        m_todo.remove(absoluteFileName);
        return;
    }

    // Otherwise process the document
    document->setUtf8Source(preprocessedCode);
    document->keepSourceAndAST();
    document->tokenize();
    document->check(m_workingCopy.contains(document->fileName()) ? Document::FullCheck
                                                                 : Document::FastCheck);

    m_documentFinished(document);

    m_snapshot.insert(document);
    m_todo.remove(absoluteFileName);
    switchCurrentDocument(previousDocument);
}

void CppSourceProcessor::setFileSizeLimitInMb(int fileSizeLimitInMb)
{
    m_fileSizeLimitInMb = fileSizeLimitInMb;
}

Document::Ptr CppSourceProcessor::switchCurrentDocument(Document::Ptr doc)
{
    const Document::Ptr previousDoc = m_currentDoc;
    m_currentDoc = doc;
    return previousDoc;
}

} // namespace CppEditor::Internal
