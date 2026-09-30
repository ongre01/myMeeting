#include "mainwindow.h"
#include "applicationlog.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("Local Meeting Minutes"));
    QCoreApplication::setApplicationName(QStringLiteral("myMeeting"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setApplicationDisplayName(
        QStringLiteral("Local Meeting Minutes Assistant"));

    ApplicationLog::initialize();

    MainWindow mainWindow;
    mainWindow.show();

    const int exitCode = application.exec();
    ApplicationLog::shutdown();
    return exitCode;
}
