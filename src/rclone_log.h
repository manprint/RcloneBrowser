#pragma once
#include <QtCore>
#include <optional>

struct RcloneTransferItem {
  QString name;                  // path relative to srcFs; may contain '/', ':', brackets, quotes
  qint64 bytes = 0;
  qint64 size = -1;              // -1 = unknown
  int percentage = 0;            // as reported by rclone (may exceed 100)
  double speed = 0, speedAvg = 0;
  std::optional<qint64> eta;     // seconds; nullopt when null or missing
};
struct RcloneStats {
  qint64 bytes = 0, totalBytes = 0, checks = 0, totalChecks = 0;
  qint64 transfers = 0, totalTransfers = 0, errors = 0, deletes = 0, renames = 0, listed = 0;
  qint64 serverSideCopies = 0, serverSideMoves = 0;
  double elapsedTime = 0, transferTime = 0, speed = 0;
  std::optional<qint64> eta;
  bool fatalError = false, retryError = false;
  QString lastError;
  bool hasTransferring = false;  // the key was present
  QVector<RcloneTransferItem> transferring;
};
enum class RcloneLogLevel { Debug, Info, Notice, Error, Critical, Unknown };
struct RcloneLogEvent {
  enum class Kind { Log, Stats, Text };
  Kind kind = Kind::Text;
  QDateTime time;                // invalid for Text
  RcloneLogLevel level = RcloneLogLevel::Unknown;
  QString msg;                   // Log/Stats: leading/trailing '\n' stripped; Text: raw line without trailing "\r\n"/"\n"
  QString object, source;
  RcloneStats stats;             // meaningful for Kind::Stats
};
Q_DECLARE_METATYPE(RcloneStats)
Q_DECLARE_METATYPE(RcloneLogEvent)

RcloneLogEvent parseRcloneLogLine(QByteArrayView line);
RcloneStats parseRcloneStats(const QJsonObject &stats);      // reused by rc core/stats (phase 5)
RcloneLogLevel parseRcloneLevel(QStringView level);
QString rcloneLevelName(RcloneLogLevel level);                // "DEBUG","INFO","NOTICE","ERROR","CRITICAL",""
QDateTime parseRfc3339(const QString &s);                     // invalid on failure
QString renderRcloneLogEvent(const RcloneLogEvent &e,
                             const QTimeZone &tz = QTimeZone::systemTimeZone());
