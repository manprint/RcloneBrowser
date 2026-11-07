#include <QtTest>

#include "mount_launch.h"
#include "rb_test_support.h"

class TestMountLaunch : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-MOUNT-ARGV (I-5)
  void T_MOUNT_ARGV() {
    const RcCredentials creds{"rb-user", "secret-pass"};
    QProcessEnvironment base;
    base.insert("KEEP", "1");

    const MountLaunch ml = buildMountLaunch(
        {"mount", "r:", "/m", "--rc", "--rc-addr", "localhost:5600",
         "--rc-user=old", "--rc-pass=old2", "--rc-user", "x",
         "--vfs-cache-mode", "full"},
        base, creds);
    QCOMPARE(ml.args,
             QStringList({"mount", "r:", "/m", "--rc", "--rc-addr",
                          "localhost:5600", "--vfs-cache-mode", "full"}));
    QVERIFY(ml.rcEnabled);
    QCOMPARE(ml.creds.user, creds.user);
    QCOMPARE(ml.creds.pass, creds.pass);
    QCOMPARE(ml.env.value("RCLONE_RC_USER"), creds.user);
    QCOMPARE(ml.env.value("RCLONE_RC_PASS"), creds.pass);
    QCOMPARE(ml.env.value("KEEP"), QString("1"));

    // the value token of "--rc-pass" is dropped as well, and a trailing
    // "--rc-pass" without a value is dropped without reading past the end
    const MountLaunch tail = buildMountLaunch(
        {"mount", "r:", "/m", "--rc", "--rc-pass", "p", "--read-only",
         "--rc-pass"},
        base, creds);
    QCOMPARE(tail.args,
             QStringList({"mount", "r:", "/m", "--rc", "--read-only"}));
    QVERIFY(tail.rcEnabled);

    // similar flags are not credentials
    const MountLaunch other = buildMountLaunch(
        {"--rc", "--rc-addr", "x", "--rc-user-x", "--rc-passwd=1"}, base,
        creds);
    QCOMPARE(other.args,
             QStringList({"--rc", "--rc-addr", "x", "--rc-user-x",
                          "--rc-passwd=1"}));
  }

  // T-MOUNT-NORC
  void T_MOUNT_NORC() {
    const RcCredentials creds{"rb-user", "secret-pass"};
    QProcessEnvironment base;
    base.insert("KEEP", "1");

    const MountLaunch ml = buildMountLaunch(
        {"mount", "r:", "/m", "--rc-user=old", "--vfs-cache-mode", "full"},
        base, creds);
    QVERIFY(!ml.rcEnabled);
    QCOMPARE(ml.args, QStringList({"mount", "r:", "/m", "--vfs-cache-mode",
                                   "full"}));
    QVERIFY(!ml.env.contains("RCLONE_RC_USER"));
    QVERIFY(!ml.env.contains("RCLONE_RC_PASS"));
    QCOMPARE(ml.env.value("KEEP"), QString("1"));
    QCOMPARE(ml.creds.user, creds.user);
  }

  // A FUSE mount is unmounted with fusermount, or with fusermount3 on a
  // system which has only fuse3.
  void T_MOUNT_UNMOUNT_COMMAND_data() {
    QTest::addColumn<QStringList>("installed");
    QTest::addColumn<QStringList>("command");

    QTest::newRow("fuse2 and fuse3")
        << QStringList({"fusermount", "fusermount3"})
        << QStringList({"/usr/bin/fusermount", "-u", "/m n"});
    QTest::newRow("fuse2 only") << QStringList({"fusermount"})
                                << QStringList({"/usr/bin/fusermount", "-u",
                                                "/m n"});
    QTest::newRow("fuse3 only") << QStringList({"fusermount3"})
                                << QStringList({"/usr/bin/fusermount3", "-u",
                                                "/m n"});
    // the start fails and reports that fusermount is missing
    QTest::newRow("neither")
        << QStringList() << QStringList({"fusermount", "-u", "/m n"});
  }

  void T_MOUNT_UNMOUNT_COMMAND() {
    QFETCH(QStringList, installed);
    QFETCH(QStringList, command);
    const auto find = [&installed](const QString &program) {
      return installed.contains(program) ? "/usr/bin/" + program : QString();
    };
    QCOMPARE(fuseUnmountCommand("/m n", find), command);
  }
};

QTEST_GUILESS_MAIN(TestMountLaunch)
#include "tst_mount_launch.moc"
