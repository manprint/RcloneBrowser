#include <QtTest>

#include "rb_test_support.h"
#include "rclone_format.h"

class TestRcloneFormat : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-FMT-BYTES
  void T_FMT_BYTES_data() {
    QTest::addColumn<qint64>("value");
    QTest::addColumn<QString>("expected");
    QTest::newRow("negative") << qint64(-1) << "-";
    QTest::newRow("zero") << qint64(0) << "0 B";
    QTest::newRow("1023") << qint64(1023) << "1023 B";
    QTest::newRow("1024") << qint64(1024) << "1.000 KiB";
    QTest::newRow("1536") << qint64(1536) << "1.500 KiB";
    QTest::newRow("4500003") << qint64(4500003) << "4.292 MiB";
    QTest::newRow("1GiB") << qint64(1073741824) << "1.000 GiB";
    QTest::newRow("1TiB") << (qint64(1) << 40) << "1.000 TiB";
    QTest::newRow("max") << std::numeric_limits<qint64>::max() << "8.000 EiB";
  }
  void T_FMT_BYTES() {
    QFETCH(qint64, value);
    QFETCH(QString, expected);
    QCOMPARE(formatBytes(value), expected);
    QCOMPARE(formatRate(1536.0), QStringLiteral("1.500 KiB/s"));
    QCOMPARE(formatRate(0.0), QStringLiteral("0 B/s"));
  }

  // T-FMT-DURATION
  void T_FMT_DURATION_data() {
    QTest::addColumn<bool>("hasValue");
    QTest::addColumn<qint64>("seconds");
    QTest::addColumn<QString>("expected");
    QTest::newRow("nullopt") << false << qint64(0) << "-";
    QTest::newRow("negative") << true << qint64(-1) << "-";
    QTest::newRow("0") << true << qint64(0) << "0s";
    QTest::newRow("65") << true << qint64(65) << "1m5s";
    QTest::newRow("3600") << true << qint64(3600) << "1h0m0s";
    QTest::newRow("3723") << true << qint64(3723) << "1h2m3s";
    QTest::newRow("90061") << true << qint64(90061) << "1d1h1m1s";
  }
  void T_FMT_DURATION() {
    QFETCH(bool, hasValue);
    QFETCH(qint64, seconds);
    QFETCH(QString, expected);
    const std::optional<qint64> v =
        hasValue ? std::optional<qint64>(seconds) : std::nullopt;
    QCOMPARE(formatDuration(v), expected);
    QCOMPARE(formatElapsed(65.9), QStringLiteral("1m5s"));
  }

  // T-FMT-PERCENT
  void T_FMT_PERCENT_data() {
    QTest::addColumn<qint64>("part");
    QTest::addColumn<qint64>("total");
    QTest::addColumn<QString>("expected");
    QTest::newRow("zero total") << qint64(5) << qint64(0) << "-";
    QTest::newRow("negative total") << qint64(5) << qint64(-1) << "-";
    QTest::newRow("over") << qint64(150) << qint64(100) << "100%";
    QTest::newRow("half") << qint64(50) << qint64(100) << "50%";
    QTest::newRow("floor") << qint64(1) << qint64(3) << "33%";
    QTest::newRow("negative part") << qint64(-5) << qint64(100) << "0%";
    QTest::newRow("zero part") << qint64(0) << qint64(100) << "0%";
  }
  void T_FMT_PERCENT() {
    QFETCH(qint64, part);
    QFETCH(qint64, total);
    QFETCH(QString, expected);
    QCOMPARE(formatPercent(part, total), expected);
  }

  void cleanupTestCase() {}
};

QTEST_GUILESS_MAIN(TestRcloneFormat)
#include "tst_rclone_format.moc"
