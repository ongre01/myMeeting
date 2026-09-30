#include "applicationlog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>

namespace {
QMutex logMutex;
QFile logFile;
QElapsedTimer applicationClock;

QString eventName(ApplicationLog::Event event)
{
    using Event = ApplicationLog::Event;
    switch (event) {
    case Event::ApplicationStarted: return QStringLiteral("application_started");
    case Event::ApplicationStopped: return QStringLiteral("application_stopped");
    case Event::RecordingStarted: return QStringLiteral("recording_started");
    case Event::RecordingCompleted: return QStringLiteral("recording_completed");
    case Event::RecordingError: return QStringLiteral("recording_error");
    case Event::BackendStarted: return QStringLiteral("backend_started");
    case Event::TranscriptionStarted: return QStringLiteral("transcription_started");
    case Event::TranscriptionCompleted: return QStringLiteral("transcription_completed");
    case Event::TranscriptionFailed: return QStringLiteral("transcription_failed");
    case Event::AnalysisStarted: return QStringLiteral("analysis_started");
    case Event::AnalysisCompleted: return QStringLiteral("analysis_completed");
    case Event::AnalysisFailed: return QStringLiteral("analysis_failed");
    case Event::BackendCompleted: return QStringLiteral("backend_completed");
    case Event::BackendFailed: return QStringLiteral("backend_failed");
    case Event::WorkflowFailed: return QStringLiteral("workflow_failed");
    case Event::ExportCompleted: return QStringLiteral("export_completed");
    case Event::ExportFailed: return QStringLiteral("export_failed");
    }
    return QStringLiteral("unknown");
}

bool isError(ApplicationLog::Event event)
{
    using Event = ApplicationLog::Event;
    return event == Event::RecordingError
           || event == Event::TranscriptionFailed
           || event == Event::AnalysisFailed
           || event == Event::BackendFailed
           || event == Event::WorkflowFailed
           || event == Event::ExportFailed;
}

QString exportFormatName(ApplicationLog::ExportFormat format)
{
    using ExportFormat = ApplicationLog::ExportFormat;
    switch (format) {
    case ExportFormat::Markdown: return QStringLiteral("markdown");
    case ExportFormat::Text: return QStringLiteral("text");
    case ExportFormat::Json: return QStringLiteral("json");
    case ExportFormat::None: return {};
    }
    return {};
}

QString defaultLogPath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("logs/meeting-minutes.log"));
}

void writeRecord(ApplicationLog::Event event,
                 qint64 durationMilliseconds,
                 ApplicationLog::ExportFormat exportFormat)
{
    if (!logFile.isOpen()) {
        return;
    }

    QJsonObject entry{
        {QStringLiteral("timestamp"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("level"),
         isError(event) ? QStringLiteral("error") : QStringLiteral("info")},
        {QStringLiteral("event"), eventName(event)}
    };
    if (durationMilliseconds >= 0) {
        entry.insert(QStringLiteral("duration_ms"), durationMilliseconds);
    }
    const QString formatName = exportFormatName(exportFormat);
    if (!formatName.isEmpty()) {
        entry.insert(QStringLiteral("format"), formatName);
    }

    QByteArray line = QJsonDocument(entry).toJson(QJsonDocument::Compact);
    line.append('\n');
    if (logFile.write(line) == line.size()) {
        logFile.flush();
    }
}
} // namespace

namespace ApplicationLog {

bool initialize(const QString &customFilePath)
{
    QMutexLocker<QMutex> locker(&logMutex);
    if (logFile.isOpen()) {
        return true;
    }

    const QString requestedPath = customFilePath.trimmed().isEmpty()
                                      ? defaultLogPath()
                                      : QFileInfo(customFilePath).absoluteFilePath();
    const QFileInfo info(requestedPath);
    if (!QDir().mkpath(info.absolutePath())) {
        return false;
    }

    logFile.setFileName(info.absoluteFilePath());
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        logFile.setFileName(QString());
        return false;
    }

    applicationClock.start();
    writeRecord(Event::ApplicationStarted, -1, ExportFormat::None);
    return true;
}

void shutdown()
{
    QMutexLocker<QMutex> locker(&logMutex);
    if (!logFile.isOpen()) {
        return;
    }

    const qint64 duration = applicationClock.isValid()
                                ? applicationClock.elapsed()
                                : -1;
    writeRecord(Event::ApplicationStopped, duration, ExportFormat::None);
    logFile.close();
    logFile.setFileName(QString());
    applicationClock.invalidate();
}

bool isInitialized()
{
    QMutexLocker<QMutex> locker(&logMutex);
    return logFile.isOpen();
}

QString filePath()
{
    QMutexLocker<QMutex> locker(&logMutex);
    return logFile.fileName();
}

void record(Event event,
            qint64 durationMilliseconds,
            ExportFormat exportFormat)
{
    QMutexLocker<QMutex> locker(&logMutex);
    writeRecord(event, durationMilliseconds, exportFormat);
}

} // namespace ApplicationLog
