#ifndef WAVFILEWRITER_H
#define WAVFILEWRITER_H

#include <QAudioFormat>
#include <QFile>
#include <QIODevice>
#include <QString>

class WavFileWriter : public QIODevice
{
    Q_OBJECT

public:
    explicit WavFileWriter(QObject *parent = nullptr);
    ~WavFileWriter() override;

    bool openFile(const QString &filePath,
                  const QAudioFormat &format,
                  QString *errorMessage = nullptr);
    bool finalize(QString *errorMessage = nullptr);

    QString filePath() const;
    quint64 pcmBytesWritten() const;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    static bool isSupportedFormat(const QAudioFormat &format);
    static QByteArray wavHeader(const QAudioFormat &format, quint32 dataSize);
    void reportError(const QString &message, QString *errorMessage);

    QFile m_file;
    QAudioFormat m_format;
    quint64 m_pcmBytesWritten = 0;
    bool m_finalized = false;
};

#endif // WAVFILEWRITER_H
