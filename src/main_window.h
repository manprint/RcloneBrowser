#pragma once
#include "icon_cache.h"
#include "job_options.h"
#include "pch.h"
#include "ui_main_window.h"
#ifdef Q_OS_MACOS
#include "mac_os_power_saving.h"
#endif

class JobWidget;
class RcloneDaemon;
class ServeManager;
class ServesWindow;
class UpdateChecker;
struct RcloneVersion;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();
  ~MainWindow();

public slots:
  // asks for the config password (one prompt at a time) and unlocks the rc
  // daemon; reloads the open remote views once the config is readable
  void ensureRcUnlocked();

private slots:
  void rcloneGetVersion();
  void rcloneConfig();
  // native remote dialog (Create) when the rc daemon runs, else rcloneConfig()
  void openRemoteConfig();
  // native remote dialog (Update mode) for `name`
  void editRemote(const QString &name);
  // context menu of the remotes list: Edit remote and Advanced (terminal)
  void showRemotesMenu(const QPoint &pos);
  // the window of the running serves
  void showServes();
  // asks how to serve `fs` ("remote:path") and starts it
  void openServeDialog(const QString &fs);
  void rcloneListRemotes();
  void listTasks();

  void addTransfer(const QString &message, const QString &source,
                   const QString &dest, const QStringList &args,
                   const QString &uniqueId, const QString &transferMode,
                   const QString &requestId);
  void addStream(const QString &remote, const QString &stream,
                 const QString &remoteType);

  void addNewMount(const QString &remote, const QString &folder,
                   const QString &remoteType, const QStringList &args,
                   const QString &script, const QString &uniqueId,
                   const QString &info);

  void addScheduler(const QString &taskId, const QString &taskName,
                    const QStringList &args);

  void addSavedTransfer(const QString &uniqueId, bool dryRun, bool addToQueue);

  void runScript(const QString &script);

  void slotCloseTab(int index);

  bool saveQueueFile(void);
  bool saveSchedulerFile(void);

  void autoStartMounts(void);

  // quit RB but only when all processes finished
  void quitApp(void);

private:
  void showUnsupportedRcloneDialog(const QString &problem);
  // A non-blocking box (rcloneProblemBox, one at a time) that says why rclone
  // cannot be used and offers Preferences, where the path can be changed
  // (#145, #159, #209). Also logs "rclone problem: <text> <path>".
  void showRclonePathProblem(const QString &text);
  void startUpdateChecks(const RcloneVersion &rcloneVersion);

  // stops the rc daemon and waits for it (4 s at most); no-op when not running
  void stopDaemonAndWait();
  // gives GetRclonePassword() to a running daemon, ignoring the reply
  void unlockDaemonSilently();
  // one round of the ensureRcUnlocked() prompt loop
  void promptRcUnlock(bool wrongPassword);
  // both are created on first use, as children of this window
  ServeManager *serveManager();
  ServesWindow *servesWindow();

  // the constructor body, split in contiguous blocks that run in this order
  void setupPowerSavingAndTitle();
  void setupDaemonAndUpdateChecks();
  void setupWindowAndTheme();
  void setupSettingsDefaults();
  void setupActions();
  void setupInitialStatus();
  void setupPreferences();
  void setupTabClosure();
  void setupMenuActions();
  void setupRemotesTab();
  void setupTasksTab();
  void setupQueueContextMenu();
  void setupJobsTab();
  void setupTasksActions();
  void setupScheduler();
  void setupJobsSorting();
  void setupSchedulerAdd();
  void setupQueue();
  void setupQueueEditing();
  void setupListSignalsAndIcons();
  void setupTabs();
  void setupShortcuts();
  void setupRestoredState();
  void setupTrayAndStartup();

  Ui::MainWindow ui;

  QSystemTrayIcon mSystemTray;
  // Qt 6 removed QSound; QSoundEffect replaces it for the finished-transfer
  // notification chime (only when the Multimedia module is available).
#ifdef RB_HAVE_MULTIMEDIA
  QSoundEffect mNotificationSound;
#endif
  JobWidget *mLastFinished = nullptr;

  // private rclone rcd (also in global.daemon); a child of this window
  RcloneDaemon *mDaemon = nullptr;
  // ensureRcUnlocked() prompt loop is active
  bool mRcUnlockBusy = false;
  ServeManager *mServeManager = nullptr;
  ServesWindow *mServesWindow = nullptr;

  bool mAlwaysShowInTray;
  bool mCloseToTray;
  bool mNotifyFinishedTransfers;
  bool mSoundNotif;

  QLabel *mStatusMessage;
  QPointer<QMessageBox> mRcloneProblemBox;

  IconCache mIcons;

  bool mFirstTime = true;
  UpdateChecker *mRcloneUpdateChecker = nullptr;
  UpdateChecker *mBrowserUpdateChecker = nullptr;
  int mJobCount = 0;

  // keep track of number of active transfers
  int mTransferJobCount = 0;

  // false = Queue Paused, true = Queue running
  bool mQueueStatus = false;

  // number of schedulers
  int mSchedulersCount = 0;
  int mRunningSchedulersCount = 0;

  // number of queued tasks
  int mQueueCount = 0;
  // is queued task running
  bool mQueueTaskRunning = false;

  // make queue logic aware that app is quiting
  // so job is not removed from the queue
  bool mAppQuittingStatus = false;

  // don't sort then stopping all transfers
  bool mDoNotSort = false;

  bool canClose();
  void closeEvent(QCloseEvent *ev) override;
  // What getConfigPassword() made of a failed rclone run that read the config
  enum class ConfigPasswordResult {
    Retry,             // a password was entered: run rclone again
    Cancelled,         // the prompt was cancelled, or the problem was shown
    NotPasswordProblem // rclone failed for another reason, nothing was shown
  };
  ConfigPasswordResult getConfigPassword(int exitCode,
                                         const QByteArray &stderrBytes);

  // sort QListWidget view/selection
  QList<QListWidgetItem *> sortListWidget(const QList<QListWidgetItem *> &list,
                                          bool sortOrder = false);

  // set screen buttons logic mess in one place
  void setQueueButtons(void);
  void setTasksButtons(void);
  // hides the tasks the Tasks filter does not match (#21)
  void applyTasksFilter();

  void addEmptyJobsMessage();

  void runItem(JobOptionsListWidgetItem *item, const QString &transferMode,
               const QString &requestId, bool dryrun = false);
  // clipboard: the command Run would start, one line per selected task
  void copySelectedTaskCommands();
  // the rclone arguments of a mount task (runItem), and the same plus the
  // default mount options and --config (addNewMount)
  static QStringList mountTaskArgs(const JobOptions &jo);
  static QStringList mountArgsWithDefaults(const QStringList &args);
  void editSelectedTask();
  // #139: a copy of the selected task with a new id and "<description> (copy)"
  void duplicateSelectedTask();
  // the "Ask before running a task" question of Run, Dry run and a job's
  // Restart (#144): true when the user agreed or the preference is off
  bool confirmTaskRun(const QString &title, const QString &question,
                      const QString &names);
  QIcon mUploadIcon;
  QIcon mDownloadIcon;
  QIcon mMountIcon;
  // icons made by setupActions() and used by later setup blocks
  QIcon mArrowDownIcon;
  QIcon mArrowUpIcon;
  QIcon mMount1Icon;
  QIcon mSortZAIcon;
  QIcon mSortAZIcon;
  QIcon mSortTimeZAIcon;
  QIcon mSortTimeAZIcon;
  QMessageBox *mQuittingErrorMsgBox = NULL;

#ifdef Q_OS_MACOS
  MacOsPowerSaving *mMacOsPowerSaving;
#endif

  // used for tasks transitions - prevent race conditions
  QMutex mMutex;
  QMutex mSaveQueueFileMutex;
  QMutex mSaveSchedulerFileMutex;
  QMutex mStopTaskMutex;
  QMutex mRunTaskMutex;
  QMutex mRunItemMutex;
  QMutex mJobsSortMutex;

  // if waiting for processes we show dialog - this is used to calculate delay
  int mQuitInfoDelay = 0;

  void addTasksToQueue();

  void restoreSchedulersFromFile();

  void sortJobs();
  void applyJobsFilter();
  bool mJobsTimeSortOrder = false;
  bool mJobsStatusSortOrder = false;
  QString mJobsSort = "byDate";
  QString mIconsLayout;
};
