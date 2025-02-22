// Copyright (C) 2022 The Qt Company Ltd
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/environment.h>
#include <utils/qtcprocess.h>

#include <QObject>
#include <QStringList>
#include <QWindowList>

#include <memory>

QT_BEGIN_NAMESPACE
class QFile;
class QFileSystemWatcher;
QT_END_NAMESPACE

namespace Squish {
namespace Internal {

class SquishXmlOutputHandler;

class SquishTools : public QObject
{
    Q_OBJECT
public:
    explicit SquishTools(QObject *parent = nullptr);
    ~SquishTools() override;

    static SquishTools *instance();

    enum State {
        Idle,
        ServerStarting,
        ServerStarted,
        ServerStartFailed,
        ServerStopped,
        ServerStopFailed,
        RunnerStarting,
        RunnerStarted,
        RunnerStartFailed,
        RunnerStopped
    };

    State state() const { return m_state; }
    void runTestCases(const QString &suitePath,
                      const QStringList &testCases = QStringList(),
                      const QStringList &additionalServerArgs = QStringList(),
                      const QStringList &additionalRunnerArgs = QStringList());
    void queryServerSettings();
    void writeServerSettingsChanges(const QList<QStringList> &changes);

signals:
    void logOutputReceived(const QString &output);
    void squishTestRunStarted();
    void squishTestRunFinished();
    void resultOutputCreated(const QByteArray &output);
    void queryFinished(const QByteArray &output);
    void configChangesFailed(QProcess::ProcessError error);
    void configChangesWritten();

private:
    enum Request {
        None,
        ServerStopRequested,
        ServerConfigChangeRequested,
        RunnerQueryRequested,
        RunTestRequested,
        RecordTestRequested,
        KillOldBeforeRunRunner,
        KillOldBeforeRecordRunner,
        KillOldBeforeQueryRunner
    };

    void setState(State state);
    void startSquishServer(Request request);
    void stopSquishServer();
    void startSquishRunner();
    void executeRunnerQuery();
    static Utils::Environment squishEnvironment();
    void onServerFinished();
    void onRunnerFinished();
    void onServerOutput();
    void onServerErrorOutput();
    void onRunnerOutput();
    void onRunnerErrorOutput();
    void onResultsDirChanged(const QString &filePath);
    static void logrotateTestResults();
    void minimizeQtCreatorWindows();
    void restoreQtCreatorWindows();
    bool isValidToStartRunner();
    bool setupRunnerPath();
    void setupAndStartSquishRunnerProcess(const QStringList &arg,
                                          const QString &caseReportFilePath = {});

    std::unique_ptr<SquishXmlOutputHandler> m_xmlOutputHandler;
    Utils::QtcProcess m_serverProcess;
    Utils::QtcProcess m_runnerProcess;
    int m_serverPort = -1;
    QString m_serverHost;
    Request m_request = None;
    State m_state = Idle;
    QString m_suitePath;
    QStringList m_testCases;
    QStringList m_reportFiles;
    QString m_currentResultsDirectory;
    QFile *m_currentResultsXML = nullptr;
    QFileSystemWatcher *m_resultsFileWatcher = nullptr;
    QStringList m_additionalServerArguments;
    QStringList m_additionalRunnerArguments;
    QList<QStringList> m_serverConfigChanges;
    QWindowList m_lastTopLevelWindows;
    enum RunnerMode { NoMode, TestingMode, QueryMode} m_squishRunnerMode = NoMode;
    qint64 m_readResultsCount;
};

} // namespace Internal
} // namespace Squish
