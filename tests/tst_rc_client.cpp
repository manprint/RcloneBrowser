#include <QtTest>

#include "rb_test_support.h"
#include "rc_client.h"
#include "rclone_log.h"

class TestRcClient : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-RC-CREDS
  void T_RC_CREDS() {
    const QRegularExpression userRe("^rb-[A-Za-z0-9_-]{12}$");
    const QRegularExpression passRe("^[A-Za-z0-9_-]{32}$");
    QSet<QString> users, passes;
    for (int i = 0; i < 100; ++i) {
      const RcCredentials c = generateRcCredentials();
      QVERIFY2(userRe.match(c.user).hasMatch(), qPrintable(c.user));
      QVERIFY2(passRe.match(c.pass).hasMatch(), qPrintable(c.pass));
      users.insert(c.user);
      passes.insert(c.pass);
    }
    QCOMPARE(users.size(), 100);
    QCOMPARE(passes.size(), 100);

    QProcessEnvironment env;
    env.insert("KEEP", "1");
    const RcCredentials c{"u", "p"};
    applyRcCredentials(env, c);
    QCOMPARE(env.keys().size(), 3);
    QCOMPARE(env.value("RCLONE_RC_USER"), QString("u"));
    QCOMPARE(env.value("RCLONE_RC_PASS"), QString("p"));
    QCOMPARE(env.value("KEEP"), QString("1"));
  }

  // T-RC-PORT
  void T_RC_PORT() {
    QCOMPARE(parseRcServingPort(u"Serving remote control on http://127.0.0.1:36637/"),
             std::optional<int>(36637));
    QCOMPARE(parseRcServingPort(u"Serving remote control on http://localhost:5572/"),
             std::optional<int>(5572));
    QCOMPARE(parseRcServingPort(u"Serving remote control on http://[::1]:8080/"),
             std::optional<int>(8080));
    QVERIFY(!parseRcServingPort(u"nothing"));
    QVERIFY(!parseRcServingPort(u""));
    QVERIFY(!parseRcServingPort(u"Serving remote control on http://10.0.0.1:5572/"));
    QVERIFY(!parseRcServingPort(u"Serving remote control on http://127.0.0.1:/"));
    QVERIFY(!parseRcServingPort(u"Serving remote control on http://127.0.0.1:0/"));
    QVERIFY(!parseRcServingPort(u"Serving remote control on http://127.0.0.1:65536/"));
    QVERIFY(!parseRcServingPort(u"Serving remote control on http://127.0.0.1:99999999999999/"));
    QCOMPARE(parseRcServingPort(u"Serving remote control on http://127.0.0.1:65535/"),
             std::optional<int>(65535));

    // the first line of a real rcd start
    QFile f(rbFixture("rclone-1.75/rcd_start.stderr"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray line = f.readLine();
    const RcloneLogEvent event = parseRcloneLogLine(line);
    const QRegularExpressionMatch m =
        QRegularExpression("http://127\\.0\\.0\\.1:(\\d+)/").match(event.msg);
    QVERIFY(m.hasMatch());
    const std::optional<int> port = parseRcServingPort(event.msg);
    QVERIFY(port.has_value());
    QVERIFY(*port > 0);
    QCOMPARE(*port, m.captured(1).toInt());
  }

  // T-RC-REPLY
  void T_RC_REPLY() {
    RcReply r = parseRcReply(200, R"({"a":1})", QString());
    QVERIFY(r.ok);
    QCOMPARE(r.httpStatus, 200);
    QCOMPARE(r.result.value("a").toInt(), 1);

    r = parseRcReply(200, "[1]", QString());
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QString("invalid JSON reply"));
    r = parseRcReply(200, "", QString());
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QString("invalid JSON reply"));

    r = parseRcReply(500, R"({"error":"x"})", "Internal Server Error");
    QVERIFY(!r.ok);
    QCOMPARE(r.httpStatus, 500);
    QCOMPARE(r.error, QString("x"));

    r = parseRcReply(401, "", "Host requires authentication");
    QVERIFY(!r.ok);
    QCOMPARE(r.httpStatus, 401);
    QCOMPARE(r.error, QString("Host requires authentication"));

    r = parseRcReply(404, "<html>", QString());
    QCOMPARE(r.error, QString("HTTP 404"));

    r = parseRcReply(0, "", "Connection refused");
    QVERIFY(!r.ok);
    QCOMPARE(r.httpStatus, 0);
    QCOMPARE(r.error, QString("Connection refused"));
    r = parseRcReply(0, "", QString());
    QCOMPARE(r.error, QString("no reply"));

    r = parseRcReply(302, "{}", QString());
    QVERIFY(!r.ok);
    QCOMPARE(r.httpStatus, 302);
    QCOMPARE(r.error, QString("unexpected HTTP status 302"));
  }

  // T-RC-FS
  void T_RC_FS() {
    QCOMPARE(rcListFs("gd", "a/b", "main"), QString("gd:a/b"));
    QCOMPARE(rcListFs("gd", "a/b", "shared"), QString("gd,shared_with_me=true:a/b"));
    QCOMPARE(rcListFs("gd", "a/b", "trash"), QString("gd,trashed_only=true:a/b"));
    QCOMPARE(rcListFs("gd", "a/b", ""), QString("gd:a/b"));
    QCOMPARE(rcListFs("gd", "a/b", "other"), QString("gd:a/b"));
    QCOMPARE(rcListFs("gd", "", "main"), QString("gd:"));
  }
};

QTEST_MAIN(TestRcClient)
#include "tst_rc_client.moc"
