#include <QtTest>

#include "lsjson_parser.h"
#include "rb_test_support.h"
#include "rclone_process.h"
#include "utils.h"

// R8: encrypted rclone config, three ways to fail and one way to succeed.
class TestE2eConfigPassword : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QString mConf;

  struct Result {
    bool finished = false;
    int exitCode = -1;
    QByteArray out;
    QByteArray err;
  };

  static QProcessEnvironment baseEnv() {
    QProcessEnvironment e = QProcessEnvironment::systemEnvironment();
    e.remove("RCLONE_CONFIG_PASS");
    e.remove("RCLONE_PASSWORD_COMMAND");
    return e;
  }

  static Result run(const QStringList &args, const QProcessEnvironment &env) {
    Result r;
    QProcess p;
    p.setProcessEnvironment(env);
    p.start(rbRclone(), args);
    p.closeWriteChannel();
    r.finished = p.waitForFinished(20000);
    if (!r.finished) {
      p.kill();
      p.waitForFinished(5000);
    }
    r.exitCode = p.exitCode();
    r.out = p.readAllStandardOutput();
    r.err = p.readAllStandardError();
    return r;
  }

  QStringList listArgs() const {
    return {"listremotes", "--long", "--json", "--ask-password=false",
            "--config", mConf};
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    QVERIFY(mDir.isValid());
    mConf = QDir(mDir.path()).filePath("c.conf");
    QFile f(mConf);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[mem]\ntype = memory\n");
    f.close();

    QProcessEnvironment env = baseEnv();
    env.insert("RCLONE_PASSWORD_COMMAND", "echo pw");
    const Result r = run({"config", "encryption", "set", "--config", mConf},
                         RcloneEnvironment(RcloneEnvMode::Parsed, env));
    QVERIFY2(r.finished && r.exitCode == 0, r.err.constData());
    QFile c(mConf);
    QVERIFY(c.open(QIODevice::ReadOnly));
    QVERIFY(c.readAll().startsWith("# Encrypted rclone configuration File"));
    SetRclonePassword("");
  }

  // T-E2E-CFGPASS-COMMAND (#79)
  void T_E2E_CFGPASS_COMMAND() {
    QProcessEnvironment env = baseEnv();
    env.insert("RCLONE_PASSWORD_COMMAND", "echo pw");
    const QProcessEnvironment parsed =
        RcloneEnvironment(RcloneEnvMode::Parsed, env);
    QCOMPARE(parsed.value("RCLONE_PASSWORD_COMMAND"), QString("echo pw"));
    const Result r = run(listArgs(), parsed);
    QVERIFY2(r.finished, "timeout");
    QVERIFY2(r.exitCode == 0, r.err.constData());
    QCOMPARE(int(classifyConfigLoadError(r.exitCode, r.err)),
             int(ConfigLoadProblem::None));
    const auto remotes = parseListRemotesJson(r.out);
    QVERIFY2(remotes.has_value(), r.out.constData());
    QCOMPARE(remotes->size(), 1);
    QCOMPARE(remotes->at(0).name, QString("mem"));
  }

  // T-E2E-CFGPASS-NEEDS
  void T_E2E_CFGPASS_NEEDS() {
    SetRclonePassword("");
    const QProcessEnvironment env =
        RcloneEnvironment(RcloneEnvMode::Parsed, baseEnv());
    QVERIFY(!env.contains("RCLONE_CONFIG_PASS"));
    QVERIFY(!env.contains("RCLONE_PASSWORD_COMMAND"));
    const Result r = run(listArgs(), env);
    QVERIFY2(r.finished, "timeout");
    QVERIFY2(r.exitCode != 0, r.out.constData());
    QCOMPARE(int(classifyConfigLoadError(r.exitCode, r.err)),
             int(ConfigLoadProblem::NeedsPassword));
  }

  // T-E2E-CFGPASS-WRONG
  void T_E2E_CFGPASS_WRONG() {
    SetRclonePassword("bad");
    const QProcessEnvironment env =
        RcloneEnvironment(RcloneEnvMode::Parsed, baseEnv());
    SetRclonePassword("");
    QCOMPARE(env.value("RCLONE_CONFIG_PASS"), QString("bad"));
    const Result r = run(listArgs(), env);
    QVERIFY2(r.finished, "timeout");
    QVERIFY2(r.exitCode != 0, r.out.constData());
    QCOMPARE(int(classifyConfigLoadError(r.exitCode, r.err)),
             int(ConfigLoadProblem::WrongPassword));
  }

  // T-E2E-CFGPASS-CMDFAIL
  void T_E2E_CFGPASS_CMDFAIL() {
    SetRclonePassword("");
    QProcessEnvironment env = baseEnv();
    env.insert("RCLONE_PASSWORD_COMMAND", "false");
    const Result r =
        run(listArgs(), RcloneEnvironment(RcloneEnvMode::Parsed, env));
    QVERIFY2(r.finished, "timeout");
    QVERIFY2(r.exitCode != 0, r.out.constData());
    QCOMPARE(int(classifyConfigLoadError(r.exitCode, r.err)),
             int(ConfigLoadProblem::PasswordCommandFailed));
  }

  void cleanupTestCase() { SetRclonePassword(""); }
};

QTEST_GUILESS_MAIN(TestE2eConfigPassword)
#include "tst_e2e_config_password.moc"
