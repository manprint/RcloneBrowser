#include <QtTest>
#include <QtWidgets>

#include "instance_lock.h"
#include "rb_test_support.h"
#include "terminal_command.h"

class TestPlatformHelpers : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-LOCK-NAME
  void T_LOCK_NAME() {
    QCOMPARE(instanceLockFileName(false, 0),
             QString(".RcloneBrowser_4q6RgLs2RpbJA.lock"));
    QCOMPARE(instanceLockFileName(true, 1000),
             QString(".RcloneBrowser_4q6RgLs2RpbJA_1000.lock"));
  }

  // T-LOCK-CONFLICT: the same user's lock is taken once; another uid is free.
  void T_LOCK_CONFLICT() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString base = dir.path() + "/";

    QLockFile first(base + instanceLockFileName(true, 1000));
    QVERIFY(first.tryLock(100));
    QLockFile second(base + instanceLockFileName(true, 1000));
    QVERIFY(!second.tryLock(100));

    QLockFile otherUser(base + instanceLockFileName(true, 1001));
    QVERIFY(otherUser.tryLock(100));
  }

  // T-TERM-QUOTE
  void T_TERM_QUOTE() {
    QCOMPARE(shellQuote("a b"), QString("'a b'"));
    QCOMPARE(shellQuote("it's"), QString("'it'\\''s'"));
    QCOMPARE(shellQuote(""), QString("''"));
  }

  // T-TERM-BUILD
  void T_TERM_BUILD_data() {
    QTest::addColumn<QString>("terminal");
    QTest::addColumn<QStringList>("prefix"); // arguments before "sh -c <cmd>"

    QTest::newRow("gnome-terminal")
        << QString("/usr/bin/gnome-terminal")
        << QStringList{"--wait", "--"};
    QTest::newRow("konsole")
        << QString("/usr/bin/konsole")
        << QStringList{"--nofork", "-e"};
    QTest::newRow("xfce4-terminal")
        << QString("/usr/bin/xfce4-terminal")
        << QStringList{"--disable-server", "-x"};
    QTest::newRow("xterm") << QString("/usr/bin/xterm") << QStringList{"-e"};
    QTest::newRow("myterm")
        << QString("/opt/foo/myterm")
        << QStringList{"-e"};
  }

  void T_TERM_BUILD() {
    QFETCH(QString, terminal);
    QFETCH(QStringList, prefix);

    const TerminalLaunch launch = buildTerminalLaunch(terminal, "echo hi");
    QCOMPARE(launch.program, terminal);
    const QStringList want = prefix + QStringList{"sh", "-c", "echo hi"};
    QCOMPARE(launch.args, want);
  }

  // T-TERM-CANDIDATES
  void T_TERM_CANDIDATES() {
    QProcessEnvironment withTerminal;
    withTerminal.insert("TERMINAL", "/x/t");
    const QStringList first = terminalCandidates(withTerminal);
    QVERIFY(!first.isEmpty());
    QCOMPARE(first.first(), QString("/x/t"));

    const QStringList plain = terminalCandidates(QProcessEnvironment());
    QVERIFY(!plain.isEmpty());
    QCOMPARE(plain.first(), QString("gnome-terminal"));
  }

  // T-TERM-CONFIGCMD
  void T_TERM_CONFIGCMD() {
    const QString cmd = rcloneConfigShellCommand(
        "/a b/rclone", {"--config", "/c d/r.conf"});
    QCOMPARE(cmd, QString("'/a b/rclone' config --config '/c d/r.conf'; "
                          "printf '\\n%s' 'Press Enter to close'; read _"));
  }
};

QTEST_MAIN(TestPlatformHelpers)
#include "tst_platform_helpers.moc"
