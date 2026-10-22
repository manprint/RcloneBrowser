#include <QtTest>
#include <QtWidgets>

#include "job_options.h"
#include "job_widget.h"
#include "list_of_job_options.h"
#include "main_window.h"
#include "rb_test_support.h"
#include "utils.h"

namespace {

// Persists a copy task between two (never touched) paths. Every field that
// getOptions() reads is set, so two tasks with the same paths give the same
// argv.
JobOptions *mkTask(const QString &description, const QString &source,
                   const QString &dest) {
  auto *jo = new JobOptions(false);
  jo->description = description;
  jo->operation = JobOptions::Copy;
  jo->transfers = QStringLiteral("4");
  jo->checkers = QStringLiteral("8");
  jo->connectTimeout = QStringLiteral("60");
  jo->idleTimeout = QStringLiteral("300");
  jo->retries = QStringLiteral("3");
  jo->lowLevelRetries = QStringLiteral("10");
  jo->source = source;
  jo->dest = dest;
  ListOfJobOptions::getInstance()->Persist(jo);
  return jo;
}

// Gives every persisted field of the task a value that is not the default, so
// a copy that forgets one of them is caught. Mount settings only matter for a
// mount task.
void fillEveryField(JobOptions *jo, JobOptions::Operation operation) {
  jo->operation = operation;
  jo->jobType = JobOptions::Download;
  jo->dryRun = true; // not persisted
  jo->sync = true;
  jo->syncTiming = JobOptions::After;
  jo->skipNewer = true;
  jo->skipExisting = true;
  jo->compare = true;
  jo->compareOption = JobOptions::SizeOnly;
  jo->verbose = true;
  jo->sameFilesystem = true;
  jo->dontUpdateModified = true;
  jo->transfers = QStringLiteral("5");
  jo->checkers = QStringLiteral("9");
  jo->bandwidth = QStringLiteral("2M");
  jo->minSize = QStringLiteral("3k");
  jo->minAge = QStringLiteral("4d");
  jo->maxAge = QStringLiteral("40d");
  jo->maxDepth = 6;
  jo->connectTimeout = QStringLiteral("61");
  jo->idleTimeout = QStringLiteral("301");
  jo->retries = QStringLiteral("7");
  jo->lowLevelRetries = QStringLiteral("11");
  jo->deleteExcluded = true;
  jo->excluded = QStringLiteral("._*");
  jo->extra = QStringLiteral("--fast-list");
  jo->DriveSharedWithMe = true;
  jo->isFolder = true;
  jo->remoteMode = QStringLiteral("shared");
  jo->remoteType = QStringLiteral("drive");
  jo->included = QStringLiteral("*.txt");
  jo->noTraverse = true;
  jo->createEmptySrcDirs = true;
  jo->filtered = QStringLiteral("+ *.jpg");
  jo->deleteEmptySrcDirs = true;
  jo->mountReadOnly = true;
  jo->mountCacheLevel = JobOptions::Full;
  jo->mountVolume = QStringLiteral("volume");
  // a mount task that starts with the window would run at once: the test sets
  // it after the window exists
  jo->mountAutoStart = operation != JobOptions::Mount;
  jo->mountRcPort = QStringLiteral("5599");
  jo->mountScript = QStringLiteral("echo mounted");
  jo->mountWinDriveMode = true;
}

JobOptions *mkFullTask(const QString &description, const QString &source,
                       const QString &dest, JobOptions::Operation operation) {
  auto *jo = new JobOptions(true);
  jo->description = description;
  jo->source = source;
  jo->dest = dest;
  fillEveryField(jo, operation);
  ListOfJobOptions::getInstance()->Persist(jo);
  return jo;
}

// Every persisted field except the id and the description. The mount start is
// compared only when `compareAutoStart` is set (a copy never starts at once).
void compareFields(const JobOptions *a, const JobOptions *b,
                   bool compareAutoStart = true) {
#define RB_SAME(field)                                                         \
  QCOMPARE(a->field, b->field);                                                \
  if (QTest::currentTestFailed()) {                                            \
    return;                                                                    \
  }
  RB_SAME(jobType)
  RB_SAME(operation)
  RB_SAME(sync)
  RB_SAME(syncTiming)
  RB_SAME(skipNewer)
  RB_SAME(skipExisting)
  RB_SAME(compare)
  RB_SAME(compareOption)
  RB_SAME(verbose)
  RB_SAME(sameFilesystem)
  RB_SAME(dontUpdateModified)
  RB_SAME(transfers)
  RB_SAME(checkers)
  RB_SAME(bandwidth)
  RB_SAME(minSize)
  RB_SAME(minAge)
  RB_SAME(maxAge)
  RB_SAME(maxDepth)
  RB_SAME(connectTimeout)
  RB_SAME(idleTimeout)
  RB_SAME(retries)
  RB_SAME(lowLevelRetries)
  RB_SAME(deleteExcluded)
  RB_SAME(excluded)
  RB_SAME(extra)
  RB_SAME(DriveSharedWithMe)
  RB_SAME(source)
  RB_SAME(dest)
  RB_SAME(isFolder)
  RB_SAME(remoteMode)
  RB_SAME(remoteType)
  RB_SAME(mountReadOnly)
  RB_SAME(mountCacheLevel)
  RB_SAME(mountVolume)
  RB_SAME(mountRcPort)
  RB_SAME(mountScript)
  RB_SAME(mountWinDriveMode)
  RB_SAME(included)
  RB_SAME(noTraverse)
  RB_SAME(createEmptySrcDirs)
  RB_SAME(filtered)
  RB_SAME(deleteEmptySrcDirs)
  if (compareAutoStart) {
    RB_SAME(mountAutoStart)
  }
#undef RB_SAME
}

// The singleton's constructor is protected: the probe reloads the tasks file.
struct Probe : ListOfJobOptions {
  Probe() {}
};

// Answers every QMessageBox the main window opens (they are modal and would
// block the test) and records their texts.
struct Answerer {
  QStringList texts;
  QMessageBox::StandardButton answer = QMessageBox::No;
  QTimer timer;
  Answerer() {
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, &timer, [this]() {
      if (auto *box =
              qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
        texts << box->text();
        if (auto *b = box->button(answer)) {
          b->click();
        } else {
          box->close();
        }
      }
    });
    timer.start();
  }
};

// Remembers the info text of every JobWidget, in the order the main window
// puts them in its jobs list. The widget is complete when it is reparented, so
// the first ChildAdded that carries it tells which task it runs; sortJobs()
// reparents again, which is why only the first one counts.
struct StartOrder : QObject {
  QList<QPointer<JobWidget>> seen;
  QStringList infos;
  StartOrder() { qApp->installEventFilter(this); }
  ~StartOrder() override { qApp->removeEventFilter(this); }
  bool eventFilter(QObject *, QEvent *e) override {
    if (e->type() == QEvent::ChildAdded) {
      auto *jw = qobject_cast<JobWidget *>(static_cast<QChildEvent *>(e)->child());
      if (jw && !seen.contains(jw)) {
        seen.append(jw);
        auto *info = jw->findChild<QLineEdit *>(QStringLiteral("info"));
        infos << (info ? info->text() : QString());
      }
    }
    return false;
  }
};

} // namespace

// The Tasks tab actions of 14.2: Duplicate (#139), several tasks at once
// (#80, #98) and the confirmation of Run and Dry run (#144).
class TestTaskActions : public QObject {
  Q_OBJECT

  QString mFakeRclone;

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

  // the text of a mount task item carries an auto-start mark, the data does not
  static void selectByDescription(QListWidget *list, const QString &description) {
    list->clearSelection();
    for (int i = 0; i < list->count(); ++i) {
      auto *item = static_cast<JobOptionsListWidgetItem *>(list->item(i));
      if (item->GetData()->description == description) {
        item->setSelected(true);
      }
    }
  }

  static QStringList itemTexts(QListWidget *list) {
    QStringList out;
    for (int i = 0; i < list->count(); ++i) {
      out << list->item(i)->text();
    }
    return out;
  }

  static QAction *action(MainWindow &w, const char *name) {
    return w.findChild<QAction *>(QString::fromLatin1(name));
  }

  static void waitAllEnded(MainWindow &w) {
    const auto ended = [&w]() {
      const auto jobs = w.findChildren<JobWidget *>();
      for (JobWidget *j : jobs) {
        if (j->isRunning) {
          return false;
        }
      }
      return true;
    };
    QTRY_VERIFY_WITH_TIMEOUT(ended(), 20000);
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    // A private HOME keeps the tasks of other tests out of the constructor's
    // restore paths (see tst_mainwindow_snapshot).
    QVERIFY(QDir().mkpath(rbTestDir().path() + QStringLiteral("/home")));
    qputenv("HOME", (rbTestDir().path() + QStringLiteral("/home")).toUtf8());

    // These tests spin the event loop with a MainWindow, so its start-up
    // version check must succeed: a fake rclone prints a valid version block
    // and exits 0 for everything else (never /bin/false here).
    mFakeRclone = rbTestDir().path() + QStringLiteral("/fake-rclone.sh");
    QFile script(mFakeRclone);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write(
        "#!/bin/sh\n"
        "case \"$1\" in\n"
        " version) printf 'rclone v1.75.2\\n- os/version: test\\n"
        "- os/kernel: test\\n- os/type: linux\\n- os/arch: amd64\\n"
        "- go/version: go1.25\\n- go/linking: static\\n- go/tags: none\\n';;\n"
        " listremotes) echo '[]';;\n"
        " rcd) exit 1;;\n"
        " *) exit 0;;\n"
        "esac\n");
    script.close();
    QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                  QFileDevice::ExeOwner));

    SetRclone(mFakeRclone);
    auto settings = GetSettings();
    settings->setValue(QStringLiteral("Settings/rclone"), mFakeRclone);
    // no network, no tray message, no sound
    settings->setValue(QStringLiteral("Settings/checkRcloneUpdates"), false);
    settings->setValue(QStringLiteral("Settings/checkRcloneBrowserUpdates"),
                       false);
    settings->setValue(QStringLiteral("Settings/notifyFinishedTransfers"),
                       false);
    settings->setValue(QStringLiteral("Settings/soundNotif"), false);
  }

  void cleanup() {
    auto *list = ListOfJobOptions::getInstance();
    const QList<JobOptions *> all = list->getTasks();
    for (JobOptions *jo : all) {
      list->Forget(jo);
    }
    GetSettings()->remove(QStringLiteral("Settings/confirmTaskRun"));
    GetSettings()->remove(QStringLiteral("Settings/sortTask"));
  }

  // T-TASK-DUP (#139)
  void T_TASK_DUP() {
    // two tasks with a non-default value in every persisted field: a download
    // copy with a Drive remote, and a mount task
    JobOptions *orig =
        mkFullTask(QStringLiteral("orig"), QStringLiteral("/nonexistent/src"),
                   QStringLiteral("/nonexistent/dst"), JobOptions::Copy);
    JobOptions *mnt =
        mkFullTask(QStringLiteral("mnt"), QStringLiteral("/nonexistent/msrc"),
                   QStringLiteral("/nonexistent/mdst"), JobOptions::Mount);
    const QUuid origId = orig->uniqueId;
    const QUuid mntId = mnt->uniqueId;

    MainWindow w;
    auto *list = w.findChild<QListWidget *>(QStringLiteral("tasksListWidget"));
    QVERIFY(list);
    auto *dup = action(w, "actionDuplicateTask");
    QVERIFY(dup);
    QCOMPARE(list->count(), 2);
    // the constructor fills in the default rclone options (--fast-list)
    QStringList origOptions = orig->getOptions();
    QVERIFY(origOptions.contains(QStringLiteral("--transfers")));
    // the original is a dry run, the copy is not
    QVERIFY(origOptions.removeAll(QStringLiteral("--dry-run")) == 1);
    // set only now, a window that finds it would start the mount
    mnt->mountAutoStart = true;

    selectOnly(list, {QStringLiteral("orig")});
    dup->trigger();

    const auto findByDescription = [](const QList<JobOptions *> &tasks,
                                      const QString &description) -> JobOptions * {
      for (JobOptions *jo : tasks) {
        if (jo->description == description) {
          return jo;
        }
      }
      return nullptr;
    };
    QList<JobOptions *> tasks = ListOfJobOptions::getInstance()->getTasks();
    QCOMPARE(tasks.size(), 3);
    QCOMPARE(list->count(), 3);
    const JobOptions *copy =
        findByDescription(tasks, QStringLiteral("orig (copy)"));
    QVERIFY(copy);
    QVERIFY(copy->uniqueId != origId);
    QVERIFY(!copy->uniqueId.isNull());
    QVERIFY(copy->description.endsWith(QStringLiteral("(copy)")));
    QCOMPARE(copy->getOptions(), origOptions);
    QVERIFY(!copy->dryRun);
    QVERIFY(orig->dryRun);
    compareFields(orig, copy);
    QVERIFY(!QTest::currentTestFailed());
    QCOMPARE(itemTexts(list).count(QStringLiteral("orig (copy)")), 1);
    // the new task is the selection
    QCOMPARE(list->selectedItems().size(), 1);
    QCOMPARE(list->selectedItems().first()->text(),
             QStringLiteral("orig (copy)"));

    // a second duplicate of the original gets the same literal suffix
    selectOnly(list, {QStringLiteral("orig")});
    dup->trigger();
    QCOMPARE(ListOfJobOptions::getInstance()->getTasks().size(), 4);
    QCOMPARE(itemTexts(list).count(QStringLiteral("orig (copy)")), 2);

    // a mount copy never starts at once and is not a dry run
    selectByDescription(list, QStringLiteral("mnt"));
    dup->trigger();
    tasks = ListOfJobOptions::getInstance()->getTasks();
    QCOMPARE(tasks.size(), 5);
    const JobOptions *mntCopy =
        findByDescription(tasks, QStringLiteral("mnt (copy)"));
    QVERIFY(mntCopy);
    QVERIFY(mntCopy->uniqueId != mntId);
    QVERIFY(!mntCopy->mountAutoStart);
    QVERIFY(!mntCopy->dryRun);
    QCOMPARE(mntCopy->operation, JobOptions::Mount);
    compareFields(mnt, mntCopy, false);
    QVERIFY(!QTest::currentTestFailed());
    QVERIFY(mnt->mountAutoStart);

    // only a single selection is duplicated
    list->selectAll();
    QCOMPARE(list->selectedItems().size(), 5);
    dup->trigger();
    QCOMPARE(ListOfJobOptions::getInstance()->getTasks().size(), 5);

    // the tasks file reloads with all of them, field by field
    Probe probe;
    QVERIFY(ListOfJobOptions::RestoreFromUserData(probe));
    const QList<JobOptions *> loaded = probe.getTasks();
    QCOMPARE(loaded.size(), 5);
    const JobOptions *rOrig = findByDescription(loaded, QStringLiteral("orig"));
    const JobOptions *rMnt = findByDescription(loaded, QStringLiteral("mnt"));
    const JobOptions *rMntCopy =
        findByDescription(loaded, QStringLiteral("mnt (copy)"));
    QVERIFY(rOrig && rMnt && rMntCopy);
    QCOMPARE(rOrig->uniqueId, origId);
    QCOMPARE(rMnt->uniqueId, mntId);
    QVERIFY(rMnt->mountAutoStart);
    QVERIFY(!rMntCopy->mountAutoStart);
    int copies = 0;
    for (const JobOptions *r : loaded) {
      if (r->description == QStringLiteral("orig (copy)")) {
        ++copies;
        QVERIFY(r->uniqueId != origId);
        compareFields(rOrig, r);
        QVERIFY(!QTest::currentTestFailed());
      }
    }
    QCOMPARE(copies, 2);
    compareFields(rMnt, rMntCopy, false);
    QVERIFY(!QTest::currentTestFailed());
    qDeleteAll(loaded);
  }

  // T-TASK-MULTI (#80, #98): Run starts every selected task, in list order;
  // Edit only works for one task.
  void T_TASK_MULTI() {
    // With the key off no box opens. The answerer says Yes to the question that
    // the tree before #144 always asked, so that this test gives the same
    // verdict on that tree (R-REPRO of #80 and #98).
    GetSettings()->setValue(QStringLiteral("Settings/confirmTaskRun"), false);
    mkTask(QStringLiteral("task-a"), QStringLiteral("/nonexistent/a"),
           QStringLiteral("/nonexistent/da"));
    mkTask(QStringLiteral("task-b"), QStringLiteral("/nonexistent/b"),
           QStringLiteral("/nonexistent/db"));

    StartOrder order;
    MainWindow w;
    Answerer box;
    box.answer = QMessageBox::Yes;
    auto *list = w.findChild<QListWidget *>(QStringLiteral("tasksListWidget"));
    auto *queue = w.findChild<QListWidget *>(QStringLiteral("queueListWidget"));
    auto *edit = w.findChild<QToolButton *>(QStringLiteral("buttonEditTask"));
    auto *run = action(w, "actionRun");
    QVERIFY(list && queue && edit && run);
    QCOMPARE(list->count(), 2);
    // the list order, with Settings/sortTask unset: descending
    QCOMPARE(itemTexts(list), (QStringList{QStringLiteral("task-b"),
                                           QStringLiteral("task-a")}));

    selectOnly(list, {QStringLiteral("task-a")});
    QVERIFY(edit->isEnabled());
    selectOnly(list, {QStringLiteral("task-a"), QStringLiteral("task-b")});
    QCOMPARE(list->selectedItems().size(), 2);
    QVERIFY(!edit->isEnabled());

    run->trigger();

    QCOMPARE(w.findChildren<JobWidget *>().size(), 2);
    // the tasks run at once, none of them waits in the queue
    QCOMPARE(queue->count(), 0);
    QCOMPARE(order.infos.size(), 2);
    QVERIFY2(order.infos.at(0).contains(QStringLiteral("\"task-b\"")),
             qPrintable(order.infos.at(0)));
    QVERIFY2(order.infos.at(1).contains(QStringLiteral("\"task-a\"")),
             qPrintable(order.infos.at(1)));
    waitAllEnded(w);
  }

  // T-TASK-CONFIRM (#144)
  void T_TASK_CONFIRM() {
    mkTask(QStringLiteral("task-one"), QStringLiteral("/nonexistent/one"),
           QStringLiteral("/nonexistent/d1"));
    mkTask(QStringLiteral("task-two"), QStringLiteral("/nonexistent/two"),
           QStringLiteral("/nonexistent/d2"));
    // the key is absent: the question is on by default (D1 of 14.2), which is
    // what every user who never saved the Preferences has
    QVERIFY(!GetSettings()->contains(QStringLiteral("Settings/confirmTaskRun")));

    MainWindow w;
    auto *list = w.findChild<QListWidget *>(QStringLiteral("tasksListWidget"));
    auto *run = action(w, "actionRun");
    auto *dryRun = action(w, "actionDryRun");
    QVERIFY(list && run && dryRun);
    const auto jobs = [&w]() { return w.findChildren<JobWidget *>().size(); };

    Answerer box;

    // on (by default) and answered No: nothing starts, the box names the task
    box.answer = QMessageBox::No;
    selectOnly(list, {QStringLiteral("task-one")});
    run->trigger();
    QCOMPARE(box.texts.size(), 1);
    QVERIFY2(box.texts.at(0).contains(QStringLiteral("task-one")),
             qPrintable(box.texts.at(0)));
    QCOMPARE(jobs(), 0);
    dryRun->trigger();
    QCOMPARE(box.texts.size(), 2);
    QCOMPARE(jobs(), 0);

    // on and answered Yes: it starts, Dry run too
    box.answer = QMessageBox::Yes;
    run->trigger();
    QCOMPARE(box.texts.size(), 3);
    QCOMPARE(jobs(), 1);
    waitAllEnded(w);
    dryRun->trigger();
    QCOMPARE(box.texts.size(), 4);
    QCOMPARE(jobs(), 2);
    waitAllEnded(w);

    // several tasks: one question naming all of them
    selectOnly(list, {QStringLiteral("task-one"), QStringLiteral("task-two")});
    box.answer = QMessageBox::No;
    run->trigger();
    QCOMPARE(box.texts.size(), 5);
    QVERIFY(box.texts.at(4).contains(QStringLiteral("task-one")));
    QVERIFY(box.texts.at(4).contains(QStringLiteral("task-two")));
    QCOMPARE(jobs(), 2);

    // off: no box and the tasks start
    GetSettings()->setValue(QStringLiteral("Settings/confirmTaskRun"), false);
    selectOnly(list, {QStringLiteral("task-two")});
    run->trigger();
    QCOMPARE(box.texts.size(), 5);
    QCOMPARE(jobs(), 3);
    waitAllEnded(w);
    dryRun->trigger();
    QCOMPARE(box.texts.size(), 5);
    QCOMPARE(jobs(), 4);
    waitAllEnded(w);
  }
};

QTEST_MAIN(TestTaskActions)
#include "tst_task_actions.moc"
