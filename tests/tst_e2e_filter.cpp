#include <QtTest>
#include <QtWidgets>

#include "job_options.h"
#include "job_widget.h"
#include "list_of_job_options.h"
#include "main_window.h"
#include "mount_widget.h"
#include "rb_test_support.h"
#include "stream_widget.h"
#include "utils.h"

namespace {

// Persists a copy task. The description shares no token with the paths.
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
  jo->uniqueId = QUuid::createUuid();
  ListOfJobOptions::getInstance()->Persist(jo);
  return jo;
}

// The texts of the items that are not hidden.
QStringList visibleTexts(QListWidget *list) {
  QStringList out;
  for (int i = 0; i < list->count(); ++i) {
    if (!list->item(i)->isHidden()) {
      out << list->item(i)->text();
    }
  }
  return out;
}

bool hiddenSelected(QListWidget *list) {
  const auto selected = list->selectedItems();
  for (const QListWidgetItem *item : selected) {
    if (item->isHidden()) {
      return true;
    }
  }
  return false;
}

// The jobs layout: a widget, its HLine, ... then the "no jobs" label and the
// spacer.
QVBoxLayout *jobsLayout(MainWindow &w) {
  return w.findChild<QVBoxLayout *>(QStringLiteral("jobs"));
}

JobWidget *jobWithSource(MainWindow &w, const QString &source) {
  const auto jobs = w.findChildren<JobWidget *>();
  for (JobWidget *j : jobs) {
    if (j->filterFields().at(1) == source) {
      return j;
    }
  }
  return nullptr;
}

// The HLine that follows `widget` in the jobs layout.
QWidget *lineAfter(MainWindow &w, QWidget *widget) {
  QVBoxLayout *jobs = jobsLayout(w);
  const int at = jobs->indexOf(widget);
  return at < 0 ? nullptr : jobs->itemAt(at + 1)->widget();
}

// A widget the filter hid: hidden AND flagged as hidden on purpose, so the
// layout's queued show leaves it alone (a never-shown parent reports a new
// child as not hidden, a shown one as hidden without the flag).
bool hiddenOnPurpose(const QWidget *widget) {
  return widget->isHidden() &&
         widget->testAttribute(Qt::WA_WState_ExplicitShowHide);
}

void addJob(MainWindow &w, const QString &source, const QString &dest,
            const QString &text = QString()) {
  const QString message = text.isEmpty()
                              ? QStringLiteral("Copy %1 to %2").arg(source, dest)
                              : text;
  const QStringList args{QStringLiteral("copy"), source, dest};
  QMetaObject::invokeMethod(&w, "addTransfer", Qt::DirectConnection,
                            Q_ARG(QString, message), Q_ARG(QString, source),
                            Q_ARG(QString, dest), Q_ARG(QStringList, args),
                            Q_ARG(QString, QUuid::createUuid().toString()),
                            Q_ARG(QString, QStringLiteral("task")),
                            Q_ARG(QString, QStringLiteral("r1")));
}

} // namespace

// The search filter of the Tasks and Jobs lists (#21), on the real window.
class TestE2eFilter : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    // the singleton loads tasks.bin once, under this HOME
    QVERIFY(QDir().mkpath(rbTestDir().path() + QStringLiteral("/home")));
    qputenv("HOME", (rbTestDir().path() + QStringLiteral("/home")).toUtf8());
    SetRclone(QStringLiteral("/bin/false"));
    auto settings = GetSettings();
    // without it the constructor waits in a modal box
    settings->setValue(QStringLiteral("Settings/rclone"),
                       QStringLiteral("/bin/false"));
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
  }

  // T-E2E-FILTER-TASKS: the Tasks filter hides, deselects and survives
  // listTasks().
  void T_E2E_FILTER_TASKS() {
    mkTask(QStringLiteral("alpha one"), QStringLiteral("/rb-filter/s1"),
           QStringLiteral("/rb-filter/d1"));
    mkTask(QStringLiteral("bravo two"), QStringLiteral("/rb-filter/s2"),
           QStringLiteral("/rb-filter/d2"));
    mkTask(QStringLiteral("charlie three"), QStringLiteral("/rb-filter/s3"),
           QStringLiteral("/rb-filter/d3"));
    // the tasks file is the one of this test
    const QString tasksFile =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
        QStringLiteral("/tasks.bin");
    QVERIFY(tasksFile.startsWith(rbTestDir().path()));
    QVERIFY(QFileInfo::exists(tasksFile));

    MainWindow w;
    auto *list = w.findChild<QListWidget *>(QStringLiteral("tasksListWidget"));
    auto *filter = w.findChild<QLineEdit *>(QStringLiteral("tasksFilter"));
    QVERIFY(list);
    QVERIFY(filter);
    QTRY_COMPARE(list->count(), 3);
    QCOMPARE(visibleTexts(list).size(), 3);

    // (1) all three selected
    list->selectAll();
    QCOMPARE(list->selectedItems().size(), 3);

    // (2) one task left, the others are deselected
    filter->setText(QStringLiteral("bravo two"));
    QCOMPARE(visibleTexts(list), QStringList{QStringLiteral("bravo two")});
    QVERIFY(!hiddenSelected(list));

    // (3) a hidden row cannot be selected: neither one by one, nor by select
    // all, nor by a range of the selection model (the itemSelectionChanged
    // guard, not only the deselect in the filter)
    QListWidgetItem *hidden =
        list->findItems(QStringLiteral("alpha one"), Qt::MatchExactly).value(0);
    QVERIFY(hidden);
    QVERIFY(hidden->isHidden());
    hidden->setSelected(true);
    QVERIFY(!hidden->isSelected());
    list->selectAll();
    QVERIFY(!hiddenSelected(list));
    list->selectionModel()->select(
        QItemSelection(list->model()->index(0, 0),
                       list->model()->index(list->count() - 1, 0)),
        QItemSelectionModel::Select);
    QVERIFY(!hiddenSelected(list));

    // (4) case and token order do not matter
    filter->setText(QStringLiteral("BRAVO"));
    QCOMPARE(visibleTexts(list), QStringList{QStringLiteral("bravo two")});
    filter->setText(QStringLiteral("two BRA"));
    QCOMPARE(visibleTexts(list), QStringList{QStringLiteral("bravo two")});

    // (4b) the source and the dest are searched too; their tokens are not in
    // any description, and the tokens of one filter may sit in both
    filter->setText(QStringLiteral("s2"));
    QCOMPARE(visibleTexts(list), QStringList{QStringLiteral("bravo two")});
    filter->setText(QStringLiteral("/rb-filter/d3"));
    QCOMPARE(visibleTexts(list), QStringList{QStringLiteral("charlie three")});
    filter->setText(QStringLiteral("bravo d2"));
    QCOMPARE(visibleTexts(list), QStringList{QStringLiteral("bravo two")});
    filter->setText(QStringLiteral("charlie d2"));
    QCOMPARE(visibleTexts(list), QStringList());
    filter->setText(QStringLiteral("two BRA"));
    QCOMPARE(visibleTexts(list), QStringList{QStringLiteral("bravo two")});

    // (5) a new task rebuilds the list (listTasks): the filter still holds
    mkTask(QStringLiteral("delta four"), QStringLiteral("/rb-filter/s4"),
           QStringLiteral("/rb-filter/d4"));
    QTRY_COMPARE(list->count(), 4);
    QCOMPARE(visibleTexts(list), QStringList{QStringLiteral("bravo two")});
    list->selectAll();
    QVERIFY(!hiddenSelected(list));

    // (6) cleared: everything is back
    filter->clear();
    QCOMPARE(visibleTexts(list).size(), 4);
  }

  // T-E2E-FILTER-JOBS: the Jobs filter hides a job with its separator, also
  // for a job added while it is set; the job count keeps counting all.
  void T_E2E_FILTER_JOBS() {
    MainWindow w;
    auto *filter = w.findChild<QLineEdit *>(QStringLiteral("jobsFilter"));
    auto *tabs = w.findChild<QTabWidget *>(QStringLiteral("tabs"));
    auto *content =
        w.findChild<QWidget *>(QStringLiteral("scrollAreaWidgetContents"));
    QVERIFY(filter);
    QVERIFY(tabs);
    QVERIFY(content);
    QVERIFY(jobsLayout(w));

    // a shown window on the Jobs tab: a job inserted now starts hidden only
    // because it was never shown, the case the filter has to override
    tabs->setCurrentIndex(1);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    // /bin/false ends at the next event loop turn: no event loop until the
    // assertions on the first two jobs are done
    addJob(w, QStringLiteral("a:one"), QStringLiteral("b:x"));
    addJob(w, QStringLiteral("c:two"), QStringLiteral("d:y"));
    QCOMPARE(w.findChildren<JobWidget *>().size(), 2);
    int lines = 0;
    const auto children = content->children();
    for (QObject *c : children) {
      auto *frame = qobject_cast<QFrame *>(c);
      if (frame && !qobject_cast<QLabel *>(frame) &&
          frame->frameShape() == QFrame::HLine) {
        ++lines;
      }
    }
    QCOMPARE(lines, 2);
    QCOMPARE(tabs->tabText(1), QStringLiteral("Jobs (2)"));

    JobWidget *one = jobWithSource(w, QStringLiteral("a:one"));
    JobWidget *two = jobWithSource(w, QStringLiteral("c:two"));
    QVERIFY(one);
    QVERIFY(two);
    QWidget *lineOne = lineAfter(w, one);
    QWidget *lineTwo = lineAfter(w, two);
    QVERIFY(lineOne);
    QVERIFY(lineTwo);

    filter->setText(QStringLiteral("a:one"));
    QVERIFY(!one->isHidden());
    QVERIFY(!lineOne->isHidden());
    QVERIFY(hiddenOnPurpose(two));
    QVERIFY(hiddenOnPurpose(lineTwo));

    filter->clear();
    QVERIFY(!one->isHidden());
    QVERIFY(!lineOne->isHidden());
    QVERIFY(!two->isHidden());
    QVERIFY(!lineTwo->isHidden());

    // a job added while the filter is set follows it
    filter->setText(QStringLiteral("a:one"));
    addJob(w, QStringLiteral("e:three"), QStringLiteral("f:z"));
    JobWidget *three = jobWithSource(w, QStringLiteral("e:three"));
    QVERIFY(three);
    QWidget *lineThree = lineAfter(w, three);
    QVERIFY(lineThree);
    QCOMPARE(w.findChildren<JobWidget *>().size(), 3);
    // hidden on purpose right after the insertion: the layout's queued show
    // (_q_showIfNotHidden) skips such a widget, and setVisible must be called
    // also when the widget already reads as hidden
    QVERIFY(hiddenOnPurpose(three));
    QVERIFY(hiddenOnPurpose(lineThree));
    QVERIFY(!one->isHidden());
    QCOMPARE(tabs->tabText(1), QStringLiteral("Jobs (3)"));

    // after the queued shows ran (and the ended jobs re-sorted) it holds
    QTest::qWait(100);
    QVERIFY(three->isHidden());
    QVERIFY(lineThree->isHidden());
    QVERIFY(two->isHidden());
    QVERIFY(!one->isHidden());

    filter->clear();
    QVERIFY(!three->isHidden());
    QVERIFY(!lineThree->isHidden());

    // the filter reads the source and the dest too: this message holds
    // neither of them
    addJob(w, QStringLiteral("g:four"), QStringLiteral("h:w"),
           QStringLiteral("job four"));
    JobWidget *four = jobWithSource(w, QStringLiteral("g:four"));
    QVERIFY(four);
    filter->setText(QStringLiteral("g:four"));
    QVERIFY(!four->isHidden());
    QVERIFY(hiddenOnPurpose(one));
    QVERIFY(hiddenOnPurpose(two));
    QVERIFY(hiddenOnPurpose(three));
    filter->setText(QStringLiteral("job h:w"));
    QVERIFY(!four->isHidden());
    QVERIFY(hiddenOnPurpose(one));

    // Stop All keeps sortJobs() from running (mDoNotSort) while its modal box
    // is open, and the event loop of the box lets a queue task start a job:
    // only the call right after the insertion can hide that job. A transfer
    // is running (no event loop turn since addJob), so the box opens.
    filter->setText(QStringLiteral("g:four"));
    bool fiveHidden = false;
    bool lineFiveHidden = false;
    bool boxClosed = false;
    QTimer::singleShot(0, &w, [&]() {
      addJob(w, QStringLiteral("i:five"), QStringLiteral("j:v"));
      JobWidget *five = jobWithSource(w, QStringLiteral("i:five"));
      if (five) {
        fiveHidden = hiddenOnPurpose(five);
        QWidget *lineFive = lineAfter(w, five);
        lineFiveHidden = lineFive && hiddenOnPurpose(lineFive);
      }
      auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
      if (box && box->button(QMessageBox::No)) {
        boxClosed = true;
        box->button(QMessageBox::No)->click();
      } else if (box) {
        box->reject();
        boxClosed = true;
      }
    });
    auto *stopAll = w.findChild<QAction *>(QStringLiteral("actionStopAllTransfers"));
    QVERIFY(stopAll);
    stopAll->trigger();
    QVERIFY(boxClosed);
    QVERIFY(fiveHidden);
    QVERIFY(lineFiveHidden);
    QTest::qWait(100);
  }

  // T-E2E-FILTER-FIELDS: what each kind of job widget offers to the filter.
  void T_E2E_FILTER_FIELDS() {
    QObject holder;
    auto *mountProc = new QProcess(&holder); // never started
    MountWidget mount(mountProc, QStringLiteral("remote:"),
                      QStringLiteral("/mnt/x y"),
                      QStringList{QStringLiteral("mount")}, QString(),
                      QStringLiteral("uid"), QString());
    QCOMPARE(mount.filterFields(),
             (QStringList{QStringLiteral("remote: on /mnt/x y"),
                          QStringLiteral("remote:"),
                          QStringLiteral("/mnt/x y")}));

    auto *jobProc = new QProcess(&holder);
    JobWidget job(jobProc, QStringLiteral("Copy s: to d:"),
                  QStringList{QStringLiteral("copy")}, QStringLiteral("s:a"),
                  QStringLiteral("d:b"), QStringLiteral("uid"),
                  QStringLiteral("copy"), QStringLiteral("req"));
    QCOMPARE(job.filterFields(),
             (QStringList{QStringLiteral("Copy s: to d:"),
                          QStringLiteral("s:a"), QStringLiteral("d:b")}));

    auto *rcloneProc = new QProcess(&holder);
    auto *playerProc = new QProcess(&holder);
    StreamWidget stream(rcloneProc, playerProc, QStringLiteral("remote:f.mp4"),
                        QStringLiteral("vlc -"),
                        QStringList{QStringLiteral("cat")});
    QCOMPARE(stream.filterFields(),
             (QStringList{QStringLiteral("remote:f.mp4"),
                          QStringLiteral("remote:f.mp4"),
                          QStringLiteral("vlc -")}));
  }
};

QTEST_MAIN(TestE2eFilter)
#include "tst_e2e_filter.moc"
