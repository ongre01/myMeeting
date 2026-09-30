#include "meetingminutes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

namespace {
bool rejectUnknownFields(const QJsonObject &object,
                         const QSet<QString> &allowedFields,
                         const QString &context,
                         QString *errorMessage)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!allowedFields.contains(iterator.key())) {
            *errorMessage = QStringLiteral("%1에 알 수 없는 필드 '%2'이(가) 있습니다.")
                                .arg(context, iterator.key());
            return false;
        }
    }
    return true;
}

bool requiredString(const QJsonObject &object,
                    const QString &field,
                    const QString &context,
                    QString *value,
                    QString *errorMessage)
{
    const QJsonValue jsonValue = object.value(field);
    if (!jsonValue.isString()) {
        *errorMessage = QStringLiteral("%1의 '%2' 필드는 문자열이어야 합니다.")
                            .arg(context, field);
        return false;
    }
    *value = jsonValue.toString();
    return true;
}

bool nullableString(const QJsonObject &object,
                    const QString &field,
                    const QString &context,
                    std::optional<QString> *value,
                    QString *errorMessage)
{
    const QJsonValue jsonValue = object.value(field);
    if (jsonValue.isNull()) {
        value->reset();
        return true;
    }
    if (!jsonValue.isString()) {
        *errorMessage = QStringLiteral("%1의 '%2' 필드는 문자열 또는 null이어야 합니다.")
                            .arg(context, field);
        return false;
    }
    *value = jsonValue.toString();
    return true;
}

bool requiredArray(const QJsonObject &object,
                   const QString &field,
                   QJsonArray *value,
                   QString *errorMessage)
{
    const QJsonValue jsonValue = object.value(field);
    if (!jsonValue.isArray()) {
        *errorMessage = QStringLiteral("meeting.json의 '%1' 필드는 배열이어야 합니다.")
                            .arg(field);
        return false;
    }
    *value = jsonValue.toArray();
    return true;
}

bool parseStringArray(const QJsonArray &array,
                      const QString &field,
                      QStringList *values,
                      QString *errorMessage)
{
    values->clear();
    values->reserve(array.size());
    for (qsizetype index = 0; index < array.size(); ++index) {
        if (!array.at(index).isString()) {
            *errorMessage = QStringLiteral("meeting.json의 '%1[%2]' 항목은 문자열이어야 합니다.")
                                .arg(field)
                                .arg(index);
            return false;
        }
        values->append(array.at(index).toString());
    }
    return true;
}
} // namespace

bool MeetingMinutes::fromJson(const QByteArray &json,
                              MeetingMinutes *minutes,
                              QString *errorMessage)
{
    if (!minutes || !errorMessage) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *errorMessage = QStringLiteral("meeting.json이 올바른 JSON 객체가 아닙니다: %1")
                            .arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    static const QSet<QString> rootFields = {
        QStringLiteral("title"),
        QStringLiteral("date"),
        QStringLiteral("summary"),
        QStringLiteral("topics"),
        QStringLiteral("decisions"),
        QStringLiteral("action_items"),
        QStringLiteral("open_issues"),
    };
    if (!rejectUnknownFields(root, rootFields, QStringLiteral("meeting.json"),
                             errorMessage)) {
        return false;
    }

    MeetingMinutes parsed;
    if (!requiredString(root, QStringLiteral("title"), QStringLiteral("meeting.json"),
                        &parsed.title, errorMessage)
        || !nullableString(root, QStringLiteral("date"), QStringLiteral("meeting.json"),
                           &parsed.date, errorMessage)
        || !requiredString(root, QStringLiteral("summary"), QStringLiteral("meeting.json"),
                           &parsed.summary, errorMessage)) {
        return false;
    }

    QJsonArray topics;
    QJsonArray decisions;
    QJsonArray actionItems;
    QJsonArray openIssues;
    if (!requiredArray(root, QStringLiteral("topics"), &topics, errorMessage)
        || !requiredArray(root, QStringLiteral("decisions"), &decisions, errorMessage)
        || !requiredArray(root, QStringLiteral("action_items"), &actionItems, errorMessage)
        || !requiredArray(root, QStringLiteral("open_issues"), &openIssues, errorMessage)) {
        return false;
    }

    static const QSet<QString> topicFields = {
        QStringLiteral("topic"), QStringLiteral("discussion")};
    parsed.topics.reserve(topics.size());
    for (qsizetype index = 0; index < topics.size(); ++index) {
        if (!topics.at(index).isObject()) {
            *errorMessage = QStringLiteral("meeting.json의 'topics[%1]' 항목은 객체여야 합니다.")
                                .arg(index);
            return false;
        }
        const QJsonObject object = topics.at(index).toObject();
        const QString context = QStringLiteral("topics[%1]").arg(index);
        DiscussionTopic topic;
        if (!rejectUnknownFields(object, topicFields, context, errorMessage)
            || !requiredString(object, QStringLiteral("topic"), context,
                               &topic.topic, errorMessage)
            || !requiredString(object, QStringLiteral("discussion"), context,
                               &topic.discussion, errorMessage)) {
            return false;
        }
        parsed.topics.append(topic);
    }

    if (!parseStringArray(decisions, QStringLiteral("decisions"),
                          &parsed.decisions, errorMessage)
        || !parseStringArray(openIssues, QStringLiteral("open_issues"),
                             &parsed.openIssues, errorMessage)) {
        return false;
    }

    static const QSet<QString> actionItemFields = {
        QStringLiteral("task"), QStringLiteral("owner"), QStringLiteral("due_date")};
    parsed.actionItems.reserve(actionItems.size());
    for (qsizetype index = 0; index < actionItems.size(); ++index) {
        if (!actionItems.at(index).isObject()) {
            *errorMessage = QStringLiteral("meeting.json의 'action_items[%1]' 항목은 객체여야 합니다.")
                                .arg(index);
            return false;
        }
        const QJsonObject object = actionItems.at(index).toObject();
        const QString context = QStringLiteral("action_items[%1]").arg(index);
        MeetingActionItem actionItem;
        if (!rejectUnknownFields(object, actionItemFields, context, errorMessage)
            || !requiredString(object, QStringLiteral("task"), context,
                               &actionItem.task, errorMessage)
            || !nullableString(object, QStringLiteral("owner"), context,
                               &actionItem.owner, errorMessage)
            || !nullableString(object, QStringLiteral("due_date"), context,
                               &actionItem.dueDate, errorMessage)) {
            return false;
        }
        parsed.actionItems.append(actionItem);
    }

    *minutes = parsed;
    errorMessage->clear();
    return true;
}

QJsonObject MeetingMinutes::toJson() const
{
    QJsonArray topicArray;
    for (const DiscussionTopic &topic : topics) {
        topicArray.append(QJsonObject{
            {QStringLiteral("topic"), topic.topic},
            {QStringLiteral("discussion"), topic.discussion},
        });
    }

    QJsonArray decisionArray;
    for (const QString &decision : decisions) {
        decisionArray.append(decision);
    }

    QJsonArray actionItemArray;
    for (const MeetingActionItem &actionItem : actionItems) {
        actionItemArray.append(QJsonObject{
            {QStringLiteral("task"), actionItem.task},
            {QStringLiteral("owner"),
             actionItem.owner ? QJsonValue(*actionItem.owner) : QJsonValue::Null},
            {QStringLiteral("due_date"),
             actionItem.dueDate ? QJsonValue(*actionItem.dueDate) : QJsonValue::Null},
        });
    }

    QJsonArray openIssueArray;
    for (const QString &issue : openIssues) {
        openIssueArray.append(issue);
    }

    return QJsonObject{
        {QStringLiteral("title"), title},
        {QStringLiteral("date"), date ? QJsonValue(*date) : QJsonValue::Null},
        {QStringLiteral("summary"), summary},
        {QStringLiteral("topics"), topicArray},
        {QStringLiteral("decisions"), decisionArray},
        {QStringLiteral("action_items"), actionItemArray},
        {QStringLiteral("open_issues"), openIssueArray},
    };
}
