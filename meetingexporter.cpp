#include "meetingexporter.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace {
QString escapeMarkdownInline(const QString &text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(u'\r', u'\n');

    QString escaped;
    escaped.reserve(normalized.size());
    const QString punctuation = QStringLiteral("\\`*_{}[]()#+-.!|><");
    for (const QChar character : normalized) {
        if (character == u'\n') {
            escaped += QStringLiteral("<br>");
            continue;
        }
        if (punctuation.contains(character)) {
            escaped += u'\\';
        }
        escaped += character;
    }
    return escaped;
}

void appendMarkdownList(QString *output, const QStringList &items)
{
    if (items.isEmpty()) {
        *output += QStringLiteral("- 없음\n");
        return;
    }

    for (const QString &item : items) {
        *output += QStringLiteral("- %1\n").arg(escapeMarkdownInline(item));
    }
}

void appendTextList(QString *output, const QStringList &items)
{
    if (items.isEmpty()) {
        *output += QStringLiteral("(없음)\n");
        return;
    }

    for (const QString &item : items) {
        *output += QStringLiteral("- %1\n").arg(item);
    }
}
} // namespace

QByteArray MeetingExporter::serialize(const MeetingMinutes &minutes, Format format)
{
    switch (format) {
    case Format::Markdown:
        return toMarkdown(minutes).toUtf8();
    case Format::Text:
        return toText(minutes).toUtf8();
    case Format::Json:
        return QJsonDocument(minutes.toJson()).toJson(QJsonDocument::Indented);
    }

    return {};
}

QString MeetingExporter::defaultFileName(Format format)
{
    switch (format) {
    case Format::Markdown:
        return QStringLiteral("meeting.md");
    case Format::Text:
        return QStringLiteral("meeting.txt");
    case Format::Json:
        return QStringLiteral("meeting.json");
    }

    return {};
}

QString MeetingExporter::displayName(Format format)
{
    switch (format) {
    case Format::Markdown:
        return QStringLiteral("Markdown");
    case Format::Text:
        return QStringLiteral("TXT");
    case Format::Json:
        return QStringLiteral("JSON");
    }

    return {};
}

bool MeetingExporter::save(const MeetingMinutes &minutes,
                           Format format,
                           const QString &directory,
                           QString *savedPath,
                           QString *errorMessage)
{
    if (savedPath) {
        savedPath->clear();
    }
    if (errorMessage) {
        errorMessage->clear();
    }

    const QFileInfo directoryInfo(directory);
    if (directory.trimmed().isEmpty() || !directoryInfo.exists()
        || !directoryInfo.isDir()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("회의록을 내보낼 로컬 폴더를 찾을 수 없습니다: %1")
                                .arg(QDir::toNativeSeparators(directory));
        }
        return false;
    }

    const QString path = QDir(directoryInfo.absoluteFilePath())
                             .filePath(defaultFileName(format));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 회의록 파일을 저장할 수 없습니다: %2 (%3)")
                                .arg(displayName(format),
                                     QDir::toNativeSeparators(path),
                                     file.errorString());
        }
        return false;
    }

    const QByteArray contents = serialize(minutes, format);
    if (file.write(contents) != contents.size() || !file.commit()) {
        const QString fileError = file.errorString();
        file.cancelWriting();
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 회의록 파일을 안전하게 저장할 수 없습니다: %2 (%3)")
                                .arg(displayName(format),
                                     QDir::toNativeSeparators(path),
                                     fileError);
        }
        return false;
    }

    if (savedPath) {
        *savedPath = QFileInfo(path).absoluteFilePath();
    }
    return true;
}

QString MeetingExporter::toMarkdown(const MeetingMinutes &minutes)
{
    QString output;
    output += QStringLiteral("# %1\n\n").arg(escapeMarkdownInline(minutes.title));
    output += QStringLiteral("- 일시: %1\n\n")
                  .arg(escapeMarkdownInline(minutes.date.value_or(QString())));

    output += QStringLiteral("## 회의 요약\n\n%1\n\n")
                  .arg(escapeMarkdownInline(minutes.summary));

    output += QStringLiteral("## 주요 논의 사항\n\n");
    if (minutes.topics.isEmpty()) {
        output += QStringLiteral("- 없음\n");
    } else {
        for (const DiscussionTopic &topic : minutes.topics) {
            output += QStringLiteral("### %1\n\n%2\n\n")
                          .arg(escapeMarkdownInline(topic.topic),
                               escapeMarkdownInline(topic.discussion));
        }
    }
    output += u'\n';

    output += QStringLiteral("## 결정 사항\n\n");
    appendMarkdownList(&output, minutes.decisions);
    output += u'\n';

    output += QStringLiteral("## Action Items\n\n");
    output += QStringLiteral("| 작업 | 담당자 | 기한 |\n");
    output += QStringLiteral("|---|---|---|\n");
    for (const MeetingActionItem &actionItem : minutes.actionItems) {
        output += QStringLiteral("| %1 | %2 | %3 |\n")
                      .arg(escapeMarkdownInline(actionItem.task),
                           escapeMarkdownInline(
                               actionItem.owner.value_or(QString())),
                           escapeMarkdownInline(
                               actionItem.dueDate.value_or(QString())));
    }
    output += u'\n';

    output += QStringLiteral("## 미해결 이슈\n\n");
    appendMarkdownList(&output, minutes.openIssues);
    return output;
}

QString MeetingExporter::toText(const MeetingMinutes &minutes)
{
    QString output;
    output += QStringLiteral("회의 제목: %1\n").arg(minutes.title);
    output += QStringLiteral("일시: %1\n\n")
                  .arg(minutes.date.value_or(QString()));

    output += QStringLiteral("[회의 요약]\n%1\n\n").arg(minutes.summary);

    output += QStringLiteral("[주요 논의 사항]\n");
    if (minutes.topics.isEmpty()) {
        output += QStringLiteral("(없음)\n");
    } else {
        for (qsizetype index = 0; index < minutes.topics.size(); ++index) {
            const DiscussionTopic &topic = minutes.topics.at(index);
            output += QStringLiteral("%1. %2\n%3\n")
                          .arg(index + 1)
                          .arg(topic.topic, topic.discussion);
        }
    }
    output += u'\n';

    output += QStringLiteral("[결정 사항]\n");
    appendTextList(&output, minutes.decisions);
    output += u'\n';

    output += QStringLiteral("[Action Items]\n");
    if (minutes.actionItems.isEmpty()) {
        output += QStringLiteral("(없음)\n");
    } else {
        for (qsizetype index = 0; index < minutes.actionItems.size(); ++index) {
            const MeetingActionItem &actionItem = minutes.actionItems.at(index);
            output += QStringLiteral("%1. 작업: %2\n   담당자: %3\n   기한: %4\n")
                          .arg(index + 1)
                          .arg(actionItem.task,
                               actionItem.owner.value_or(QString()),
                               actionItem.dueDate.value_or(QString()));
        }
    }
    output += u'\n';

    output += QStringLiteral("[미해결 이슈]\n");
    appendTextList(&output, minutes.openIssues);
    return output;
}
