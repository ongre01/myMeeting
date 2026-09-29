#include "wavfilewriter.h"

#include <QDataStream>

#include <limits>

namespace {
constexpr qint64 wavHeaderSize = 44;
constexpr quint32 pcmFormatCode = 1;
constexpr quint64 maximumPcmDataSize =
    static_cast<quint64>(std::numeric_limits<quint32>::max()) - 36u;
}

WavFileWriter::WavFileWriter(QObject *parent)
    : QIODevice(parent)
{
}

WavFileWriter::~WavFileWriter()
{
    if (isOpen()) {
        finalize();
    }
}

bool WavFileWriter::openFile(const QString &filePath,
                             const QAudioFormat &format,
                             QString *errorMessage)
{
    if (isOpen()) {
        reportError(QStringLiteral("WAV 파일이 이미 열려 있습니다."), errorMessage);
        return false;
    }

    if (filePath.trimmed().isEmpty()) {
        reportError(QStringLiteral("WAV 저장 경로가 비어 있습니다."), errorMessage);
        return false;
    }

    if (!isSupportedFormat(format)) {
        reportError(
            QStringLiteral("WAV 형식은 16 kHz 또는 48 kHz, 16-bit mono PCM이어야 합니다."),
            errorMessage);
        return false;
    }

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reportError(QStringLiteral("WAV 파일을 열 수 없습니다: %1")
                        .arg(m_file.errorString()),
                    errorMessage);
        return false;
    }

    m_format = format;
    m_pcmBytesWritten = 0;
    m_finalized = false;

    const QByteArray header = wavHeader(m_format, 0);
    if (m_file.write(header) != wavHeaderSize) {
        const QString fileError = m_file.errorString();
        m_file.close();
        m_file.remove();
        reportError(QStringLiteral("WAV 헤더를 저장할 수 없습니다: %1")
                        .arg(fileError),
                    errorMessage);
        return false;
    }

    if (!QIODevice::open(QIODevice::WriteOnly)) {
        m_file.close();
        m_file.remove();
        reportError(QStringLiteral("WAV 출력 장치를 열 수 없습니다."), errorMessage);
        return false;
    }

    return true;
}

bool WavFileWriter::finalize(QString *errorMessage)
{
    if (m_finalized) {
        return true;
    }

    if (!isOpen() || !m_file.isOpen()) {
        reportError(QStringLiteral("완료할 WAV 파일이 열려 있지 않습니다."),
                    errorMessage);
        return false;
    }

    bool succeeded = m_file.flush();
    QString failure;

    if (!succeeded) {
        failure = QStringLiteral("WAV 데이터를 저장할 수 없습니다: %1")
                      .arg(m_file.errorString());
    }

    const QByteArray header = wavHeader(
        m_format, static_cast<quint32>(m_pcmBytesWritten));
    if (succeeded
        && (!m_file.seek(0) || m_file.write(header) != wavHeaderSize
            || !m_file.flush())) {
        succeeded = false;
        failure = QStringLiteral("WAV 헤더를 완료할 수 없습니다: %1")
                      .arg(m_file.errorString());
    }

    m_file.close();
    QIODevice::close();
    m_finalized = true;

    if (!succeeded) {
        reportError(failure, errorMessage);
    }

    return succeeded;
}

QString WavFileWriter::filePath() const
{
    return m_file.fileName();
}

quint64 WavFileWriter::pcmBytesWritten() const
{
    return m_pcmBytesWritten;
}

qint64 WavFileWriter::readData(char *, qint64)
{
    return -1;
}

qint64 WavFileWriter::writeData(const char *data, qint64 maxSize)
{
    if (!m_file.isOpen()) {
        setErrorString(QStringLiteral("WAV 파일이 열려 있지 않습니다."));
        return -1;
    }

    if (maxSize <= 0) {
        return 0;
    }

    if (static_cast<quint64>(maxSize)
        > maximumPcmDataSize - m_pcmBytesWritten) {
        setErrorString(QStringLiteral("WAV 파일이 4 GiB 크기 제한을 초과했습니다."));
        return -1;
    }

    const qint64 written = m_file.write(data, maxSize);
    if (written < 0) {
        setErrorString(QStringLiteral("WAV 데이터를 저장할 수 없습니다: %1")
                           .arg(m_file.errorString()));
        return -1;
    }

    m_pcmBytesWritten += static_cast<quint64>(written);
    return written;
}

bool WavFileWriter::isSupportedFormat(const QAudioFormat &format)
{
    return format.isValid()
           && format.sampleFormat() == QAudioFormat::Int16
           && format.channelCount() == 1
           && (format.sampleRate() == 16000
               || format.sampleRate() == 48000);
}

QByteArray WavFileWriter::wavHeader(const QAudioFormat &format,
                                    quint32 dataSize)
{
    QByteArray header;
    header.reserve(wavHeaderSize);

    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    const quint16 channels = static_cast<quint16>(format.channelCount());
    const quint16 bitsPerSample =
        static_cast<quint16>(format.bytesPerSample() * 8);
    const quint32 sampleRate = static_cast<quint32>(format.sampleRate());
    const quint16 blockAlign = static_cast<quint16>(
        channels * format.bytesPerSample());
    const quint32 byteRate = sampleRate * blockAlign;

    stream.writeRawData("RIFF", 4);
    stream << static_cast<quint32>(36u + dataSize);
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << static_cast<quint32>(16u);
    stream << static_cast<quint16>(pcmFormatCode);
    stream << channels;
    stream << sampleRate;
    stream << byteRate;
    stream << blockAlign;
    stream << bitsPerSample;
    stream.writeRawData("data", 4);
    stream << dataSize;

    return header;
}

void WavFileWriter::reportError(const QString &message,
                                QString *errorMessage)
{
    setErrorString(message);
    if (errorMessage) {
        *errorMessage = message;
    }
}
