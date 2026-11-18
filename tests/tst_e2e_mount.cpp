#include <QtTest>
#include <QtWidgets>

#include "mount_launch.h"
#include "mount_widget.h"
#include "rb_test_support.h"
#include "rc_client.h"
#include "rclone_process.h"

// MountWidget with a real rclone mount. The remote is a fake one ("fake", an
// alias of a temporary folder) in a config of the test: no network, never the
// user's remotes. The mount needs /dev/fuse and fusermount, which the default
// dev.sh container lacks: T_E2E_MOUNT_FAKE runs only with RB_TEST_FUSE=1.

namespace {

bool writeText(const QString &path, const QByteArray &data) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

QByteArray readText(const QString &path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QString outputOf(const QWidget &w) {
  const auto *o = w.findChild<QPlainTextEdit *>(QStringLiteral("output"));
  return o ? o->toPlainText() : QString();
}

} // namespace

class TestE2eMount : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
  }

  // T-E2E-MOUNT-NOSTART: an rclone that cannot start emits no finished() from
  // QProcess; the mount still ends as an error and can be closed (it used to
  // stay "Mounted" forever).
  void T_E2E_MOUNT_NOSTART() {
    QTemporaryDir mnt;
    QVERIFY(mnt.isValid());
    QObject holder;
    auto *proc = new QProcess(&holder);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    MountWidget widget(proc, QStringLiteral("fake:"), mnt.path(),
                       {QStringLiteral("mount")}, QString(),
                       QStringLiteral("uid"), QString());
    QSignalSpy spy(&widget, &MountWidget::finished);
    proc->start(QDir(mnt.path()).filePath(QStringLiteral("no-rclone")),
                {QStringLiteral("mount")});
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
    QVERIFY(!widget.isRunning);
    QCOMPARE(widget.getStatus(), QStringLiteral("1_zmount_error"));
    auto *cancel = widget.findChild<QToolButton *>(QStringLiteral("cancel"));
    QVERIFY(cancel && cancel->isEnabled());
  }

  // T-E2E-MOUNT-FAKE: mount of the fake remote as MainWindow::addNewMount
  // starts it (rc credentials in the environment only): files read and
  // written through the mount, the post-mount script gets the rc port and the
  // mount point, "Refresh cache" (vfs/refresh over the rc) shows a file added
  // behind the mount, and the unmount ends the job as finished.
  void T_E2E_MOUNT_FAKE() {
    if (qEnvironmentVariable("RB_TEST_FUSE") != QStringLiteral("1")) {
      // QSKIP with one argument is a C++20 extension for clang -Werror
      // (macOS, FreeBSD, OpenBSD); this is what the macro expands to
      QTest::qSkip("RB_TEST_FUSE=1 not set (needs /dev/fuse and fusermount)",
                   __FILE__, __LINE__);
      return;
    }
    QTemporaryDir src, mnt, work;
    QVERIFY(src.isValid() && mnt.isValid() && work.isValid());
    const QDir s(src.path()), m(mnt.path()), w(work.path());
    QVERIFY(writeText(s.filePath(QStringLiteral("hello.txt")), "hello"));

    const QString conf = w.filePath(QStringLiteral("rclone.conf"));
    QVERIFY(writeText(conf, QStringLiteral("[fake]\ntype = alias\nremote = %1\n")
                                .arg(src.path())
                                .toUtf8()));
    const QString scriptOut = w.filePath(QStringLiteral("script.out"));
    const QString script = w.filePath(QStringLiteral("post-mount.sh"));
    QVERIFY(writeText(script, QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$2\" "
                                             "\"$5\" \"$RB_RC_PORT\" > '%1'\n")
                                  .arg(scriptOut)
                                  .toUtf8()));
    QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                              QFile::ExeOwner));

    const QStringList args = {QStringLiteral("mount"),
                              QStringLiteral("fake:"),
                              mnt.path(),
                              QStringLiteral("--vfs-cache-mode"),
                              QStringLiteral("writes"),
                              QStringLiteral("--rc"),
                              QStringLiteral("--rc-addr"),
                              QStringLiteral("127.0.0.1:0"),
                              QStringLiteral("--config"),
                              conf};
    const MountLaunch ml = buildMountLaunch(
        args, RcloneEnvironment(RcloneEnvMode::Parsed), generateRcCredentials());
    QVERIFY(ml.rcEnabled);

    QObject holder;
    auto *proc = new QProcess(&holder);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    proc->setProcessEnvironment(ml.env);
    MountWidget widget(proc, QStringLiteral("fake:"), mnt.path(), ml.args,
                       script, QStringLiteral("uid"), QString());
    widget.setRcCredentials(ml.creds, ml.rcEnabled);
    QSignalSpy spy(&widget, &MountWidget::finished);
    proc->start(rbRclone(), ml.args);

    QTRY_VERIFY2_WITH_TIMEOUT(QFile::exists(m.filePath(QStringLiteral("hello.txt"))),
                              qPrintable(outputOf(widget)), 20000);
    QCOMPARE(readText(m.filePath(QStringLiteral("hello.txt"))), QByteArray("hello"));

    // a write through the mount reaches the remote
    QVERIFY(writeText(m.filePath(QStringLiteral("dir/new.txt")), "new"));
    QTRY_COMPARE_WITH_TIMEOUT(readText(s.filePath(QStringLiteral("dir/new.txt"))),
                              QByteArray("new"), 15000);

    // the post-mount script: $2 the rc port, $5 the mount point
    QTRY_VERIFY_WITH_TIMEOUT(readText(scriptOut).count('\n') == 3, 10000);
    const QStringList lines =
        QString::fromUtf8(readText(scriptOut)).split('\n', Qt::SkipEmptyParts);
    QCOMPARE(lines.size(), 3);
    QVERIFY(lines.at(0).toInt() > 0);
    QCOMPARE(lines.at(1), mnt.path());
    QCOMPARE(lines.at(2), lines.at(0));

    // the listing is cached (dir-cache-time 5m): a file added behind the mount
    // shows up only after Refresh cache
    QVERIFY(QDir(mnt.path()).entryList(QDir::Files).contains(QStringLiteral("hello.txt")));
    QVERIFY(writeText(s.filePath(QStringLiteral("later.txt")), "later"));
    auto *refresh = widget.findChild<QToolButton *>(QStringLiteral("refreshCache"));
    QVERIFY(refresh);
    QTRY_VERIFY(refresh->isEnabled());
    refresh->click();
    QTRY_VERIFY2_WITH_TIMEOUT(outputOf(widget).contains(QStringLiteral("Refresh cache: OK")),
                              qPrintable(outputOf(widget)), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(QDir(mnt.path()).entryList(QDir::Files).contains(QStringLiteral("later.txt")),
                             10000);

    // unmount
    widget.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 20000);
    QVERIFY(!widget.isRunning);
    QCOMPARE(widget.getUnmountingError(), QStringLiteral("0"));
    QCOMPARE(widget.getStatus(), QStringLiteral("1_zmount_finished"));
    QVERIFY(!QFile::exists(m.filePath(QStringLiteral("hello.txt"))));
  }
};

QTEST_MAIN(TestE2eMount)
#include "tst_e2e_mount.moc"
