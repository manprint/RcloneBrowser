#include <QtTest>
#include <QtWidgets>

#include "command_line.h"
#include "list_of_job_options.h"
#include "main_window.h"
#include "mount_launch.h"
#include "mount_widget.h"
#include "rc_client.h"
#include "rclone_process.h"
#include "rb_test_support.h"
#include "utils.h"

namespace {

// The inverse of quoteArgPosix, for the only forms it emits: bare words and
// '...' words with the '\'' escape for a quote.
QStringList parsePosix(const QString &s) {
  QStringList out;
  QString cur;
  bool inWord = false;
  bool inQuote = false;
  for (qsizetype i = 0; i < s.size(); ++i) {
    const QChar c = s.at(i);
    if (inQuote) {
      if (c == QLatin1Char('\'')) {
        inQuote = false;
      } else {
        cur += c;
      }
    } else if (c == QLatin1Char('\'')) {
      inQuote = true;
      inWord = true;
    } else if (c == QLatin1Char('\\') && i + 1 < s.size() &&
               s.at(i + 1) == QLatin1Char('\'')) {
      cur += QLatin1Char('\'');
      inWord = true;
      ++i;
    } else if (c == QLatin1Char(' ')) {
      if (inWord) {
        out << cur;
        cur.clear();
        inWord = false;
      }
    } else {
      cur += c;
      inWord = true;
    }
  }
  if (inWord) {
    out << cur;
  }
  return out;
}

// What Run starts for a transfer task: dryRun is never part of a copied
// command.
QString expectedTransferLine(JobOptions jo) {
  jo.dryRun = false;
  return commandLineForClipboard(
      QDir::toNativeSeparators(GetRclone()),
      PrepareTransferArgs(jo.getOptions()).args + GetRcloneConf(),
      nativeShell());
}

} // namespace

// The "copy command" sites that need a widget or the main window: the mount
// widget (#194) and the Tasks tab action (#176, #123).
class TestCopySites : public QObject {
  Q_OBJECT

  QString mCopyDesc = QStringLiteral("copy task");
  QString mMountDesc = QStringLiteral("mount task");
  JobOptions *mCopy = nullptr;
  JobOptions *mMount = nullptr;
  QString mConf; // an rclone.conf path with a space and an apostrophe

  static QString clipboardText() {
    return QGuiApplication::clipboard()->text();
  }

  // Selects the items with these descriptions, in the order given (the order
  // selectedItems() reports follows the order of selection).
  static void selectOnly(QListWidget *list, const QStringList &descriptions) {
    list->clearSelection();
    for (const QString &d : descriptions) {
      for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->text() == d) {
          list->item(i)->setSelected(true);
        }
      }
    }
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    // A private HOME keeps the tasks of other tests out of the constructor's
    // restore paths (see tst_mainwindow_snapshot).
    QVERIFY(QDir().mkpath(rbTestDir().path() + QStringLiteral("/home")));
    qputenv("HOME", (rbTestDir().path() + QStringLiteral("/home")).toUtf8());
    SetRclone(QStringLiteral("/bin/false"));
    // Without the key the MainWindow constructor searches PATH and blocks in a
    // modal box when it finds nothing.
    GetSettings()->setValue(QStringLiteral("Settings/rclone"),
                            QStringLiteral("/bin/false"));
    // The constructor calls SetRcloneConf() with this key, so the config is
    // set here and not with SetRcloneConf() (I-9: a temporary path, never the
    // host config). Both keys are non-empty, so the copied commands carry the
    // --config tail and the default mount options of Run.
    mConf = rbTestDir().path() + QStringLiteral("/conf dir's/rclone.conf");
    GetSettings()->setValue(QStringLiteral("Settings/rcloneConf"), mConf);
    GetSettings()->setValue(
        QStringLiteral("Settings/mount"),
        QStringLiteral("--allow-other --vfs-read-chunk-size=\"64M\""));
  }

  // T-CMD-MOUNTCOPY (#194): the copy button of a mount widget copies the argv
  // of the mount, quoted, without the rc credentials.
  void T_CMD_MOUNTCOPY() {
    const QStringList user = {"mount",     "remote:",    "/mnt/x y",
                              "--rc",      "--rc-addr",  "localhost:5572",
                              "--rc-user", "u",          "--rc-pass",
                              "p"};
    const MountLaunch ml = buildMountLaunch(user, QProcessEnvironment(),
                                            generateRcCredentials());
    QVERIFY(ml.rcEnabled);

    QObject holder;
    auto *proc = new QProcess(&holder); // never started
    MountWidget widget(proc, QStringLiteral("remote:"),
                       QStringLiteral("/mnt/x y"), ml.args, QString(),
                       QStringLiteral("uid"), QString());
    auto *copy = widget.findChild<QToolButton *>(QStringLiteral("copy"));
    QVERIFY(copy);
    QGuiApplication::clipboard()->clear();
    copy->click();

    const QString text = clipboardText();
    QVERIFY(!text.isEmpty());
    const QStringList words = parsePosix(text);
    QCOMPARE(words,
             QStringList{QDir::toNativeSeparators(GetRclone())} + ml.args);
    QVERIFY(words.contains(QStringLiteral("mount")));
    QVERIFY(words.contains(QStringLiteral("/mnt/x y")));
    QVERIFY(!text.contains(QStringLiteral("--rc-user")));
    QVERIFY(!text.contains(QStringLiteral("--rc-pass")));
    QVERIFY(!words.contains(QStringLiteral("u")));
    QVERIFY(!words.contains(QStringLiteral("p")));
    QVERIFY(!words.contains(ml.creds.user));
    QVERIFY(!words.contains(ml.creds.pass));
  }

  // T-CMD-TASKCOPY (#176, #123): "Copy command" in the Tasks tab copies what
  // Run starts, from a copy of the stored task.
  void T_CMD_TASKCOPY() {
    ListOfJobOptions *ljo = ListOfJobOptions::getInstance();

    mCopy = new JobOptions(true);
    mCopy->description = mCopyDesc;
    mCopy->operation = JobOptions::Copy;
    mCopy->source = QStringLiteral("/tmp/src dir's/a b");
    mCopy->dest = QStringLiteral("remote:dest dir's/c d");
    mCopy->transfers = QStringLiteral("4");
    mCopy->checkers = QStringLiteral("8");
    mCopy->retries = QStringLiteral("3");
    mCopy->lowLevelRetries = QStringLiteral("10");
    mCopy->connectTimeout = QStringLiteral("60");
    mCopy->idleTimeout = QStringLiteral("300");
    QVERIFY(ljo->Persist(mCopy));

    mMount = new JobOptions(true);
    mMount->description = mMountDesc;
    mMount->operation = JobOptions::Mount;
    mMount->source = QStringLiteral("remote:");
    mMount->dest = QStringLiteral("/mnt/x y");
    mMount->mountRcPort = QStringLiteral("5572");
    mMount->extra = QStringLiteral("--rc-user x --rc-pass y");
    QVERIFY(ljo->Persist(mMount));

    MainWindow w;
    auto *list = w.findChild<QListWidget *>(QStringLiteral("tasksListWidget"));
    auto *action =
        w.findChild<QAction *>(QStringLiteral("actionCopyTaskCommand"));
    QVERIFY(list);
    QVERIFY(action);
    // the constructor lists the tasks from the event loop
    QTRY_COMPARE(list->count(), 2);

    // one transfer task
    selectOnly(list, {mCopyDesc});
    QGuiApplication::clipboard()->clear();
    action->trigger();
    const QString expected = expectedTransferLine(*mCopy);
    QCOMPARE(clipboardText(), expected);
    // the argv survives the quoting: the paths with a space and an apostrophe
    const QStringList words = parsePosix(clipboardText());
    QVERIFY(words.contains(mCopy->source));
    QVERIFY(words.contains(mCopy->dest));
    // --config comes last, as addTransfer starts the process (D2)
    QCOMPARE(words.mid(words.size() - 2),
             (QStringList{QStringLiteral("--config"), mConf}));

    // after a Dry Run the stored task keeps dryRun, the command never has it
    mCopy->dryRun = true;
    QGuiApplication::clipboard()->clear();
    action->trigger();
    QCOMPARE(clipboardText(), expected);
    QVERIFY(!clipboardText().contains(QStringLiteral("--dry-run")));
    QVERIFY(mCopy->dryRun);
    QVERIFY(mCopy->getOptions().contains(QStringLiteral("--dry-run")));
    mCopy->dryRun = false;

    // one mount task: no rc credentials in the command
    selectOnly(list, {mMountDesc});
    QGuiApplication::clipboard()->clear();
    action->trigger();
    const QString mountText = clipboardText();
    const QStringList mountWords = parsePosix(mountText);
    // what Run starts: the task argv, the default mount options of the
    // preferences with their quotes removed, then --config; no credentials
    QCOMPARE(mountWords,
             (QStringList{QDir::toNativeSeparators(GetRclone()),
                          QStringLiteral("mount"), QStringLiteral("remote:"),
                          QStringLiteral("/mnt/x y"), QStringLiteral("--rc"),
                          QStringLiteral("--rc-addr"),
                          QStringLiteral("localhost:5572"),
                          QStringLiteral("--allow-other"),
                          QStringLiteral("--vfs-read-chunk-size=64M"),
                          QStringLiteral("--config"), mConf}));
    QVERIFY(!mountText.contains(QStringLiteral("--rc-user")));
    QVERIFY(!mountText.contains(QStringLiteral("--rc-pass")));
    QVERIFY(!mountWords.contains(QStringLiteral("x")));
    QVERIFY(!mountWords.contains(QStringLiteral("y")));

    // both: one line per task, sorted like Run (D9). The tasks are selected in
    // the opposite order of the expected one, so an unsorted selection fails.
    GetSettings()->setValue(QStringLiteral("Settings/sortTask"), false);
    selectOnly(list, {mCopyDesc, mMountDesc});
    QGuiApplication::clipboard()->clear();
    action->trigger();
    QCOMPARE(clipboardText().split(QLatin1Char('\n')),
             (QStringList{mountText, expected})); // descending by description

    GetSettings()->setValue(QStringLiteral("Settings/sortTask"), true);
    selectOnly(list, {mMountDesc, mCopyDesc});
    QGuiApplication::clipboard()->clear();
    action->trigger();
    QCOMPARE(clipboardText().split(QLatin1Char('\n')),
             (QStringList{expected, mountText})); // ascending

    // no selection: the clipboard stays as it is
    list->clearSelection();
    QGuiApplication::clipboard()->setText(QStringLiteral("untouched"));
    action->trigger();
    QCOMPARE(clipboardText(), QStringLiteral("untouched"));
  }
};

QTEST_MAIN(TestCopySites)
#include "tst_copy_sites.moc"
