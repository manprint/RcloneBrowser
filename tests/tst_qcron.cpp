#include <QtTest>

#include "qcron.h"
#include "rb_test_support.h"
#include "scheduler_widget.h"

// The qcron sources are compiled into this test with QT_FORCE_ASSERTS
// (tests/CMakeLists.txt), so an out-of-range QString index aborts here as it
// does in a Debug build of the app.

namespace {

// Saturday 7 November 2026, 01:30 local time.
QDateTime start() { return QDateTime(QDate(2026, 11, 7), QTime(1, 30)); }

QDateTime nextOf(QString pattern, const QDateTime &from) {
  QCron cron(pattern);
  return cron.next(from);
}

} // namespace

class TestQCron : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  void T_QCRON_NEXT_data() {
    QTest::addColumn<QString>("pattern");
    QTest::addColumn<QDateTime>("next");

    QTest::newRow("every minute")
        << "* * * * * *" << QDateTime(QDate(2026, 11, 7), QTime(1, 31));
    QTest::newRow("fixed time")
        << "0 3 * * * *" << QDateTime(QDate(2026, 11, 7), QTime(3, 0));
    QTest::newRow("every 15 minutes")
        << "*/15 * * * * *" << QDateTime(QDate(2026, 11, 7), QTime(1, 45));
    QTest::newRow("weekdays")
        << "0 0 * * 1-5 *" << QDateTime(QDate(2026, 11, 9), QTime(0, 0));
    QTest::newRow("list")
        << "0 0 * * 2,4 *" << QDateTime(QDate(2026, 11, 10), QTime(0, 0));
    QTest::newRow("29 February")
        << "0 0 29 2 * *" << QDateTime(QDate(2028, 2, 29), QTime(0, 0));
    QTest::newRow("31st")
        << "30 2 31 * * *" << QDateTime(QDate(2026, 12, 31), QTime(2, 30));
    QTest::newRow("two digit values")
        << "59 23 31 12 * *" << QDateTime(QDate(2026, 12, 31), QTime(23, 59));
    // the step applies to the range before it, which is not a list element
    // of its own: 30 and the even minutes from 2 to 10
    QTest::newRow("list with a stepped range")
        << "30,1-10/2 * * * * *" << QDateTime(QDate(2026, 11, 7), QTime(2, 2));
    QTest::newRow("list with a step")
        << "1,*/20 * * * * *" << QDateTime(QDate(2026, 11, 7), QTime(1, 40));
    QTest::newRow("range in a list")
        << "0 0 * * 1-2,5 *" << QDateTime(QDate(2026, 11, 9), QTime(0, 0));
  }

  void T_QCRON_NEXT() {
    QFETCH(QString, pattern);
    QFETCH(QDateTime, next);
    QCron cron(pattern);
    QVERIFY2(cron.isValid(), qPrintable(cron.error()));
    QCOMPARE(cron.next(start()), next);
  }

  void T_QCRON_INVALID_data() {
    QTest::addColumn<QString>("pattern");

    QTest::newRow("zero minute step") << "*/0 * * * * *";
    QTest::newRow("zero hour step") << "0 */0 * * * *";
    QTest::newRow("step without a start") << "/5 * * * * *";
    QTest::newRow("trailing comma") << "1, * * * * *";
    QTest::newRow("empty list element") << "1,,2 * * * * *";
    QTest::newRow("list element without a start") << "1,-5 * * * * *";
    QTest::newRow("list step without a start") << "1,/5 * * * * *";
    QTest::newRow("range without an end") << "1- * * * * *";
    QTest::newRow("step without a value") << "*/ * * * * *";
    QTest::newRow("huge value") << "99999999999 * * * * *";
    QTest::newRow("out of range") << "60 * * * * *";
    QTest::newRow("reversed range") << "5-1 * * * * *";
    QTest::newRow("letters") << "a * * * * *";
    QTest::newRow("five fields") << "* * * * *";
  }

  // An invalid pattern is reported, never a crash.
  void T_QCRON_INVALID() {
    QFETCH(QString, pattern);
    QCron cron(pattern);
    QVERIFY(!cron.isValid());
    QVERIFY(!cron.error().isEmpty());
    QVERIFY(!cron.next(start()).isValid());
  }

  void T_QCRON_NEVER_data() {
    QTest::addColumn<QString>("pattern");

    QTest::newRow("30 February") << "0 0 30 2 * *";
    QTest::newRow("31 April") << "0 0 31 4 * *";
    QTest::newRow("31 February every hour") << "0 * 31 2 * *";
    QTest::newRow("step skipping its start") << "1/5 * * * * *";
  }

  // A valid pattern which matches no date has no next run, and finding that
  // out does not hang.
  void T_QCRON_NEVER() {
    QFETCH(QString, pattern);
    QCron cron(pattern);
    QVERIFY2(cron.isValid(), qPrintable(cron.error()));
    QElapsedTimer timer;
    timer.start();
    QVERIFY(!cron.next(start()).isValid());
    QVERIFY2(timer.elapsed() < 5000, "next() took too long");
  }

  void T_QCRON_NEXT_FROM_INVALID() {
    QVERIFY(!nextOf("* * * * * *", QDateTime()).isValid());
  }

  // Five successive runs, as the scheduler's Validate button shows them.
  void T_QCRON_CHAIN() {
    QString pattern("0 12 * * 6 *");
    QCron cron(pattern);
    QVERIFY(cron.isValid());
    QDateTime t = start();
    QList<QDate> days;
    for (int i = 0; i < 5; ++i) {
      t = cron.next(t);
      QVERIFY(t.isValid());
      QCOMPARE(t.time(), QTime(12, 0));
      days << t.date();
    }
    QCOMPARE(days, (QList<QDate>{QDate(2026, 11, 7), QDate(2026, 11, 14),
                                 QDate(2026, 11, 21), QDate(2026, 11, 28),
                                 QDate(2026, 12, 5)}));
  }

  // The scheduler's expression: five fields, names of days and months.
  void T_SCHEDULER_CRON_NEXT_RUN() {
    QString error;
    QCOMPARE(cronNextRun(" 0 12 * * sat ", start(), &error),
             QDateTime(QDate(2026, 11, 7), QTime(12, 0)));
    QVERIFY(error.isEmpty());
    QCOMPARE(cronNextRun("0 0 1 jan *", start()),
             QDateTime(QDate(2027, 1, 1), QTime(0, 0)));

    QVERIFY(!cronNextRun("0 0 30 feb *", start(), &error).isValid());
    QCOMPARE(error, QString("It matches no date."));

    error.clear();
    QVERIFY(!cronNextRun("*/0 * * * *", start(), &error).isValid());
    QVERIFY2(error.contains("step"), qPrintable(error));

    error.clear();
    QVERIFY(!cronNextRun("0 0 * * * *", start(), &error).isValid());
    QVERIFY2(error.contains("Wrong number of fields"), qPrintable(error));
  }

  // The scheduler checks at the start of each minute, the 59th included.
  void T_SCHEDULER_NEXT_MINUTE_data() {
    QTest::addColumn<QTime>("now");
    QTest::addColumn<qint64>("msecs");
    QTest::newRow("start of a minute") << QTime(12, 0) << qint64(60000);
    QTest::newRow("mid minute") << QTime(13, 20, 30) << qint64(30000);
    QTest::newRow("minute 59") << QTime(13, 59, 30) << qint64(30000);
    QTest::newRow("end of the day")
        << QTime(23, 59, 59, 500) << qint64(500);
  }

  void T_SCHEDULER_NEXT_MINUTE() {
    QFETCH(QTime, now);
    QFETCH(qint64, msecs);
    QCOMPARE(msecsToNextMinute(QDateTime(QDate(2026, 11, 7), now)), msecs);
  }
};

QTEST_GUILESS_MAIN(TestQCron)
#include "tst_qcron.moc"
