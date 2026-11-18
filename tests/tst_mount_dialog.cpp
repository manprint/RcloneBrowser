#include <QtTest>
#include <QtWidgets>

#include "list_of_job_options.h"
#include "mount_dialog.h"
#include "rb_test_support.h"

// The mount point field and the options built from it are the Linux and macOS
// ones: on Windows the dialog takes a drive letter or a base folder instead.
class TestMountDialog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

#if !defined(Q_OS_WIN)
  // T-MOUNTDIALOG-RUN-OPTIONS: "Run mount" reads the arguments with
  // getOptions() and saves no task. The task options that getOptions() made
  // for it are freed with the dialog (an ASan build reports a leak otherwise),
  // and no task is added to the task list.
  void T_MOUNTDIALOG_RUN_OPTIONS() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ListOfJobOptions *list = ListOfJobOptions::getInstance();
    const qsizetype before = list->getTasks().size();
    {
      MountDialog dialog(QStringLiteral("rem"), QDir(QStringLiteral("/a/b")),
                         QStringLiteral("s3"), QString());
      auto *mountPoint =
          dialog.findChild<QLineEdit *>(QStringLiteral("le_mountPointNotWin"));
      auto *rcPort = dialog.findChild<QLineEdit *>(QStringLiteral("le_rcPort"));
      auto *readOnly =
          dialog.findChild<QCheckBox *>(QStringLiteral("cb_readOnly"));
      auto *cache =
          dialog.findChild<QComboBox *>(QStringLiteral("combob_cacheLevel"));
      auto *extra =
          dialog.findChild<QPlainTextEdit *>(QStringLiteral("textExtra"));
      QVERIFY(mountPoint && rcPort && readOnly && cache && extra);

      mountPoint->setText(dir.path());
      rcPort->setText(QStringLiteral("5599"));
      readOnly->setChecked(true);
      cache->setCurrentIndex(2);
      extra->setPlainText(QStringLiteral("--opt \"a b\""));

      const QStringList want{QStringLiteral("mount"),
                             QStringLiteral("rem:/a/b"),
                             dir.path(),
                             QStringLiteral("--rc"),
                             QStringLiteral("--rc-addr"),
                             QStringLiteral("localhost:5599"),
                             QStringLiteral("--read-only"),
                             QStringLiteral("--vfs-cache-mode"),
                             QStringLiteral("writes"),
                             QStringLiteral("--opt"),
                             QStringLiteral("a b")};
      QCOMPARE(dialog.getOptions(), want);
      // a second call reuses the same task options
      QCOMPARE(dialog.getOptions(), want);
      QCOMPARE(dialog.getMountPoint(), dir.path());
    }
    QCOMPARE(list->getTasks().size(), before);
  }

  // T-MOUNTDIALOG-SAVE-TASK: "Save task" gives the task options to the task
  // list, which keeps using them after the dialog is gone.
  void T_MOUNTDIALOG_SAVE_TASK() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ListOfJobOptions *list = ListOfJobOptions::getInstance();
    const qsizetype before = list->getTasks().size();
    {
      MountDialog dialog(QStringLiteral("rem"), QDir(QStringLiteral("/a/b")),
                         QStringLiteral("s3"), QString());
      auto *mountPoint =
          dialog.findChild<QLineEdit *>(QStringLiteral("le_mountPointNotWin"));
      auto *taskName =
          dialog.findChild<QLineEdit *>(QStringLiteral("taskName"));
      auto *box =
          dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
      QVERIFY(mountPoint && taskName && box);
      mountPoint->setText(dir.path());
      taskName->setText(QStringLiteral("mount dialog save test"));

      QPushButton *save = nullptr;
      for (QAbstractButton *b : box->buttons()) {
        if (b->text() == QStringLiteral("&Save task")) {
          save = qobject_cast<QPushButton *>(b);
        }
      }
      QVERIFY(save);
      save->click();
    }
    QCOMPARE(list->getTasks().size(), before + 1);
    JobOptions *jo = list->getTasks().last();
    QCOMPARE(jo->description, QStringLiteral("mount dialog save test"));
    QCOMPARE(jo->operation, JobOptions::Mount);
    QCOMPARE(jo->source, QStringLiteral("rem:/a/b"));
    QCOMPARE(jo->dest, dir.path());
    QVERIFY(list->Forget(jo));
    delete jo;
  }
#endif
};

QTEST_MAIN(TestMountDialog)
#include "tst_mount_dialog.moc"
