#include <algorithm>

#include "main_window.h"
#include <QTabBar>
#include "global.h"
#include "job_options.h"
#include "job_widget.h"
#include "list_of_job_options.h"
#include "mount_dialog.h"
#include "mount_launch.h"
#include "mount_widget.h"
#include "preferences_dialog.h"
#include "rc_client.h"
#include "rclone_daemon.h"
#include "rclone_version.h"
#include "remote_config_dialog.h"
#include "remote_widget.h"
#include "scheduler_widget.h"
#include "stream_widget.h"
#include "transfer_dialog.h"
#include "lsjson_parser.h"
#include "rclone_process.h"
#include "terminal_command.h"
#include "update_check.h"
#include "utils.h"
#ifdef Q_OS_MACOS
#include "global.h"
#include "mac_os_notifications.h"
#include "mac_os_power_saving.h"
#include "osx_helper.h"
#endif
#include "file_dialog.h"

void MainWindow::setupInitialStatus() {
  // initial queue status
  ui.labelQueueInfoStop->setText("Queue is not running.");
  setQueueButtons();

  ui.tabs->setTabText(4, QString("Scheduler (0)>>(0)"));
  ui.labelSchedulerInfoStart->setText("Scheduler is running.");
  ui.labelSchedulerInfoStop->setText("Scheduler is not running.");
  ui.labelSchedulerInfoStart->show();
  ui.labelSchedulerInfoStop->hide();
}

void MainWindow::setupTasksTab() {
  auto settings = GetSettings();

  QObject::connect(ui.tasksListWidget, &QListWidget::itemClicked, this,
                   [=]() { setTasksButtons(); });

  QObject::connect(ui.tasksListWidget, &QListWidget::currentItemChanged, this,
                   [=]() { setTasksButtons(); });

  QObject::connect(ui.tasksListWidget, &QListWidget::itemChanged, this,
                   [=]() { setTasksButtons(); });

  QObject::connect(ui.tasksListWidget, &QListWidget::itemSelectionChanged, this,
                   [=]() { setTasksButtons(); });

  QObject::connect(ui.tasksListWidget, &QListWidget::itemDoubleClicked, this,
                   [=]() { editSelectedTask(); });

  bool sortTask = settings->value("Settings/sortTask").toBool();

  if (sortTask) {
    ui.actionSortTask->setIcon(mSortZAIcon);
    ui.buttonSortTask->setToolTip("Sort Descending");
    ui.tasksListWidget->setSortingEnabled(true);
    ui.tasksListWidget->sortItems(Qt::AscendingOrder);
  } else {
    ui.actionSortTask->setIcon(mSortAZIcon);
    ui.buttonSortTask->setToolTip("Sort Ascending");
    ui.tasksListWidget->setSortingEnabled(true);
    ui.tasksListWidget->sortItems(Qt::DescendingOrder);
  }

  QObject::connect(
      ui.tasksListWidget, &QWidget::customContextMenuRequested, this,
      [=](const QPoint &pos) {
        setTasksButtons();
        auto items = ui.tasksListWidget->selectedItems();
        bool isMount = false;
        bool isRunning = false;
        bool isScheduled = false;
        int schedulersCount = ui.schedulers->count();

        if (items.count() > 0) {

          foreach (auto i, items) {
            JobOptionsListWidgetItem *item =
                static_cast<JobOptionsListWidgetItem *>(i);
            JobOptions *jo = item->GetData();

            if (jo->operation == JobOptions::Mount) {
              isMount = true;
            }

            int widgetsCount = ui.jobs->count();
            for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
              QWidget *widget = ui.jobs->itemAt(j)->widget();

              if (auto transfer = qobject_cast<JobWidget *>(widget)) {
                if ((transfer->getUniqueID() == jo->uniqueId.toString()) &&
                    (transfer->isRunning)) {
                  isRunning = true;
                }
              }

              if (auto mount = qobject_cast<MountWidget *>(widget)) {
                if ((mount->getUniqueID() == jo->uniqueId.toString()) &&
                    (mount->isRunning)) {
                  isRunning = true;
                }
              }

              if (!isScheduled) {
                for (int k = schedulersCount - 2; k >= 0; k = k - 2) {
                  QWidget *widget = ui.schedulers->itemAt(k)->widget();
                  if (auto scheduler =
                          qobject_cast<SchedulerWidget *>(widget)) {

                    if (scheduler->getSchedulerTaskId() ==
                        jo->uniqueId.toString()) {

                      isScheduled = true;
                      break;
                    }
                  }
                }
              }
            }
          }

          QMenu menu;
          if (!isMount) {
            menu.addAction(ui.actionAddToQueue);
            menu.addAction(ui.actionAddToScheduler);
          }
          menu.addSeparator();
          if (!isMount) {
            if (!isRunning) {
              menu.addAction(ui.actionDryRun);
            }
          }
          if (!isRunning) {
            menu.addAction(ui.actionRun);
          }
          if (isRunning) {
            menu.addAction(ui.actionStop);
          }
          menu.addSeparator();
          if (items.count() == 1) {
            menu.addAction(ui.actionEdit);
          }
          if (!isRunning && !isScheduled) {
            menu.addAction(ui.actionDelete);
          }
          menu.exec(ui.tasksListWidget->viewport()->mapToGlobal(pos));
        }
      });
}

void MainWindow::setupQueueContextMenu() {
  QObject::connect(ui.queueListWidget, &QWidget::customContextMenuRequested,
                   this, [=](const QPoint &pos) {
                     QMenu menu;
                     menu.addAction(ui.actionUpQueue);
                     menu.addAction(ui.actionDownQueue);
                     menu.addSeparator();
                     menu.addAction(ui.actionRemoveFromQueue);
                     menu.exec(
                         ui.queueListWidget->viewport()->mapToGlobal(pos));
                   });
}

void MainWindow::setupTasksActions() {
  QObject::connect(ui.actionSortTask, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool sortTask = settings->value("Settings/sortTask").toBool();

    if (sortTask) {
      ui.actionSortTask->setIcon(mSortAZIcon);
      ui.buttonSortTask->setToolTip("Sort Ascending");
      ui.tasksListWidget->setSortingEnabled(true);
      ui.tasksListWidget->sortItems(Qt::DescendingOrder);
      settings->setValue("Settings/sortTask", "false");
    } else {
      ui.actionSortTask->setIcon(mSortZAIcon);
      settings->setValue("Settings/sortTask", "true");
      ui.tasksListWidget->setSortingEnabled(true);
      ui.tasksListWidget->sortItems(Qt::AscendingOrder);
      ui.buttonSortTask->setToolTip("Sort Descending");
    }
  });

  QObject::connect(ui.actionEdit, &QAction::triggered, this, [=]() {
    auto items = ui.tasksListWidget->selectedItems();

    if (items.count() != 1) {
      QMessageBox::information(this, tr("Edit task"),
                               tr("Please select one task to edit"));
    }

    else {
      editSelectedTask();
    }
  });

  QObject::connect(ui.actionDryRun, &QAction::triggered, this, [=]() {
    auto selection = ui.tasksListWidget->selectedItems();

    auto settings = GetSettings();
    bool sortTask = settings->value("Settings/sortTask").toBool();

    auto items = sortListWidget(selection, sortTask);

    QString itemsToRun;

    foreach (auto i, items) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(i);
      JobOptions *jo = item->GetData();
      itemsToRun = itemsToRun + jo->description + "\n";
    }

    if (items.count() > 0) {
      int button = QMessageBox::question(
          this, "Dry run",
          QString(
              "Are you sure you want to dry run the following task(s)?\n\n" +
              itemsToRun),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (button == QMessageBox::Yes) {
        foreach (auto i, items) {
          JobOptionsListWidgetItem *item =
              static_cast<JobOptionsListWidgetItem *>(i);
          runItem(item, "task", "requestId_placeholder", true);
        }
      }
    }
    ui.tasksListWidget->setFocus();
  });

  QObject::connect(ui.actionStop, &QAction::triggered, this, [=]() {
    auto selection = ui.tasksListWidget->selectedItems();

    auto settings = GetSettings();
    bool sortTask = settings->value("Settings/sortTask").toBool();

    auto items = sortListWidget(selection, sortTask);

    QString itemsToStop;

    foreach (auto i, items) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(i);
      JobOptions *jo = item->GetData();
      itemsToStop = itemsToStop + jo->description + "\n";
    }

    if (items.count() > 0) {
      int button = QMessageBox::question(
          this, "Stop tasks",
          QString("Are you sure you want to stop the following task(s)?\n\n" +
                  itemsToStop),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (button == QMessageBox::Yes) {
        foreach (auto i, items) {
          JobOptionsListWidgetItem *item =
              static_cast<JobOptionsListWidgetItem *>(i);

          JobOptions *jo = item->GetData();

          int widgetsCount = ui.jobs->count();
          for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
            QWidget *widget = ui.jobs->itemAt(j)->widget();

            if (auto transfer = qobject_cast<JobWidget *>(widget)) {
              if ((transfer->getUniqueID() == jo->uniqueId.toString()) &&
                  (transfer->isRunning)) {
                transfer->cancel();
              }
            }
            if (auto mount = qobject_cast<MountWidget *>(widget)) {
              if ((mount->getUniqueID() == jo->uniqueId.toString()) &&
                  (mount->isRunning)) {
                mount->cancel();
              }
            }
          }
        }
      }
    }
    ui.tasksListWidget->setFocus();
  });

  QObject::connect(ui.actionRun, &QAction::triggered, this, [=]() {
    auto selection = ui.tasksListWidget->selectedItems();

    auto settings = GetSettings();
    bool sortTask = settings->value("Settings/sortTask").toBool();

    auto items = sortListWidget(selection, sortTask);

    QString itemsToRun;
    QString itemsAlreadyRunning;

    foreach (auto i, items) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(i);
      JobOptions *jo = item->GetData();
      itemsToRun = itemsToRun + jo->description + "\n";

      // check if task already running
      int widgetsCount = ui.jobs->count();
      for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
        QWidget *widget = ui.jobs->itemAt(j)->widget();

        if (auto transfer = qobject_cast<JobWidget *>(widget)) {
          if ((transfer->getUniqueID() == jo->uniqueId.toString()) &&
              (transfer->isRunning)) {
            itemsAlreadyRunning = itemsAlreadyRunning + jo->description + "\n";
          }
        }

        if (auto mount = qobject_cast<MountWidget *>(widget)) {
          if ((mount->getUniqueID() == jo->uniqueId.toString()) &&
              (mount->isRunning)) {
            itemsAlreadyRunning = itemsAlreadyRunning + jo->description + "\n";
          }
        }
      }
    }

    if (!itemsAlreadyRunning.isEmpty()) {
      QMessageBox::critical(
          this, "Run",
          QString("Failed as the following task(s) are already running.\n\n" +
                  itemsAlreadyRunning),
          QMessageBox::Ok);
      return;
    }

    if (items.count() > 0) {
      int button = QMessageBox::question(
          this, "Run",
          QString("Are you sure you want to run the following task(s)?\n\n" +
                  itemsToRun),
          QMessageBox::No | QMessageBox::Yes, QMessageBox::No);
      if (button == QMessageBox::Yes) {

        foreach (auto i, items) {
          JobOptionsListWidgetItem *item =
              static_cast<JobOptionsListWidgetItem *>(i);
          runItem(item, "task", "requestID_placeholder");
        }
      }
    }
    ui.tasksListWidget->setFocus();
    setTasksButtons();
  });

  QObject::connect(ui.actionDelete, &QAction::triggered, this, [=]() {
    auto selection = ui.tasksListWidget->selectedItems();

    auto settings = GetSettings();
    bool sortTask = settings->value("Settings/sortTask").toBool();

    auto items = sortListWidget(selection, sortTask);

    QString itemsToDelete;
    QString itemsAlreadyRunning;

    foreach (auto i, items) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(i);
      JobOptions *jo = item->GetData();
      itemsToDelete = itemsToDelete + jo->description + "\n";

      // check if any tasks already running
      int widgetsCount = ui.jobs->count();
      for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
        QWidget *widget = ui.jobs->itemAt(j)->widget();

        if (auto transfer = qobject_cast<JobWidget *>(widget)) {
          if ((transfer->getUniqueID() == jo->uniqueId.toString()) &&
              (transfer->isRunning)) {
            itemsAlreadyRunning = itemsAlreadyRunning + jo->description + "\n";
          }
        }
        if (auto mount = qobject_cast<MountWidget *>(widget)) {
          if ((mount->getUniqueID() == jo->uniqueId.toString()) &&
              (mount->isRunning)) {
            itemsAlreadyRunning = itemsAlreadyRunning + jo->description + "\n";
          }
        }
      }
    }

    // don't delete running tasks
    if (!itemsAlreadyRunning.isEmpty()) {
      QMessageBox::critical(
          this, "Run",
          QString("Failed as the following task(s) are running:\n\n" +
                  itemsAlreadyRunning +
                  "\n\nYou can't delete running tasks.\nStop them first."),
          QMessageBox::Ok);
      ui.tasksListWidget->setFocus();
      setTasksButtons();
      return;
    }

    if (items.count() > 0) {
      // all clear - delete
      int button = QMessageBox::warning(
          this, "Delete",
          QString("Are you sure you want to delete the following task(s)?\n\n" +
                  itemsToDelete),
          QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

      if (button == QMessageBox::Yes) {

        QStringList itemsIDsToDelete;
        foreach (auto i, items) {
          JobOptionsListWidgetItem *item =
              static_cast<JobOptionsListWidgetItem *>(i);
          JobOptions *jo = item->GetData();
          itemsIDsToDelete << jo->uniqueId.toString();
        }
        for (const auto &i : itemsIDsToDelete) {
          int widgetsCount = ui.tasksListWidget->count();
          for (int j = widgetsCount - 1; j >= 0; j = j - 1) {
            JobOptionsListWidgetItem *item =
                static_cast<JobOptionsListWidgetItem *>(
                    ui.tasksListWidget->item(j));
            JobOptions *jo = item->GetData();
            if (jo->uniqueId.toString() == i) {
              {
                ListOfJobOptions::getInstance()->Forget(jo);
              }
            }
          }
        }

      } else {
        ui.tasksListWidget->setFocus();
      }
    }

    // restore active tasks colours
    int widgetsCount = ui.jobs->count();
    for (int k = 0; k < ui.tasksListWidget->count(); k = k + 1) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(ui.tasksListWidget->item(k));

      JobOptions *joTasks = item->GetData();

      for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
        QWidget *widget = ui.jobs->itemAt(j)->widget();

        if (auto transfer = qobject_cast<JobWidget *>(widget)) {
          if ((transfer->getUniqueID() == joTasks->uniqueId.toString()) &&
              (transfer->isRunning)) {
            ui.tasksListWidget->item(k)->setBackground(Qt::darkGreen);
          }
        }

        if (auto mount = qobject_cast<MountWidget *>(widget)) {
          if ((mount->getUniqueID() == joTasks->uniqueId.toString()) &&
              (mount->isRunning)) {
            ui.tasksListWidget->item(k)->setBackground(Qt::darkGreen);
          }
        }
      }
    }
  });
}

void MainWindow::setupScheduler() {
  //!!! QObject::connect(ui.actionStartScheduler
  QObject::connect(ui.actionStartScheduler, &QAction::triggered, this, [=]() {
    mDoNotSort = true;
    auto settings = GetSettings();
    settings->setValue("Settings/schedulerStatus", "true");

    int schedulersCount = ui.schedulers->count();
    for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
      QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
      if (auto scheduler = qobject_cast<SchedulerWidget *>(schedulerWidget)) {

        scheduler->startScheduler();
      }
    }

    ui.buttonStartScheduler->setEnabled(false);
    ui.buttonStopScheduler->setEnabled(true);

    ui.labelSchedulerInfoStart->show();
    ui.labelSchedulerInfoStop->hide();

    ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                               .arg(mSchedulersCount)
                               .arg(mRunningSchedulersCount));
    mDoNotSort = false;
    sortJobs();
  });

  //!!!  QObject::connect(ui.actionStopScheduler
  QObject::connect(ui.actionStopScheduler, &QAction::triggered, this, [=]() {
    mDoNotSort = true;
    auto settings = GetSettings();
    settings->setValue("Settings/schedulerStatus", "false");

    int schedulersCount = ui.schedulers->count();
    for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
      QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
      if (auto scheduler = qobject_cast<SchedulerWidget *>(schedulerWidget)) {

        scheduler->stopScheduler();
      }
    }

    ui.buttonStartScheduler->setEnabled(true);
    ui.buttonStopScheduler->setEnabled(false);

    ui.labelSchedulerInfoStart->hide();
    ui.labelSchedulerInfoStop->show();

    ui.tabs->setTabText(4, QString("Scheduler (%1)").arg(mSchedulersCount));
    mDoNotSort = false;
    sortJobs();
  });
}

void MainWindow::setupSchedulerAdd() {
  //!!!  QObject::connect(ui.actionAddToScheduler
  QObject::connect(ui.actionAddToScheduler, &QAction::triggered, this, [=]() {
    auto selection = ui.tasksListWidget->selectedItems();

    auto settings = GetSettings();
    bool sortTask = settings->value("Settings/sortTask").toBool();

    auto items = sortListWidget(selection, !sortTask);

    QString itemsToAdd;

    // create list of selected tasks' names
    foreach (auto i, items) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(i);
      JobOptions *jo = item->GetData();
      itemsToAdd = itemsToAdd + jo->description + "\n";

      if (jo->operation == JobOptions::Mount) {

        QMessageBox::information(
            this, tr("Mount tasks selected!"),
            tr("You selected mount tasks - they can't be added to the queue."));

        return;
      }
    }

    if (items.count() > 0) {
      int button = QMessageBox::question(
          this, "Add to the scheduler",
          QString("Are you sure you want to add the following "
                  "task(s) to the scheduler?\n\n" +
                  itemsToAdd),
          QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

      if (button == QMessageBox::Yes) {

        foreach (auto i, items) {
          JobOptionsListWidgetItem *item =
              static_cast<JobOptionsListWidgetItem *>(i);

          JobOptions *jo = item->GetData();

          jo->uniqueId.toString();
          mSchedulersCount++;
          QStringList args;
          args << "NewScheduler";
          addScheduler(jo->uniqueId.toString(), jo->description, args);

          if ((settings->value("Settings/schedulerStatus").toBool())) {

            ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                       .arg(mSchedulersCount)
                                       .arg(mRunningSchedulersCount));
          } else {
            ui.tabs->setTabText(
                4, QString("Scheduler (%1)").arg(mSchedulersCount));

            int schedulersCount = ui.schedulers->count();
            for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
              QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
              if (auto scheduler =
                      qobject_cast<SchedulerWidget *>(schedulerWidget)) {

                scheduler->stopScheduler();
              }
            }
          }
        }
        listTasks();
        //        saveSchedulerFile();
      } else {
        return;
      }
    }
  });
}

void MainWindow::setupQueue() {
  //!!! QObject::connect(ui.actionAddToQueue
  QObject::connect(ui.actionAddToQueue, &QAction::triggered, this, [=]() {
    auto selection = ui.tasksListWidget->selectedItems();

    auto settings = GetSettings();
    bool sortTask = settings->value("Settings/sortTask").toBool();

    auto items = sortListWidget(selection, sortTask);

    QString itemsToAdd;

    // create list of selected tasks' names
    foreach (auto i, items) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(i);
      JobOptions *jo = item->GetData();
      itemsToAdd = itemsToAdd + jo->description + "\n";

      if (jo->operation == JobOptions::Mount) {

        QMessageBox::information(
            this, tr("Mount tasks selected!"),
            tr("You selected mount tasks - they can't be added to the queue."));

        return;
      }
    }

    // if queue is empty we have to try to start first task
    bool isQueueEmpty = (ui.queueListWidget->count() == 0);

    if (items.count() > 0) {
      int button = QMessageBox::question(
          this, "Add to the queue",
          QString("Are you sure you want to add the following "
                  "task(s) to the queue?\n\n" +
                  itemsToAdd),
          QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

      if (button == QMessageBox::Yes) {

        mQueueCount = mQueueCount + items.count();

        foreach (auto i, items) {
          JobOptionsListWidgetItem *item =
              static_cast<JobOptionsListWidgetItem *>(i);

          JobOptions *jo = item->GetData();

          QIcon jobIcon;

          if (jo->jobType == JobOptions::JobType::Download) {
            if (jo->operation == JobOptions::Mount) {
              jobIcon = mMountIcon;
            } else {
              jobIcon = mDownloadIcon;
            }
          }
          if (jo->jobType == JobOptions::JobType::Upload) {
            jobIcon = mUploadIcon;
          }

          JobOptionsListWidgetItem *newitem = new JobOptionsListWidgetItem(
              jo, jobIcon, jo->description, QUuid::createUuid().toString());

          ui.queueListWidget->addItem(newitem);
        }

        if (ui.queueListWidget->count() > 0) {
          ui.buttonPurgeQueue->setEnabled(true);
        } else {
          ui.buttonPurgeQueue->setEnabled(false);
        }
        // save new queue to file
        saveQueueFile();
        ui.queueListWidget->setFocus();
      } else {
        // user pressed No
        return;
      }
    }

    // if queue was empty we start first task if queue is running and there is
    // no other transfer job running
    if (mQueueStatus && isQueueEmpty && (mTransferJobCount == 0)) {

      if (mQueueCount > 0) {

        JobOptionsListWidgetItem *item =
            static_cast<JobOptionsListWidgetItem *>(
                ui.queueListWidget->item(0));

        runItem(item, "queue", item->GetRequestId());
        ui.queueListWidget->item(0)->setBackground(Qt::darkGreen);
        mQueueTaskRunning = true;
        ui.tabs->setTabText(3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
      }

    } else {

      if (mQueueStatus) {

        if (mQueueTaskRunning) {
          ui.tabs->setTabText(3,
                              QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
        } else {

          ui.tabs->setTabText(3, QString("Queue (%1)>>(0)").arg(mQueueCount));
        }

      } else {
        ui.tabs->setTabText(3, QString("Queue (%1)").arg(mQueueCount));
      }
    }
  });

  //!!!  QObject::connect(ui.actionStartQueue
  QObject::connect(ui.actionStartQueue, &QAction::triggered, this, [=]() {
    mQueueStatus = true;

    auto settings = GetSettings();
    settings->setValue("Settings/queueStatus", "true");

    ui.tabs->setTabText(3, QString("Queue (%1)>>(0)").arg(mQueueCount));

    ui.buttonStopQueue->setEnabled(true);
    ui.buttonStartQueue->setEnabled(false);

    ui.buttonPurgeQueue->setEnabled(false);

    if (mQueueCount > 1) {
      ui.buttonPurgeQueue->setEnabled(true);
    } else {
      ui.buttonPurgeQueue->setEnabled(false);
    }

    if (ui.queueListWidget->currentRow() == 0 ||
        ui.queueListWidget->currentRow() == ui.queueListWidget->count() - 1) {
      ui.buttonDownQueue->setEnabled(false);
    } else {
      ui.buttonDownQueue->setEnabled(true);
    }

    if (ui.queueListWidget->currentRow() == 1 ||
        ui.queueListWidget->currentRow() == 0) {
      ui.buttonUpQueue->setEnabled(false);
    } else {
      ui.buttonUpQueue->setEnabled(true);
    }

    if (ui.queueListWidget->currentRow() == 0) {

      ui.buttonRemoveFromQueue->setEnabled(false);
      ui.buttonUpQueue->setEnabled(false);
      ui.buttonDownQueue->setEnabled(false);

    } else {
      ui.buttonRemoveFromQueue->setEnabled(true);
    }

    if (mQueueCount == 0) {
      ui.buttonRemoveFromQueue->setEnabled(false);
      ui.buttonPurgeQueue->setEnabled(false);
      ui.buttonUpQueue->setEnabled(false);
      ui.buttonDownQueue->setEnabled(false);
    }

    ui.labelQueueInfoStart->setText("Queue is running.");
    ui.labelQueueInfoStart->show();
    ui.labelQueueInfoStop->hide();

    // if not empty and nothing else running try to start first task
    if (mQueueCount > 0 && mTransferJobCount == 0) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(ui.queueListWidget->item(0));

      // start only when not running already
      //      if (!isAlreadyRunning) {
      mQueueTaskRunning = true;
      runItem(item, "queue", item->GetRequestId());
      ui.tabs->setTabText(3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
      ui.queueListWidget->item(0)->setBackground(Qt::darkGreen);
      ui.queueListWidget->item(0)->setSelected(false);
      //      }
    }
  });

  QObject::connect(ui.actionStopQueue, &QAction::triggered, this, [=]() {
    mQueueStatus = false;

    auto settings = GetSettings();
    settings->setValue("Settings/queueStatus", "false");

    if (ui.queueListWidget->count() > 0) {
      ui.queueListWidget->item(0)->setBackground(QBrush());
    }

    ui.buttonStopQueue->setEnabled(false);
    ui.buttonStartQueue->setEnabled(true);
    ui.buttonPurgeQueue->setEnabled(true);
    ui.buttonRemoveFromQueue->setEnabled(true);

    if (mQueueCount == 0) {
      ui.buttonRemoveFromQueue->setEnabled(false);
      ui.buttonPurgeQueue->setEnabled(false);
      ui.buttonUpQueue->setEnabled(false);
      ui.buttonDownQueue->setEnabled(false);
      ui.tabs->setTabText(3, QString("Queue"));
    } else {
      ui.tabs->setTabText(3, QString("Queue (%1)").arg(mQueueCount));
    }

    if (!(ui.queueListWidget->item(0) == nullptr)) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(ui.queueListWidget->item(0));

      JobOptions *jo = item->GetData();

      int widgetsCount = ui.jobs->count();
      for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
        QWidget *widget = ui.jobs->itemAt(j)->widget();
        if (auto transfer = qobject_cast<JobWidget *>(widget)) {

          if ((transfer->getUniqueID() == jo->uniqueId.toString()) &&
              (transfer->isRunning)) {

            if (transfer->getRequestId() == item->GetRequestId()) {

              emit transfer->cancel();
            }
          }
        }
      }
    }

    ui.labelQueueInfoStop->setText("Queue is not running.");
    ui.labelQueueInfoStop->show();
    ui.labelQueueInfoStart->hide();
  });
}

void MainWindow::setupQueueEditing() {
  //!!! QObject::connect(ui.actionPurgeQueue
  QObject::connect(ui.actionPurgeQueue, &QAction::triggered, this, [=]() {
    if (ui.queueListWidget->count() > 0) {

      int button = QMessageBox::warning(
          this, "Clean the queue",
          QString("Are you sure you want to remove all\nnot running tasks from "
                  "the queue?"),
          QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

      if (button == QMessageBox::Yes) {

        int itemsCount = ui.queueListWidget->count();
        for (int i = 0; i < itemsCount; i++) {

          if (mQueueStatus && mQueueTaskRunning) {
            if (i != 0) {
              --mQueueCount;

              JobOptionsListWidgetItem *item_queue =
                  static_cast<JobOptionsListWidgetItem *>(
                      ui.queueListWidget->item(1));

              QString requestId = item_queue->GetRequestId();
              // notify schedulers
              int schedulersCount = ui.schedulers->count();
              for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
                QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
                if (auto scheduler =
                        qobject_cast<SchedulerWidget *>(schedulerWidget)) {
                  scheduler->updateTaskStatus(requestId,
                                              "removed from the queue");

                  if (scheduler->getSchedulerRequestId() ==
                      item_queue->GetRequestId()) {

                    mRunningSchedulersCount--;
                    ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                               .arg(mSchedulersCount)
                                               .arg(mRunningSchedulersCount));
                  }
                }
              }

              ui.queueListWidget->takeItem(1);
            }
          } else {
            --mQueueCount;

            JobOptionsListWidgetItem *item_queue =
                static_cast<JobOptionsListWidgetItem *>(
                    ui.queueListWidget->item(0));

            QString requestId = item_queue->GetRequestId();
            // notify schedulers
            int schedulersCount = ui.schedulers->count();
            for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
              QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
              if (auto scheduler =
                      qobject_cast<SchedulerWidget *>(schedulerWidget)) {
                scheduler->updateTaskStatus(requestId,
                                            "removed from the queue");

                if (scheduler->getSchedulerRequestId() ==
                    item_queue->GetRequestId()) {
                  mRunningSchedulersCount--;
                  ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                             .arg(mSchedulersCount)
                                             .arg(mRunningSchedulersCount));
                }
              }
            }

            ui.queueListWidget->takeItem(0);
          }

          ui.buttonRemoveFromQueue->setEnabled(false);
          ui.buttonPurgeQueue->setEnabled(false);
          ui.buttonUpQueue->setEnabled(false);
          ui.buttonDownQueue->setEnabled(false);
        }
      } else {
        ui.queueListWidget->setFocus();
      }
    }

    if (mQueueStatus) {

      if (mQueueCount == 0) {
        ui.tabs->setTabText(3, QString("Queue (%1)>>(0)").arg(mQueueCount));
      } else {

        if (!mQueueTaskRunning) {
          ui.tabs->setTabText(3, QString("Queue (%1)>>(0)").arg(mQueueCount));
        } else {
          ui.tabs->setTabText(3,
                              QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
        }
      }

    } else {
      if (mQueueCount != 0) {
        ui.tabs->setTabText(3, QString("Queue (%1)").arg(mQueueCount));
      } else {
        ui.tabs->setTabText(3, QString("Queue"));
      }
    }

    saveQueueFile();
  });

  QObject::connect(ui.actionRemoveFromQueue, &QAction::triggered, this, [=]() {
    if (mQueueStatus) {

      if (mQueueCount > 0) {

        JobOptionsListWidgetItem *item_queue =
            static_cast<JobOptionsListWidgetItem *>(
                ui.queueListWidget->item(ui.queueListWidget->currentRow()));

        QString requestId = item_queue->GetRequestId();

        // notify schedulers
        int schedulersCount = ui.schedulers->count();
        for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
          QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
          if (auto scheduler =
                  qobject_cast<SchedulerWidget *>(schedulerWidget)) {
            scheduler->updateTaskStatus(requestId, "removed from the queue");

            if (scheduler->getSchedulerRequestId() == requestId) {
              mRunningSchedulersCount--;
              ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                         .arg(mSchedulersCount)
                                         .arg(mRunningSchedulersCount));
            }
          }
        }

        --mQueueCount;
        ui.queueListWidget->takeItem(ui.queueListWidget->currentRow());

        if (ui.queueListWidget->currentRow() == 0 ||
            ui.queueListWidget->currentRow() ==
                ui.queueListWidget->count() - 1) {
          ui.buttonDownQueue->setEnabled(false);
        } else {
          ui.buttonDownQueue->setEnabled(true);
        }

        if (ui.queueListWidget->currentRow() == 0 ||
            ui.queueListWidget->currentRow() == 1) {
          ui.buttonUpQueue->setEnabled(false);
        } else {
          ui.buttonUpQueue->setEnabled(true);
        }
      }
      if (ui.queueListWidget->count() == 1) {
        ui.buttonRemoveFromQueue->setEnabled(false);
      }
      if (mQueueCount == 0) {
        ui.tabs->setTabText(3, QString("Queue (%1)>>(0)").arg(mQueueCount));
      } else {

        if (!mQueueTaskRunning) {
          ui.tabs->setTabText(3, QString("Queue (%1)>>(0)").arg(mQueueCount));
        } else {
          ui.tabs->setTabText(3,
                              QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
        }
      }

    } else {

      --mQueueCount;

      JobOptionsListWidgetItem *item_queue =
          static_cast<JobOptionsListWidgetItem *>(
              ui.queueListWidget->item(ui.queueListWidget->currentRow()));

      QString requestId = item_queue->GetRequestId();

      // notify schedulers
      int schedulersCount = ui.schedulers->count();
      for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
        QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
        if (auto scheduler = qobject_cast<SchedulerWidget *>(schedulerWidget)) {
          scheduler->updateTaskStatus(requestId, "removed from the queue");

          if (scheduler->getSchedulerRequestId() == requestId) {
            mRunningSchedulersCount--;
            ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                       .arg(mSchedulersCount)
                                       .arg(mRunningSchedulersCount));
          }
        }
      }

      ui.queueListWidget->takeItem(ui.queueListWidget->currentRow());

      if (ui.queueListWidget->currentRow() == ui.queueListWidget->count() - 1) {
        ui.buttonDownQueue->setEnabled(false);
      } else {
        ui.buttonDownQueue->setEnabled(true);
      }
      if (ui.queueListWidget->currentRow() == 0) {
        ui.buttonUpQueue->setEnabled(false);
      } else {
        ui.buttonUpQueue->setEnabled(true);
      }

      if (ui.queueListWidget->count() == 0) {

        ui.buttonRemoveFromQueue->setEnabled(false);
        ui.buttonPurgeQueue->setEnabled(false);
        ui.buttonUpQueue->setEnabled(false);
        ui.buttonDownQueue->setEnabled(false);
        ui.tabs->setTabText(3, QString("Queue"));

      } else {
        ui.tabs->setTabText(3, QString("Queue (%1)").arg(mQueueCount));
      }
    }

    saveQueueFile();
  });

  QObject::connect(ui.actionDownQueue, &QAction::triggered, this, [=]() {
    auto item = ui.queueListWidget->takeItem(ui.queueListWidget->currentRow());
    ui.queueListWidget->insertItem(ui.queueListWidget->currentRow() + 1, item);
    ui.queueListWidget->setCurrentRow(ui.queueListWidget->row(item));

    saveQueueFile();
  });

  QObject::connect(ui.actionUpQueue, &QAction::triggered, this, [=]() {
    if (ui.queueListWidget->currentRow() == ui.queueListWidget->count() - 1) {
      // last row needs special treatment
      auto item =
          ui.queueListWidget->takeItem(ui.queueListWidget->currentRow());
      ui.queueListWidget->insertItem(ui.queueListWidget->currentRow(), item);
      ui.queueListWidget->setCurrentRow(ui.queueListWidget->row(item));
    } else {
      auto item =
          ui.queueListWidget->takeItem(ui.queueListWidget->currentRow());
      ui.queueListWidget->insertItem(ui.queueListWidget->currentRow() - 1,
                                     item);
      ui.queueListWidget->setCurrentRow(ui.queueListWidget->row(item));
    }

    saveQueueFile();
  });
}

void MainWindow::setupRestoredState() {
  auto settings = GetSettings();

  {
    restoreSchedulersFromFile();
    listTasks();
    addTasksToQueue();
  }

  if (!(settings->value("Settings/schedulerStatus").toBool())) {

    int schedulersCount = ui.schedulers->count();
    for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
      QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
      if (auto scheduler = qobject_cast<SchedulerWidget *>(schedulerWidget)) {

        scheduler->stopScheduler();
      }
    }

    ui.buttonStartScheduler->setEnabled(true);
    ui.buttonStopScheduler->setEnabled(false);

    ui.labelSchedulerInfoStart->hide();
    ui.labelSchedulerInfoStop->show();

    ui.tabs->setTabText(4, QString("Scheduler (%1)").arg(mSchedulersCount));
  }

  if ((settings->value("Settings/queueStatus").toBool())) {
    ui.actionStartQueue->trigger();
  }
}

void MainWindow::setTasksButtons() {
  auto items = ui.tasksListWidget->selectedItems();
  if (items.isEmpty()) {
    ui.buttonDeleteTask->setEnabled(false);
    ui.buttonEditTask->setEnabled(false);
    ui.buttonRunTask->setEnabled(false);
    ui.buttonDryrunTask->setEnabled(false);
    ui.buttonAddToQueue->setEnabled(false);
    ui.buttonAddToScheduler->setEnabled(false);
  } else {

    ui.buttonDeleteTask->setEnabled(true);
    ui.buttonEditTask->setEnabled(true);
    ui.buttonRunTask->setEnabled(true);
    ui.buttonDryrunTask->setEnabled(true);
    ui.buttonAddToQueue->setEnabled(true);
    ui.buttonAddToScheduler->setEnabled(true);
  }

  if (ui.tasksListWidget->count() > 1) {
    ui.buttonSortTask->setEnabled(true);
  } else {
    ui.buttonSortTask->setEnabled(false);
  }

  if (items.count() > 1) {
    ui.buttonEditTask->setEnabled(false);
  }

  bool isMount = false;
  bool isRunning = false;
  int isNotRunning = items.count();
  bool isScheduled = false;
  int schedulersCount = ui.schedulers->count();

  if (items.count() > 0) {

    foreach (auto i, items) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(i);
      JobOptions *jo = item->GetData();

      if (!isScheduled) {
        for (int i = schedulersCount - 2; i >= 0; i = i - 2) {
          QWidget *widget = ui.schedulers->itemAt(i)->widget();
          if (auto scheduler = qobject_cast<SchedulerWidget *>(widget)) {

            if (scheduler->getSchedulerTaskId() == jo->uniqueId.toString()) {

              isScheduled = true;
              break;
            }
          }
        }
      }

      if (jo->operation == JobOptions::Mount) {
        isMount = true;
      }

      int widgetsCount = ui.jobs->count();
      for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
        QWidget *widget = ui.jobs->itemAt(j)->widget();

        if (auto transfer = qobject_cast<JobWidget *>(widget)) {
          if ((transfer->getUniqueID() == jo->uniqueId.toString()) &&
              (transfer->isRunning)) {
            isRunning = true;
            --isNotRunning;
          } else {

            if ((transfer->getUniqueID() == jo->uniqueId.toString())) {
            }
          }
        }

        if (auto mount = qobject_cast<MountWidget *>(widget)) {
          if ((mount->getUniqueID() == jo->uniqueId.toString()) &&
              (mount->isRunning)) {
            isRunning = true;
            --isNotRunning;
          } else {

            if ((mount->getUniqueID() == jo->uniqueId.toString())) {
            }
          }
        }
      }
    }
  }

  if (isRunning) {
    ui.buttonDeleteTask->setEnabled(false);
    ui.buttonRunTask->setEnabled(false);
    ui.buttonDryrunTask->setEnabled(false);
  }

  if (isNotRunning == 0 && items.count() != 0) {
    ui.buttonStop->setEnabled(true);
  } else {
    ui.buttonStop->setEnabled(false);
  }

  if (isMount) {
    ui.buttonDryrunTask->setEnabled(false);
    ui.buttonAddToQueue->setEnabled(false);
    ui.buttonAddToScheduler->setEnabled(false);
  }

  if (isScheduled) {
    ui.buttonDeleteTask->setEnabled(false);
  }

  return;
}

void MainWindow::setQueueButtons() {

  if (ui.queueListWidget->selectedItems().empty()) {

    ui.buttonDownQueue->setEnabled(false);
    ui.buttonUpQueue->setEnabled(false);
    ui.buttonRemoveFromQueue->setEnabled(false);
    ui.actionDownQueue->setEnabled(false);
    ui.actionUpQueue->setEnabled(false);
    ui.actionRemoveFromQueue->setEnabled(false);

    if (mQueueStatus) {

      if (ui.queueListWidget->count() > 1) {
        ui.buttonPurgeQueue->setEnabled(true);
      } else {
        ui.buttonPurgeQueue->setEnabled(false);
      }

      if (ui.queueListWidget->count() > 0) {
        ui.queueListWidget->item(0)->setSelected(false);
      }

    } else {

      if (ui.queueListWidget->count() > 0) {
        ui.buttonPurgeQueue->setEnabled(true);
      } else {
        ui.buttonPurgeQueue->setEnabled(false);
      }
    }
    return;
  }

  if (mQueueStatus) {

    if (ui.queueListWidget->count() > 1) {
      ui.buttonPurgeQueue->setEnabled(true);
    } else {
      ui.buttonPurgeQueue->setEnabled(false);
    }

    if (ui.queueListWidget->currentRow() == 0 ||
        ui.queueListWidget->currentRow() == ui.queueListWidget->count() - 1) {
      ui.buttonDownQueue->setEnabled(false);
      ui.actionDownQueue->setEnabled(false);
    } else {
      ui.buttonDownQueue->setEnabled(true);
      ui.actionDownQueue->setEnabled(true);
    }

    if (ui.queueListWidget->currentRow() == 1 ||
        ui.queueListWidget->currentRow() == 0) {
      ui.buttonUpQueue->setEnabled(false);
      ui.actionUpQueue->setEnabled(false);
    } else {
      ui.buttonUpQueue->setEnabled(true);
      ui.actionUpQueue->setEnabled(true);
    }

    if (ui.queueListWidget->currentRow() == 0) {
      ui.buttonRemoveFromQueue->setEnabled(false);
      ui.actionRemoveFromQueue->setEnabled(false);
    } else {
      ui.buttonRemoveFromQueue->setEnabled(true);
      ui.actionRemoveFromQueue->setEnabled(true);
    }

    if (ui.queueListWidget->count() > 0) {
      ui.queueListWidget->item(0)->setSelected(false);
    }

  } else {

    if (ui.queueListWidget->count() > 0) {
      ui.buttonPurgeQueue->setEnabled(true);
    } else {
      ui.buttonPurgeQueue->setEnabled(false);
    }

    if (ui.queueListWidget->currentRow() == 0) {
      ui.buttonUpQueue->setEnabled(false);
      ui.actionUpQueue->setEnabled(false);
    } else {
      ui.buttonUpQueue->setEnabled(true);
      ui.actionUpQueue->setEnabled(true);
    }

    if (ui.queueListWidget->currentRow() == ui.queueListWidget->count() - 1) {
      ui.buttonDownQueue->setEnabled(false);
      ui.actionDownQueue->setEnabled(false);
    } else {
      ui.buttonDownQueue->setEnabled(true);
      ui.actionDownQueue->setEnabled(true);
    }

    if (ui.queueListWidget->count() > 0) {
      ui.buttonPurgeQueue->setEnabled(true);
    } else {
      ui.buttonPurgeQueue->setEnabled(false);
    }

    ui.buttonRemoveFromQueue->setEnabled(true);
    ui.actionRemoveFromQueue->setEnabled(true);
  }
}

void MainWindow::restoreSchedulersFromFile() {
  // make sure that tasks are already listed so we can cross check

  QString filePath = GetConfigDir().absoluteFilePath("scheduler.conf");
  QFile file(filePath);
  QTextStream in(&file);

  if (!file.open(QIODevice::ReadOnly)) {
    return;
  } else {

    while (!in.atEnd()) {

      QString line = in.readLine();

      // check if corresponding task exist?
      // get scheduler taskId
      QStringList args = line.split(",");
      QString schedulerTaskID = args.at(args.indexOf("mTaskId") + 1);

      ListOfJobOptions *ljo = ListOfJobOptions::getInstance();

      for (JobOptions *jo : ljo->getTasks()) {
        if (jo->uniqueId.toString() == schedulerTaskID) {
          mSchedulersCount++;
          addScheduler("", "", args);
        }
      }
    }

    file.close();
  }

  ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                             .arg(mSchedulersCount)
                             .arg(mRunningSchedulersCount));
}

void MainWindow::addTasksToQueue() {
  // restore queue from file
  // ignore no more existing

  ui.queueListWidget->clear();

  auto items = ui.tasksListWidget->selectedItems();

  ListOfJobOptions *ljo = ListOfJobOptions::getInstance();

  QString filePath = GetConfigDir().absoluteFilePath("queue.conf");
  QFile file(filePath);
  QTextStream in(&file);

  QString fileTaskId;
  QString fileRequestId;

  if (!file.open(QIODevice::ReadOnly)) {
    return;
  } else {

    QString taskNameDisplay;

    while (!in.atEnd()) {

      QString line = in.readLine();

      if (line.indexOf(",") == -1) {
        // old task file
        fileTaskId = line;
        fileRequestId = QUuid::createUuid().toString();

      } else {

        fileTaskId = line.left(line.indexOf(","));
        fileRequestId = line.right(line.length() - (line.indexOf(",") + 1));
      }

      for (JobOptions *jo : ljo->getTasks()) {

        QIcon jobIcon = mDownloadIcon;

        if (jo->jobType == JobOptions::JobType::Download) {
          if (jo->operation == JobOptions::Mount) {
            jobIcon = mMountIcon;
          } else {
            jobIcon = mDownloadIcon;
          }
        }
        if (jo->jobType == JobOptions::JobType::Upload) {
          jobIcon = mUploadIcon;
        }

        if (jo->operation == JobOptions::Mount) {
          if (jo->mountAutoStart) {
            taskNameDisplay = jo->description + "(autostart)";
          } else {
            taskNameDisplay = jo->description;
          }
        } else {
          taskNameDisplay = jo->description;
        }

        // check if task is from scheduler
        bool transferModeSch = false;
        int schedulersCount = ui.schedulers->count();
        for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
          QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
          if (auto scheduler =
                  qobject_cast<SchedulerWidget *>(schedulerWidget)) {
            if (fileRequestId == scheduler->getSchedulerRequestId()) {
              transferModeSch = true;
              scheduler->updateTaskStatus(fileRequestId, "in the queue");
            }
          }
        }

        if (jo->uniqueId.toString() == fileTaskId) {
          if (transferModeSch) {
            taskNameDisplay = taskNameDisplay + " (*Sch)";
            mRunningSchedulersCount++;
            ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                       .arg(mSchedulersCount)
                                       .arg(mRunningSchedulersCount));
          }

          JobOptionsListWidgetItem *item = new JobOptionsListWidgetItem(
              jo, jobIcon, taskNameDisplay, fileRequestId);

          ++mQueueCount;
          ui.queueListWidget->addItem(item);
        }
      }
    }

    file.close();

    if (mQueueCount == 0) {
      ui.tabs->setTabText(3, QString("Queue"));
    } else {
      ui.tabs->setTabText(3, QString("Queue (%1)").arg(mQueueCount));
    }

    ui.labelQueueInfoStop->setText("Queue is not running.");
    ui.labelQueueInfoStop->show();
    ui.labelQueueInfoStart->hide();
  }

  ui.buttonStopQueue->setEnabled(false);
  ui.buttonStartQueue->setEnabled(true);
  ui.buttonDownQueue->setEnabled(false);
  ui.buttonUpQueue->setEnabled(false);
  ui.buttonRemoveFromQueue->setEnabled(false);

  if (ui.queueListWidget->count() > 0) {
    ui.buttonPurgeQueue->setEnabled(true);
  } else {
    ui.buttonPurgeQueue->setEnabled(false);
  }
}

void MainWindow::listTasks() {

  ui.tasksListWidget->clear();

  // make tasks list flow (wrap) in its window
  ui.tasksListWidget->setViewMode(QListWidget::ListMode);
  ui.tasksListWidget->setResizeMode(QListView::Adjust);
  ui.tasksListWidget->setSpacing(5);
  ui.tasksListWidget->setWrapping(true);

  ui.queueListWidget->setViewMode(QListWidget::ListMode);
  ui.queueListWidget->setResizeMode(QListView::Adjust);
  ui.queueListWidget->setSpacing(5);
  ui.queueListWidget->setWrapping(true);

  // enable drag and drop reordering (there is no persistence of order
  // implemented yet)
  // ui.tasksListWidget->setDragDropMode(QAbstractItemView::InternalMove);

  auto settings = GetSettings();
  bool sortTask = settings->value("Settings/sortTask").toBool();
  if (sortTask) {
    //    ui.actionSortTask->setIcon(sortZAIcon);
    //    ui.buttonSortTask->setToolTip("Sort Descending");
    ui.tasksListWidget->setSortingEnabled(true);
    ui.tasksListWidget->sortItems(Qt::AscendingOrder);
  } else {
    //    ui.actionSortTask->setIcon(sortAZIcon);
    //    ui.buttonSortTask->setToolTip("Sort Ascending");
    ui.tasksListWidget->setSortingEnabled(true);
    ui.tasksListWidget->sortItems(Qt::DescendingOrder);
  }

  ListOfJobOptions *ljo = ListOfJobOptions::getInstance();

  for (JobOptions *jo : ljo->getTasks()) {

    QIcon jobIcon;
    QString taskNameDisplay;

    if (jo->jobType == JobOptions::JobType::Download) {
      if (jo->operation == JobOptions::Mount) {
        jobIcon = mMountIcon;
      } else {
        jobIcon = mDownloadIcon;
      }
    }
    if (jo->jobType == JobOptions::JobType::Upload) {
      jobIcon = mUploadIcon;
    }

    if (jo->operation == JobOptions::Mount) {
      if (jo->mountAutoStart) {
        taskNameDisplay = jo->description + " (*auto)";
      } else {
        taskNameDisplay = jo->description;
      }
    } else {
      taskNameDisplay = jo->description;
    }

    JobOptionsListWidgetItem *item = new JobOptionsListWidgetItem(
        jo, jobIcon, taskNameDisplay, "uniqueID_placeholder_task");
    ui.tasksListWidget->addItem(item);
  }

  ui.buttonDeleteTask->setEnabled(false);
  ui.buttonEditTask->setEnabled(false);
  ui.buttonRunTask->setEnabled(false);
  ui.buttonStop->setEnabled(false);
  ui.buttonDryrunTask->setEnabled(false);
  ui.buttonAddToQueue->setEnabled(false);
  ui.buttonAddToScheduler->setEnabled(false);

  if (ui.tasksListWidget->count() > 1) {
    ui.buttonSortTask->setEnabled(true);
  } else {
    ui.buttonSortTask->setEnabled(false);
  }

  // update ui.queueListWidget when task changed
  QString uniqueId_queue;
  QString uniqueId_task;
  bool itemFound = false;

  // for every item in queue
  if (ui.queueListWidget->count() > 0) {
    for (int i = 0; i < ui.queueListWidget->count(); i++) {

      ui.queueListWidget->item(i);

      JobOptionsListWidgetItem *item_queue =
          static_cast<JobOptionsListWidgetItem *>(ui.queueListWidget->item(i));
      JobOptions *jo_queue = item_queue->GetData();
      uniqueId_queue = jo_queue->uniqueId.toString();
      // preserve requestId
      QString requestId = item_queue->GetRequestId();

      // if no corresponding item found in the queue means task has been deleted
      // and have to be removed from the queue as well
      itemFound = false;

      if (ui.tasksListWidget->count() > 0) {

        // check if corresponding item in the task list
        for (int j = 0; j < ui.tasksListWidget->count(); j++) {

          JobOptionsListWidgetItem *item_task =
              static_cast<JobOptionsListWidgetItem *>(
                  ui.tasksListWidget->item(j));
          JobOptions *jo_task = item_task->GetData();

          uniqueId_task = jo_task->uniqueId.toString();

          // uniqueId never changes, name can be edited so we check Id
          if (uniqueId_queue == uniqueId_task) {
            itemFound = true;
            // update ui.queueListWidget

            ui.queueListWidget->takeItem(i);

            QIcon jobIcon = mDownloadIcon;

            if (jo_task->jobType == JobOptions::JobType::Download) {
              if (jo_task->operation == JobOptions::Mount) {
                jobIcon = mMountIcon;
              } else {
                jobIcon = mDownloadIcon;
              }
            }
            if (jo_task->jobType == JobOptions::JobType::Upload) {
              jobIcon = mUploadIcon;
            }

            // check if task is from scheduler
            bool transferModeSch = false;
            int schedulersCount = ui.schedulers->count();
            for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
              QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
              if (auto scheduler =
                      qobject_cast<SchedulerWidget *>(schedulerWidget)) {
                if (requestId == scheduler->getSchedulerRequestId()) {
                  transferModeSch = true;
                }
              }
            }

            QString taskNameDisplay = jo_task->description;
            if (transferModeSch) {

              taskNameDisplay = taskNameDisplay + " (*Sch)";
            }

            JobOptionsListWidgetItem *item_insert =
                new JobOptionsListWidgetItem(jo_task, jobIcon, taskNameDisplay,
                                             requestId);

            ui.queueListWidget->insertItem(i, item_insert);

            if (i == 0 && mQueueStatus) {
              ui.queueListWidget->item(0)->setBackground(Qt::darkGreen);
            }
          }
        } // for j
      }

      // remove item from ui.queueListWidget if removed from tasks
      if (mQueueStatus) {
        // running queue
        if (!itemFound) {
          // only if already running leave it
          // never should happen - as not possible to delete already running
          if (i != 0) {
            --mQueueCount;
            ui.queueListWidget->takeItem(i);
            if (mQueueCount == 0) {
              ui.tabs->setTabText(3,
                                  QString("Queue (%1)>>(0)").arg(mQueueCount));
            } else {
              ui.tabs->setTabText(
                  3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
            }
          }
        }

      } else {
        // stopped queue
        if (!itemFound) {
          --mQueueCount;
          ui.queueListWidget->takeItem(i);
          if (mQueueCount == 0) {
            ui.tabs->setTabText(3, QString("Queue"));
          } else {
            ui.tabs->setTabText(3, QString("Queue (%1)").arg(mQueueCount));
          }
        }

      } // mQueueStatus
    }   // for i

    saveQueueFile();
  }

  // update schedulers tasks names and add "(*Sch)" to tasks' names

  // loop over all tasks
  for (int i = 0; i < ui.tasksListWidget->count(); i = i + 1) {
    JobOptionsListWidgetItem *item =
        static_cast<JobOptionsListWidgetItem *>(ui.tasksListWidget->item(i));
    JobOptions *joTasks = item->GetData();

    // loop over all schedulers
    int schedulersCount = ui.schedulers->count();
    for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
      QWidget *widget = ui.schedulers->itemAt(j)->widget();

      if (auto scheduler = qobject_cast<SchedulerWidget *>(widget)) {

        if (joTasks->uniqueId.toString() == scheduler->getSchedulerTaskId()) {

          // update task name in scheduler

          scheduler->updateTaskName(joTasks->description);

          // update task name in tasks list + " (scheduled)"
          ui.tasksListWidget->takeItem(i);

          QIcon jobIcon = mDownloadIcon;

          if (joTasks->jobType == JobOptions::JobType::Download) {
            if (joTasks->operation == JobOptions::Mount) {
              jobIcon = mMountIcon;
            } else {
              jobIcon = mDownloadIcon;
            }
          }
          if (joTasks->jobType == JobOptions::JobType::Upload) {
            jobIcon = mUploadIcon;
          }

          JobOptionsListWidgetItem *item_insert = new JobOptionsListWidgetItem(
              joTasks, jobIcon, joTasks->description + " (*Sch)",
              "placeholder");

          ui.tasksListWidget->insertItem(i, item_insert);
        }
      }
    }
  }

  // restore active tasks colours
  int widgetsCount = ui.jobs->count();
  for (int k = 0; k < ui.tasksListWidget->count(); k = k + 1) {
    JobOptionsListWidgetItem *item =
        static_cast<JobOptionsListWidgetItem *>(ui.tasksListWidget->item(k));

    JobOptions *joTasks = item->GetData();

    for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
      QWidget *widget = ui.jobs->itemAt(j)->widget();

      if (auto transfer = qobject_cast<JobWidget *>(widget)) {
        if ((transfer->getUniqueID() == joTasks->uniqueId.toString()) &&
            (transfer->isRunning)) {
          ui.tasksListWidget->item(k)->setBackground(Qt::darkGreen);
        }
      }

      if (auto mount = qobject_cast<MountWidget *>(widget)) {
        if ((mount->getUniqueID() == joTasks->uniqueId.toString()) &&
            (mount->isRunning)) {
          ui.tasksListWidget->item(k)->setBackground(Qt::darkGreen);
        }
      }
    }
  }

  ui.queueListWidget->setFocus();
  setQueueButtons();
  ui.tasksListWidget->setFocus();
  setTasksButtons();
  saveSchedulerFile();

} // MainWindow::listTasks()

void MainWindow::runItem(JobOptionsListWidgetItem *item,
                         const QString &transferMode, const QString &requestId,
                         bool dryrun) {

  QMutexLocker locker(&mRunItemMutex);

  if (item == nullptr)
    return;

  // if more than n jobs let's delete some old inactive one

  int widgetsCount = ui.jobs->count();

  if ((widgetsCount - 2) / 2 > 75) {

    // find the oldest inactive job

    QDateTime dt = QDateTime::currentDateTime();
    QDateTime widgetStartDateTime = dt;
    int remove = 0;
    bool foundOldest = false;

    for (int i = widgetsCount - 2; i >= 0; i = i - 2) {

      QWidget *widget = ui.jobs->itemAt(i)->widget();

      if (auto transfer = qobject_cast<JobWidget *>(widget)) {

        if (!(transfer->isRunning)) {
          widgetStartDateTime = transfer->getStartDateTime();
          foundOldest = true;
        }

      } else if (auto mount = qobject_cast<MountWidget *>(widget)) {
        if (!(mount->isRunning)) {
          widgetStartDateTime = mount->getStartDateTime();
          foundOldest = true;
        }
      } else if (auto stream = qobject_cast<StreamWidget *>(widget)) {
        if (!(stream->isRunning)) {
          widgetStartDateTime = stream->getStartDateTime();
          foundOldest = true;
        }
      }

      if (dt > widgetStartDateTime) {
        dt = widgetStartDateTime;
        remove = i;
      }

    } //  for (int i

    if (foundOldest) {
      // remove the oldest from jobs
      QWidget *widget = ui.jobs->itemAt(remove)->widget();

      if (auto transfer = qobject_cast<JobWidget *>(widget)) {
        emit transfer->closed();
      } else if (auto mount = qobject_cast<MountWidget *>(widget)) {
        emit mount->closed();
      } else if (auto stream = qobject_cast<StreamWidget *>(widget)) {
        emit stream->closed();
      } else {
      }
    }
  }

  JobOptions *jo = item->GetData();

  // running items have darkGreen background
  for (int k = 0; k < ui.tasksListWidget->count(); k = k + 1) {
    JobOptionsListWidgetItem *item =
        static_cast<JobOptionsListWidgetItem *>(ui.tasksListWidget->item(k));
    JobOptions *joTasks = item->GetData();
    if (joTasks->uniqueId.toString() == jo->uniqueId.toString()) {
      ui.tasksListWidget->item(k)->setBackground(Qt::darkGreen);
    }
  }

  if (jo->operation != JobOptions::Mount) {
    // transfer
    jo->dryRun = dryrun;
    QStringList args = jo->getOptions();

    QString info;
    QString operation;

    if (int(jo->operation) == 1)
      operation = "Copy";
    if (int(jo->operation) == 2)
      operation = "Move";
    if (int(jo->operation) == 3)
      operation = "Sync";

    if (dryrun) {
      info = QString("Dry run, task: \"%1\", %2 from %3")
                 .arg(jo->description)
                 .arg(operation.toLower())
                 .arg(jo->source);
    } else {
      if (transferMode == "queue") {
        info = QString("Queue task: \"%1\", %2 from %3")
                   .arg(jo->description)
                   .arg(operation)
                   .arg(jo->source);
      } else {

        info = QString("Task: \"%1\", %2 from %3")
                   .arg(jo->description)
                   .arg(operation)
                   .arg(jo->source);
      }
    }

    // check if run item is from scheduler

    bool transferModeSch = false;
    int schedulersCount = ui.schedulers->count();
    for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
      QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
      if (auto scheduler = qobject_cast<SchedulerWidget *>(schedulerWidget)) {
        if (requestId == scheduler->getSchedulerRequestId()) {
          transferModeSch = true;
        }
      }
    }

    if (transferMode == "scheduler" || transferModeSch) {

      // check if not already running
      int widgetsCount = ui.jobs->count();
      bool alreadyRunning = false;
      for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
        QWidget *transferWidget = ui.jobs->itemAt(j)->widget();
        if (auto transfer = qobject_cast<JobWidget *>(transferWidget)) {
          if ((transfer->getUniqueID() == jo->uniqueId.toString()) &&
              (transfer->isRunning)) {
            alreadyRunning = true;
            break;
          }
        }
      }

      QString schedulerTaskStatus;
      if (alreadyRunning) {
        schedulerTaskStatus = "already running";
        mRunningSchedulersCount--;
        ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                   .arg(mSchedulersCount)
                                   .arg(mRunningSchedulersCount));

      } else {
        info = QString("Scheduled task: \"%1\", %2 from %3")
                   .arg(jo->description)
                   .arg(operation)
                   .arg(jo->source);
        addTransfer(info, jo->source, jo->dest, args, jo->uniqueId.toString(),
                    transferMode, requestId);
        schedulerTaskStatus = "running";
      }

      // notify schedulers
      int schedulersCount = ui.schedulers->count();
      for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
        QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
        if (auto scheduler = qobject_cast<SchedulerWidget *>(schedulerWidget)) {
          scheduler->updateTaskStatus(requestId, schedulerTaskStatus);
        }
      }

    } else {

      addTransfer(info, jo->source, jo->dest, args, jo->uniqueId.toString(),
                  transferMode, requestId);
    }

  } else {
    // mount

    QStringList args;

    args << "mount";

    args << jo->source;

    args << jo->dest;

    if (!jo->mountRcPort.isEmpty()) {

      args << "--rc";
      args << "--rc-addr";
      args << "localhost:" + jo->mountRcPort;
    }
    if (jo->remoteType == "drive") {
      if (jo->remoteMode == "shared") {
        args << "--drive-shared-with-me";
        if (!jo->mountReadOnly) {
          args << "--read-only";
        }
      }
      if (jo->remoteMode == "trash") {
        args << "--drive-trashed-only";
      }
    }

    if (jo->mountReadOnly) {
      args << "--read-only";
    }

    if (!(jo->mountVolume.trimmed().isEmpty())) {
      args << "--volname";
      args << jo->mountVolume;
    }

    switch (jo->mountCacheLevel) {
    case JobOptions::MountCacheLevel::Off:
      break;
    case JobOptions::MountCacheLevel::Minimal:
      args << "--vfs-cache-mode";
      args << "minimal";
      break;
    case JobOptions::MountCacheLevel::Writes:
      args << "--vfs-cache-mode";
      args << "writes";
      break;
    case JobOptions::MountCacheLevel::Full:
      args << "--vfs-cache-mode";
      args << "full";
      break;
    case JobOptions::MountCacheLevel::UnknownCacheLevel:
      break;
    }

    if (!jo->extra.trimmed().isEmpty()) {
      for (auto line : jo->extra.trimmed().split('\n')) {
        if (!line.isEmpty()) {
          for (QString arg :
               line.split(QRegularExpression(" (?=[^\"]*(\"[^\"]*\"[^\"]*)*$)"))) {
            if (!arg.isEmpty()) {
              args << arg.replace("\"", "");
            }
          }
        }
      }
    }

    addNewMount(jo->source, jo->dest, jo->remoteType, args, jo->mountScript,
                jo->uniqueId.toString(), "Mounting task: " + jo->description);
  }
}

void MainWindow::editSelectedTask() {

  auto items = ui.tasksListWidget->selectedItems();

  // remember which item we edit
  int selection = (ui.tasksListWidget->selectionModel()->currentIndex()).row();

  foreach (auto i, items) {

    JobOptionsListWidgetItem *item = static_cast<JobOptionsListWidgetItem *>(i);

    JobOptions *jo = item->GetData();
    bool isDownload = (jo->jobType == JobOptions::Download);
    QString remoteType = (jo->remoteType);
    QString remoteMode = (jo->remoteMode);

    QString jobType = "";

    if (jo->jobType == JobOptions::Download) {
      if (jo->operation == JobOptions::Mount) {
        jobType = "Mount";
      } else {
        jobType = "Download";
      }
    }
    if (jo->jobType == JobOptions::Upload) {
      jobType = "Upload";
    }

    QString remote = isDownload ? jo->source : jo->dest;
    QString path = isDownload ? jo->dest : jo->source;
    // qDebug() << "remote:" + remote;
    // qDebug() << "path:" + path;

    if (jobType == "Download" || jobType == "Upload") {

      QStringList empty;
      TransferDialog td(isDownload, false, remote, path, jo->isFolder,
                        remoteType, remoteMode, false, empty, this, jo, true);
      td.exec();

    } else {
      if (jobType == "Mount") {
        MountDialog md(remote, path, remoteType, remoteMode, this, jo, true);
        md.exec();
      }
    }
  }

  // restore the selection to help user keep track of what s/he was doing
  ui.tasksListWidget->setCurrentItem(ui.tasksListWidget->item(selection));
  ui.tasksListWidget->setFocus();
  setTasksButtons();

  // restore active tasks colours
  int widgetsCount = ui.jobs->count();
  for (int k = 0; k < ui.tasksListWidget->count(); k = k + 1) {
    JobOptionsListWidgetItem *item =
        static_cast<JobOptionsListWidgetItem *>(ui.tasksListWidget->item(k));

    JobOptions *joTasks = item->GetData();

    for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
      QWidget *widget = ui.jobs->itemAt(j)->widget();

      if (auto transfer = qobject_cast<JobWidget *>(widget)) {
        if ((transfer->getUniqueID() == joTasks->uniqueId.toString()) &&
            (transfer->isRunning)) {
          ui.tasksListWidget->item(k)->setBackground(Qt::darkGreen);
        }
      }

      if (auto mount = qobject_cast<MountWidget *>(widget)) {
        if ((mount->getUniqueID() == joTasks->uniqueId.toString()) &&
            (mount->isRunning)) {
          ui.tasksListWidget->item(k)->setBackground(Qt::darkGreen);
        }
      }
    }
  }

  // edit mode on the TransferDialog suppresses the usual Accept buttons
  // and the Save Task button closes it... so there is nothing more to do here
}

bool MainWindow::saveQueueFile(void) {

  QMutexLocker locker(&mSaveQueueFileMutex);
  QString filePath = GetConfigDir().absoluteFilePath("queue.conf");

#if QT_VERSION < QT_VERSION_CHECK(5, 1, 0)
  QFile file(filePath);
#else
  QSaveFile file(filePath);
#endif

  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }

  QTextStream out(&file);

  // loop over ui.queueListWidget
  for (int i = 0; i < ui.queueListWidget->count(); ++i) {
    QListWidgetItem *item = ui.queueListWidget->item(i);

    JobOptionsListWidgetItem *jobItem =
        static_cast<JobOptionsListWidgetItem *>(item);

    JobOptions *jo = jobItem->GetData();

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 1)
    out << jo->uniqueId.toString() << "," << jobItem->GetRequestId()
        << Qt::endl;
#else
    out << jo->uniqueId.toString() << "," << jobItem->GetRequestId() << endl;
#endif
  }

  out.flush();

#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
  return out.status() == QTextStream::Ok && file.commit();
#else
  return out.status() == QTextStream::Ok;
#endif
}

bool MainWindow::saveSchedulerFile(void) {

  QMutexLocker locker(&mSaveSchedulerFileMutex);
  QString filePath = GetConfigDir().absoluteFilePath("scheduler.conf");

#if QT_VERSION < QT_VERSION_CHECK(5, 1, 0)
  QFile file(filePath);
#else
  QSaveFile file(filePath);
#endif

  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  };

  QTextStream out(&file);
  int schedulersCount = ui.schedulers->count();

  for (int i = schedulersCount - 2; i >= 0; i = i - 2) {
    QWidget *widget = ui.schedulers->itemAt(i)->widget();
    if (auto scheduler = qobject_cast<SchedulerWidget *>(widget)) {
      QStringList args = scheduler->getSchedulerParameters();

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 1)
      out << args.join(",") << Qt::endl;
#else
      out << args.join(",") << endl;
#endif
    }
  }

  out.flush();

#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
  return out.status() == QTextStream::Ok && file.commit();
#else
  return out.status() == QTextStream::Ok;
#endif
}

void MainWindow::addSavedTransfer(const QString &uniqueId, bool dryRun,
                                  bool addToQueue) {

  QMutexLocker locker(&mMutex);

  // keep for future use
  if (dryRun) {
  }

  // find task based on taskID
  for (int k = 0; k < ui.tasksListWidget->count(); k = k + 1) {
    JobOptionsListWidgetItem *item =
        static_cast<JobOptionsListWidgetItem *>(ui.tasksListWidget->item(k));
    JobOptions *joTask = item->GetData();

    if (uniqueId == joTask->uniqueId.toString()) {

      if (!addToQueue) {

        // run immediately
        runItem(item, "task", QUuid::createUuid().toString(), false);
        break;
      } else {
        // add to queue

        bool isQueueEmpty = (ui.queueListWidget->count() == 0);

        QIcon jobIcon;

        if (joTask->jobType == JobOptions::JobType::Download) {
          if (joTask->operation == JobOptions::Mount) {
            jobIcon = mMountIcon;
          } else {
            jobIcon = mDownloadIcon;
          }
        }
        if (joTask->jobType == JobOptions::JobType::Upload) {
          jobIcon = mUploadIcon;
        }

        JobOptionsListWidgetItem *newitem =
            new JobOptionsListWidgetItem(joTask, jobIcon, joTask->description,
                                         QUuid::createUuid().toString());

        ui.queueListWidget->addItem(newitem);
        mQueueCount = mQueueCount + 1;

        // if queue was empty we start first taks if queue is running and
        // there is no other transfer job running
        if (mQueueStatus && isQueueEmpty && (mTransferJobCount == 0)) {

          if (mQueueCount > 0) {

            JobOptionsListWidgetItem *item =
                static_cast<JobOptionsListWidgetItem *>(
                    ui.queueListWidget->item(0));

            runItem(item, "scheduler", item->GetRequestId());
            ui.queueListWidget->item(0)->setBackground(Qt::darkGreen);
            mQueueTaskRunning = true;
            ui.tabs->setTabText(
                3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
          }

        } else {

          if (mQueueStatus) {

            if (mQueueCount == 0) {
              ui.tabs->setTabText(3,
                                  QString("Queue (%1)>>(0)").arg(mQueueCount));
            } else {
              if (!mQueueTaskRunning) {
                ui.tabs->setTabText(
                    3, QString("Queue (%1)>>(0)").arg(mQueueCount));
              } else {
                ui.tabs->setTabText(
                    3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
              }
            }
          } else {
            if (mQueueCount == 0) {
              ui.tabs->setTabText(3, QString("Queue"));
            } else {
              ui.tabs->setTabText(3, QString("Queue (%1)").arg(mQueueCount));
            }
          }
        }
        saveQueueFile();
        break;
      }
    }
  }
}

//  runs Script
void MainWindow::runScript(const QString &script) {

  QProcess *p = new QProcess();

  QObject::connect(p,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, [=](int code, QProcess::ExitStatus) {
                     if (code == 0) {
                     }
                     p->deleteLater();
                   });

  QStringList scriptList;

  for (QString arg : script.split(QRegularExpression(" (?=[^\"]*(\"[^\"]*\"[^\"]*)*$)"))) {
    if (!arg.isEmpty()) {
      scriptList << arg.replace("\"", "");
    }
  }

  QString scriptCmd = scriptList.takeAt(0);
  QStringList scriptArgs = scriptList;

  p->start(scriptCmd, scriptArgs, QIODevice::ReadOnly);
}

void MainWindow::addScheduler(const QString &taskId, const QString &taskName,
                              const QStringList &args) {

  if (ui.schedulers->count() == 2) {
    ui.noSchedulesAvailable->hide();
  }

  auto widget = new SchedulerWidget(taskId, taskName, args);

  auto line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  QObject::connect(widget, &SchedulerWidget::closed, this, [=]() {
    ui.schedulers->removeWidget(widget);
    ui.schedulers->removeWidget(line);
    widget->deleteLater();
    delete line;

    mSchedulersCount--;

    auto settings = GetSettings();
    if ((settings->value("Settings/schedulerStatus").toBool())) {

      ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                 .arg(mSchedulersCount)
                                 .arg(mRunningSchedulersCount));
    } else {

      ui.tabs->setTabText(4, QString("Scheduler (%1)").arg(mSchedulersCount));
    }

    saveSchedulerFile();
    if (ui.schedulers->count() == 2) {
      ui.noSchedulesAvailable->show();
    }
    QTimer::singleShot(0, this, SLOT(listTasks()));
  });

  QObject::connect(widget, &SchedulerWidget::save, this,
                   [=]() { saveSchedulerFile(); });

  QObject::connect(widget, &SchedulerWidget::stopTask, this, [=]() {
    QMutexLocker locker(&mStopTaskMutex);
    mDoNotSort = true;
    QString requestID = widget->getSchedulerRequestId();

    // stop if running
    int widgetsCount = ui.jobs->count();
    for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
      QWidget *widget = ui.jobs->itemAt(j)->widget();

      if (auto transfer = qobject_cast<JobWidget *>(widget)) {

        if (requestID == transfer->getRequestId()) {

          if (transfer->isRunning) {
            emit transfer->cancel();

            //               mRunningSchedulersCount--;
            ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                       .arg(mSchedulersCount)
                                       .arg(mRunningSchedulersCount));

            if (ui.queueListWidget->count() > 0) {
              for (int i = 0; i < ui.queueListWidget->count(); i++) {

                JobOptionsListWidgetItem *item_queue =
                    static_cast<JobOptionsListWidgetItem *>(
                        ui.queueListWidget->item(i));

                if (item_queue->GetRequestId() == requestID) {

                  ui.queueListWidget->takeItem(i);

                  break;
                }
              }
            }
          }
        }
      }
    }

    // remove from the queue
    // for every item in queue
    if (ui.queueListWidget->count() > 0) {
      for (int i = 0; i < ui.queueListWidget->count(); i++) {

        JobOptionsListWidgetItem *item_queue =
            static_cast<JobOptionsListWidgetItem *>(
                ui.queueListWidget->item(i));

        if (item_queue->GetRequestId() == requestID) {

          --mQueueCount;
          ui.queueListWidget->takeItem(i);
          widget->updateTaskStatus(requestID, "removed from the queue");
          mRunningSchedulersCount--;
          ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                     .arg(mSchedulersCount)
                                     .arg(mRunningSchedulersCount));

          break;
        }
      }

      if (mQueueStatus) {

        if (mQueueCount == 0) {
          ui.tabs->setTabText(3, QString("Queue (%1)>>(0)").arg(mQueueCount));
        } else {

          if (!mQueueTaskRunning) {
            ui.tabs->setTabText(3, QString("Queue (%1)>>(0)").arg(mQueueCount));
          } else {
            ui.tabs->setTabText(
                3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
          }
        }
      } else {
        if (mQueueCount == 0) {
          ui.tabs->setTabText(3, QString("Queue"));
        } else {
          ui.tabs->setTabText(3, QString("Queue (%1)").arg(mQueueCount));
        }
      }
    }
    mDoNotSort = false;
    sortJobs();
  });

  QObject::connect(widget, &SchedulerWidget::editTask, this, [=]() {
    QString taskID = widget->getSchedulerTaskId();

    for (int k = 0; k < ui.tasksListWidget->count(); k = k + 1) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(ui.tasksListWidget->item(k));
      JobOptions *joTasks = item->GetData();

      if (taskID == joTasks->uniqueId.toString()) {

        bool isDownload = (joTasks->jobType == JobOptions::Download);
        QString remoteType = (joTasks->remoteType);
        QString remoteMode = (joTasks->remoteMode);

        QString remote = isDownload ? joTasks->source : joTasks->dest;
        QString path = isDownload ? joTasks->dest : joTasks->source;

        QStringList empty;
        TransferDialog td(isDownload, false, remote, path, joTasks->isFolder,
                          remoteType, remoteMode, false, empty, this, joTasks,
                          true);
        td.exec();
        break;
      }
    }
  });

  QObject::connect(widget, &SchedulerWidget::runTask, this, [=]() {
    QMutexLocker locker(&mMutex);
    mDoNotSort = true;
    // when quitting (waiting for unmount) don't start new tasks
    if (mAppQuittingStatus) {
      mDoNotSort = false;
      return;
    }

    QString taskID = widget->getSchedulerTaskId();
    QString requestID = widget->getSchedulerRequestId();
    int executionMode = widget->getExecutionMode();

    // find task based on taskID
    for (int k = 0; k < ui.tasksListWidget->count(); k = k + 1) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(ui.tasksListWidget->item(k));
      JobOptions *joTask = item->GetData();

      if (taskID == joTask->uniqueId.toString()) {

        if (executionMode == 0) {
          // run immediately
          mRunningSchedulersCount++;
          ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                     .arg(mSchedulersCount)
                                     .arg(mRunningSchedulersCount));

          runItem(item, "scheduler", requestID);
        }

        if (executionMode == 1) {
          // add to the queue

          // if the same task is already in the queue we skip adding another one
          int queueCount = ui.queueListWidget->count();
          for (int i = 0; i < queueCount; i++) {
            JobOptionsListWidgetItem *item_queue =
                static_cast<JobOptionsListWidgetItem *>(
                    ui.queueListWidget->item(i));

            JobOptions *jo_queue = item_queue->GetData();
            QString uniqueId_queue = jo_queue->uniqueId.toString();
            if (taskID == uniqueId_queue) {
              widget->updateTaskStatus(requestID, "task already in the queue");
              mDoNotSort = false;
              return;
            }
          }

          bool isQueueEmpty = (ui.queueListWidget->count() == 0);

          QIcon jobIcon;

          if (joTask->jobType == JobOptions::JobType::Download) {
            if (joTask->operation == JobOptions::Mount) {
              jobIcon = mMountIcon;
            } else {
              jobIcon = mDownloadIcon;
            }
          }
          if (joTask->jobType == JobOptions::JobType::Upload) {
            jobIcon = mUploadIcon;
          }

          JobOptionsListWidgetItem *newitem = new JobOptionsListWidgetItem(
              joTask, jobIcon, joTask->description + " (*Sch)", requestID);

          ui.queueListWidget->addItem(newitem);
          mQueueCount = mQueueCount + 1;

          widget->updateTaskStatus(requestID, "in the queue");
          mRunningSchedulersCount++;
          ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                     .arg(mSchedulersCount)
                                     .arg(mRunningSchedulersCount));

          // if queue was empty we start first taks if queue is running and
          // there is no other transfer job running
          if (mQueueStatus && isQueueEmpty && (mTransferJobCount == 0)) {

            if (mQueueCount > 0) {

              JobOptionsListWidgetItem *item =
                  static_cast<JobOptionsListWidgetItem *>(
                      ui.queueListWidget->item(0));

              runItem(item, "scheduler", item->GetRequestId());
              ui.queueListWidget->item(0)->setBackground(Qt::darkGreen);
              mQueueTaskRunning = true;
              ui.tabs->setTabText(
                  3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
            }

          } else {

            if (mQueueStatus) {

              if (mQueueCount == 0) {
                ui.tabs->setTabText(
                    3, QString("Queue (%1)>>(0)").arg(mQueueCount));
              } else {
                if (!mQueueTaskRunning) {
                  ui.tabs->setTabText(
                      3, QString("Queue (%1)>>(0)").arg(mQueueCount));
                } else {
                  ui.tabs->setTabText(
                      3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
                }
              }
            } else {
              if (mQueueCount == 0) {
                ui.tabs->setTabText(3, QString("Queue"));
              } else {
                ui.tabs->setTabText(3, QString("Queue (%1)").arg(mQueueCount));
              }
            }
          }
          saveQueueFile();
        }
      }
    }
    mDoNotSort = false;
    sortJobs();
  });

  ui.schedulers->insertWidget(0, widget);
  ui.schedulers->insertWidget(1, line);
}
