#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QAudioDevice>
#include <QAudioFormat>
#include <QList>
#include <QObject>
#include <QString>

#include <memory>

class QAudioSource;
class WavFileWriter;

class AudioRecorder : public QObject
{
    Q_OBJECT

public:
    explicit AudioRecorder(QObject *parent = nullptr);
    ~AudioRecorder() override;

    static QList<QAudioDevice> availableInputDevices();
    static QAudioFormat supportedRecordingFormat(const QAudioDevice &device);

    bool startRecording(const QAudioDevice &device, const QString &filePath);
    void stopRecording();

    bool isRecording() const;
    QString filePath() const;
    QAudioFormat format() const;

signals:
    void recordingStarted();
    void recordingStopped(const QString &filePath);
    void recordingError(const QString &message);

private:
    void handleSourceStateChanged(QAudio::State state);
    void failActiveRecording(const QString &message);
    void clearCapture();
    QString sourceErrorMessage() const;

    std::unique_ptr<QAudioSource> m_audioSource;
    std::unique_ptr<WavFileWriter> m_output;
    QString m_filePath;
    QAudioFormat m_format;
    quint64 m_captureGeneration = 0;
    bool m_recording = false;
    bool m_stopRequested = false;
};

#endif // AUDIORECORDER_H
