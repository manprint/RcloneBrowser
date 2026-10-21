#include <QtTest>

#include "rb_test_support.h"
#include "transfer_monitor.h"

namespace {

// Reads a fixture and splits it on '\n' (a final newline does not add an empty line).
QList<QByteArray> readLines(const QString &rel) {
  QFile f(rbFixture(rel));
  if (!f.open(QIODevice::ReadOnly)) {
    qWarning() << "cannot open fixture" << f.fileName();
    return {};
  }
  QList<QByteArray> lines = f.readAll().split('\n');
  if (!lines.isEmpty() && lines.last().isEmpty()) {
    lines.removeLast();
  }
  return lines;
}

int countKind(const QList<QByteArray> &lines, RcloneLogEvent::Kind kind) {
  int n = 0;
  for (const QByteArray &l : lines) {
    if (parseRcloneLogLine(l).kind == kind) {
      ++n;
    }
  }
  return n;
}

} // namespace

class TestTransferMonitor : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    qRegisterMetaType<RcloneStats>();
    qRegisterMetaType<RcloneLogEvent>();
    qRegisterMetaType<QVector<RcloneTransferItem>>();
    qRegisterMetaType<TransferMonitor::Result>();
  }

  // T-MON-COPY
  void T_MON_COPY() {
    const QList<QByteArray> lines = readLines("rclone-1.75/copy_ok.jsonl");
    QVERIFY(!lines.isEmpty());
    const int statsLines = countKind(lines, RcloneLogEvent::Kind::Stats);
    QVERIFY(statsLines >= 2);

    TransferMonitor m;
    QCOMPARE(m.result(), TransferMonitor::Result::Running);
    QVERIFY(!m.hasStats());
    QSignalSpy statsSpy(&m, &TransferMonitor::statsUpdated);
    QSignalSpy activeSpy(&m, &TransferMonitor::activeTransfersChanged);
    for (const QByteArray &l : lines) {
      m.consumeLine(l);
    }
    QVERIFY(m.hasStats());
    QCOMPARE(statsSpy.count(), statsLines);
    QCOMPARE(activeSpy.count(), statsLines);
    const auto lastActive = activeSpy.last().at(0).value<QVector<RcloneTransferItem>>();
    QVERIFY(lastActive.isEmpty());
    QVERIFY(!m.lastStatsText().isEmpty());

    QSignalSpy finishedSpy(&m, &TransferMonitor::finished);
    m.finish(0, QProcess::NormalExit, false);
    QCOMPARE(m.result(), TransferMonitor::Result::Finished);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.at(0).at(0).value<TransferMonitor::Result>(),
             TransferMonitor::Result::Finished);
    QCOMPARE(m.errorCount(), qint64(0));
    QVERIFY(m.lastError().isEmpty());
  }

  // T-MON-ERR
  void T_MON_ERR() {
    const QList<QByteArray> lines = readLines("rclone-1.75/copy_errors.jsonl");
    QVERIFY(!lines.isEmpty());

    TransferMonitor m;
    QSignalSpy errSpy(&m, &TransferMonitor::errorsChanged);
    RcloneStats lastStats;
    for (const QByteArray &l : lines) {
      const RcloneLogEvent e = parseRcloneLogLine(l);
      if (e.kind == RcloneLogEvent::Kind::Stats) {
        lastStats = e.stats;
      }
      m.consumeLine(l);
    }
    QVERIFY(m.errorCount() >= 6);
    QVERIFY(!lastStats.lastError.isEmpty());
    QCOMPARE(m.lastError(), lastStats.lastError);
    QVERIFY(m.recentErrors().size() <= 20);
    QVERIFY(!m.recentErrors().isEmpty());
    QVERIFY2(m.recentErrors().first().contains(QStringLiteral("Failed to copy")),
             qPrintable(m.recentErrors().first()));
    QVERIFY(errSpy.count() >= 1);
    m.finish(1, QProcess::NormalExit, false);
    QCOMPARE(m.result(), TransferMonitor::Result::Error);
  }

  // T-MON-CRITICAL
  void T_MON_CRITICAL() {
    const QList<QByteArray> lines = readLines("rclone-1.75/config_fail.jsonl");
    QVERIFY(!lines.isEmpty());

    TransferMonitor m;
    QSignalSpy statsSpy(&m, &TransferMonitor::statsUpdated);
    for (const QByteArray &l : lines) {
      m.consumeLine(l);
    }
    QCOMPARE(statsSpy.count(), 0);
    QVERIFY(!m.hasStats());
    QCOMPARE(m.errorCount(), qint64(1));
    QVERIFY2(m.lastError().contains(QStringLiteral("didn't find section")),
             qPrintable(m.lastError()));
    m.finish(1, QProcess::NormalExit, false);
    QCOMPARE(m.result(), TransferMonitor::Result::Error);
  }

  // T-MON-CLAMP (I-4)
  void T_MON_CLAMP() {
    auto item = [](qint64 bytes, qint64 size) {
      RcloneTransferItem it;
      it.bytes = bytes;
      it.size = size;
      return it;
    };
    QCOMPARE(clampedPercent(item(150, 100)), 100);
    QCOMPARE(clampedPercent(item(0, -1)), -1);
    QCOMPARE(clampedPercent(item(50, 0)), -1);
    QCOMPARE(clampedPercent(item(5, 10)), 50);
  }

  // T-MON-STOP
  void T_MON_STOP() {
    TransferMonitor m;
    QSignalSpy finishedSpy(&m, &TransferMonitor::finished);
    m.finish(9, QProcess::CrashExit, true);
    QCOMPARE(m.result(), TransferMonitor::Result::Stopped);
    m.finish(0, QProcess::NormalExit, false);
    QCOMPARE(m.result(), TransferMonitor::Result::Stopped);
    QCOMPARE(finishedSpy.count(), 1);
  }

  // T-MON-TEXT
  void T_MON_TEXT() {
    const QList<QByteArray> lines = readLines("synthetic/panic.txt");
    QVERIFY(!lines.isEmpty());

    TransferMonitor m;
    QSignalSpy logSpy(&m, &TransferMonitor::logEvent);
    QSignalSpy statsSpy(&m, &TransferMonitor::statsUpdated);
    for (const QByteArray &l : lines) {
      m.consumeLine(l);
    }
    QCOMPARE(logSpy.count(), 3);
    for (const QList<QVariant> &args : logSpy) {
      QCOMPARE(args.at(0).value<RcloneLogEvent>().kind, RcloneLogEvent::Kind::Text);
    }
    QCOMPARE(statsSpy.count(), 0);
    QCOMPARE(m.errorCount(), qint64(0));
  }
};

QTEST_GUILESS_MAIN(TestTransferMonitor)
#include "tst_transfer_monitor.moc"
