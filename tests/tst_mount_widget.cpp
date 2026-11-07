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
};

QTEST_MAIN(TestMountWidget)
#include "tst_mount_widget.moc"
