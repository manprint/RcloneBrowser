#include <QtTest>
#include <QtWidgets>

#include "mount_widget.h"
#include "rb_test_support.h"

class TestMountWidget : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // An unmount program which cannot be started (neither fusermount nor
  // fusermount3 installed) is a failed unmount: the mount is not left
  // "Unmounting" with its button disabled, and quitting does not wait for it.
  void T_MOUNT_UNMOUNT_NOT_STARTED() {
    QObject holder;
    auto *proc = new QProcess(&holder); // never started: the mount "runs"
    MountWidget widget(proc, QStringLiteral("remote:"),
                       QStringLiteral("/nonexistent/rb-mount"),
                       {QStringLiteral("mount")}, QString(),
                       QStringLiteral("uid"), QString());
    auto *cancel = widget.findChild<QToolButton *>(QStringLiteral("cancel"));
    QVERIFY(cancel);
    QVERIFY(widget.isRunning);

    QTemporaryDir emptyPath;
    QVERIFY(emptyPath.isValid());
    const QByteArray path = qgetenv("PATH");
    qputenv("PATH", emptyPath.path().toUtf8());
    widget.cancel();
    qputenv("PATH", path);

    QTRY_COMPARE(widget.getUnmountingError(), QStringLiteral("-1"));
    QVERIFY(cancel->isEnabled());
    QCOMPARE(widget.getStatus(), QStringLiteral("0_zmount_mounted"));
  }
  // T-MOUNT-SCRIPT-FINISHED: a mount script that exits 0 is "Finished",
  // whatever the icon theme (the dark-icon branch showed it as an error code)
  void T_MOUNT_SCRIPT_FINISHED() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString script = QDir(dir.path()).filePath("after-mount.sh");
    {
      QFile f(script);
      QVERIFY(f.open(QIODevice::WriteOnly));
      f.write("#!/bin/sh\nexit 0\n");
    }
    QVERIFY(QFile::setPermissions(
        script, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                    QFileDevice::ExeOwner));

    QObject holder;
    auto *proc = new QProcess(&holder);
    MountWidget widget(proc, QStringLiteral("remote:"),
                       QStringLiteral("/nonexistent/rb-mount"),
                       {QStringLiteral("mount")}, script,
                       QStringLiteral("uid"), QString());
    auto *label = widget.findChild<QLabel *>(QStringLiteral("l_script"));
    QVERIFY(label);
    // the script starts once rclone reports its rc address
    proc->start("/bin/sh",
                {"-c", "echo '2026/01/01 00:00:00 NOTICE: Serving remote "
                       "control on http://127.0.0.1:5572/'; exec sleep 30"});
    QVERIFY(proc->waitForStarted(5000));
    QTRY_COMPARE_WITH_TIMEOUT(label->text(), QStringLiteral("Finished"), 10000);
    proc->kill();
    proc->waitForFinished(5000);
  }
};

QTEST_MAIN(TestMountWidget)
#include "tst_mount_widget.moc"
