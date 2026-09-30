#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "aibackendclient.h"
#include "audiorecorder.h"
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

    Ui::MainWindow *ui;
    AudioRecorder *m_audioRecorder;
    AiBackendClient *m_aiBackendClient;
    MeetingStorage m_storage;
    QTimer *m_elapsedTimer;
    QElapsedTimer m_elapsedClock;
    QString m_currentMeetingDirectory;
    QString m_currentWavPath;
    QString m_lastMeetingDirectory;
    QString m_lastRecordingPath;
    QString m_lastTranscriptPath;
    bool m_recordingStarted = false;
};
#endif // MAINWINDOW_H
