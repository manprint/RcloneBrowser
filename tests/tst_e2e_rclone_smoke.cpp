#include <QtTest>

#include "rb_test_support.h"

class TestE2eRcloneSmoke : public QObject {
  Q_OBJECT

  // I-9: drop every RCLONE_* variable of the host, then point RCLONE_CONFIG
  // back to the empty temporary config made by rbInitTestEnvironment().
  static QProcessEnvironment cleanEnv() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QStringList keys = env.keys();
    for (const QString &key : keys) {
      if (key.startsWith("RCLONE_")) {
        env.remove(key);
      }
    }
    env.insert("RCLONE_CONFIG", qEnvironmentVariable("RCLONE_CONFIG"));
    return env;
  }

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-E2E-SMOKE-VERSION
  void T_E2E_SMOKE_VERSION() {
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    QProcess p;
    p.setProcessEnvironment(cleanEnv());
    p.start(rbRclone(), {"version"});
    QVERIFY(p.waitForFinished(30000));
    QCOMPARE(p.exitStatus(), QProcess::NormalExit);
    QCOMPARE(p.exitCode(), 0);
    const QString first =
        QString::fromUtf8(p.readAllStandardOutput()).split('\n').value(0);
    QVERIFY2(QRegularExpression(R"(^rclone v1\.(7[5-9]|[89]\d)\.)")
                 .match(first)
                 .hasMatch(),
             qPrintable(first));
  }

  // T-E2E-SMOKE-LSJSON
  void T_E2E_SMOKE_LSJSON() {
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile f(QDir(dir.path()).filePath("a.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    QProcess p;
    p.setProcessEnvironment(cleanEnv());
    p.start(rbRclone(), {"lsjson", dir.path()});
    QVERIFY(p.waitForFinished(30000));
    QCOMPARE(p.exitStatus(), QProcess::NormalExit);
    QCOMPARE(p.exitCode(), 0);

    QJsonParseError err;
    const QJsonDocument doc =
        QJsonDocument::fromJson(p.readAllStandardOutput(), &err);
    QCOMPARE(err.error, QJsonParseError::NoError);
    QVERIFY(doc.isArray());
    QCOMPARE(doc.array().size(), 1);
    const QJsonObject o = doc.array().at(0).toObject();
    QCOMPARE(o.value("Name").toString(), QString("a.txt"));
    QCOMPARE(o.value("Size").toInt(), 1);
  }

  void cleanupTestCase() {}
};

QTEST_GUILESS_MAIN(TestE2eRcloneSmoke)
#include "tst_e2e_rclone_smoke.moc"
