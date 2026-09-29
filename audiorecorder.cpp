#include "audiorecorder.h"

#include "wavfilewriter.h"

#include <QAudioSource>
#include <QMediaDevices>
#include <QTimer>

AudioRecorder::AudioRecorder(QObject *parent)
    : QObject(parent)
{
}

AudioRecorder::~AudioRecorder()
{
    if (m_audioSource) {
        disconnect(m_audioSource.get(), nullptr, this, nullptr);
        m_audioSource->stop();
    }

    if (m_output && m_output->isOpen()) {
        m_output->finalize();
    }
}

QList<QAudioDevice> AudioRecorder::availableInputDevices()
{
    return QMediaDevices::audioInputs();
}

QAudioFormat AudioRecorder::supportedRecordingFormat(
    const QAudioDevice &device)
{
    if (device.isNull() || device.mode() != QAudioDevice::Input) {
        return {};
    }

    for (const int sampleRate : {48000, 16000}) {
        QAudioFormat format;
        format.setSampleRate(sampleRate);
        format.setChannelCount(1);
        format.setSampleFormat(QAudioFormat::Int16);

        if (device.isFormatSupported(format)) {
            return format;
        }
    }

    return {};
}

bool AudioRecorder::startRecording(const QAudioDevice &device,
                                   const QString &filePath)
{
    if (m_recording) {
        emit recordingError(QStringLiteral("이미 녹음 중입니다."));
        return false;
    }

    if (device.isNull() || device.mode() != QAudioDevice::Input) {
        emit recordingError(QStringLiteral("사용할 수 있는 마이크가 선택되지 않았습니다."));
        return false;
    }

    const QAudioFormat recordingFormat = supportedRecordingFormat(device);
    if (!recordingFormat.isValid()) {
        emit recordingError(
            QStringLiteral("선택한 마이크가 16 kHz 또는 48 kHz, 16-bit mono PCM 녹음을 지원하지 않습니다."));
        return false;
    }

    auto audioSource = std::make_unique<QAudioSource>(device, recordingFormat);
    if (audioSource->isNull()) {
        emit recordingError(QStringLiteral("선택한 마이크를 열 수 없습니다."));
        return false;
    }

    auto output = std::make_unique<WavFileWriter>();
    QString errorMessage;
    if (!output->openFile(filePath, recordingFormat, &errorMessage)) {
        emit recordingError(errorMessage);
        return false;
    }

    m_audioSource = std::move(audioSource);
    m_output = std::move(output);
    m_filePath = filePath;
    m_format = recordingFormat;
    ++m_captureGeneration;
    m_recording = true;
    m_stopRequested = false;

    connect(m_audioSource.get(), &QAudioSource::stateChanged,
            this, &AudioRecorder::handleSourceStateChanged);

    m_audioSource->start(m_output.get());
    if (m_audioSource->error() != QAudio::NoError) {
        const QString sourceError = sourceErrorMessage();
        failActiveRecording(sourceError);
        return false;
    }

    emit recordingStarted();
    return true;
}

void AudioRecorder::stopRecording()
{
    if (!m_recording) {
        return;
    }

    m_stopRequested = true;
    const QString completedFilePath = m_filePath;

    if (m_audioSource) {
        disconnect(m_audioSource.get(), nullptr, this, nullptr);
        m_audioSource->stop();
        m_audioSource.reset();
    }

    QString errorMessage;
    const bool finalized = m_output && m_output->finalize(&errorMessage);
    m_output.reset();
    clearCapture();

    if (!finalized) {
        emit recordingError(errorMessage.isEmpty()
                                ? QStringLiteral("WAV 파일을 완료할 수 없습니다.")
                                : errorMessage);
        return;
    }

    emit recordingStopped(completedFilePath);
}

bool AudioRecorder::isRecording() const
{
    return m_recording;
}

QString AudioRecorder::filePath() const
{
    return m_filePath;
}

QAudioFormat AudioRecorder::format() const
{
    return m_format;
}

void AudioRecorder::handleSourceStateChanged(QAudio::State state)
{
    if (state != QAudio::StoppedState || !m_recording || m_stopRequested) {
        return;
    }

    const QString errorMessage = sourceErrorMessage();
    const quint64 captureGeneration = m_captureGeneration;
    QTimer::singleShot(0, this, [this, errorMessage, captureGeneration] {
        if (m_recording && !m_stopRequested
            && m_captureGeneration == captureGeneration) {
            failActiveRecording(errorMessage);
        }
    });
}

void AudioRecorder::failActiveRecording(const QString &message)
{
    if (!m_recording) {
        return;
    }

    m_stopRequested = true;
    if (m_audioSource) {
        disconnect(m_audioSource.get(), nullptr, this, nullptr);
        m_audioSource->stop();
        m_audioSource.reset();
    }

    QString finalizeError;
    if (m_output && m_output->isOpen()) {
        m_output->finalize(&finalizeError);
    }
    m_output.reset();
    clearCapture();

    QString combinedMessage = message;
    if (!finalizeError.isEmpty()) {
        combinedMessage += QStringLiteral(" %1").arg(finalizeError);
    }
    emit recordingError(combinedMessage);
}

void AudioRecorder::clearCapture()
{
    m_filePath.clear();
    m_format = {};
    m_recording = false;
    m_stopRequested = false;
}

QString AudioRecorder::sourceErrorMessage() const
{
    if (!m_audioSource) {
        return QStringLiteral("마이크 녹음이 중지되었습니다.");
    }

    switch (m_audioSource->error()) {
    case QAudio::NoError:
        return QStringLiteral("마이크 녹음이 예기치 않게 중지되었습니다.");
    case QAudio::OpenError:
        return QStringLiteral("선택한 마이크를 열 수 없습니다.");
    case QAudio::IOError:
        return QStringLiteral("마이크에서 음성 데이터를 읽을 수 없습니다.");
    case QAudio::FatalError:
        return QStringLiteral("마이크 녹음 중 복구할 수 없는 오류가 발생했습니다.");
    default:
        break;
    }

    return QStringLiteral("마이크 녹음 중 알 수 없는 오류가 발생했습니다.");
}
