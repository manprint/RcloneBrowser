#include "lsjson_parser.h"

#include "rclone_log.h"

namespace {

enum class LineKind { Ignored, Garbage, Object };

// Ignored: blank line, "[" or "]" (after trimming and one trailing ',').
// Garbage: not a JSON object (text such as a log line, or a parse error).
// Object: a JSON object; it may still lack a string "Name" (see lsjsonEntryFromObject).
LineKind classifyLsjsonLine(QByteArrayView raw, QJsonObject &obj) {
  QByteArray line = raw.toByteArray().trimmed();
  if (line.endsWith(',')) {
    line.chop(1);
  }
  if (line.isEmpty() || line == "[" || line == "]") {
    return LineKind::Ignored;
  }
  QJsonParseError err;
  const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    return LineKind::Garbage;
  }
  obj = doc.object();
  return LineKind::Object;
}

} // namespace

std::optional<ListEntry> lsjsonEntryFromObject(const QJsonObject &o) {
  const QJsonValue nameV = o.value(QStringLiteral("Name"));
  if (!nameV.isString()) {
    return std::nullopt;
  }
  ListEntry e;
  e.name = nameV.toString();
  const QJsonValue pathV = o.value(QStringLiteral("Path"));
  e.path = pathV.isString() ? pathV.toString() : e.name;
  e.isDir = o.value(QStringLiteral("IsDir")).toBool();
  const QJsonValue sizeV = o.value(QStringLiteral("Size"));
  e.size = sizeV.isDouble() ? qRound64(sizeV.toDouble()) : qint64(-1);
  const QJsonValue modV = o.value(QStringLiteral("ModTime"));
  if (modV.isString()) {
    e.modTime = parseRfc3339(modV.toString());
  }
  e.mimeType = o.value(QStringLiteral("MimeType")).toString();
  return e;
}

std::optional<ListEntry> parseLsjsonLine(QByteArrayView line) {
  QJsonObject obj;
  if (classifyLsjsonLine(line, obj) != LineKind::Object) {
    return std::nullopt;
  }
  return lsjsonEntryFromObject(obj);
}

QString lsjsonModifiedText(const QDateTime &t, const QTimeZone &tz) {
  if (!t.isValid()) {
    return QString();
  }
  return t.toTimeZone(tz).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void LsjsonStreamParser::feed(QByteArrayView chunk) {
  if (chunk.isEmpty()) {
    return;
  }
  // Raw bytes are buffered, so a chunk may end inside a multi-byte UTF-8 character.
  mBuffer.append(chunk.data(), chunk.size());
  qsizetype start = 0;
  qsizetype nl = mBuffer.indexOf('\n', start);
  while (nl >= 0) {
    processLine(QByteArrayView(mBuffer.constData() + start, nl - start));
    start = nl + 1;
    nl = mBuffer.indexOf('\n', start);
  }
  mBuffer.remove(0, start);
}

void LsjsonStreamParser::finish() {
  if (!mBuffer.isEmpty()) {
    processLine(mBuffer);
    mBuffer.clear();
  }
}

QVector<ListEntry> LsjsonStreamParser::takeEntries() {
  QVector<ListEntry> out;
  out.swap(mEntries);
  return out;
}

int LsjsonStreamParser::skippedLines() const {
  return mSkipped;
}

void LsjsonStreamParser::processLine(QByteArrayView raw) {
  QJsonObject obj;
  switch (classifyLsjsonLine(raw, obj)) {
  case LineKind::Ignored:
    return;
  case LineKind::Garbage:
    ++mSkipped;
    return;
  case LineKind::Object:
    break;
  }
  // A JSON object without a string "Name" is dropped but not counted as skipped.
  if (std::optional<ListEntry> e = lsjsonEntryFromObject(obj)) {
    mEntries.append(*e);
  }
}

std::optional<QVector<RemoteInfo>> parseListRemotesJson(const QByteArray &json) {
  QJsonParseError err;
  const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
  if (err.error != QJsonParseError::NoError || !doc.isArray()) {
    return std::nullopt;
  }
  const QJsonArray arr = doc.array();
  QVector<RemoteInfo> out;
  out.reserve(arr.size());
  for (const QJsonValue &v : arr) {
    if (!v.isObject()) {
      continue;
    }
    const QJsonObject o = v.toObject();
    const QJsonValue nameV = o.value(QStringLiteral("name"));
    const QJsonValue typeV = o.value(QStringLiteral("type"));
    if (!nameV.isString() || !typeV.isString()) {
      continue;
    }
    RemoteInfo r;
    r.name = nameV.toString();
    r.type = typeV.toString();
    r.source = o.value(QStringLiteral("source")).toString();
    r.description = o.value(QStringLiteral("description")).toString();
    out.append(r);
  }
  return out;
}

QString lastRcloneStderrMessage(const QByteArray &stderrBytes) {
  static const QRegularExpression prefixRe(QStringLiteral(
      "^\\d{4}/\\d\\d/\\d\\d \\d\\d:\\d\\d:\\d\\d [A-Z]+\\s*: "));
  const QList<QByteArray> lines = stderrBytes.split('\n');
  for (qsizetype i = lines.size() - 1; i >= 0; --i) {
    const QString line = QString::fromUtf8(lines.at(i)).trimmed();
    if (line.isEmpty()) {
      continue;
    }
    const QRegularExpressionMatch m = prefixRe.match(line);
    return m.hasMatch() ? line.mid(m.capturedEnd()).trimmed() : line;
  }
  return QString();
}
