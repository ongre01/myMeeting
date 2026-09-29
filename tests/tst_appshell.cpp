#include "audiorecorder.h"
#include "mainwindow.h"
#include "meetingstorage.h"
#include "wavfilewriter.h"

#include <QAudioFormat>
#include <QDir>
#include <QFile>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

namespace {
QAudioFormat monoPcmFormat(int sampleRate = 16000)
{
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

quint16 littleEndian16(const QByteArray &data, qsizetype offset)
{
    return qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar *>(data.constData() + offset));
}

quint32 littleEndian32(const QByteArray &data, qsizetype offset)
{
    return qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(data.constData() + offset));
}
} // namespace

class AppShellTest : public QObject
{
    Q_OBJECT

private slots:
    void createsEmptyMainWindow();
    void createsUniqueMeetingDirectoriesAndPaths();
    void rejectsUnavailableStorageLocation();
    void keepsTitleInsideStorageRoot();
    void rejectsInvalidStartTime();
    void writesPcm16MonoWav();
    void writesValidZeroSecondWav();
    void reportsWavStorageFailure();
    void rejectsMissingAudioDevice();
};

void AppShellTest::createsEmptyMainWindow()
{
    MainWindow window;

    QCOMPARE(window.windowTitle(),
             QStringLiteral("Local Meeting Minutes Assistant"));
    QVERIFY(window.centralWidget() != nullptr);
    QVERIFY(window.centralWidget()->children().isEmpty());
    QVERIFY(window.findChild<QStatusBar *>(QStringLiteral("statusbar")) != nullptr);
}

void AppShellTest::createsUniqueMeetingDirectoriesAndPaths()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    MeetingStorage storage(temporaryDirectory.path());
    const QDateTime startedAt(QDate(2026, 9, 29), QTime(10, 30, 0));

    const MeetingStorage::Result first = storage.createMeeting(
        QStringLiteral("주간 회의"), startedAt);
    const MeetingStorage::Result second = storage.createMeeting(
        QStringLiteral("주간 회의"), startedAt);
    const MeetingStorage::Result withoutTitle = storage.createMeeting(
        QString(), startedAt.addSecs(1));

    QVERIFY2(first.succeeded(), qPrintable(first.errorMessage));
    QVERIFY2(second.succeeded(), qPrintable(second.errorMessage));
    QVERIFY2(withoutTitle.succeeded(), qPrintable(withoutTitle.errorMessage));
    QVERIFY(first.paths.directory != second.paths.directory);
    QCOMPARE(QFileInfo(first.paths.directory).fileName(),
             QStringLiteral("2026-09-29_103000_주간 회의"));
    QCOMPARE(QFileInfo(second.paths.directory).fileName(),
             QStringLiteral("2026-09-29_103000_주간 회의_001"));
    QCOMPARE(QFileInfo(withoutTitle.paths.directory).fileName(),
             QStringLiteral("2026-09-29_103001"));
    QVERIFY(QFileInfo(first.paths.directory).isDir());
    QVERIFY(QFileInfo(second.paths.directory).isDir());
    QCOMPARE(first.paths.wav,
             QDir(first.paths.directory).filePath(QStringLiteral("meeting.wav")));
    QCOMPARE(first.paths.transcript,
             QDir(first.paths.directory).filePath(QStringLiteral("transcript.txt")));
    QCOMPARE(first.paths.json,
             QDir(first.paths.directory).filePath(QStringLiteral("meeting.json")));
    QCOMPARE(first.paths.markdown,
             QDir(first.paths.directory).filePath(QStringLiteral("meeting.md")));
}

void AppShellTest::rejectsUnavailableStorageLocation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString blockingFile = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("not-a-directory"));
    QFile file(blockingFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    const MeetingStorage storage(
        QDir(blockingFile).filePath(QStringLiteral("meetings")));
    const MeetingStorage::Result result = storage.createMeeting();

    QVERIFY(!result.succeeded());
    QVERIFY(result.paths.isEmpty());
    QVERIFY(!result.errorMessage.isEmpty());
}

void AppShellTest::keepsTitleInsideStorageRoot()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const MeetingStorage storage(temporaryDirectory.path());
    const MeetingStorage::Result result = storage.createMeeting(
        QStringLiteral("../../외부\\폴더:회의?*"),
        QDateTime(QDate(2026, 9, 29), QTime(10, 30, 0)));

    QVERIFY2(result.succeeded(), qPrintable(result.errorMessage));
    const QString relativePath = QDir(temporaryDirectory.path()).relativeFilePath(
        result.paths.directory);
    QVERIFY(!relativePath.startsWith(QStringLiteral("..")));
    QVERIFY(!relativePath.contains(u'/'));
    QVERIFY(!relativePath.contains(u'\\'));
    QCOMPARE(QFileInfo(result.paths.directory).absolutePath(),
             QDir(temporaryDirectory.path()).absolutePath());
}

void AppShellTest::rejectsInvalidStartTime()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const MeetingStorage storage(temporaryDirectory.path());

    const MeetingStorage::Result result = storage.createMeeting(
        QString(), QDateTime());

    QVERIFY(!result.succeeded());
    QVERIFY(result.paths.isEmpty());
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(QDir(temporaryDirectory.path()).isEmpty());
}

void AppShellTest::writesPcm16MonoWav()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString wavPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("recording.wav"));
    const QByteArray pcmData = QByteArray::fromHex("0080ff7f");

    WavFileWriter writer;
    QString errorMessage;
    QVERIFY2(writer.openFile(wavPath, monoPcmFormat(), &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(writer.write(pcmData), static_cast<qint64>(pcmData.size()));
    QCOMPARE(writer.pcmBytesWritten(), static_cast<quint64>(pcmData.size()));
    QVERIFY2(writer.finalize(&errorMessage), qPrintable(errorMessage));

    QFile file(wavPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray wav = file.readAll();

    QCOMPARE(wav.size(), 44 + pcmData.size());
    QCOMPARE(wav.mid(0, 4), QByteArray("RIFF"));
    QCOMPARE(littleEndian32(wav, 4), static_cast<quint32>(36 + pcmData.size()));
    QCOMPARE(wav.mid(8, 4), QByteArray("WAVE"));
    QCOMPARE(wav.mid(12, 4), QByteArray("fmt "));
    QCOMPARE(littleEndian32(wav, 16), quint32(16));
    QCOMPARE(littleEndian16(wav, 20), quint16(1));
    QCOMPARE(littleEndian16(wav, 22), quint16(1));
    QCOMPARE(littleEndian32(wav, 24), quint32(16000));
    QCOMPARE(littleEndian32(wav, 28), quint32(32000));
    QCOMPARE(littleEndian16(wav, 32), quint16(2));
    QCOMPARE(littleEndian16(wav, 34), quint16(16));
    QCOMPARE(wav.mid(36, 4), QByteArray("data"));
    QCOMPARE(littleEndian32(wav, 40), static_cast<quint32>(pcmData.size()));
    QCOMPARE(wav.mid(44), pcmData);
}

void AppShellTest::writesValidZeroSecondWav()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString wavPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("zero-seconds.wav"));

    WavFileWriter writer;
    QString errorMessage;
    QVERIFY2(writer.openFile(wavPath, monoPcmFormat(48000), &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(writer.finalize(&errorMessage), qPrintable(errorMessage));

    QFile file(wavPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray wav = file.readAll();

    QCOMPARE(wav.size(), 44);
    QCOMPARE(littleEndian32(wav, 4), quint32(36));
    QCOMPARE(littleEndian32(wav, 24), quint32(48000));
    QCOMPARE(littleEndian32(wav, 40), quint32(0));
}

void AppShellTest::reportsWavStorageFailure()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString wavPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("missing/recording.wav"));

    WavFileWriter writer;
    QString errorMessage;
    QVERIFY(!writer.openFile(wavPath, monoPcmFormat(), &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(!QFileInfo::exists(wavPath));
}

void AppShellTest::rejectsMissingAudioDevice()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    AudioRecorder recorder;
    QSignalSpy errorSpy(&recorder, &AudioRecorder::recordingError);

    const bool started = recorder.startRecording(
        QAudioDevice(),
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("meeting.wav")));

    QVERIFY(!started);
    QVERIFY(!recorder.isRecording());
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(!errorSpy.at(0).at(0).toString().isEmpty());
}

QTEST_MAIN(AppShellTest)

#include "tst_appshell.moc"
