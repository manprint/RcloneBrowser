#pragma once
#include <QtCore>

#include "rclone_log.h"

// Pure model of one rclone transfer job, fed by rclone JSON log lines (D4).
// No widgets, no process handling: JobWidget only renders what it emits.
class TransferMonitor : public QObject {
  Q_OBJECT
public:
  enum class Result { Running, Finished, Error, Stopped };
  Q_ENUM(Result)

  explicit TransferMonitor(QObject *parent = nullptr);

  // Parses one raw line. Empty lines (after stripping a trailing "\r"/"\n") are ignored.
  void consumeLine(QByteArrayView line);
  void consumeEvent(const RcloneLogEvent &e);
  // Sets the result and emits finished() once; later calls are ignored.
  void finish(int exitCode, QProcess::ExitStatus status, bool stoppedByUser);

  bool hasStats() const { return mHasStats; }
  const RcloneStats &stats() const { return mStats; }
  QString lastStatsText() const { return mLastStatsText; }  // msg of the last Stats event
  qint64 errorCount() const;
  QString lastError() const;
  QStringList recentErrors() const { return mRecentErrors; }
  Result result() const { return mResult; }

signals:
  void statsUpdated(const RcloneStats &stats);
  void activeTransfersChanged(const QVector<RcloneTransferItem> &items);
  void logEvent(const RcloneLogEvent &e);  // Log and Text only
  void errorsChanged(qint64 count, const QString &last);
  void finished(TransferMonitor::Result result);

private:
  static constexpr int kMaxRecentErrors = 20;

  RcloneStats mStats;
  bool mHasStats = false;
  QString mLastStatsText;
  qint64 mErrorEvents = 0;  // all Error/Critical log events, not only the kept ones
  QStringList mRecentErrors;
  Result mResult = Result::Running;
};

// Progress percentage clamped to 0..100; -1 when the size is unknown (I-4).
int clampedPercent(const RcloneTransferItem &item);
