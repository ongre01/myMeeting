#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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
    ~MainWindow() override;

private slots:
    void refreshInputDevices();
    void startRecording();
    void stopRecording();
    void handleRecordingStarted();
    void handleRecordingStopped(const QString &filePath);
    void handleRecordingError(const QString &message);
    void updateElapsedTime();

private:
    MainWindow(AudioRecorder *audioRecorder,
               const QString &meetingsRoot,
               bool takeRecorderOwnership,
               QWidget *parent);

    static QString defaultMeetingsRoot();
    void initialize();
    void setRecordingControls(bool recording);
    void discardUnusedMeeting();

    Ui::MainWindow *ui;
    AudioRecorder *m_audioRecorder;
    MeetingStorage m_storage;
    QTimer *m_elapsedTimer;
    QElapsedTimer m_elapsedClock;
    QString m_currentMeetingDirectory;
    QString m_currentWavPath;
    bool m_recordingStarted = false;
};
#endif // MAINWINDOW_H
