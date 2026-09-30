#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "aibackendclient.h"
#include "audiorecorder.h"
#include "meetingexporter.h"
#include "meetingminutes.h"
#include "meetingstorage.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    MainWindow(AudioRecorder *audioRecorder,
               const QString &meetingsRoot,
               QWidget *parent = nullptr);
    MainWindow(AudioRecorder *audioRecorder,
               AiBackendClient *aiBackendClient,
               const QString &meetingsRoot,
               QWidget *parent = nullptr);
    ~MainWindow() override;

    bool hasCurrentMinutes() const;
    const MeetingMinutes &currentMinutes() const;

signals:
    void minutesChanged();

private slots:
    void refreshInputDevices();
    void startRecording();
    void stopRecording();
    void handleRecordingStarted();
    void handleRecordingStopped(const QString &filePath);
    void handleRecordingError(const QString &message);
    void updateElapsedTime();
    void startBackendProcessing();
    void saveTranscript();
    void reloadTranscript();
    void handleTranscriptChanged();
    void handleBackendStateChanged(AiBackendClient::State state,
                                   const QString &message);
    void handleBackendCompleted(const QString &outputPath,
                                const QString &transcriptPath);
    void handleBackendFailed(const QString &message);
    void handleMinutesEditorChanged();
    void addTopic();
    void removeTopic();
    void addDecision();
    void removeDecision();
    void addActionItem();
    void removeActionItem();
    void addOpenIssue();
    void removeOpenIssue();
    void exportMarkdownMinutes();
    void exportTextMinutes();
    void exportJsonMinutes();

private:
    MainWindow(AudioRecorder *audioRecorder,
               AiBackendClient *aiBackendClient,
               const QString &meetingsRoot,
               bool takeRecorderOwnership,
               bool takeBackendOwnership,
               QWidget *parent);

    static QString defaultMeetingsRoot();
    void initialize();
    void setRecordingControls(bool recording);
    void discardUnusedMeeting();
    bool saveTranscriptToDisk(QString *errorMessage);
    bool loadTranscriptFromDisk(const QString &path, QString *errorMessage);
    bool loadMinutesFromDisk(const QString &path, QString *errorMessage);
    void exportCurrentMinutes(MeetingExporter::Format format);
    void showExportFailure(const QString &message);
    void populateMinutesEditor();
    void clearMinutesEditor();
    void updateMinutesEmptyStates();
    void setMinutesEditorEnabled(bool enabled);

    Ui::MainWindow *ui;
    AudioRecorder *m_audioRecorder;
    AiBackendClient *m_aiBackendClient;
    MeetingStorage m_storage;
    QTimer *m_elapsedTimer;
    QElapsedTimer m_elapsedClock;
    QElapsedTimer m_backendClock;
    QElapsedTimer m_backendPhaseClock;
    QString m_currentMeetingDirectory;
    QString m_currentWavPath;
    QString m_lastMeetingDirectory;
    QString m_lastRecordingPath;
    QString m_lastTranscriptPath;
    QString m_currentMinutesPath;
    MeetingMinutes m_currentMinutes;
    bool m_recordingStarted = false;
    bool m_hasCurrentMinutes = false;
    bool m_populatingMinutesEditor = false;
    bool m_backendOperationActive = false;
    AiBackendClient::State m_loggedBackendPhase = AiBackendClient::State::Idle;
};
#endif // MAINWINDOW_H
