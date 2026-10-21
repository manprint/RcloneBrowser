#include <QtTest>

#include "rb_test_support.h"
#include "rclone_log.h"

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

QList<RcloneLogEvent> parseFixture(const QString &rel) {
  QList<RcloneLogEvent> events;
  const QList<QByteArray> lines = readLines(rel);
  events.reserve(lines.size());
  for (const QByteArray &l : lines) {
    events.append(parseRcloneLogLine(l));
  }
  return events;
}

QList<RcloneLogEvent> statsOf(const QList<RcloneLogEvent> &events) {
  QList<RcloneLogEvent> r;
  for (const RcloneLogEvent &e : events) {
    if (e.kind == RcloneLogEvent::Kind::Stats) {
      r.append(e);
    }
  }
  return r;
}

} // namespace

class TestRcloneLog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-LOG-COPY
  void T_LOG_COPY() {
    const QList<RcloneLogEvent> events = parseFixture("rclone-1.75/copy_ok.jsonl");
    QVERIFY(!events.isEmpty());
    const QList<RcloneLogEvent> stats = statsOf(events);
    QVERIFY(stats.size() >= 2);
    for (const RcloneLogEvent &e : stats) {
      QVERIFY(!e.stats.eta.has_value() || *e.stats.eta >= 0);
    }

    const QSet<QString> expected = {
        QStringLiteral("file 1.bin"), QStringLiteral("file 2.bin"),
        QStringLiteral("file 3.bin"),
        QString::fromUtf8("dir [x] (y)/ñ\"quote'.txt"),
        QStringLiteral("12345 /trail.txt"), QStringLiteral("a:b.txt")};
    bool sawTransferring = false;
    for (const RcloneLogEvent &e : stats) {
      if (e.stats.hasTransferring && !e.stats.transferring.isEmpty()) {
        sawTransferring = true;
        for (const RcloneTransferItem &it : e.stats.transferring) {
          QVERIFY2(expected.contains(it.name), qPrintable(it.name));
        }
      }
    }
    QVERIFY(sawTransferring);

    const RcloneStats &last = stats.last().stats;
    QVERIFY(!last.hasTransferring);
    QCOMPARE(last.transfers, qint64(6));
    QCOMPARE(last.totalTransfers, qint64(6));
    QCOMPARE(last.bytes, last.totalBytes);

    QSet<QString> copied;
    for (const RcloneLogEvent &e : events) {
      QVERIFY(e.level != RcloneLogLevel::Debug);
      if (e.kind == RcloneLogEvent::Kind::Log &&
          e.msg == QLatin1String("Copied (new)")) {
        copied.insert(e.object);
      }
    }
    QCOMPARE(copied, expected);
  }

  // T-LOG-CHECKS
  void T_LOG_CHECKS() {
    const QList<RcloneLogEvent> stats =
        statsOf(parseFixture("rclone-1.75/sync_checks.jsonl"));
    QVERIFY(!stats.isEmpty());
    for (const RcloneLogEvent &e : stats) {
      QVERIFY(e.stats.eta == std::nullopt);
    }
    const RcloneStats &last = stats.last().stats;
    QCOMPARE(last.checks, qint64(6));
    QCOMPARE(last.totalChecks, qint64(6));
    QCOMPARE(last.transfers, qint64(0));
  }

  // T-LOG-ERRORS
  void T_LOG_ERRORS() {
    const QList<RcloneLogEvent> events =
        parseFixture("rclone-1.75/copy_errors.jsonl");
    int errorsWithObject = 0;
    for (const RcloneLogEvent &e : events) {
      if (e.kind == RcloneLogEvent::Kind::Log &&
          e.level == RcloneLogLevel::Error && !e.object.isEmpty() &&
          e.msg.contains(QLatin1String("permission denied"))) {
        ++errorsWithObject;
      }
    }
    QVERIFY2(errorsWithObject >= 6, qPrintable(QString::number(errorsWithObject)));
    const QList<RcloneLogEvent> stats = statsOf(events);
    QVERIFY(!stats.isEmpty());
    const RcloneStats &last = stats.last().stats;
    QVERIFY(last.errors >= 1);
    QVERIFY(!last.lastError.isEmpty());
  }

  // T-LOG-CONFIGFAIL
  void T_LOG_CONFIGFAIL() {
    const QList<RcloneLogEvent> events =
        parseFixture("rclone-1.75/config_fail.jsonl");
    int nonEmpty = 0;
    for (const RcloneLogEvent &e : events) {
      QVERIFY(e.kind != RcloneLogEvent::Kind::Stats);
      if (e.kind == RcloneLogEvent::Kind::Text && e.msg.trimmed().isEmpty()) {
        continue;
      }
      ++nonEmpty;
      QCOMPARE(int(e.kind), int(RcloneLogEvent::Kind::Log));
      QCOMPARE(int(e.level), int(RcloneLogLevel::Critical));
      QVERIFY2(e.msg.contains(QLatin1String("didn't find section in config file")),
               qPrintable(e.msg));
    }
    QCOMPARE(nonEmpty, 1);
  }

  // T-LOG-GARBAGE
  void T_LOG_GARBAGE() {
    const QList<QByteArray> panic = readLines("synthetic/panic.txt");
    QCOMPARE(panic.size(), 4);
    for (const QByteArray &l : panic) {
      const RcloneLogEvent e = parseRcloneLogLine(l);
      QCOMPARE(int(e.kind), int(RcloneLogEvent::Kind::Text));
      QCOMPARE(e.msg, QString::fromUtf8(l));
      QVERIFY(!e.time.isValid());
    }

    QByteArray buf;
    for (int i = 0; i < 512; ++i) {
      buf.append(char(i % 256));
    }
    QCOMPARE(int(parseRcloneLogLine(buf).kind), int(RcloneLogEvent::Kind::Text));

    const QList<QByteArray> v = readLines("synthetic/stats_variants.jsonl");
    QCOMPARE(v.size(), 9);
    QCOMPARE(int(parseRcloneLogLine(v[3]).kind), int(RcloneLogEvent::Kind::Text)); // (d)
    QCOMPARE(int(parseRcloneLogLine(v[4]).kind), int(RcloneLogEvent::Kind::Text)); // (e)
    QCOMPARE(int(parseRcloneLogLine(v[7]).kind), int(RcloneLogEvent::Kind::Text)); // (h)
    const RcloneLogEvent f = parseRcloneLogLine(v[5]);
    QCOMPARE(int(f.kind), int(RcloneLogEvent::Kind::Log));
    QCOMPARE(int(f.level), int(RcloneLogLevel::Notice));
    const RcloneLogEvent g = parseRcloneLogLine(v[6]);
    QCOMPARE(int(g.kind), int(RcloneLogEvent::Kind::Log));
    QCOMPARE(int(g.level), int(RcloneLogLevel::Error));
    QCOMPARE(g.object, QStringLiteral("o"));
  }

  // T-LOG-VARIANTS
  void T_LOG_VARIANTS() {
    const QList<QByteArray> v = readLines("synthetic/stats_variants.jsonl");
    QCOMPARE(v.size(), 9);

    const RcloneLogEvent a = parseRcloneLogLine(v[0]);
    QCOMPARE(int(a.kind), int(RcloneLogEvent::Kind::Stats));
    QVERIFY(a.stats.hasTransferring);
    QCOMPARE(a.stats.transferring.size(), 1);
    QCOMPARE(a.stats.transferring[0].size, qint64(-1));
    QVERIFY(a.stats.transferring[0].eta == std::nullopt);

    const RcloneLogEvent b = parseRcloneLogLine(v[1]);
    QCOMPARE(int(b.kind), int(RcloneLogEvent::Kind::Stats));
    QCOMPARE(b.stats.transferring.size(), 1);
    QCOMPARE(b.stats.transferring[0].bytes, qint64(150));
    QCOMPARE(b.stats.transferring[0].percentage, 150);

    const RcloneLogEvent c = parseRcloneLogLine(v[2]);
    QCOMPARE(int(c.kind), int(RcloneLogEvent::Kind::Stats));
    const RcloneStats &s = c.stats;
    QCOMPARE(s.bytes, qint64(5));
    QCOMPARE(s.totalBytes, qint64(0));
    QCOMPARE(s.checks, qint64(0));
    QCOMPARE(s.totalChecks, qint64(0));
    QCOMPARE(s.transfers, qint64(0));
    QCOMPARE(s.totalTransfers, qint64(0));
    QCOMPARE(s.errors, qint64(0));
    QCOMPARE(s.deletes, qint64(0));
    QCOMPARE(s.renames, qint64(0));
    QCOMPARE(s.listed, qint64(0));
    QCOMPARE(s.serverSideCopies, qint64(0));
    QCOMPARE(s.serverSideMoves, qint64(0));
    QCOMPARE(s.elapsedTime, 0.0);
    QCOMPARE(s.transferTime, 0.0);
    QCOMPARE(s.speed, 0.0);
    QVERIFY(s.eta == std::nullopt);
    QVERIFY(!s.fatalError);
    QVERIFY(!s.retryError);
    QVERIFY(s.lastError.isEmpty());
    QVERIFY(!s.hasTransferring);
    QVERIFY(s.transferring.isEmpty());

    const RcloneLogEvent i = parseRcloneLogLine(v[8]);
    QCOMPARE(int(i.kind), int(RcloneLogEvent::Kind::Log));
    QCOMPARE(int(i.level), int(RcloneLogLevel::Info));
    QCOMPARE(i.msg, QStringLiteral("Bisync successful"));
    QCOMPARE(i.object, QStringLiteral("p"));
  }

  // T-LOG-TIME
  void T_LOG_TIME() {
    const QDateTime a = parseRfc3339(QStringLiteral("2026-10-21T17:19:56.651053406+02:00"));
    QVERIFY(a.isValid());
    QCOMPARE(a.time().msec(), 651);
    QCOMPARE(a.offsetFromUtc(), 7200);
    const QDateTime b = parseRfc3339(QStringLiteral("2026-10-21T15:22:53.56219+00:00"));
    QVERIFY(b.isValid());
    QCOMPARE(b.time().msec(), 562);
    QVERIFY(!parseRfc3339(QStringLiteral("garbage")).isValid());
  }

  // T-LOG-RENDER
  void T_LOG_RENDER() {
    const QTimeZone utc(QByteArray("UTC"));
    QVERIFY(utc.isValid());

    const RcloneLogEvent log = parseRcloneLogLine(QByteArrayView(
        R"J({"time":"2026-10-21T17:19:56.651053406+02:00","level":"info","msg":"Copied (new)","object":"a b.txt"})J"));
    QCOMPARE(int(log.kind), int(RcloneLogEvent::Kind::Log));
    QCOMPARE(renderRcloneLogEvent(log, utc),
             QStringLiteral("2026/10/21 15:19:56 INFO  : a b.txt: Copied (new)"));

    const RcloneLogEvent st = parseRcloneLogLine(QByteArrayView(
        R"J({"time":"2026-10-21T17:19:56.651053406+02:00","level":"notice","msg":"\nTransferred: 0 B\n\n","stats":{"bytes":0}})J"));
    QCOMPARE(int(st.kind), int(RcloneLogEvent::Kind::Stats));
    QVERIFY(renderRcloneLogEvent(st, utc)
                .startsWith(QStringLiteral("2026/10/21 15:19:56 NOTICE: ")));

    const RcloneLogEvent text = parseRcloneLogLine(QByteArrayView("panic: boom\r\n"));
    QCOMPARE(int(text.kind), int(RcloneLogEvent::Kind::Text));
    QCOMPARE(renderRcloneLogEvent(text, utc), QStringLiteral("panic: boom"));
  }

  void cleanupTestCase() {}
};

QTEST_GUILESS_MAIN(TestRcloneLog)
#include "tst_rclone_log.moc"
