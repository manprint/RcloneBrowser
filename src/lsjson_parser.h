#pragma once
#include <QtCore>
#include <optional>

struct ListEntry {
  QString path;        // "Path" (relative to the listed root; recursive listings contain '/')
  QString name;        // "Name"
  bool isDir = false;  // "IsDir"
  qint64 size = -1;    // "Size"; -1 unknown
  QDateTime modTime;   // "ModTime" via parseRfc3339; invalid when missing/unparsable
  QString mimeType;    // "MimeType"; may be empty
};
std::optional<ListEntry> lsjsonEntryFromObject(const QJsonObject &o);   // nullopt if "Name" missing or not a string
std::optional<ListEntry> parseLsjsonLine(QByteArrayView line);           // trims whitespace and one trailing ','; "[" / "]" / "" -> nullopt
QString lsjsonModifiedText(const QDateTime &t,
                           const QTimeZone &tz = QTimeZone::systemTimeZone()); // "yyyy-MM-dd HH:mm:ss"; invalid -> ""

class LsjsonStreamParser {
public:
  void feed(QByteArrayView chunk);      // appends; parses every complete '\n'-terminated line
  void finish();                        // parses the buffered remainder (no trailing newline)
  QVector<ListEntry> takeEntries();     // returns and clears the parsed entries
  int skippedLines() const;             // non-empty lines that are not "[", "]" and not an entry

private:
  void processLine(QByteArrayView raw);

  QByteArray mBuffer;
  QVector<ListEntry> mEntries;
  int mSkipped = 0;
};

struct RemoteInfo { QString name, type, source, description; };
std::optional<QVector<RemoteInfo>> parseListRemotesJson(const QByteArray &json); // nullopt if not a JSON array; skips elements without string name/type

QString lastRcloneStderrMessage(const QByteArray &stderrBytes);
// last non-empty line; strips a leading "yyyy/MM/dd HH:mm:ss LEVEL : " prefix
// (regex ^\d{4}/\d\d/\d\d \d\d:\d\d:\d\d [A-Z]+\s*: ); returns "" if none
