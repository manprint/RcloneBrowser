#include <QtTest>
#include <QtWidgets>

#include "job_options.h"
#include "job_widget.h"
#include "list_of_job_options.h"
#include "main_window.h"
#include "rb_test_support.h"
#include "rclone_process.h"
#include "utils.h"

namespace {

bool writeFile(const QString &path) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write("x") == 1;
}

// A copy task with every field getOptions() reads.
JobOptions makeCopy(const QString &source, const QString &dest) {
  JobOptions jo(false);
  jo.description = QStringLiteral("e2e");
  jo.operation = JobOptions::Copy;
  jo.transfers = QStringLiteral("4");
  jo.checkers = QStringLiteral("8");
  jo.connectTimeout = QStringLiteral("60");
  jo.idleTimeout = QStringLiteral("300");
  jo.retries = QStringLiteral("3");
  jo.lowLevelRetries = QStringLiteral("10");
  jo.source = source;
  jo.dest = dest;
  return jo;
}

// The files below dir, relative and sorted.
QStringList filesUnder(const QString &dir) {
  QStringList out;
  const QDir base(dir);
  QDirIterator it(dir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    out << base.relativeFilePath(it.next());
  }
  out.sort();
  return out;
}

// true when `flag` is directly followed by `value` somewhere in the argv
bool hasPair(const QStringList &argv, const QString &flag,
             const QString &value) {
  for (int i = 0; i + 1 < argv.size(); ++i) {
    if (argv.at(i) == flag && argv.at(i + 1) == value) {
      return true;
    }
  }
  return false;
}

// the argv after the value of --low-level-retries, where the filter flags are
QStringList filterTail(const QStringList &argv) {
  const int at = argv.indexOf(QStringLiteral("--low-level-retries"));
  return at < 0 ? QStringList() : argv.mid(at + 2);
}

// Answers the QMessageBox the main window opens (it is modal and would block
// the test) and records its text.
struct BoxCloser {
  QStringList texts;
  QTimer timer;
  BoxCloser() {
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, &timer, [this]() {
      if (auto *box =
              qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
        texts << box->text();
        box->close();
      }
    });
    timer.start();
  }
};

} // namespace

class TestE2eExclude : public QObject {
  Q_OBJECT

  QTemporaryDir mTmp;
  QString mSrc;
  int mRun = 0;

  struct Outcome {
    QStringList argv;
    QStringList files;
    int exitCode = -1;
    QString output;
  };

  // Runs the job's argv with the real rclone into a fresh destination.
  Outcome run(const JobOptions &jo) {
    Outcome o;
    const QString dest = mTmp.filePath(QStringLiteral("dest%1").arg(++mRun));
    JobOptions copy = jo;
    copy.source = mSrc;
    copy.dest = dest;
    o.argv = copy.getOptions();
    const TransferArgs ta = PrepareTransferArgs(
        o.argv, RcloneEnvironment(RcloneEnvMode::Parsed));
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.setProcessEnvironment(RcloneEnvironment(RcloneEnvMode::Parsed));
    p.start(rbRclone(), ta.args);
    if (!p.waitForFinished(60000)) {
      p.kill();
      p.waitForFinished(5000);
    }
    o.exitCode = p.exitCode();
    o.output = QString::fromUtf8(p.readAll());
    o.files = filesUnder(dest);
    return o;
  }

  // the filter part of the argv, for the review row
  static void report(const char *name, const Outcome &o) {
    const int from = o.argv.indexOf(QStringLiteral("--low-level-retries"));
    qInfo().noquote()
        << QStringLiteral("R-REPRO %1: exit %2, copied [%3], argv tail [%4]")
               .arg(QLatin1String(name))
               .arg(o.exitCode)
               .arg(o.files.join(QStringLiteral(", ")),
                    o.argv.mid(from >= 0 ? from : 0).join(QStringLiteral(" ")));
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY(!rbRclone().isEmpty());
    QVERIFY(mTmp.isValid());
    SetRclone(rbRclone());
    mSrc = mTmp.filePath(QStringLiteral("src"));
    for (const char *f :
         {"._a", "._c.txt", "b.txt", "c.jpg", "sub/._d", "sub/e.txt"}) {
      QVERIFY(writeFile(mSrc + QLatin1Char('/') + QLatin1String(f)));
    }
    // A private HOME keeps the tasks of other tests out of the main window of
    // T_E2E_JOB_RESTART.
    QVERIFY(QDir().mkpath(rbTestDir().path() + QStringLiteral("/home")));
    qputenv("HOME", (rbTestDir().path() + QStringLiteral("/home")).toUtf8());
  }

  // T-E2E-EXCLUDE (#179): the Exclude tab and the extra options against the
  // real rclone, alone and next to an Include text.
  void T_E2E_EXCLUDE() {
    // every file that does not start with ._
    const QStringList keptAll = {QStringLiteral("b.txt"),
                                 QStringLiteral("c.jpg"),
                                 QStringLiteral("sub/e.txt")};
    // the same without the .jpg file, which only a "+ *.jpg" rule lets through
    const QStringList keptTxt = {QStringLiteral("b.txt"),
                                 QStringLiteral("sub/e.txt")};

    // A: Exclude tab ._* alone keeps the plain --exclude flag
    JobOptions a = makeCopy(QString(), QString());
    a.excluded = QStringLiteral("._*");
    const Outcome oa = run(a);
    report("A (exclude tab)", oa);
    QCOMPARE(oa.exitCode, 0);
    QCOMPARE(oa.files, keptAll);
    QVERIFY(hasPair(oa.argv, QStringLiteral("--exclude"), QStringLiteral("._*")));
    QVERIFY(!oa.argv.contains(QStringLiteral("--filter")));

    // B: extra options --exclude "._*" alone
    JobOptions b = makeCopy(QString(), QString());
    b.extra = QStringLiteral("--exclude \"._*\"");
    const Outcome ob = run(b);
    report("B (extra --exclude)", ob);
    QCOMPARE(ob.exitCode, 0);
    QCOMPARE(ob.files, keptAll);
    QVERIFY(hasPair(ob.argv, QStringLiteral("--exclude"), QStringLiteral("._*")));
    QVERIFY(!ob.argv.contains(QStringLiteral("--filter")));

    // C: Include *.txt and the Exclude tab ._*
    JobOptions c = makeCopy(QString(), QString());
    c.included = QStringLiteral("*.txt");
    c.excluded = QStringLiteral("._*");
    const Outcome oc = run(c);
    report("C (include + exclude tab)", oc);
    QCOMPARE(oc.exitCode, 0);
    QCOMPARE(oc.files, keptTxt);
    // the ordered filter rules: excludes, includes, then the catch-all
    QCOMPARE(filterTail(oc.argv),
             (QStringList{QStringLiteral("--filter"), QStringLiteral("- ._*"),
                          QStringLiteral("--filter"), QStringLiteral("+ *.txt"),
                          QStringLiteral("--filter"), QStringLiteral("- **")}));
    QVERIFY(!oc.argv.contains(QStringLiteral("--include")));
    QVERIFY(!oc.argv.contains(QStringLiteral("--exclude")));

    // C2: the same with a Filter tab line. The user's rules sit between the
    // "+" rules and the final "- **", or they would never be reached.
    JobOptions c2 = makeCopy(QString(), QString());
    c2.included = QStringLiteral("*.txt");
    c2.excluded = QStringLiteral("._*");
    c2.filtered = QStringLiteral("+ *.jpg");
    const Outcome oc2 = run(c2);
    report("C2 (include + exclude tab + filter tab)", oc2);
    QCOMPARE(oc2.exitCode, 0);
    QCOMPARE(filterTail(oc2.argv),
             (QStringList{QStringLiteral("--filter"), QStringLiteral("- ._*"),
                          QStringLiteral("--filter"), QStringLiteral("+ *.txt"),
                          QStringLiteral("--filter"), QStringLiteral("+ *.jpg"),
                          QStringLiteral("--filter"), QStringLiteral("- **")}));
    QCOMPARE(oc2.files, keptAll);

    // D: Include *.txt and --exclude "._*" in the extra options. This is an
    // rclone rule, not a bug of the app: rclone adds every include rule before
    // any exclude rule whatever the order of the flags, so the first include
    // that matches wins and the exclude never sees ._c.txt. The result is
    // pinned here, as the evidence for the README FAQ.
    JobOptions d = makeCopy(QString(), QString());
    d.included = QStringLiteral("*.txt");
    d.extra = QStringLiteral("--exclude \"._*\"");
    const Outcome od = run(d);
    report("D (include + extra --exclude)", od);
    QCOMPARE(od.exitCode, 0);
    QCOMPARE(od.files, (QStringList{QStringLiteral("._c.txt"),
                                    QStringLiteral("b.txt"),
                                    QStringLiteral("sub/e.txt")}));
    // an Include text alone keeps the plain flag, no ordered rules
    QVERIFY(hasPair(od.argv, QStringLiteral("--include"), QStringLiteral("*.txt")));
    QVERIFY(hasPair(od.argv, QStringLiteral("--exclude"), QStringLiteral("._*")));
    QVERIFY(!od.argv.contains(QStringLiteral("--filter")));

    // E: a blank line between two Exclude patterns
    JobOptions e = makeCopy(QString(), QString());
    e.excluded = QStringLiteral("._*\n\n*.zip");
    const Outcome oe = run(e);
    report("E (exclude tab with a blank line)", oe);
    // rclone accepts the empty --exclude value: no need to drop blank lines
    QCOMPARE(oe.exitCode, 0);
    QCOMPARE(oe.files, keptAll);
    QVERIFY(!oe.argv.contains(QStringLiteral("--filter")));
  }

  // T-E2E-JOB-RESTART (#118): a job that failed is run again with the same
  // command, source, destination and task id.
  void T_E2E_JOB_RESTART() {
    auto settings = GetSettings();
    settings->setValue(QStringLiteral("Settings/rclone"), rbRclone());
    settings->setValue(QStringLiteral("Settings/checkRcloneUpdates"), false);
    settings->setValue(QStringLiteral("Settings/checkRcloneBrowserUpdates"),
                       false);
    settings->setValue(QStringLiteral("Settings/notifyFinishedTransfers"),
                       false);
    settings->setValue(QStringLiteral("Settings/soundNotif"), false);
    settings->setValue(QStringLiteral("Settings/confirmTaskRun"), false);

    // a nonexistent source fails at once for any user (a read-only parent
    // would not for root)
    const QString missing = mTmp.filePath(QStringLiteral("no such dir"));
    const QString dest = mTmp.filePath(QStringLiteral("restart-dest"));
    JobOptions probe = makeCopy(missing, dest);
    probe.description = QStringLiteral("restart me");
    auto *jo = new JobOptions(probe);
    jo->uniqueId = QUuid::createUuid();
    ListOfJobOptions::getInstance()->Persist(jo);

    {
      MainWindow w;
      auto *list = w.findChild<QListWidget *>(QStringLiteral("tasksListWidget"));
      auto *run = w.findChild<QAction *>(QStringLiteral("actionRun"));
      QVERIFY(list && run);
      QCOMPARE(list->count(), 1);
      list->item(0)->setSelected(true);
      run->trigger();

      auto jobs = w.findChildren<JobWidget *>();
      QCOMPARE(jobs.size(), 1);
      JobWidget *first = jobs.first();
      auto *restart = first->findChild<QToolButton *>(QStringLiteral("restart"));
      QVERIFY(restart);
      // hidden while the job runs ...
      QVERIFY(first->isRunning);
      QVERIFY(!restart->isVisibleTo(first));

      // ... and there once it ended with an error
      QTRY_VERIFY_WITH_TIMEOUT(!first->isRunning, 30000);
      QCOMPARE(first->getStatus(), QStringLiteral("2_transfer_error"));
      QVERIFY(restart->isVisibleTo(first));

      // the end of the job took the "running" colour off its task
      QVERIFY(list->item(0)->background().color() != QColor(Qt::darkGreen));

      // with the default "Ask before running a task" Restart asks first;
      // closing the question answers No and starts nothing
      settings->remove(QStringLiteral("Settings/confirmTaskRun"));
      {
        BoxCloser closer;
        restart->click();
        QCOMPARE(closer.texts.size(), 1);
        QVERIFY(closer.texts.at(0).startsWith(
            QStringLiteral("Are you sure you want to run this transfer again?")));
        QVERIFY(closer.texts.at(0).contains(QStringLiteral("restart me")));
      }
      QCOMPARE(w.findChildren<JobWidget *>().size(), 1);
      settings->setValue(QStringLiteral("Settings/confirmTaskRun"), false);

      restart->click();
      jobs = w.findChildren<JobWidget *>();
      QCOMPARE(jobs.size(), 2);
      JobWidget *second = jobs.at(0) == first ? jobs.at(1) : jobs.at(0);

      // No event loop has run since the click, so the new job still runs: its
      // task is marked, it has no Restart yet and it runs in the same mode.
      QVERIFY(second->isRunning);
      QVERIFY(list->item(0)->background().color() == QColor(Qt::darkGreen));
      auto *secondRestart =
          second->findChild<QToolButton *>(QStringLiteral("restart"));
      QVERIFY(secondRestart);
      QVERIFY(!secondRestart->isVisibleTo(second));
      QCOMPARE(first->getTransferMode(), QStringLiteral("task"));
      QCOMPARE(second->getTransferMode(), first->getTransferMode());

      // Duplicate rebuilds the tasks list while the task runs: the running
      // task keeps its mark, the new copy has none. It is checked before any
      // event loop runs (the refusal box below spins one), so the second job
      // is certainly still running and the mark is certainly still set.
      list->clearSelection();
      list->item(0)->setSelected(true);
      auto *dup = w.findChild<QAction *>(QStringLiteral("actionDuplicateTask"));
      QVERIFY(dup);
      dup->trigger();
      QCOMPARE(list->count(), 2);
      for (int i = 0; i < list->count(); ++i) {
        auto *item = static_cast<JobOptionsListWidgetItem *>(list->item(i));
        const bool running = item->GetData()->uniqueId == jo->uniqueId;
        QCOMPARE(item->background().color() == QColor(Qt::darkGreen), running);
      }

      // Restart on the ended job is refused while the task runs again: the
      // question is a box, and no third job appears. The refusal is decided
      // before the box opens, so the second job ending meanwhile changes
      // nothing.
      {
        BoxCloser closer;
        restart->click();
        QCOMPARE(closer.texts.size(), 1);
        QCOMPARE(closer.texts.at(0), QStringLiteral("This job is already running."));
      }
      QCOMPARE(w.findChildren<JobWidget *>().size(), 2);

      const auto text = [](JobWidget *j, const char *name) {
        auto *e = j->findChild<QLineEdit *>(QString::fromLatin1(name));
        return e ? e->text() : QString();
      };
      const auto command = [](JobWidget *j) {
        return j->findChild<QToolButton *>(QStringLiteral("showOutput"))
            ->toolTip();
      };
      QVERIFY(!command(first).isEmpty());
      QCOMPARE(command(second), command(first));
      QCOMPARE(text(second, "source"), text(first, "source"));
      QCOMPARE(text(second, "dest"), text(first, "dest"));
      QCOMPARE(text(second, "source"), missing);
      QCOMPARE(text(second, "dest"), dest);
      QCOMPARE(second->getUniqueID(), first->getUniqueID());
      QCOMPARE(second->getUniqueID(), jo->uniqueId.toString());
      QVERIFY(second->getRequestId() != first->getRequestId());

      QTRY_VERIFY_WITH_TIMEOUT(!second->isRunning, 30000);
      QCOMPARE(second->getStatus(), QStringLiteral("2_transfer_error"));
    }

    // the destroyed window must not leave an rcd behind
    int exitCode = -1;
    for (int i = 0; i < 20 && exitCode != 1; ++i) {
      QProcess p;
      p.start(QStringLiteral("pgrep"),
              {QStringLiteral("-f"), QStringLiteral("rcd --rc-addr 127.0.0.1:0")});
      QVERIFY(p.waitForFinished(5000));
      exitCode = p.exitCode();
      if (exitCode != 1) {
        QTest::qWait(100);
      }
    }
    QCOMPARE(exitCode, 1);

    // the original and the copy of the Duplicate above
    const QList<JobOptions *> all = ListOfJobOptions::getInstance()->getTasks();
    for (JobOptions *task : all) {
      ListOfJobOptions::getInstance()->Forget(task);
    }
  }
};

QTEST_MAIN(TestE2eExclude)
#include "tst_e2e_exclude.moc"
