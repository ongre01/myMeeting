#ifndef MEETINGSTORAGE_H
#define MEETINGSTORAGE_H

#include <QDateTime>
#include <QString>

class MeetingStorage
{
public:
    struct Paths
    {
        QString directory;
        QString wav;
        QString transcript;
        QString json;
        QString markdown;

        bool isEmpty() const;
    };

    struct Result
    {
        Paths paths;
        QString errorMessage;

        bool succeeded() const;
    };

    explicit MeetingStorage(QString meetingsRoot);

    Result createMeeting(
        const QString &title = QString(),
        const QDateTime &startedAt = QDateTime::currentDateTime()) const;

private:
    QString m_meetingsRoot;
};

#endif // MEETINGSTORAGE_H
