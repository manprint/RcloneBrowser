#include <QtNetwork>
#include <QtTest>

#include "config_flow.h"
#include "rb_test_support.h"
#include "rc_client.h"
#include "rclone_daemon.h"
#include "utils.h"

// R5/R13/R18: ConfigFlow against a real rcd on an empty temporary config
// (I-9: never the host config). The tests run in order: SFTP creates "s",
// UPDATE edits it, OAUTH_CANCEL and BADTYPE leave no remote behind.
class TestE2eConfigFlow : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QString mConf;
  RcloneDaemon *mDaemon = nullptr;
  QVector<ConfigOption> mAsked;

  void startDaemon() {
    mDaemon = new RcloneDaemon;
    mDaemon->start();
    QTRY_VERIFY2_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Running,
                              qPrintable(mDaemon->lastError()), 10000);
  }

  RcCallFn callFn() {
    RcClient *client = mDaemon->client();
    return [this, client](const QString &path, const QJsonObject &params,
                          std::function<void(const RcReply &)> done,
                          int timeoutMs) {
      client->call(path, params, this, std::move(done), timeoutMs);
    };
  }

  // Answers every question from `answers` (by option name), else with the
  // default; a password that is not in the map gets "". Answers are queued.
  void drive(ConfigFlow *flow, const QMap<QString, QString> &answers) {
    mAsked.clear();
    connect(flow, &ConfigFlow::question, flow,
            [this, flow, answers](const ConfigOption &o, const QString &) {
              mAsked.append(o);
              QString text;
              if (answers.contains(o.name)) {
                text = answers.value(o.name);
              } else if (!o.isPassword) {
                text = o.defaultStr;
              }
              QMetaObject::invokeMethod(
                  flow, [flow, text]() { flow->answer(text); },
                  Qt::QueuedConnection);
            });
  }

  QByteArray confBytes() const {
    QFile f(mConf);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
  }

  // The "key = value" lines of [name] in order, empty when absent.
  QStringList section(const QString &name) const {
    QStringList out;
    bool in = false;
    const QStringList lines = QString::fromUtf8(confBytes()).split('\n');
    for (const QString &line : lines) {
      if (line.startsWith('[')) {
        in = line.trimmed() == "[" + name + "]";
        continue;
      }
      if (in && !line.trimmed().isEmpty()) {
        out << line;
      }
    }
    return out;
  }

  static QString keyOf(const QString &line) {
    return line.section(" = ", 0, 0);
  }
  static QString valueOf(const QString &line) {
    return line.section(" = ", 1);
  }
  QString lineFor(const QString &sec, const QString &key) const {
    for (const QString &l : section(sec)) {
      if (keyOf(l) == key) {
        return l;
      }
    }
    return QString();
  }
  QStringList keys(const QString &sec) const {
    QStringList k;
    for (const QString &l : section(sec)) {
      k << keyOf(l);
    }
    return k;
  }

  // `rclone reveal` on the temporary config (I-9).
  QString reveal(const QString &obscured) const {
    QProcess p;
    p.start(rbRclone(), {"reveal", obscured, "--config", mConf});
    p.closeWriteChannel();
    if (!p.waitForFinished(20000)) {
      p.kill();
      p.waitForFinished(5000);
      return QString();
    }
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
  }

  static bool connectionRefused() {
    QTcpSocket s;
    s.connectToHost("127.0.0.1", 53682);
    if (s.waitForConnected(1000)) {
      s.abort();
      return false;
    }
    return s.error() == QAbstractSocket::ConnectionRefusedError;
  }

  // config_fs_advanced and Advanced options are auto-answered, never asked.
  void verifyNoAutoAnswered() const {
    for (const ConfigOption &o : mAsked) {
      QVERIFY2(o.name != QLatin1String("config_fs_advanced"),
               "config_fs_advanced was emitted");
      QVERIFY2(!o.advanced, qPrintable(o.name + " (advanced) was emitted"));
    }
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    qRegisterMetaType<ConfigOption>();
    QVERIFY(!rbRclone().isEmpty());
    QVERIFY(mDir.isValid());
    SetRclone(rbRclone());
    // I-9: rcd and reveal use this empty temporary config, never the host one.
    mConf = mDir.filePath("rclone.conf");
    QFile f(mConf);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
    SetRcloneConf(mConf);
  }

  void cleanup() {
    delete mDaemon;
    mDaemon = nullptr;
  }

  void cleanupTestCase() {
#ifdef Q_OS_UNIX
    int exitCode = -1;
    for (int i = 0; i < 20 && exitCode != 1; ++i) {
      QProcess p;
      p.start("pgrep", {"-f", "rcd --rc-addr 127.0.0.1:0"});
      QVERIFY(p.waitForFinished(5000));
      exitCode = p.exitCode();
      if (exitCode != 1) {
        QTest::qWait(100);
      }
    }
    QCOMPARE(exitCode, 1);
#endif
  }


  // T-E2E-CONFIG-SFTP
  void T_E2E_CONFIG_SFTP() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    ConfigFlow flow(callFn());
    QSignalSpy finished(&flow, &ConfigFlow::finished);
    drive(&flow, {{"host", "example.invalid"},
                  {"user", "bob"},
                  {"port", "2222"},
                  {"pass", "s3cret"}});
    flow.startCreate("s", "sftp");
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
    QVERIFY2(finished.at(0).at(0).toBool(),
             qPrintable(finished.at(0).at(1).toString()));

    verifyNoAutoAnswered();
    if (QTest::currentTestFailed()) {
      return;
    }
    QCOMPARE(keys("s"),
             QStringList({"type", "host", "user", "port", "pass"}));
    QCOMPARE(valueOf(lineFor("s", "type")), QString("sftp"));
    QCOMPARE(valueOf(lineFor("s", "host")), QString("example.invalid"));
    QCOMPARE(valueOf(lineFor("s", "user")), QString("bob"));
    QCOMPARE(valueOf(lineFor("s", "port")), QString("2222"));
    for (const QString &l : section("s")) {
      QVERIFY2(!valueOf(l).isEmpty(), qPrintable(l));
    }
    const QString pass = valueOf(lineFor("s", "pass"));
    QVERIFY(!pass.isEmpty());
    QVERIFY(pass != "s3cret");
    QCOMPARE(reveal(pass), QString("s3cret"));
  }

  // T-E2E-CONFIG-UPDATE
  void T_E2E_CONFIG_UPDATE() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    const QString passBefore = lineFor("s", "pass");
    const QString userBefore = lineFor("s", "user");
    const QString portBefore = lineFor("s", "port");
    QVERIFY(!passBefore.isEmpty());

    {
      ConfigFlow flow(callFn());
      QSignalSpy finished(&flow, &ConfigFlow::finished);
      drive(&flow, {{"host", "other.invalid"}});
      flow.startUpdate("s");
      QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
      QVERIFY2(finished.at(0).at(0).toBool(),
               qPrintable(finished.at(0).at(1).toString()));
      QVERIFY(!mAsked.isEmpty());
      QCOMPARE(mAsked.at(0).name, QString("host"));
      QCOMPARE(mAsked.at(0).defaultStr, QString("example.invalid"));
    }
    QCOMPARE(valueOf(lineFor("s", "host")), QString("other.invalid"));
    QCOMPARE(lineFor("s", "pass"), passBefore); // byte-identical
    QCOMPARE(lineFor("s", "user"), userBefore);
    QCOMPARE(lineFor("s", "port"), portBefore);
    QCOMPARE(keys("s"),
             QStringList({"type", "host", "user", "port", "pass"}));

    {
      ConfigFlow flow(callFn());
      QSignalSpy finished(&flow, &ConfigFlow::finished);
      drive(&flow, {{"pass", "n3w"}});
      flow.startUpdate("s");
      QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
      QVERIFY2(finished.at(0).at(0).toBool(),
               qPrintable(finished.at(0).at(1).toString()));
    }
    const QString pass = valueOf(lineFor("s", "pass"));
    QVERIFY(!pass.isEmpty());
    QVERIFY(lineFor("s", "pass") != passBefore);
    QCOMPARE(reveal(pass), QString("n3w"));
    QCOMPARE(valueOf(lineFor("s", "host")), QString("other.invalid"));
  }

  // T-E2E-CONFIG-OAUTH-CANCEL
  void T_E2E_CONFIG_OAUTH_CANCEL() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    ConfigFlow flow(callFn());
    QSignalSpy oauth(&flow, &ConfigFlow::oauthUrl);
    QSignalSpy finished(&flow, &ConfigFlow::finished);
    drive(&flow, {{"config_is_local", "true"}});
    flow.startCreate("g", "drive");
    QTRY_COMPARE_WITH_TIMEOUT(oauth.count(), 1, 10000);
    const QString url = oauth.at(0).at(0).toUrl().toString();
    QVERIFY2(url.startsWith("http://127.0.0.1:53682/auth"), qPrintable(url));
    QVERIFY(flow.isOAuthRunning());
    QCOMPARE(finished.count(), 0);
    verifyNoAutoAnswered();
    if (QTest::currentTestFailed()) {
      return;
    }

    flow.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QVERIFY(!flow.isOAuthRunning());
    verifyNoAutoAnswered();
    QVERIFY(section("g").isEmpty());
    QVERIFY(!confBytes().contains("[g]"));
    QTRY_VERIFY_WITH_TIMEOUT(connectionRefused(), 5000);
  }

  // T-E2E-CONFIG-BADTYPE
  void T_E2E_CONFIG_BADTYPE() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    const QByteArray before = confBytes();
    ConfigFlow flow(callFn());
    QSignalSpy finished(&flow, &ConfigFlow::finished);
    drive(&flow, {});
    flow.startCreate("x", "nosuch");
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QVERIFY(!finished.at(0).at(1).toString().isEmpty());
    QCOMPARE(confBytes(), before);
  }
};

QTEST_GUILESS_MAIN(TestE2eConfigFlow)
#include "tst_e2e_config_flow.moc"
