#ifndef MEETINGEXPORTER_H
#define MEETINGEXPORTER_H

#include "meetingminutes.h"

#include <QByteArray>
#include <QString>

class MeetingExporter
{
public:
    enum class Format
    {
        Markdown,
        Text,
        Json,
    };

    static QByteArray serialize(const MeetingMinutes &minutes, Format format);
    static QString defaultFileName(Format format);
    static QString displayName(Format format);

    static bool save(const MeetingMinutes &minutes,
                     Format format,
                     const QString &directory,
                     QString *savedPath,
                     QString *errorMessage);

private:
    static QString toMarkdown(const MeetingMinutes &minutes);
    static QString toText(const MeetingMinutes &minutes);
};

#endif // MEETINGEXPORTER_H
