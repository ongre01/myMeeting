#ifndef APPLICATIONLOG_H
#define APPLICATIONLOG_H

#include <QString>
#include <QtGlobal>

namespace ApplicationLog {

enum class Event {
    ApplicationStarted,
    ApplicationStopped,
    RecordingStarted,
    RecordingCompleted,
    RecordingError,
    BackendStarted,
    TranscriptionStarted,
    TranscriptionCompleted,
    TranscriptionFailed,
    AnalysisStarted,
    AnalysisCompleted,
    AnalysisFailed,
    BackendCompleted,
    BackendFailed,
    WorkflowFailed,
    ExportCompleted,
    ExportFailed
};

enum class ExportFormat {
    None,
    Markdown,
    Text,
    Json
};

// The logger deliberately accepts no free-form text or paths. This keeps meeting
// audio, transcripts, generated minutes, and user-facing errors out of the log.
bool initialize(const QString &customFilePath = QString());
void shutdown();
bool isInitialized();
QString filePath();
void record(Event event,
            qint64 durationMilliseconds = -1,
            ExportFormat exportFormat = ExportFormat::None);

} // namespace ApplicationLog

#endif // APPLICATIONLOG_H
