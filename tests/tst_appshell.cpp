#include "mainwindow.h"

#include <QStatusBar>
#include <QtTest>

class AppShellTest : public QObject
{
    Q_OBJECT

private slots:
    void createsEmptyMainWindow();
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

QTEST_MAIN(AppShellTest)

#include "tst_appshell.moc"
