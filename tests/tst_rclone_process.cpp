#include <QtTest>

#include "rb_test_support.h"
#include "rclone_process.h"
#include "utils.h"

class TestRcloneProcess : public QObject {
  Q_OBJECT

  static QStringList owned(const QString &verbosity = "-v") {
    return {"--use-json-log", verbosity,           "--stats",
            "1s",             "--stats-log-level", "NOTICE",
            "--stats-file-name-length", "0",       "--color",
            "NEVER"};
  }

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-ARGS-STRIP
  void T_ARGS_STRIP() {
    const TransferArgs ta = PrepareTransferArgs(
        {"copy", "a", "b", "-q", "--log-file", "/x.log", "--progress",
         "--stats=5s", "--stats-one-line", "--transfers", "4", "-vv"},
        QProcessEnvironment());
    QCOMPARE(ta.args, QStringList({"copy", "a", "b", "--transfers", "4"}) +
                          owned("-vv"));
    QCOMPARE(ta.logFile, QString("/x.log"));
    QVERIFY(ta.debug);
    QCOMPARE(ta.ignored,
             QStringList({"-q", "--log-file /x.log", "--progress",
                          "--stats=5s", "--stats-one-line", "-vv"}));
  }

  // T-ARGS-DEFAULT
  void T_ARGS_DEFAULT() {
    const TransferArgs ta =
        PrepareTransferArgs({"copy", "a", "b"}, QProcessEnvironment());
    QCOMPARE(ta.args, QStringList({"copy", "a", "b"}) + owned("-v"));
    QVERIFY(!ta.debug);
    QVERIFY(ta.ignored.isEmpty());
    QVERIFY(ta.logFile.isEmpty());
  }

  // T-ARGS-COLOR
  void T_ARGS_COLOR() {
    const TransferArgs ta = PrepareTransferArgs(
        {"copy", "a", "b", "--color", "ALWAYS"}, QProcessEnvironment());
    QCOMPARE(ta.ignored, QStringList({"--color ALWAYS"}));
    QCOMPARE(ta.args.count("--color"), 1);
    const int i = ta.args.indexOf("--color");
    QVERIFY(i >= 0 && i + 1 < ta.args.size());
    QCOMPARE(ta.args.at(i + 1), QString("NEVER"));
  }

  // T-ARGS-LOGLEVEL
  void T_ARGS_LOGLEVEL() {
    const TransferArgs ta = PrepareTransferArgs(
        {"sync", "a", "b", "--log-level=DEBUG"}, QProcessEnvironment());
    QVERIFY(ta.debug);
    QCOMPARE(ta.ignored, QStringList({"--log-level=DEBUG"}));
    QVERIFY(!ta.args.contains("--log-level"));
    QVERIFY(ta.args.contains("-vv"));
  }

  // T-ARGS-DASHDASH
  void T_ARGS_DASHDASH() {
    const TransferArgs ta = PrepareTransferArgs(
        {"copy", "a", "b", "--", "-q"}, QProcessEnvironment());
    QCOMPARE(ta.args, QStringList({"copy", "a", "b"}) + owned("-v") +
                          QStringList({"--", "-q"}));
    QVERIFY(ta.ignored.isEmpty());
  }

  // T-ARGS-MISSINGVALUE
  void T_ARGS_MISSINGVALUE() {
    const TransferArgs ta = PrepareTransferArgs(
        {"copy", "a", "b", "--log-file"}, QProcessEnvironment());
    QCOMPARE(ta.args, QStringList({"copy", "a", "b"}) + owned("-v"));
    QCOMPARE(ta.logFile, QString());
    QCOMPARE(ta.ignored, QStringList({"--log-file"}));
  }

  // T-ARGS-ENVLOGFILE
  void T_ARGS_ENVLOGFILE() {
    QProcessEnvironment env;
    env.insert("RCLONE_LOG_FILE", "/e.log");
    const TransferArgs ta = PrepareTransferArgs({"copy", "a", "b"}, env);
    QCOMPARE(ta.logFile, QString("/e.log"));
    QVERIFY(ta.ignored.isEmpty());
  }

  // T-ENV-STRIP
  void T_ENV_STRIP() {
    QProcessEnvironment base;
    const QStringList &vars = RcloneLoggingEnvVars();
    QCOMPARE(vars.size(), 22);
    for (const QString &v : vars) {
      base.insert(v, "1");
    }
    base.insert("RCLONE_CONFIG", "/c");
    base.insert("RCLONE_PASSWORD_COMMAND", "x");
    base.insert("PATH", "/usr/bin");

    const QProcessEnvironment parsed =
        RcloneEnvironment(RcloneEnvMode::Parsed, base);
    for (const QString &v : vars) {
      QVERIFY2(!parsed.contains(v), qPrintable(v));
    }
    QCOMPARE(parsed.value("RCLONE_CONFIG"), QString("/c"));
    QCOMPARE(parsed.value("RCLONE_PASSWORD_COMMAND"), QString("x"));
    QCOMPARE(parsed.value("PATH"), QString("/usr/bin"));

    const QProcessEnvironment inter =
        RcloneEnvironment(RcloneEnvMode::Interactive, base);
    for (const QString &k : base.keys()) {
      QVERIFY2(inter.contains(k), qPrintable(k));
    }
  }

  // T-ENV-PASS
  void T_ENV_PASS() {
    QProcessEnvironment base;
    base.insert("PATH", "/usr/bin");
    SetRclonePassword("pw");
    QCOMPARE(RcloneEnvironment(RcloneEnvMode::Parsed, base)
                 .value("RCLONE_CONFIG_PASS"),
             QString("pw"));
    QCOMPARE(RcloneEnvironment(RcloneEnvMode::Interactive, base)
                 .value("RCLONE_CONFIG_PASS"),
             QString("pw"));
    SetRclonePassword("");
    QVERIFY(!RcloneEnvironment(RcloneEnvMode::Parsed, base)
                 .contains("RCLONE_CONFIG_PASS"));
    QVERIFY(!RcloneEnvironment(RcloneEnvMode::Interactive, base)
                 .contains("RCLONE_CONFIG_PASS"));
  }

  // T-CFGPASS-CLASSIFY
  void T_CFGPASS_CLASSIFY_data() {
    QTest::addColumn<int>("exitCode");
    QTest::addColumn<QString>("file");
    QTest::addColumn<int>("expected");
    QTest::newRow("needs") << 1 << "synthetic/config_errors/needs.txt"
                           << int(ConfigLoadProblem::NeedsPassword);
    QTest::newRow("wrong") << 1 << "synthetic/config_errors/wrong.txt"
                           << int(ConfigLoadProblem::WrongPassword);
    QTest::newRow("cmdfail") << 1 << "synthetic/config_errors/cmdfail.txt"
                             << int(ConfigLoadProblem::PasswordCommandFailed);
    QTest::newRow("other") << 1 << "synthetic/config_errors/other.txt"
                           << int(ConfigLoadProblem::Other);
    QTest::newRow("exit0") << 0 << QString() << int(ConfigLoadProblem::None);
  }

  void T_CFGPASS_CLASSIFY() {
    QFETCH(int, exitCode);
    QFETCH(QString, file);
    QFETCH(int, expected);
    QByteArray stderrBytes;
    if (!file.isEmpty()) {
      QFile f(rbFixture(file));
      QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
      stderrBytes = f.readAll();
      QVERIFY(!stderrBytes.isEmpty());
    }
    QCOMPARE(int(classifyConfigLoadError(exitCode, stderrBytes)), expected);
  }

  void cleanupTestCase() { SetRclonePassword(""); }
};

QTEST_GUILESS_MAIN(TestRcloneProcess)
#include "tst_rclone_process.moc"
