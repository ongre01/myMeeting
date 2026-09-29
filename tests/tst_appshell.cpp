#include "mainwindow.h"
#include "meetingstorage.h"

#include <QDir>
#include <QFile>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QtTest>

class AppShellTest : public QObject
{
    Q_OBJECT

private slots:
    void createsEmptyMainWindow();
    void createsUniqueMeetingDirectoriesAndPaths();
    void rejectsUnavailableStorageLocation();
    void keepsTitleInsideStorageRoot();
    void rejectsInvalidStartTime();
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

QTEST_MAIN(AppShellTest)

#include "tst_appshell.moc"
