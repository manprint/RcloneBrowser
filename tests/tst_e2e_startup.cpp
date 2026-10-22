#include <QtTest>

#include "rb_test_support.h"

// #159: the real application binary, started offscreen with an rclone path
// that is not a usable rclone, must stay alive and say why. Every row runs
// one instance in private HOME, XDG_* and TMPDIR directories (the instance
// lock lives in TMPDIR), with an empty temporary RCLONE_CONFIG (I-9).
class TestE2eStartup : public QObject {
  Q_OBJECT

  static bool writeExecutable(const QString &path, const QByteArray &content) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
      return false;
    }
    f.write(content);
    f.close();
    return f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                            QFileDevice::ExeOwner);
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY2(!qEnvironmentVariable("RB_APP").isEmpty(), "RB_APP is not set");
    QVERIFY(QFileInfo(qEnvironmentVariable("RB_APP")).isExecutable());
  }

  void T_E2E_START_BADRCLONE_data() {
    QTest::addColumn<QString>("kind");
    QTest::newRow("control") << "control";
    QTest::newRow("fclone") << "fclone";
    QTest::newRow("missing") << "missing";
    QTest::newRow("exit1") << "exit1";
    QTest::newRow("false") << "false";
    QTest::newRow("badinterp") << "badinterp";
  }

  // T-E2E-START-BADRCLONE (#159)
  void T_E2E_START_BADRCLONE() {
    QFETCH(QString, kind);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString xdgConfig = root.filePath(QStringLiteral("config"));
    const QString xdgData = root.filePath(QStringLiteral("data"));
    const QString home = root.filePath(QStringLiteral("home"));
    const QString tmp = root.filePath(QStringLiteral("tmp"));
    for (const QString &dir : {xdgConfig, xdgData, home, tmp}) {
      QVERIFY(QDir().mkpath(dir));
    }
    const QString marker = root.filePath(QStringLiteral("marker"));

    // what each row points the rclone setting to
    QString rclone;
    if (kind == QLatin1String("control")) {
      // a working rclone: the marker proves that the settings file is read
      rclone = root.filePath(QStringLiteral("rclone-control.sh"));
      QVERIFY(writeExecutable(
          rclone,
          "#!/bin/sh\n"
          "echo \"$1\" >> '" + marker.toUtf8() + "'\n"
          "case \"$1\" in\n"
          " version) printf 'rclone v1.75.2\\n- os/version: test\\n"
          "- os/kernel: test\\n- os/type: linux\\n- os/arch: amd64\\n"
          "- go/version: go1.25\\n- go/linking: static\\n- go/tags: none\\n';;\n"
          " listremotes) echo '[]';;\n"
          " rcd) exit 1;;\n"
          " *) exit 0;;\n"
          "esac\n"));
    } else if (kind == QLatin1String("fclone")) {
      // a program that is not rclone and exits 0 (the reported fclone)
      rclone = root.filePath(QStringLiteral("fclone.sh"));
      QVERIFY(writeExecutable(rclone, "#!/bin/sh\necho 'fclone v1.0'\n"));
    } else if (kind == QLatin1String("missing")) {
      rclone = root.filePath(QStringLiteral("no/such/rclone"));
    } else if (kind == QLatin1String("exit1")) {
      rclone = root.filePath(QStringLiteral("exit1.sh"));
      QVERIFY(writeExecutable(rclone, "#!/bin/sh\nexit 1\n"));
    } else if (kind == QLatin1String("badinterp")) {
      // executable, but the interpreter does not exist: FailedToStart
      rclone = root.filePath(QStringLiteral("badinterp.sh"));
      QVERIFY(writeExecutable(rclone, "#!/nonexistent/sh\nexit 0\n"));
    } else {
      rclone = QStringLiteral("/bin/false");
    }

    // The app reads rclone-browser.conf (a .ini name would switch it to
    // portable mode).
    const QString confDir = xdgConfig + QStringLiteral("/rclone-browser");
    QVERIFY(QDir().mkpath(confDir));
    {
      QSettings s(confDir + QStringLiteral("/rclone-browser.conf"),
                  QSettings::IniFormat);
      s.setValue(QStringLiteral("Settings/rclone"), rclone);
      s.setValue(QStringLiteral("Settings/checkRcloneUpdates"), false);
      s.setValue(QStringLiteral("Settings/checkRcloneBrowserUpdates"), false);
      s.sync();
      QCOMPARE(s.status(), QSettings::NoError);
    }
    const QString emptyConf = root.filePath(QStringLiteral("rclone-empty.conf"));
    {
      QFile f(emptyConf);
      QVERIFY(f.open(QIODevice::WriteOnly));
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QStringList keys = env.keys();
    for (const QString &key : keys) {
      if (key.startsWith(QLatin1String("RCLONE_"))) {
        env.remove(key);
      }
    }
    env.remove(QStringLiteral("APPIMAGE"));
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfig);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgData);
    env.insert(QStringLiteral("HOME"), home);
    env.insert(QStringLiteral("TMPDIR"), tmp);
    env.insert(QStringLiteral("RCLONE_CONFIG"), emptyConf);

    QProcess app;
    app.setProcessEnvironment(env);
    app.setProcessChannelMode(QProcess::MergedChannels);
    app.start(qEnvironmentVariable("RB_APP"), QStringList());
    QVERIFY2(app.waitForStarted(10000), qPrintable(app.errorString()));

    // alive after 8 s; the output is kept for the checks below
    QByteArray output;
    const bool exited = app.waitForFinished(8000);
    output = app.readAll();
    if (exited) {
      QFAIL(qPrintable(QStringLiteral("the app quit (exit code %1) with rclone "
                                      "= %2\n%3")
                           .arg(app.exitCode())
                           .arg(rclone, QString::fromUtf8(output))));
    }
    QCOMPARE(app.state(), QProcess::Running);

    // stop it before any check can return: one instance at a time
    app.kill();
    QVERIFY(app.waitForFinished(10000));
    output += app.readAll();
    const QString text = QString::fromUtf8(output);

    if (kind == QLatin1String("control")) {
      QFile m(marker);
      QVERIFY2(m.open(QIODevice::ReadOnly),
               qPrintable(QStringLiteral("rclone never ran:\n") + text));
      QVERIFY2(QString::fromUtf8(m.readAll()).contains(QStringLiteral("version")),
               qPrintable(text));
      // a working rclone must not produce a problem report
      QVERIFY2(!text.contains(QStringLiteral("rclone problem:")),
               qPrintable(text));
    } else {
      // the app named the problem: "alive" is not an unrelated modal box
      // one log line says both: the problem and the path
      const QRegularExpression problemLine(
          QStringLiteral("rclone problem:[^\\n]*") +
          QRegularExpression::escape(rclone));
      QVERIFY2(problemLine.match(text).hasMatch(), qPrintable(text));
    }
  }
};

QTEST_GUILESS_MAIN(TestE2eStartup)
#include "tst_e2e_startup.moc"
