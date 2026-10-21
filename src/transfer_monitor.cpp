#include "transfer_monitor.h"

TransferMonitor::TransferMonitor(QObject *parent) : QObject(parent) {}

void TransferMonitor::consumeLine(QByteArrayView line) {
  while (!line.isEmpty() && (line.back() == '\n' || line.back() == '\r')) {
    line.chop(1);
  }
  if (line.isEmpty()) {
    return;
  }
  consumeEvent(parseRcloneLogLine(line));
}

void TransferMonitor::consumeEvent(const RcloneLogEvent &e) {
  switch (e.kind) {
  case RcloneLogEvent::Kind::Stats: {
    const bool errorsDiffer =
        e.stats.errors != mStats.errors || e.stats.lastError != mStats.lastError;
    mStats = e.stats;
    mHasStats = true;
    mLastStatsText = e.msg;
    emit statsUpdated(mStats);
    emit activeTransfersChanged(mStats.hasTransferring
                                    ? mStats.transferring
                                    : QVector<RcloneTransferItem>());
    if (errorsDiffer) {
      emit errorsChanged(errorCount(), lastError());
    }
    break;
  }
  case RcloneLogEvent::Kind::Log:
  case RcloneLogEvent::Kind::Text: {
    const bool isError = e.kind == RcloneLogEvent::Kind::Log &&
                         (e.level == RcloneLogLevel::Error ||
                          e.level == RcloneLogLevel::Critical);
    if (isError) {
      ++mErrorEvents;
      mRecentErrors.append(e.object.isEmpty() ? e.msg
                                              : e.object + QStringLiteral(": ") + e.msg);
      while (mRecentErrors.size() > kMaxRecentErrors) {
        mRecentErrors.removeFirst();
      }
    }
    emit logEvent(e);
    if (isError) {
      emit errorsChanged(errorCount(), lastError());
    }
    break;
  }
  }
}

void TransferMonitor::finish(int exitCode, QProcess::ExitStatus status, bool stoppedByUser) {
  if (mResult != Result::Running) {
    return;
  }
  if (stoppedByUser) {
    mResult = Result::Stopped;
  } else if (exitCode == 0 && status == QProcess::NormalExit && !mStats.fatalError) {
    mResult = Result::Finished;
  } else {
    mResult = Result::Error;
  }
  emit finished(mResult);
}

qint64 TransferMonitor::errorCount() const {
  return qMax(mStats.errors, mErrorEvents);
}

QString TransferMonitor::lastError() const {
  if (!mStats.lastError.isEmpty()) {
    return mStats.lastError;
  }
  if (!mRecentErrors.isEmpty()) {
    return mRecentErrors.last();
  }
  return QString();
}

int clampedPercent(const RcloneTransferItem &item) {
  if (item.size <= 0) {
    return -1;
  }
  const qint64 pct = item.bytes * 100 / item.size;
  return int(qBound(qint64(0), pct, qint64(100)));
}
