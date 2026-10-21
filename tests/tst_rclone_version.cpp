#include <QtTest>

#include "rb_test_support.h"
#include "rclone_version.h"
#include "update_check.h"

class TestRcloneVersion : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  void T_VER_PARSE_data() {
    QTest::addColumn<QString>("output");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("triple");
    QTest::addColumn<QString>("suffix");

    QTest::newRow("multi-line")
        << QString("rclone v1.75.2\n- os/version: ubuntu 24.04 (64 bit)\n"
                   "- go/version: go1.25")
        << true << "1.75.2" << "";
    QTest::newRow("beta suffix")
        << QString("rclone v1.76.0-beta.8901.0e4a5b6c7.master") << true
        << "1.76.0" << "beta.8901.0e4a5b6c7.master";
    QTest::newRow("DEV suffix")
        << QString("rclone v1.75.2-DEV") << true << "1.75.2" << "DEV";
    QTest::newRow("no v") << QString("rclone 1.75.0") << true << "1.75.0" << "";
    QTest::newRow("old DEV") << QString("rclone v1.60.1-DEV") << true
                             << "1.60.1" << "DEV";
    QTest::newRow("CRLF") << QString("rclone v1.75.2\r\n- os/version: x")
                          << true << "1.75.2" << "";
    QTest::newRow("garbage") << QString("foo") << false << "" << "";
    QTest::newRow("letters") << QString("rclone vX.Y") << false << "" << "";
    QTest::newRow("empty") << QString("") << false << "" << "";
    QTest::newRow("two parts") << QString("rclone v1.75") << false << "" << "";
    QTest::newRow("overflow")
        << QString("rclone v99999999999.1.1") << false << "" << "";
  }

  void T_VER_PARSE() {
    QFETCH(QString, output);
    QFETCH(bool, valid);
    QFETCH(QString, triple);
    QFETCH(QString, suffix);
    const RcloneVersion v = RcloneVersion::fromVersionOutput(output);
    QCOMPARE(v.valid, valid);
    if (valid) {
      QCOMPARE(QString("%1.%2.%3").arg(v.major).arg(v.minor).arg(v.patch),
               triple);
      QCOMPARE(v.suffix, suffix);
      QCOMPARE(v.toString(),
               suffix.isEmpty() ? triple : triple + "-" + suffix);
    } else {
      QCOMPARE(v.toString(), QString());
    }
    // fromTag: same triple/suffix grammar, whole string only.
    QVERIFY(RcloneVersion::fromTag(u"v1.75.2").valid);
    QVERIFY(RcloneVersion::fromTag(u"1.75.2").valid);
    QVERIFY(!RcloneVersion::fromTag(u"v1.75.2\n").valid);
    QVERIFY(!RcloneVersion::fromTag(u"latest").valid);
    QVERIFY(!RcloneVersion::fromTag(u"v1.75").valid);
  }

  void T_VER_COMPARE() {
    auto v = [](const char *tag) { return RcloneVersion::fromTag(QString(tag)); };
    QVERIFY(v("1.75.2").compare(v("1.75.0")) > 0);
    QVERIFY(v("1.75.0").compare(v("1.75.2")) < 0);
    QVERIFY(v("1.75.0").compare(v("1.75.0-beta.1")) > 0);
    QVERIFY(v("1.75.0-beta.1").compare(v("1.75.0")) < 0);
    QVERIFY(v("1.76.0-beta.1").compare(v("1.75.2")) > 0);
    QVERIFY(v("1.75.0-beta.1").compare(v("1.75.0-beta.2")) < 0);
    QCOMPARE(v("2.0.0").compare(v("2.0.0")), 0);
    QCOMPARE(v("2.0.0-rc.1").compare(v("2.0.0-rc.1")), 0);
  }

  void T_VER_SUPPORTED() {
    auto v = [](const char *tag) { return RcloneVersion::fromTag(QString(tag)); };
    QCOMPARE(minimumRcloneVersion().toString(), QString("1.75.0"));
    QVERIFY(isSupportedRclone(v("1.75.0")));
    QVERIFY(isSupportedRclone(v("1.75.0-beta.x")));
    QVERIFY(isSupportedRclone(v("1.75.2-DEV")));
    QVERIFY(isSupportedRclone(v("1.80.0")));
    QVERIFY(!isSupportedRclone(v("1.74.9")));
    QVERIFY(!isSupportedRclone(v("1.60.1")));
    QVERIFY(!isSupportedRclone(RcloneVersion()));
  }

  void T_VER_PROBLEM() {
    QVERIFY(rcloneVersionProblem("rclone v1.75.2").isEmpty());
    QVERIFY(rcloneVersionProblem("rclone v1.75.0-DEV\n- os/version: x")
                .isEmpty());

    const QString old = rcloneVersionProblem("rclone v1.60.1");
    QVERIFY(old.contains("Found: rclone v1.60.1"));
    QVERIFY(old.startsWith(
        "Rclone Browser 2 requires rclone 1.75.0 or newer.\n"));

    const QString bad = rcloneVersionProblem("garbage\nsecond line");
    QVERIFY(bad.contains("Could not recognize"));
    QVERIFY(bad.endsWith("'rclone version':\ngarbage"));

    // the echoed first line is capped at 200 characters
    const QString longLine = rcloneVersionProblem(QString(500, QChar('x')));
    QVERIFY(longLine.endsWith(":\n" + QString(200, QChar('x'))));
  }

  void T_UPD_PARSE() {
    const auto ok = parseLatestRelease(
        "{\"tag_name\":\"v2.0.0-beta.1\",\"html_url\":\"https://x\"}");
    QVERIFY(ok.has_value());
    QCOMPARE(ok->tag, QString("v2.0.0-beta.1"));
    QCOMPARE(ok->version.toString(), QString("2.0.0-beta.1"));
    QCOMPARE(ok->htmlUrl, QUrl("https://x"));

    const auto noUrl = parseLatestRelease("{\"tag_name\":\"v1.2.3\"}");
    QVERIFY(noUrl.has_value());
    QVERIFY(noUrl->htmlUrl.isEmpty());

    QVERIFY(!parseLatestRelease("{\"tag_name\":\"latest\"}").has_value());
    QVERIFY(!parseLatestRelease("{\"tag_name\":5}").has_value());
    QVERIFY(!parseLatestRelease("[]").has_value());
    QVERIFY(!parseLatestRelease("not json").has_value());
    QVERIFY(!parseLatestRelease("").has_value());
  }

  void T_UPD_SLUG() {
    QVERIFY(isValidRepoSlug("rclone/rclone"));
    QVERIFY(isValidRepoSlug("a-b/c.d_e"));
    QVERIFY(!isValidRepoSlug("rclone"));
    QVERIFY(!isValidRepoSlug("a/b/c"));
    QVERIFY(!isValidRepoSlug("a b/c"));
    QVERIFY(!isValidRepoSlug(""));
    QVERIFY(!isValidRepoSlug("a/b\n"));
  }

  void T_UPD_DUE() {
    const QDate today(2026, 10, 21);
    QVERIFY(!updateCheckDue("2026-10-21", today));
    QVERIFY(updateCheckDue("2026-10-20", today));
    QVERIFY(updateCheckDue("Wed Oct 21 2026", today)); // legacy locale format
    QVERIFY(updateCheckDue("", today));
  }
};

QTEST_MAIN(TestRcloneVersion)
#include "tst_rclone_version.moc"
