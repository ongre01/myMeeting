#ifndef MEETINGMINUTES_H
#define MEETINGMINUTES_H

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

struct DiscussionTopic
{
    QString topic;
    QString discussion;
};

struct MeetingActionItem
{
    QString task;
    std::optional<QString> owner;
    std::optional<QString> dueDate;
};

class MeetingMinutes
{
public:
    static bool fromJson(const QByteArray &json,
                         MeetingMinutes *minutes,
                         QString *errorMessage);

    QJsonObject toJson() const;

    QString title;
    std::optional<QString> date;
    QString summary;
    QList<DiscussionTopic> topics;
    QStringList decisions;
    QList<MeetingActionItem> actionItems;
    QStringList openIssues;
};

#endif // MEETINGMINUTES_H
