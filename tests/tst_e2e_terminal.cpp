#include <QtTest>
#include <QtWidgets>

#include "rb_test_support.h"
#include "terminal_command.h"

class TestE2eTerminal : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-E2E-TERMINAL-FAKE: a launch built for a fake xterm runs the shell command
  // with the arguments a terminal would receive.
  void T_E2E_TERMINAL_FAKE() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString binDir = tmp.filePath("bin");
    QVERIFY(QDir().mkpath(binDir));

    // records its arguments, then drops the "-e" and runs the rest
    const QString xterm = QDir(binDir).filePath("xterm");
    {
      QFile f(xterm);
      QVERIFY(f.open(QIODevice::WriteOnly));
      f.write("#!/bin/sh\n"
              "printf '%s\\n' \"$@\" > \"$RB_ARGV_OUT\"\n"
              "shift\n"
              "exec \"$@\"\n");
      f.close();
      QVERIFY(f.setPermissions(QFileDevice::ReadOwner |
                               QFileDevice::WriteOwner |
                               QFileDevice::ExeOwner));
    }

    const QString argvOut = tmp.filePath("argv");
    const QString out = tmp.filePath("out");
    const QString cmd = "printf '%s|%s' 'a b' \"it's\" > " + shellQuote(out) +
                        " </dev/null";
    const TerminalLaunch launch = buildTerminalLaunch(xterm, cmd);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("RB_ARGV_OUT", argvOut);
    QProcess p;
    p.setProcessEnvironment(env);
    p.setProgram(launch.program);
    p.setArguments(launch.args);
    p.start();
    QVERIFY(p.waitForFinished(10000));
    QCOMPARE(p.exitStatus(), QProcess::NormalExit);
    QCOMPARE(p.exitCode(), 0);

    QFile argvFile(argvOut);
    QVERIFY(argvFile.open(QIODevice::ReadOnly));
    const QStringList argv = QString::fromUtf8(argvFile.readAll())
                                 .split('\n', Qt::SkipEmptyParts);
    QVERIFY(!argv.isEmpty());
    QCOMPARE(argv.first(), QString("-e"));

    QFile outFile(out);
    QVERIFY(outFile.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(outFile.readAll()), QString("a b|it's"));
  }
};

QTEST_MAIN(TestE2eTerminal)
#include "tst_e2e_terminal.moc"
