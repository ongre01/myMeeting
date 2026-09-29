#include "meetingstorage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace {
constexpr int maximumTitleLength = 80;
constexpr int maximumCollisionAttempts = 10000;

QString safeTitle(const QString &title)
{
    QString result;
    result.reserve(title.size());

    bool previousWasReplacement = false;
    for (const QChar character : title.trimmed()) {
        const ushort codePoint = character.unicode();
        const bool isInvalid = codePoint < 0x20
                               || character == u'/'
                               || character == u'\\'
                               || character == u'<'
                               || character == u'>'
                               || character == u':'
                               || character == u'"'
                               || character == u'|'
                               || character == u'?'
                               || character == u'*';

        if (isInvalid) {
            if (!previousWasReplacement) {
                result.append(u'_');
                previousWasReplacement = true;
            }
        } else {
            result.append(character);
            previousWasReplacement = false;
        }
    }

    result = result.left(maximumTitleLength);
    while (result.endsWith(u'.') || result.endsWith(u' ')) {
        result.chop(1);
    }

    if (result == QStringLiteral(".") || result == QStringLiteral("..")) {
        result.clear();
    }

    return result;
}

MeetingStorage::Paths pathsForDirectory(const QString &directory)
{
    const QDir meetingDirectory(directory);
    return {
        meetingDirectory.absolutePath(),
        meetingDirectory.filePath(QStringLiteral("meeting.wav")),
        meetingDirectory.filePath(QStringLiteral("transcript.txt")),
        meetingDirectory.filePath(QStringLiteral("meeting.json")),
        meetingDirectory.filePath(QStringLiteral("meeting.md"))
    };
}

bool verifyWritable(const QString &directory, QString *errorMessage)
{
    const QString probePath = QDir(directory).filePath(
        QStringLiteral(".write-test-%1.tmp")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QFile probe(probePath);
    if (!probe.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        *errorMessage = QStringLiteral("회의 폴더에 파일을 쓸 수 없습니다: %1")
                            .arg(probe.errorString());
        return false;
    }

    probe.close();
    if (!probe.remove()) {
        *errorMessage = QStringLiteral("회의 폴더의 쓰기 검사 파일을 삭제할 수 없습니다: %1")
                            .arg(probe.errorString());
        return false;
    }

    return true;
}
} // namespace

bool MeetingStorage::Paths::isEmpty() const
{
    return directory.isEmpty()
           && wav.isEmpty()
           && transcript.isEmpty()
           && json.isEmpty()
           && markdown.isEmpty();
}

bool MeetingStorage::Result::succeeded() const
{
    return errorMessage.isEmpty() && !paths.directory.isEmpty();
}

MeetingStorage::MeetingStorage(QString meetingsRoot)
    : m_meetingsRoot(meetingsRoot.trimmed().isEmpty()
                         ? QString()
                         : QDir::cleanPath(meetingsRoot))
{
}

MeetingStorage::Result MeetingStorage::createMeeting(
    const QString &title,
    const QDateTime &startedAt) const
{
    Result result;

    if (!startedAt.isValid()) {
        result.errorMessage = QStringLiteral("회의 시작 시각이 올바르지 않습니다.");
        return result;
    }

    if (m_meetingsRoot.isEmpty()) {
        result.errorMessage = QStringLiteral("회의 저장 위치가 비어 있습니다.");
        return result;
    }

    const QFileInfo rootInfo(m_meetingsRoot);
    if (rootInfo.exists() && !rootInfo.isDir()) {
        result.errorMessage = QStringLiteral("회의 저장 위치가 디렉터리가 아닙니다: %1")
                                  .arg(QDir::toNativeSeparators(rootInfo.absoluteFilePath()));
        return result;
    }

    if (!QDir().mkpath(m_meetingsRoot)) {
        result.errorMessage = QStringLiteral("회의 저장 위치를 만들 수 없습니다: %1")
                                  .arg(QDir::toNativeSeparators(rootInfo.absoluteFilePath()));
        return result;
    }

    QDir root(m_meetingsRoot);
    const QString cleanedTitle = safeTitle(title);
    QString baseName = startedAt.toString(QStringLiteral("yyyy-MM-dd_HHmmss"));
    if (!cleanedTitle.isEmpty()) {
        baseName += u'_' + cleanedTitle;
    }

    for (int attempt = 0; attempt < maximumCollisionAttempts; ++attempt) {
        const QString directoryName = attempt == 0
                                          ? baseName
                                          : QStringLiteral("%1_%2")
                                                .arg(baseName)
                                                .arg(attempt, 3, 10, QLatin1Char('0'));

        if (!root.mkdir(directoryName)) {
            if (QFileInfo::exists(root.filePath(directoryName))) {
                continue;
            }

            result.errorMessage = QStringLiteral("회의 폴더를 만들 수 없습니다: %1")
                                      .arg(QDir::toNativeSeparators(
                                          root.filePath(directoryName)));
            return result;
        }

        const QString directory = root.filePath(directoryName);
        if (!verifyWritable(directory, &result.errorMessage)) {
            root.rmdir(directoryName);
            return result;
        }

        result.paths = pathsForDirectory(directory);
        return result;
    }

    result.errorMessage = QStringLiteral("사용 가능한 회의 폴더 이름을 찾지 못했습니다.");
    return result;
}
