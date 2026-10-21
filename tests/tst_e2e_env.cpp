#include <QtTest>

#include "rb_test_support.h"
#include "rclone_log.h"
#include "rclone_process.h"

class TestE2eEnv : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-E2E-ENV-POLLUTED: a user environment with logging variables must not
  // break the app-owned flags (RCLONE_VERBOSE=1 plus --log-level aborts rclone).
  void T_E2E_ENV_POLLUTED() {
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    QTemporaryDir src;
    QTemporaryDir dst;
    QVERIFY(src.isValid() && dst.isValid());
    QFile f(QDir(src.path()).filePath("a.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("hello");
    f.close();

    // The base keeps the temporary RCLONE_CONFIG of rbInitTestEnvironment()
    // (not an I-2 variable) so the host rclone config is never read.
    QProcessEnvironment base = QProcessEnvironment::systemEnvironment();
    QVERIFY(base.contains("RCLONE_CONFIG"));
    base.insert("RCLONE_VERBOSE", "1");
    base.insert("RCLONE_STATS", "5s");
    base.insert("RCLONE_STATS_ONE_LINE", "true");
    const QProcessEnvironment env =
        RcloneEnvironment(RcloneEnvMode::Parsed, base);
    QVERIFY(!env.contains("RCLONE_VERBOSE"));
    QCOMPARE(env.value("RCLONE_CONFIG"), qEnvironmentVariable("RCLONE_CONFIG"));

    const QStringList args =
        PrepareTransferArgs({"copy", src.path(), dst.path()}, env).args;

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.setProcessEnvironment(env);
    p.start(rbRclone(), args);
    QVERIFY(p.waitForFinished(60000));
    QCOMPARE(p.exitStatus(), QProcess::NormalExit);
    const QByteArray out = p.readAll();
    QVERIFY2(p.exitCode() == 0, out.constData());

    int stats = 0;
    int debug = 0;
    for (const QByteArray &line : out.split('\n')) {
      if (line.trimmed().isEmpty()) {
        continue;
      }
      QVERIFY2(!line.contains("Can't set -v and --log-level"), line.constData());
      const RcloneLogEvent e = parseRcloneLogLine(line);
      if (e.kind == RcloneLogEvent::Kind::Stats) {
        ++stats;
      }
      if (e.kind == RcloneLogEvent::Kind::Log &&
          e.level == RcloneLogLevel::Debug) {
        ++debug;
      }
    }
    QVERIFY2(stats >= 1, out.constData());
    QCOMPARE(debug, 0);
    QVERIFY(QFile::exists(QDir(dst.path()).filePath("a.txt")));
  }

  void cleanupTestCase() {}
};

QTEST_GUILESS_MAIN(TestE2eEnv)
#include "tst_e2e_env.moc"
