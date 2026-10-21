#include "rclone_log.h"

#include <cmath>

namespace {

qint64 jsonInt(const QJsonValue &v) {
  const double d = v.toDouble();
  if (!std::isfinite(d)) {
    return 0;
  }
  return qRound64(d);
}

std::optional<qint64> jsonEta(const QJsonValue &v) {
  if (!v.isDouble()) {
    return std::nullopt;
  }
  return jsonInt(v);
}

QString stripAnsi(const QString &s) {
  static const QRegularExpression re(QStringLiteral("\x1B\\[[0-9;]*m"));
  QString r = s;
  r.remove(re);
  return r;
}

QString stripNewlines(const QString &s) {
  int b = 0;
  int e = s.size();
  while (b < e && s.at(b) == QLatin1Char('\n')) {
    ++b;
  }
  while (e > b && s.at(e - 1) == QLatin1Char('\n')) {
    --e;
  }
  return s.mid(b, e - b);
}

} // namespace

RcloneLogLevel parseRcloneLevel(QStringView level) {
  const QString l = level.toString().toLower();
  if (l == QLatin1String("debug")) {
    return RcloneLogLevel::Debug;
  }
  if (l == QLatin1String("info")) {
    return RcloneLogLevel::Info;
  }
  if (l == QLatin1String("notice") || l == QLatin1String("warning")) {
    return RcloneLogLevel::Notice;
  }
  if (l == QLatin1String("error")) {
    return RcloneLogLevel::Error;
  }
  if (l == QLatin1String("critical") || l == QLatin1String("fatal") ||
      l == QLatin1String("alert") || l == QLatin1String("emergency")) {
    return RcloneLogLevel::Critical;
  }
  return RcloneLogLevel::Unknown;
}

QString rcloneLevelName(RcloneLogLevel level) {
  switch (level) {
  case RcloneLogLevel::Debug:
    return QStringLiteral("DEBUG");
  case RcloneLogLevel::Info:
    return QStringLiteral("INFO");
  case RcloneLogLevel::Notice:
    return QStringLiteral("NOTICE");
  case RcloneLogLevel::Error:
    return QStringLiteral("ERROR");
  case RcloneLogLevel::Critical:
    return QStringLiteral("CRITICAL");
  case RcloneLogLevel::Unknown:
    break;
  }
  return QString();
}

QDateTime parseRfc3339(const QString &s) {
  QString t = s.trimmed();
  const int dot = t.indexOf(QLatin1Char('.'));
  if (dot >= 0) {
    int end = dot + 1;
    while (end < t.size() && t.at(end).isDigit()) {
      ++end;
    }
    QString frac = t.mid(dot + 1, end - dot - 1).left(3);
    while (frac.size() < 3) {
      frac.append(QLatin1Char('0'));
    }
    t = t.left(dot + 1) + frac + t.mid(end);
  }
  return QDateTime::fromString(t, Qt::ISODateWithMs);
}

RcloneStats parseRcloneStats(const QJsonObject &o) {
  RcloneStats s;
  s.bytes = jsonInt(o.value(QStringLiteral("bytes")));
  s.totalBytes = jsonInt(o.value(QStringLiteral("totalBytes")));
  s.checks = jsonInt(o.value(QStringLiteral("checks")));
  s.totalChecks = jsonInt(o.value(QStringLiteral("totalChecks")));
  s.transfers = jsonInt(o.value(QStringLiteral("transfers")));
  s.totalTransfers = jsonInt(o.value(QStringLiteral("totalTransfers")));
  s.errors = jsonInt(o.value(QStringLiteral("errors")));
  s.deletes = jsonInt(o.value(QStringLiteral("deletes")));
  s.renames = jsonInt(o.value(QStringLiteral("renames")));
  s.listed = jsonInt(o.value(QStringLiteral("listed")));
  s.serverSideCopies = jsonInt(o.value(QStringLiteral("serverSideCopies")));
  s.serverSideMoves = jsonInt(o.value(QStringLiteral("serverSideMoves")));
  s.elapsedTime = o.value(QStringLiteral("elapsedTime")).toDouble();
  s.transferTime = o.value(QStringLiteral("transferTime")).toDouble();
  s.speed = o.value(QStringLiteral("speed")).toDouble();
  s.eta = jsonEta(o.value(QStringLiteral("eta")));
  s.fatalError = o.value(QStringLiteral("fatalError")).toBool();
  s.retryError = o.value(QStringLiteral("retryError")).toBool();
  s.lastError = o.value(QStringLiteral("lastError")).toString();
  const QJsonValue tr = o.value(QStringLiteral("transferring"));
  if (tr.isArray()) {
    s.hasTransferring = true;
    const QJsonArray arr = tr.toArray();
    s.transferring.reserve(arr.size());
    for (const QJsonValue &iv : arr) {
      if (!iv.isObject()) {
        continue;
      }
      const QJsonObject io = iv.toObject();
      RcloneTransferItem it;
      it.name = io.value(QStringLiteral("name")).toString();
      it.bytes = jsonInt(io.value(QStringLiteral("bytes")));
      it.size = io.contains(QStringLiteral("size"))
                    ? jsonInt(io.value(QStringLiteral("size")))
                    : -1;
      it.percentage = int(jsonInt(io.value(QStringLiteral("percentage"))));
      it.speed = io.value(QStringLiteral("speed")).toDouble();
      it.speedAvg = io.value(QStringLiteral("speedAvg")).toDouble();
      it.eta = jsonEta(io.value(QStringLiteral("eta")));
      s.transferring.push_back(it);
    }
  }
  return s;
}

RcloneLogEvent parseRcloneLogLine(QByteArrayView lineView) {
  QByteArray line = lineView.toByteArray();
  while (!line.isEmpty() && (line.back() == '\n' || line.back() == '\r')) {
    line.chop(1);
  }

  RcloneLogEvent ev;
  ev.kind = RcloneLogEvent::Kind::Text;
  ev.msg = QString::fromUtf8(line);

  int i = 0;
  while (i < line.size() && (line.at(i) == ' ' || line.at(i) == '\t' ||
                             line.at(i) == '\n' || line.at(i) == '\r' ||
                             line.at(i) == '\v' || line.at(i) == '\f')) {
    ++i;
  }
  if (i >= line.size() || line.at(i) != '{') {
    return ev;
  }

  QJsonParseError err;
  const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    return ev;
  }
  const QJsonObject o = doc.object();
  const QJsonValue lv = o.value(QStringLiteral("level"));
  const QJsonValue mv = o.value(QStringLiteral("msg"));
  if (!lv.isString() || !mv.isString()) {
    return ev;
  }

  ev.level = parseRcloneLevel(lv.toString());
  ev.msg = stripNewlines(stripAnsi(mv.toString()));
  ev.object = stripAnsi(o.value(QStringLiteral("object")).toString());
  ev.source = o.value(QStringLiteral("source")).toString();
  const QJsonValue tv = o.value(QStringLiteral("time"));
  ev.time = tv.isString() ? parseRfc3339(tv.toString()) : QDateTime();

  const QJsonValue sv = o.value(QStringLiteral("stats"));
  if (sv.isObject()) {
    ev.kind = RcloneLogEvent::Kind::Stats;
    ev.stats = parseRcloneStats(sv.toObject());
  } else {
    ev.kind = RcloneLogEvent::Kind::Log;
  }
  return ev;
}

QString renderRcloneLogEvent(const RcloneLogEvent &e, const QTimeZone &tz) {
  if (e.kind == RcloneLogEvent::Kind::Text) {
    return e.msg;
  }
  const QString ts =
      e.time.isValid()
          ? e.time.toTimeZone(tz).toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))
          : QString();
  const bool isStats = e.kind == RcloneLogEvent::Kind::Stats;
  const QString level =
      rcloneLevelName(isStats ? RcloneLogLevel::Notice : e.level);
  QString r = ts + QLatin1Char(' ') + level.leftJustified(6) +
              QStringLiteral(": ");
  if (!isStats && !e.object.isEmpty()) {
    r += e.object + QStringLiteral(": ");
  }
  r += e.msg;
  return r;
}
