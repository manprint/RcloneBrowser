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

namespace {

const char kBrowserUpdateRepo[] = "manprint/RcloneBrowser";

// Link of an update message: the release page when the server sent a web URL,
// else the repository's latest release page.
QString releaseLink(const ReleaseInfo &release, const QString &slug) {
  const QString scheme = release.htmlUrl.scheme();
  if (release.htmlUrl.isValid() && (scheme == "https" || scheme == "http")) {
    return release.htmlUrl.toString(QUrl::FullyEncoded);
  }
  return "https://github.com/" + slug + "/releases/latest";
}

// Label of the config password prompt (3.3 flow and the rc unlock loop).
QString configPasswordLabel(bool wrongPassword) {
  QString label = "Enter password for .rclone.conf configuration file:";
  if (wrongPassword) {
    label = "Wrong password. " + label;
  }
  return label;
}

} // namespace

// Text colour of the fixed tabs (Remotes, Jobs, Tasks, Queue, Scheduler), to
// tell them apart from the tabs of the opened remotes.
// Put here the colour you like as "#rrggbb", or "" to keep the default colour.
static const char *FIXED_TABS_TEXT_COLOR = "";

static void ApplyFixedTabsTextColor(QTabWidget *tabs) {
  QColor color(FIXED_TABS_TEXT_COLOR);
  if (!color.isValid()) {
    return;
  }
  QTabBar *bar = tabs->findChild<QTabBar *>();
  if (!bar) {
    return;
  }
  for (int i = 0; i < 5; i++) {
    bar->setTabTextColor(i, color);
  }
}

// show or hide the Tasks, Queue and Scheduler tabs according to preferences;
// tab positions do not change and hidden tabs keep working in the background
static void ApplyOptionalTabsVisibility(QTabWidget *tabs) {
  auto settings = GetSettings();
  tabs->setTabVisible(2,
                      !settings->value("Settings/hideTasksTab", false).toBool());
  tabs->setTabVisible(3,
                      !settings->value("Settings/hideQueueTab", false).toBool());
  tabs->setTabVisible(
      4, !settings->value("Settings/hideSchedulerTab", false).toBool());
}

MainWindow::MainWindow() {

  ui.setupUi(this);

  setupPowerSavingAndTitle();
  setupDaemonAndUpdateChecks();
  setupWindowAndTheme();
  setupSettingsDefaults();
  setupActions();
  setupInitialStatus();
  setupPreferences();
  setupTabClosure();
  setupMenuActions();
  setupRemotesTab();
  setupTasksTab();
  setupQueueContextMenu();
  setupJobsTab();
  setupTasksActions();
  setupScheduler();
  setupJobsSorting();
  setupSchedulerAdd();
  setupQueue();
  setupQueueEditing();
  setupListSignalsAndIcons();
  setupTabs();
  setupRestoredState();
  setupTrayAndStartup();
}

void MainWindow::setupPowerSavingAndTitle() {
#ifdef Q_OS_MACOS
  // macOS power saving control object
  mMacOsPowerSaving = new MacOsPowerSaving();
#endif

  if (IsPortableMode()) {
    this->setWindowTitle("Rclone Browser - portable mode - BETA release");
  } else {
    this->setWindowTitle("Rclone Browser - BETA release");
  }
}

void MainWindow::setupDaemonAndUpdateChecks() {
  // asynchronous update checks (started from rcloneGetVersion)
  mRcloneUpdateChecker = new UpdateChecker(this);
  mBrowserUpdateChecker = new UpdateChecker(this);
  // private rclone rcd for folder listings (started once rclone is checked)
  mDaemon = new RcloneDaemon(this);
  global.daemon = mDaemon;
  connect(mDaemon, &RcloneDaemon::stateChanged, this,
          [this](RcloneDaemon::State state) {
            if (state == RcloneDaemon::State::Running) {
              unlockDaemonSilently();
            }
          });
  connect(mDaemon, &RcloneDaemon::failed, this, [this](const QString &msg) {
    // no retry: the next Preferences accept or app start tries again
    statusBar()->showMessage(
        "rclone backend unavailable, using direct commands: " + msg);
    qWarning().noquote() << "rclone backend unavailable:" << msg;
  });

  connect(mRcloneUpdateChecker, &UpdateChecker::newerAvailable, this,
          [this](const ReleaseInfo &release) {
            auto s = GetSettings();
            const QString slug =
                s->value("Settings/rcloneUpdateRepo", "rclone/rclone")
                    .toString()
                    .trimmed();
            QMessageBox::information(
                this, "",
                QString("<p>New rclone version is available</p>"
                        "<p>You have: v%1<br />New version: v%2</p>"
                        "<p>Visit rclone <a href=\"%3\">downloads</a> page "
                        "to upgrade</p>")
                    .arg(s->value("Settings/rcloneVersion")
                             .toString()
                             .toHtmlEscaped(),
                         release.version.toString().toHtmlEscaped(),
                         releaseLink(release, slug).toHtmlEscaped()));
          });
  connect(mRcloneUpdateChecker, &UpdateChecker::failed, this,
          [](const QString &reason) {
            auto s = GetSettings();
            qWarning().noquote()
                << "rclone update check failed ("
                << s->value("Settings/rcloneUpdateRepo", "rclone/rclone")
                       .toString()
                << "):" << reason;
          });
  connect(mBrowserUpdateChecker, &UpdateChecker::newerAvailable, this,
          [this](const ReleaseInfo &release) {
            QMessageBox::information(
                this, "",
                QString("<p>New Rclone Browser version is available</p>"
                        "<p>You have: v%1<br />New version: v%2</p>"
                        "<p>Visit <a href=\"%3\">releases</a> page to "
                        "download</p>")
                    .arg(QString::fromLatin1(RCLONE_BROWSER_VERSION),
                         release.version.toString().toHtmlEscaped(),
                         releaseLink(release, kBrowserUpdateRepo)
                             .toHtmlEscaped()));
          });
  connect(mBrowserUpdateChecker, &UpdateChecker::failed, this,
          [](const QString &reason) {
            qWarning().noquote()
                << "Rclone Browser update check failed (" << kBrowserUpdateRepo
                << "):" << reason;
          });
}

void MainWindow::setupWindowAndTheme() {
  auto settings = GetSettings();

#if defined(Q_OS_WIN) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  // disable "?" WindowContextHelpButton (Qt 6 no longer shows it by default)
  QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

#if !defined(Q_OS_MACOS)
  qApp->setStyle(QStyleFactory::create("Fusion"));

  bool darkMode = settings->value("Settings/darkMode").toBool();

  // enable dark mode for Windows and Linux
  if (darkMode) {

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText,
                         QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Base, QColor(42, 42, 42));
    darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text,
                         QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Disabled, QPalette::Light,
                         QColor(35, 35, 35));
    darkPalette.setColor(QPalette::Dark, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::Shadow, QColor(20, 20, 20));
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText,
                         QColor(127, 127, 127));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Disabled, QPalette::Highlight,
                         QColor(80, 80, 80));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                         QColor(127, 127, 127));
    qApp->setPalette(darkPalette);
    qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; "
                        "border: 1px solid white;}");
  }

#else

  QString sysInfo = QSysInfo::productVersion();
  // enable dark mode for older macOS
  if (sysInfo == "10.9" || sysInfo == "10.10" || sysInfo == "10.11" ||
      sysInfo == "10.12" || sysInfo == "10.13") {

    qApp->setStyle(QStyleFactory::create("Fusion"));

    auto settings = GetSettings();
    bool darkMode = settings->value("Settings/darkMode").toBool();
    if (darkMode) {

      QPalette darkPalette;
      darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
      darkPalette.setColor(QPalette::WindowText, Qt::white);
      darkPalette.setColor(QPalette::Disabled, QPalette::WindowText,
                           QColor(127, 127, 127));
      darkPalette.setColor(QPalette::Base, QColor(42, 42, 42));
      darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
      darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
      darkPalette.setColor(QPalette::ToolTipText, Qt::white);
      darkPalette.setColor(QPalette::Text, Qt::white);
      darkPalette.setColor(QPalette::Disabled, QPalette::Text,
                           QColor(127, 127, 127));
      darkPalette.setColor(QPalette::Disabled, QPalette::Light,
                           QColor(35, 35, 35));
      darkPalette.setColor(QPalette::Dark, QColor(35, 35, 35));
      darkPalette.setColor(QPalette::Shadow, QColor(20, 20, 20));
      darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
      darkPalette.setColor(QPalette::ButtonText, Qt::white);
      darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText,
                           QColor(127, 127, 127));
      darkPalette.setColor(QPalette::BrightText, Qt::red);
      darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
      darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
      darkPalette.setColor(QPalette::Disabled, QPalette::Highlight,
                           QColor(80, 80, 80));
      darkPalette.setColor(QPalette::HighlightedText, Qt::white);
      darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                           QColor(127, 127, 127));
      qApp->setPalette(darkPalette);
      qApp->setStyleSheet(
          "QToolTip { color: #ffffff; background-color: #2a82da; "
          "border: 1px solid white;}");
    }
  }
#endif

  mSystemTray.setIcon(qApp->windowIcon());
  if (settings->contains("MainWindow/geometry")) {
    restoreGeometry(settings->value("MainWindow/geometry").toByteArray());
  }
  SetRclone(settings->value("Settings/rclone").toString());
  SetRcloneConf(settings->value("Settings/rcloneConf").toString());

  mAlwaysShowInTray =
      settings->value("Settings/alwaysShowInTray", false).toBool();
  mCloseToTray = settings->value("Settings/closeToTray", false).toBool();
  mNotifyFinishedTransfers =
      settings->value("Settings/notifyFinishedTransfers", true).toBool();
  mSoundNotif = settings->value("Settings/soundNotif", false).toBool();

  mSystemTray.setVisible(mAlwaysShowInTray);
}

void MainWindow::setupSettingsDefaults() {
  auto settings = GetSettings();

  // during first run the lastUsed keys might not exist
  if (!(settings->contains("Settings/lastUsedSourceFolder"))) {
    // if lastUsedSourceFolder does not exist create new empty key
    settings->setValue("Settings/lastUsedSourceFolder", "");
  };
  if (!(settings->contains("Settings/lastUsedDestFolder"))) {
    // if lastUsedDestFolder does not exist create new empty key
    settings->setValue("Settings/lastUsedDestFolder", "");
  };
  if (!(settings->contains("Settings/defaultDownloadOptions"))) {
    // if defaultDownloadOptions does not exist create new empty key
    settings->setValue("Settings/defaultDownloadOptions", "");
  };
#ifdef Q_OS_MACOS
  // for macOS by default exclude .DS_Store files from uploads
  if (!(settings->contains("Settings/defaultUploadOptions"))) {
    // if defaultDownloadOptions does not exist create new empty key
    settings->setValue("Settings/defaultUploadOptions", "--exclude .DS_Store");
  };
#else
  if (!(settings->contains("Settings/defaultUploadOptions"))) {
    // if defaultDownloadOptions does not exist create new empty key
    settings->setValue("Settings/defaultUploadOptions", "");
  };
#endif
  if (!(settings->contains("Settings/defaultRcloneOptions"))) {
    // if defaultRcloneOptions does not exist create new empty key
    settings->setValue("Settings/defaultRcloneOptions", "--fast-list");
  };
}

void MainWindow::setupActions() {
  auto settings = GetSettings();

  QString buttonStyle = settings->value("Settings/buttonStyle").toString();
  QString buttonSize = settings->value("Settings/buttonSize").toString();
  QString iconsColour = settings->value("Settings/iconsColour").toString();

  QString img_add = "";

  if (iconsColour == "white") {
    img_add = "_inv";
  }

  ui.actionRefresh->setIcon(
      QIcon(":media/images/qbutton_icons/refresh" + img_add + ".png"));
  ui.actionOpen->setIcon(
      QIcon(":media/images/qbutton_icons/open_remote" + img_add + ".png"));
  ui.actionConfig->setIcon(
      QIcon(":media/images/qbutton_icons/rclone_config" + img_add + ".png"));
  // Preferences button action is triggered via slot defined in ui file
  // as we dont want pref icon in the menu
  ui.buttonPrefs->setIcon(
      QIcon(":media/images/qbutton_icons/preferences" + img_add + ".png"));

  ui.actionStopAllTransfers->setIcon(
      QIcon(":media/images/qbutton_icons/stop" + img_add + ".png"));
  ui.actionCleanNotRunning->setIcon(
      QIcon(":media/images/qbutton_icons/purge" + img_add + ".png"));
  // triggered via slot so button text can be different than action menu
  ui.buttonSortByTime->setIcon(
      QIcon(":media/images/qbutton_icons/sortTimeZA" + img_add + ".png"));
  ui.buttonSortByStatus->setIcon(
      QIcon(":media/images/qbutton_icons/sortZA" + img_add + ".png"));

  ui.actionDryRun->setIcon(
      QIcon(":media/images/qbutton_icons/dryrun" + img_add + ".png"));
  ui.actionRun->setIcon(
      QIcon(":media/images/qbutton_icons/run" + img_add + ".png"));
  ui.actionEdit->setIcon(
      QIcon(":media/images/qbutton_icons/edit" + img_add + ".png"));
  ui.actionDelete->setIcon(
      QIcon(":media/images/qbutton_icons/purge" + img_add + ".png"));
  ui.actionStop->setIcon(
      QIcon(":media/images/qbutton_icons/stop" + img_add + ".png"));

  // triggered via slot so button text can be different than action menu
  ui.buttonAddToQueue->setIcon(
      QIcon(":media/images/qbutton_icons/addtoqueue" + img_add + ".png"));
  ui.actionAddToQueue->setIcon(
      QIcon(":media/images/qbutton_icons/addtoqueue" + img_add + ".png"));
  ui.buttonAddToScheduler->setIcon(
      QIcon(":media/images/qbutton_icons/addtoscheduler" + img_add + ".png"));
  ui.actionAddToScheduler->setIcon(
      QIcon(":media/images/qbutton_icons/addtoscheduler" + img_add + ".png"));

  ui.actionStartQueue->setIcon(
      QIcon(":media/images/qbutton_icons/run" + img_add + ".png"));
  ui.actionStopQueue->setIcon(
      QIcon(":media/images/qbutton_icons/stop" + img_add + ".png"));
  ui.actionPurgeQueue->setIcon(
      QIcon(":media/images/qbutton_icons/purge" + img_add + ".png"));
  ui.actionRemoveFromQueue->setIcon(
      QIcon(":media/images/qbutton_icons/removefromqueue" + img_add + ".png"));
  ui.actionUpQueue->setIcon(
      QIcon(":media/images/qbutton_icons/vuparrow" + img_add + ".png"));
  ui.actionDownQueue->setIcon(
      QIcon(":media/images/qbutton_icons/vdownarrow" + img_add + ".png"));

  ui.actionStartScheduler->setIcon(
      QIcon(":media/images/qbutton_icons/run" + img_add + ".png"));
  ui.actionStopScheduler->setIcon(
      QIcon(":media/images/qbutton_icons/stop" + img_add + ".png"));

  QPixmap arrowDownPixmap(":media/images/qbutton_icons/arrowdown" + img_add +
                          ".png");
  QPixmap arrowUpPixmap(":media/images/qbutton_icons/arrowup" + img_add +
                        ".png");
  QPixmap mount1Pixmap(":media/images/qbutton_icons/mount1" + img_add + ".png");

  QPixmap sortZAPixmap(":media/images/qbutton_icons/sortZA" + img_add + ".png");
  QPixmap sortAZPixmap(":media/images/qbutton_icons/sortAZ" + img_add + ".png");
  QPixmap sortTimeZAPixmap(":media/images/qbutton_icons/sortTimeZA" + img_add +
                           ".png");
  QPixmap sortTimeAZPixmap(":media/images/qbutton_icons/sortTimeAZ" + img_add +
                           ".png");

  mArrowDownIcon = QIcon(arrowDownPixmap);
  mArrowUpIcon = QIcon(arrowUpPixmap);
  mMount1Icon = QIcon(mount1Pixmap);
  mSortZAIcon = QIcon(sortZAPixmap);
  mSortAZIcon = QIcon(sortAZPixmap);
  mSortTimeZAIcon = QIcon(sortTimeZAPixmap);
  mSortTimeAZIcon = QIcon(sortTimeAZPixmap);

  ui.buttonDryrunTask->setDefaultAction(ui.actionDryRun);
  ui.buttonRunTask->setDefaultAction(ui.actionRun);
  ui.buttonStop->setDefaultAction(ui.actionStop);
  ui.buttonEditTask->setDefaultAction(ui.actionEdit);
  ui.buttonDeleteTask->setDefaultAction(ui.actionDelete);
  ui.buttonSortTask->setDefaultAction(ui.actionSortTask);
  ui.refresh->setDefaultAction(ui.actionRefresh);
  ui.open->setDefaultAction(ui.actionOpen);
  ui.config->setDefaultAction(ui.actionConfig);
  //  ui.buttonPrefs->setDefaultAction(ui.preferences);
  ui.buttonStopAllJobs->setDefaultAction(ui.actionStopAllTransfers);
  ui.buttonCleanNotRunning->setDefaultAction(ui.actionCleanNotRunning);
  // ui.buttonAddToQueue->setDefaultAction(ui.actionAddToQueue);
  ui.buttonStartQueue->setDefaultAction(ui.actionStartQueue);
  ui.buttonStopQueue->setDefaultAction(ui.actionStopQueue);
  ui.buttonPurgeQueue->setDefaultAction(ui.actionPurgeQueue);
  ui.buttonRemoveFromQueue->setDefaultAction(ui.actionRemoveFromQueue);
  ui.buttonDownQueue->setDefaultAction(ui.actionDownQueue);
  ui.buttonUpQueue->setDefaultAction(ui.actionUpQueue);
  ui.buttonStartScheduler->setDefaultAction(ui.actionStartScheduler);
  ui.buttonStopScheduler->setDefaultAction(ui.actionStopScheduler);

  // overwrite button text, we want different menu name and different conextual
  // menu
  ui.buttonPrefs->setText("Prefs");
  ui.buttonAddToQueue->setText("Add");
  ui.buttonAddToScheduler->setText("Add");

  // open remote should be not active when there is
  // no foucs on any e.g. after start
  ui.open->setEnabled(false);

  // jobs buttons inactive after start
  ui.buttonStopAllJobs->setEnabled(false);
  ui.buttonCleanNotRunning->setEnabled(false);
  ui.buttonSortByTime->setEnabled(false);
  ui.buttonSortByStatus->setEnabled(false);

  int icon_w = 16;
  int icon_h = 16;
  //  int button_width = 32;

  if (buttonSize == "0") {
    icon_w = 24;
  }
  if (buttonSize == "1") {
    icon_w = 32;
  }
  if (buttonSize == "2") {
    icon_w = 48;
  }
  if (buttonSize == "3") {
    icon_w = 72;
  }
  if (buttonSize == "4") {
    icon_w = 96;
  }
  icon_h = icon_w;
  int button_width = 61;

  if (buttonStyle == "textandicon") {

    ui.buttonDryrunTask->setIconSize(QSize(icon_w, icon_h));
    ui.buttonRunTask->setIconSize(QSize(icon_w, icon_h));
    ui.buttonEditTask->setIconSize(QSize(icon_w, icon_h));
    ui.buttonDeleteTask->setIconSize(QSize(icon_w, icon_h));
    ui.buttonSortTask->setIconSize(QSize(icon_w, icon_h));
    ui.buttonPrefs->setIconSize(QSize(icon_w, icon_h));
    ui.refresh->setIconSize(QSize(icon_w, icon_h));
    ui.open->setIconSize(QSize(icon_w, icon_h));
    ui.config->setIconSize(QSize(icon_w, icon_h));
    ui.buttonStopAllJobs->setIconSize(QSize(icon_w, icon_h));
    ui.buttonCleanNotRunning->setIconSize(QSize(icon_w, icon_h));
    ui.buttonAddToQueue->setIconSize(QSize(icon_w, icon_h));
    ui.buttonStartQueue->setIconSize(QSize(icon_w, icon_h));
    ui.buttonStopQueue->setIconSize(QSize(icon_w, icon_h));
    ui.buttonPurgeQueue->setIconSize(QSize(icon_w, icon_h));
    ui.buttonRemoveFromQueue->setIconSize(QSize(icon_w, icon_h));
    ui.buttonDownQueue->setIconSize(QSize(icon_w, icon_h));
    ui.buttonUpQueue->setIconSize(QSize(icon_w, icon_h));

    ui.buttonDryrunTask->setMinimumWidth(button_width);
    ui.buttonRunTask->setMinimumWidth(button_width);
    ui.buttonEditTask->setMinimumWidth(button_width);
    ui.buttonDeleteTask->setMinimumWidth(button_width);
    ui.buttonSortTask->setMinimumWidth(button_width);
    ui.buttonPrefs->setMinimumWidth(button_width);
    ui.refresh->setMinimumWidth(button_width);
    ui.open->setMinimumWidth(button_width);
    ui.config->setMinimumWidth(button_width);
    ui.buttonStopAllJobs->setMinimumWidth(button_width);
    ui.buttonCleanNotRunning->setMinimumWidth(button_width);
    ui.buttonAddToQueue->setMinimumWidth(button_width);
    ui.buttonStartQueue->setMinimumWidth(button_width);
    ui.buttonStopQueue->setMinimumWidth(button_width);
    ui.buttonPurgeQueue->setMinimumWidth(button_width);
    ui.buttonRemoveFromQueue->setMinimumWidth(button_width);
    ui.buttonDownQueue->setMinimumWidth(button_width);

    ui.buttonUpQueue->setIconSize(QSize(icon_w, icon_h));
    ui.buttonUpQueue->setMinimumWidth(button_width);
    ui.buttonStop->setIconSize(QSize(icon_w, icon_h));
    ui.buttonStop->setMinimumWidth(button_width);
    ui.buttonAddToScheduler->setIconSize(QSize(icon_w, icon_h));
    ui.buttonAddToScheduler->setMinimumWidth(button_width);
    ui.buttonStopScheduler->setIconSize(QSize(icon_w, icon_h));
    ui.buttonStopScheduler->setMinimumWidth(button_width);
    ui.buttonStartScheduler->setIconSize(QSize(icon_w, icon_h));
    ui.buttonStartScheduler->setMinimumWidth(button_width);
    ui.buttonSortByTime->setIconSize(QSize(icon_w, icon_h));
    ui.buttonSortByTime->setMinimumWidth(button_width);
    ui.buttonSortByStatus->setIconSize(QSize(icon_w, icon_h));
    ui.buttonSortByStatus->setMinimumWidth(button_width);

  } else {
    if (buttonStyle == "textonly") {
      ui.buttonDryrunTask->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonRunTask->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonEditTask->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonDeleteTask->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonSortTask->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonPrefs->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.refresh->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.open->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.config->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonStopAllJobs->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonCleanNotRunning->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonAddToQueue->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonStartQueue->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonStopQueue->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonPurgeQueue->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonRemoveFromQueue->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonDownQueue->setToolButtonStyle(Qt::ToolButtonTextOnly);

      ui.buttonDryrunTask->setMinimumWidth(button_width);
      ui.buttonRunTask->setMinimumWidth(button_width);
      ui.buttonEditTask->setMinimumWidth(button_width);
      ui.buttonDeleteTask->setMinimumWidth(button_width);
      ui.buttonSortTask->setMinimumWidth(button_width);
      ui.buttonPrefs->setMinimumWidth(button_width);
      ui.refresh->setMinimumWidth(button_width);
      ui.open->setMinimumWidth(button_width);
      ui.config->setMinimumWidth(button_width);
      ui.buttonStopAllJobs->setMinimumWidth(button_width);
      ui.buttonCleanNotRunning->setMinimumWidth(button_width);
      ui.buttonAddToQueue->setMinimumWidth(button_width);
      ui.buttonStartQueue->setMinimumWidth(button_width);
      ui.buttonStopQueue->setMinimumWidth(button_width);
      ui.buttonPurgeQueue->setMinimumWidth(button_width);
      ui.buttonRemoveFromQueue->setMinimumWidth(button_width);
      ui.buttonDownQueue->setMinimumWidth(button_width);
      ui.buttonUpQueue->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonUpQueue->setMinimumWidth(button_width);
      ui.buttonStop->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonStop->setMinimumWidth(button_width);
      ui.buttonAddToScheduler->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonAddToScheduler->setMinimumWidth(button_width);
      ui.buttonStopScheduler->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonStopScheduler->setMinimumWidth(button_width);
      ui.buttonStartScheduler->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonStartScheduler->setMinimumWidth(button_width);
      ui.buttonSortByTime->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonSortByTime->setMinimumWidth(button_width);
      ui.buttonSortByStatus->setToolButtonStyle(Qt::ToolButtonTextOnly);
      ui.buttonSortByStatus->setMinimumWidth(button_width);

    } else {
      // button style - icononly
      ui.buttonDryrunTask->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonDryrunTask->setIconSize(QSize(icon_w, icon_h));
      ui.buttonRunTask->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonRunTask->setIconSize(QSize(icon_w, icon_h));
      ui.buttonEditTask->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonEditTask->setIconSize(QSize(icon_w, icon_h));
      ui.buttonDeleteTask->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonDeleteTask->setIconSize(QSize(icon_w, icon_h));
      ui.buttonSortTask->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonSortTask->setIconSize(QSize(icon_w, icon_h));
      ui.buttonPrefs->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonPrefs->setIconSize(QSize(icon_w, icon_h));
      ui.refresh->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.refresh->setIconSize(QSize(icon_w, icon_h));
      ui.open->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.open->setIconSize(QSize(icon_w, icon_h));
      ui.config->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.config->setIconSize(QSize(icon_w, icon_h));
      ui.buttonStopAllJobs->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonStopAllJobs->setIconSize(QSize(icon_w, icon_h));
      ui.buttonCleanNotRunning->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonCleanNotRunning->setIconSize(QSize(icon_w, icon_h));
      ui.buttonAddToQueue->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonAddToQueue->setIconSize(QSize(icon_w, icon_h));
      ui.buttonStartQueue->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonStartQueue->setIconSize(QSize(icon_w, icon_h));
      ui.buttonStopQueue->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonStopQueue->setIconSize(QSize(icon_w, icon_h));
      ui.buttonPurgeQueue->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonPurgeQueue->setIconSize(QSize(icon_w, icon_h));
      ui.buttonRemoveFromQueue->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonRemoveFromQueue->setIconSize(QSize(icon_w, icon_h));
      ui.buttonDownQueue->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonDownQueue->setIconSize(QSize(icon_w, icon_h));
      ui.buttonUpQueue->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonUpQueue->setIconSize(QSize(icon_w, icon_h));
      ui.buttonStop->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonStop->setIconSize(QSize(icon_w, icon_h));
      ui.buttonAddToScheduler->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonAddToScheduler->setIconSize(QSize(icon_w, icon_h));
      ui.buttonStopScheduler->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonStopScheduler->setIconSize(QSize(icon_w, icon_h));
      ui.buttonStartScheduler->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonStartScheduler->setIconSize(QSize(icon_w, icon_h));
      ui.buttonSortByTime->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonSortByTime->setIconSize(QSize(icon_w, icon_h));
      ui.buttonSortByStatus->setToolButtonStyle(Qt::ToolButtonIconOnly);
      ui.buttonSortByStatus->setIconSize(QSize(icon_w, icon_h));
    }
  }

  // initial sortByTime
  ui.buttonSortByStatus->setStyleSheet("QToolButton {border: 0;}");

  // layout spacers - otherwise layout of different tabs differ due to checkbox
  // on queue tab
  ui.layoutcontrol1->hide();
  ui.layoutcontrol2->hide();
  ui.layoutcontrol3->hide();
  ui.layoutcontrol1_sch->hide();
  ui.layoutcontrol1_queue->hide();

  // statusTips
  ui.actionRefresh->setStatusTip("Refresh remotes view");
  ui.actionConfig->setStatusTip("rclone config");
  ui.actionOpen->setStatusTip("Open remote");
  ui.preferences->setStatusTip("Rclone Browser preferences (ALT-p)");

  ui.actionStopAllTransfers->setStatusTip("Stop all running transfer jobs");
  ui.actionCleanNotRunning->setStatusTip("Remove all not running jobs");

  ui.actionDryRun->setStatusTip("Dry Run all selected tasks at once");
  ui.actionRun->setStatusTip("Run all selected tasks at once");
  ui.actionStop->setStatusTip("Stop all selected tasks");

  ui.actionEdit->setStatusTip("Edit selected task");
  ui.actionDelete->setStatusTip(
      "Delete selected tasks - only not running tasks can be deleted.");

  ui.actionAddToQueue->setStatusTip("Add selected transfer tasks to the queue");
  ui.actionAddToScheduler->setStatusTip(
      "Add selected transfer tasks to the scheduler");
  ui.actionSortTask->setStatusTip("Sort tasks by name");

  ui.actionStartQueue->setStatusTip(
      "Start actively processing items from the queue");
  ui.actionStopQueue->setStatusTip(
      "Stop the queue processing. Active task will be cancelled.");
  ui.actionUpQueue->setStatusTip("Move task up in the queue");
  ui.actionDownQueue->setStatusTip("Move task down in the queue");
  ui.actionRemoveFromQueue->setStatusTip("Remove from the queue");
  ui.actionPurgeQueue->setStatusTip(
      "Remove all not running tasks from the queue");

  ui.buttonStopScheduler->setStatusTip("Stop scheduler");
  ui.buttonStartScheduler->setStatusTip(
      "Start all previously active schedulers");

  ui.buttonStartScheduler->setEnabled(false);
  ui.buttonStopAllJobs->setEnabled(false);
  ui.buttonCleanNotRunning->setEnabled(false);
}

void MainWindow::setupPreferences() {
  QObject::connect(ui.preferences, &QAction::triggered, this, [=]() {
    PreferencesDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
      auto settings = GetSettings();
      settings->setValue("Settings/rclone", dialog.getRclone().trimmed());
      settings->setValue("Settings/rcloneConf",
                         dialog.getRcloneConf().trimmed());
      settings->setValue("Settings/stream", dialog.getStream());
      settings->setValue("Settings/mount", dialog.getMount());
      settings->setValue("Settings/defaultDownloadDir",
                         dialog.getDefaultDownloadDir().trimmed());
      settings->setValue("Settings/defaultUploadDir",
                         dialog.getDefaultUploadDir().trimmed());
      settings->setValue("Settings/defaultDownloadOptions",
                         dialog.getDefaultDownloadOptions().trimmed());
      settings->setValue("Settings/defaultUploadOptions",
                         dialog.getDefaultUploadOptions().trimmed());
      settings->setValue("Settings/defaultRcloneOptions",
                         dialog.getDefaultRcloneOptions().trimmed());

      settings->setValue("Settings/checkRcloneBrowserUpdates",
                         dialog.getCheckRcloneBrowserUpdates());
      settings->setValue("Settings/checkRcloneUpdates",
                         dialog.getCheckRcloneUpdates());
      settings->setValue("Settings/rcloneUpdateRepo",
                         dialog.getRcloneUpdateRepo().trimmed());

      settings->setValue("Settings/alwaysShowInTray",
                         dialog.getAlwaysShowInTray());
      settings->setValue("Settings/closeToTray", dialog.getCloseToTray());
      settings->setValue("Settings/startMinimisedToTray",
                         dialog.getStartMinimisedToTray());

      settings->setValue("Settings/notifyFinishedTransfers",
                         dialog.getNotifyFinishedTransfers());
      settings->setValue("Settings/soundNotif", dialog.getSoundNotif());

      settings->setValue("Settings/showFolderIcons",
                         dialog.getShowFolderIcons());
      settings->setValue("Settings/showFileIcons", dialog.getShowFileIcons());
      settings->setValue("Settings/rowColors", dialog.getRowColors());
      settings->setValue("Settings/showHidden", dialog.getShowHidden());

      settings->setValue("Settings/darkMode", dialog.getDarkMode());
      settings->setValue("Settings/rememberLastOptions",
                         dialog.getRememberLastOptions());

      settings->setValue("Settings/buttonStyle",
                         dialog.getButtonStyle().trimmed());
      settings->setValue("Settings/iconsLayout",
                         dialog.getIconsLayout().trimmed());
      settings->setValue("Settings/iconsColour",
                         dialog.getIconsColour().trimmed());

      settings->setValue("Settings/fontSize", dialog.getFontSize().trimmed());
      settings->setValue("Settings/buttonSize",
                         dialog.getButtonSize().trimmed());
      settings->setValue("Settings/iconSize", dialog.getIconSize().trimmed());

      settings->setValue("Settings/useProxy", dialog.getUseProxy());
      settings->setValue("Settings/http_proxy",
                         dialog.getHttpProxy().trimmed());
      settings->setValue("Settings/https_proxy",
                         dialog.getHttpsProxy().trimmed());
      settings->setValue("Settings/no_proxy", dialog.getNoProxy().trimmed());

      settings->setValue("Settings/preemptiveLoading",
                         dialog.getPreemptiveLoading());
      settings->setValue("Settings/preemptiveLoadingLevel",
                         dialog.getPreemptiveLoadingLevel().trimmed());

      settings->setValue("Settings/queueScript",
                         dialog.getQueueScript().trimmed());
      settings->setValue("Settings/transferOnScript",
                         dialog.getTransferOnScript().trimmed());
      settings->setValue("Settings/transferOffScript",
                         dialog.getTransferOffScript().trimmed());

      settings->setValue("Settings/queueScriptRun", dialog.getQueueScriptRun());
      settings->setValue("Settings/jobStartScriptRun",
                         dialog.getJobStartScriptRun());
      settings->setValue("Settings/jobLastFinishedScriptRun",
                         dialog.getJobLastFinishedScriptRun());

      const QString oldRclone = GetRclone();
      const QStringList oldConf = GetRcloneConf();
      SetRclone(dialog.getRclone());
      SetRcloneConf(dialog.getRcloneConf());
      if (GetRclone() != oldRclone || GetRcloneConf() != oldConf) {
        // stop() is asynchronous and start() while Running does nothing: the
        // new daemon starts from rcloneGetVersion() once the old one is gone
        stopDaemonAndWait();
      }
      mFirstTime = true;
      rcloneGetVersion();

      mAlwaysShowInTray = dialog.getAlwaysShowInTray();
      mCloseToTray = dialog.getCloseToTray();
      mNotifyFinishedTransfers = dialog.getNotifyFinishedTransfers();
      mSoundNotif = dialog.getSoundNotif();

      mSystemTray.setVisible(mAlwaysShowInTray);

      settings->setValue("Settings/hideTasksTab", dialog.getHideTasksTab());
      settings->setValue("Settings/hideQueueTab", dialog.getHideQueueTab());
      settings->setValue("Settings/hideSchedulerTab",
                         dialog.getHideSchedulerTab());
      ApplyOptionalTabsVisibility(ui.tabs);

      // hide or show the icons in the remote tabs already open; the icons of
      // the fixed tabs are not touched
      settings->setValue("Settings/hideRemoteIcons",
                         dialog.getHideRemoteIcons());
      for (int i = 0; i < ui.tabs->count(); i++) {
        if (auto remoteTab = qobject_cast<RemoteWidget *>(ui.tabs->widget(i))) {
          remoteTab->setButtonsHidden(dialog.getHideRemoteIcons());
        }
      }
    }
  });
}

void MainWindow::setupTabClosure() {
  // intercept tab closure
  MainWindow::connect(ui.tabs, SIGNAL(tabCloseRequested(int)), this,
                      SLOT(slotCloseTab(int)));
}

void MainWindow::setupMenuActions() {
  QObject::connect(ui.quit, &QAction::triggered, this, [=]() {
    mCloseToTray = false;
    close();
  });

  QObject::connect(ui.about, &QAction::triggered, this, [=]() {
    QMessageBox::about(
        this, "Rclone Browser",
        QString(
            R"(<h3>GUI for rclone, v)" RCLONE_BROWSER_VERSION "</h3>"

            R"(<p>Copyright &copy; 2019-2020 <a href="https://github.com/kapitainsky/RcloneBrowser/blob/master/LICENSE">kapitainsky</a></p>)"

            R"(<p>Current development and maintenance<br /><a href="https://github.com/kapitainsky/RcloneBrowser">kapitainsky</a></p>)"

            R"(<p>New features and fixes<br /><a href="https://github.com/kapitainsky/RcloneBrowser/graphs/contributors">contributors</a></p>)"

            R"(<p>Original version<br /><a href="https://mmozeiko.github.io/RcloneBrowser">Martins Mozeiko</a></p>)"));
  });
  QObject::connect(ui.aboutQt, &QAction::triggered, qApp,
                   &QApplication::aboutQt);
}

void MainWindow::setupRemotesTab() {
  QObject::connect(ui.remotes, &QListWidget::currentItemChanged, this, [=]() {
    if (ui.remotes->selectedItems().empty()) {
      ui.open->setEnabled(false);
    } else {
      ui.open->setEnabled(true);
    }
  });

  QObject::connect(ui.remotes, &QListWidget::itemSelectionChanged, this, [=]() {
    if (ui.remotes->selectedItems().empty()) {
      ui.open->setEnabled(false);
    } else {
      ui.open->setEnabled(true);
    }
  });

  QObject::connect(ui.remotes, &QListWidget::itemChanged, this, [=]() {
    if (ui.remotes->selectedItems().empty()) {
      ui.open->setEnabled(false);
    } else {
      ui.open->setEnabled(true);
    }
  });

  QObject::connect(ui.remotes, &QListWidget::itemClicked, this, [=]() {
    if (ui.remotes->selectedItems().empty()) {
      ui.open->setEnabled(false);
    } else {
      ui.open->setEnabled(true);
    }
  });

  QObject::connect(ui.remotes, &QListWidget::itemActivated, ui.open,
                   &QPushButton::clicked);

  QObject::connect(ui.config, &QPushButton::clicked, this,
                   &MainWindow::openRemoteConfig);

  // the remote context menu: native editing and the terminal flow
  ui.remotes->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui.remotes, &QWidget::customContextMenuRequested, this,
                   &MainWindow::showRemotesMenu);

  QObject::connect(ui.refresh, &QPushButton::clicked, this,
                   &MainWindow::rcloneListRemotes);

  QObject::connect(ui.open, &QPushButton::clicked, this, [=]() {
    if (ui.remotes->selectedItems().size() != 0) {
      auto item = ui.remotes->selectedItems().front();
      QString type = item->data(Qt::UserRole).toString();
      QString name = item->text();
      QString remoteType = type;

      auto remote = new RemoteWidget(&mIcons, name, remoteType, ui.tabs);

      QObject::connect(remote, &RemoteWidget::addNewMount, this,
                       &MainWindow::addNewMount);
      QObject::connect(remote, &RemoteWidget::addStream, this,
                       &MainWindow::addStream);
      QObject::connect(remote, &RemoteWidget::addTransfer, this,
                       &MainWindow::addTransfer);
      QObject::connect(remote, &RemoteWidget::addSavedTransfer, this,
                       &MainWindow::addSavedTransfer);
      QObject::connect(remote, &RemoteWidget::configLocked, this,
                       &MainWindow::ensureRcUnlocked);

      QString nameTrimmed = name;

      if (name.length() > 15) {
        nameTrimmed = nameTrimmed.left(12) + "...";
      }

      int index = ui.tabs->addTab(remote, nameTrimmed);
      ui.tabs->setTabToolTip(index,
                             "type: " + remoteType + "\n\nname: " + name);
      ui.tabs->setCurrentIndex(index);
    }
  });

  QObject::connect(ui.tabs, &QTabWidget::tabCloseRequested, this,
                   [=](const int &index) {
                     // delete remote widget when tab closed
                     ui.tabs->widget(index)->deleteLater();
                   });

  QObject::connect(ui.tabs, &QTabWidget::tabCloseRequested, ui.tabs,
                   &QTabWidget::removeTab);
}

void MainWindow::setupJobsTab() {
  QObject::connect(ui.actionCleanNotRunning, &QAction::triggered, this, [=]() {
    int jobsCount = ((ui.jobs->count() - 2) / 2 - mJobCount);

    int button = QMessageBox::question(this, "Rclone Browser",
                                       QString("There are %1 inactive job(s).\n"
                                               "\nDo you want to clean them?")
                                           .arg(jobsCount),
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::No);

    if (button == QMessageBox::Yes) {
      int widgetsCount = ui.jobs->count();
      for (int i = widgetsCount - 2; i >= 0; i = i - 2) {
        QWidget *widget = ui.jobs->itemAt(i)->widget();
        if (auto mount = qobject_cast<MountWidget *>(widget)) {
          if (!(mount->isRunning)) {
            emit mount->closed();
          }
        } else if (auto transfer = qobject_cast<JobWidget *>(widget)) {
          if (!(transfer->isRunning)) {
            emit transfer->closed();
          }
        } else if (auto stream = qobject_cast<StreamWidget *>(widget)) {
          if (!(stream->isRunning)) {
            emit stream->closed();
          }
        }
      }
    }
  });

  //!!!  QObject::connect(ui.actionStopAllTransfers
  QObject::connect(ui.actionStopAllTransfers, &QAction::triggered, this, [=]() {
    mDoNotSort = true;

    // we only stop transfer jobs - others are intact
    if (mTransferJobCount != 0) {

      int button = QMessageBox::question(
          this, "Rclone Browser",
          QString("There are %1 transfer job(s) running.\n"
                  "\nDo you want to stop  them?")
              .arg(mTransferJobCount),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

      if (button == QMessageBox::Yes) {

        // if queue is running and active stop it
        // to prevent race condition with new queue task auto starting
        // (triggered by stopping tasks)
        bool queueActive = false;

        if ((mQueueStatus == true) && mQueueTaskRunning &&
            (ui.queueListWidget->count() > 0)) {
          queueActive = true;
          mQueueStatus = false;
          /// remove top task from queue + save it
          ui.queueListWidget->takeItem(0);
          --mQueueCount;
          saveQueueFile();

          if (mQueueCount == 0) {
            ui.tabs->setTabText(3, QString("Queue (%1)>>(0)").arg(mQueueCount));
          } else {

            if (!mQueueTaskRunning) {
              ui.tabs->setTabText(3,
                                  QString("Queue (%1)>>(0)").arg(mQueueCount));
            } else {
              ui.tabs->setTabText(
                  3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
            }
          }
          setQueueButtons();
        }

        // now safely terminate all running transfers
        int widgetsCount = ui.jobs->count();
        for (int i = widgetsCount - 2; i >= 0; i = i - 2) {
          QWidget *widget = ui.jobs->itemAt(i)->widget();
          if (auto transfer = qobject_cast<JobWidget *>(widget)) {
            if ((transfer->isRunning)) {
              emit transfer->cancel();
            }
          }
        }

        // restart queue if it was active before
        if (queueActive == true) {
          mQueueStatus = true;

          if (ui.queueListWidget->count() == 0) {
            auto settings = GetSettings();
            if (settings->value("Settings/queueScriptRun", false).toBool()) {
              QString queueScript =
                  settings->value("Settings/queueScript").toString();
              if (!queueScript.isEmpty()) {
                runScript(queueScript);
              }
            }
          }

          if (ui.queueListWidget->count() > 0) {
            // start task
            // no transfer job is running as we just stopped all - we can start
            // without checking other running jobs
            JobOptionsListWidgetItem *item =
                static_cast<JobOptionsListWidgetItem *>(
                    ui.queueListWidget->item(0));
            mQueueTaskRunning = true;
            runItem(item, "queue", item->GetRequestId());
            ui.queueListWidget->item(0)->setBackground(Qt::darkGreen);
            setQueueButtons();
          } else {
            mQueueTaskRunning = false;
          }
        }
      }
    }

    mDoNotSort = false;
    sortJobs();
  });
}

void MainWindow::setupJobsSorting() {
  //!!!  QObject::connect(ui.actionSortByStatus
  QObject::connect(ui.actionSortByStatus, &QAction::triggered, this, [=]() {
    ui.buttonSortByTime->setStyleSheet("QToolButton {border: 0;}");
    ui.buttonSortByStatus->setStyleSheet("QToolButton {}");

    if (mJobsSort != "byDate") {
      mJobsStatusSortOrder = !mJobsStatusSortOrder;
    }

    mJobsSort = "byStatus";

    if (mJobsStatusSortOrder) {
      ui.buttonSortByStatus->setIcon(mSortAZIcon);
    } else {
      ui.buttonSortByStatus->setIcon(mSortZAIcon);
    }

    sortJobs();
  });

  //!!!  QObject::connect(ui.actionSortByTime
  QObject::connect(ui.actionSortByTime, &QAction::triggered, this, [=]() {
    ui.buttonSortByStatus->setStyleSheet("QToolButton {border: 0;}");
    ui.buttonSortByTime->setStyleSheet("QToolButton {}");

    // flip sort order
    if (mJobsSort != "byStatus") {
      mJobsTimeSortOrder = !mJobsTimeSortOrder;
    }

    mJobsSort = "byDate";

    if (mJobsTimeSortOrder) {
      ui.buttonSortByTime->setIcon(mSortTimeAZIcon);
    } else {
      ui.buttonSortByTime->setIcon(mSortTimeZAIcon);
    }

    sortJobs();
  });
}

void MainWindow::setupListSignalsAndIcons() {
  QObject::connect(ui.queueListWidget, &QListWidget::itemChanged, this,
                   [=]() { setQueueButtons(); });

  QObject::connect(ui.queueListWidget, &QListWidget::itemClicked, this,
                   [=]() { setQueueButtons(); });

  QObject::connect(ui.queueListWidget, &QListWidget::currentItemChanged, this,
                   [=]() { setQueueButtons(); });

  QObject::connect(ui.queueListWidget, &QListWidget::itemSelectionChanged, this,
                   [=]() { setQueueButtons(); });

  QObject::connect(ListOfJobOptions::getInstance(),
                   &ListOfJobOptions::tasksListUpdated, this,
                   &MainWindow::listTasks);

  mUploadIcon = mArrowUpIcon;
  mDownloadIcon = mArrowDownIcon;
  mMountIcon = mMount1Icon;
}

void MainWindow::setupTabs() {
  // remove close button from these tabs
  ui.tabs->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
  ui.tabs->tabBar()->setTabButton(0, QTabBar::LeftSide, nullptr);
  ui.tabs->tabBar()->setTabButton(1, QTabBar::RightSide, nullptr);
  ui.tabs->tabBar()->setTabButton(1, QTabBar::LeftSide, nullptr);
  ui.tabs->tabBar()->setTabButton(2, QTabBar::RightSide, nullptr);
  ui.tabs->tabBar()->setTabButton(2, QTabBar::LeftSide, nullptr);
  ui.tabs->tabBar()->setTabButton(3, QTabBar::RightSide, nullptr);
  ui.tabs->tabBar()->setTabButton(3, QTabBar::LeftSide, nullptr);
  ui.tabs->tabBar()->setTabButton(4, QTabBar::RightSide, nullptr);
  ui.tabs->tabBar()->setTabButton(4, QTabBar::LeftSide, nullptr);

  ui.tabs->setCurrentIndex(0);

  ApplyOptionalTabsVisibility(ui.tabs);
  ApplyFixedTabsTextColor(ui.tabs);
}

void MainWindow::setupTrayAndStartup() {
  auto settings = GetSettings();

  QObject::connect(&mSystemTray, &QSystemTrayIcon::activated, this,
                   [=](QSystemTrayIcon::ActivationReason reason) {
                     if (reason == QSystemTrayIcon::DoubleClick ||
                         reason == QSystemTrayIcon::Trigger) {
                       showNormal();
                       mSystemTray.setVisible(mAlwaysShowInTray);
#ifdef Q_OS_MACOS
                       osxShowDockIcon();
#endif
                     }
                   });

  QObject::connect(&mSystemTray, &QSystemTrayIcon::messageClicked, this, [=]() {
    showNormal();
    mSystemTray.setVisible(mAlwaysShowInTray);
#ifdef Q_OS_MACOS
    osxShowDockIcon();
#endif

    ui.tabs->setCurrentIndex(1);
    if (mLastFinished) {
      mLastFinished->showDetails();
      ui.jobsArea->ensureWidgetVisible(mLastFinished);
    }
  });

  QMenu *trayMenu = new QMenu(this);
  QObject::connect(
      trayMenu->addAction("&Show"), &QAction::triggered, this, [=]() {
        MainWindow::setWindowState((windowState() & ~Qt::WindowMinimized) |
                                   Qt::WindowActive);
        MainWindow::show();  // bring window to top on macOS
        MainWindow::raise(); // bring window from minimized state on macOS
        MainWindow::activateWindow(); // bring window to front/unminimize on
                                      // windows
        mSystemTray.setVisible(mAlwaysShowInTray);
#ifdef Q_OS_MACOS
        osxShowDockIcon();
#endif
      });

  QObject::connect(trayMenu->addAction("&Quit"), &QAction::triggered, this,
                   [=]() {
                     if (canClose()) {
                       saveQueueFile();
                       saveSchedulerFile();
                       stopDaemonAndWait();
                       QApplication::quit();
                     }
                   });

  mSystemTray.setContextMenu(trayMenu);

  mStatusMessage = new QLabel();
  ui.statusBar->addWidget(mStatusMessage);
  ui.statusBar->setStyleSheet("QStatusBar::item { border: 0; }");

  QTimer::singleShot(0, ui.remotes, SLOT(setFocus()));

  QString rclone = GetRclone();
  if (rclone.isEmpty()) {
    rclone = QStandardPaths::findExecutable("rclone");
#ifdef Q_OS_MACOS
    // apps started from Finder get a minimal PATH without the Homebrew
    // (/opt/homebrew/bin on Apple Silicon), rclone installer and MacPorts
    // locations
    if (rclone.isEmpty()) {
      rclone = QStandardPaths::findExecutable(
          "rclone", {"/opt/homebrew/bin", "/usr/local/bin", "/opt/local/bin"});
    }
#endif
    if (rclone.isEmpty()) {
      QMessageBox::information(
          this, "Error",
          "Cannot check rclone version!\nPlease verify rclone location.");
      emit ui.preferences->trigger();
    } else {
      auto settings = GetSettings();
      settings->setValue("Settings/rclone", rclone);
      SetRclone(rclone);
      rcloneGetVersion();
    }
  } else {
    rcloneGetVersion();
  }

  // we start all auto mount tasks with 1s delay - so RB has chance to start
  // properly
  QTimer::singleShot(1000, Qt::CoarseTimer, this, SLOT(autoStartMounts()));

  // start minimised to tray
  if ((settings->value("Settings/startMinimisedToTray").toBool())) {
#ifdef Q_OS_MACOS
    osxHideDockIcon();
#endif
    mSystemTray.show();
    QTimer::singleShot(0, this, SLOT(hide()));
  }
}

MainWindow::~MainWindow() {
  // the daemon (a child) goes away after this body: nobody may use it then
  global.daemon = nullptr;

  auto settings = GetSettings();
  settings->setValue("MainWindow/geometry", saveGeometry());
}

QList<QListWidgetItem *>
MainWindow::sortListWidget(const QList<QListWidgetItem *> &list,
                           bool sortOrder) {

  QList<QListWidgetItem *> sortedList = list;

  int n = sortedList.size();
  int min_idx;

  // basic selection sort algorithm
  for (int i = 0; i < (n - 1); i++) {
    min_idx = i;
    for (int j = i + 1; j < n; j++) {

      JobOptionsListWidgetItem *item_j =
          static_cast<JobOptionsListWidgetItem *>(sortedList.at(j));
      JobOptions *jo_j = item_j->GetData();

      JobOptionsListWidgetItem *item_min_idx =
          static_cast<JobOptionsListWidgetItem *>(sortedList.at(min_idx));
      JobOptions *jo_min_idx = item_min_idx->GetData();

      if (sortOrder) {
        if ((jo_j->description).toUpper() <
            (jo_min_idx->description).toUpper()) {
          min_idx = j;
        }
      } else {
        if ((jo_j->description).toUpper() >
            (jo_min_idx->description).toUpper()) {
          min_idx = j;
        }
      }
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    sortedList.swapItemsAt(min_idx, i);
#else
    sortedList.swap(min_idx, i);
#endif
  }
  return sortedList;
}

void MainWindow::autoStartMounts(void) {

  // autostart all auto mounts from tasks list
  for (int k = 0; k < ui.tasksListWidget->count(); k = k + 1) {
    JobOptionsListWidgetItem *item =
        static_cast<JobOptionsListWidgetItem *>(ui.tasksListWidget->item(k));
    JobOptions *joTasks = item->GetData();
    if (joTasks->operation == JobOptions::Mount && joTasks->mountAutoStart) {
      runItem(item, "autostart", "requestID_placeholder");
    }
  }
}

void MainWindow::quitApp(void) {
  // wait for all processes to stop
  if (mQuitInfoDelay == 3) {

    QMessageBox *msgBox = new QMessageBox(
        QMessageBox::Warning, "Quitting",
        "Terminating all processes\nbefore quitting, please wait.",
        QMessageBox::NoButton, this);
    msgBox->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint |
                           Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    msgBox->setStandardButtons(QMessageBox::NoButton);
    msgBox->setCursor(Qt::WaitCursor);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    msgBox->show();

    mQuittingErrorMsgBox = msgBox;
  }

  bool processActive = false;
  bool unmountingFailed = false;
  int widgetsCount = ui.jobs->count();

  // loop over all jobs and clean them
  for (int i = widgetsCount - 2; i >= 0; i = i - 2) {
    QWidget *widget = ui.jobs->itemAt(i)->widget();
    if (auto mount = qobject_cast<MountWidget *>(widget)) {
      if (mount->isRunning) {
        processActive = true;
        if (mount->getUnmountingError() != "0") {
          // there is failed unmount - quitting fails
          // but loop continues closing what possible
          unmountingFailed = true;
        }
      } else {
        emit mount->closed();
      }
    } else if (auto transfer = qobject_cast<JobWidget *>(widget)) {
      if (transfer->isRunning) {
        processActive = true;
      } else {
        emit transfer->closed();
      }
    } else if (auto stream = qobject_cast<StreamWidget *>(widget)) {
      if (stream->isRunning) {
        processActive = true;
      } else {
        emit stream->closed();
      }
    }
  };

  if (unmountingFailed) {
    // quitting failed
    // reset to 3 attempts again
    mQuitInfoDelay = 3;

    if (mQuittingErrorMsgBox != NULL) {
      mQuittingErrorMsgBox->hide();
      mQuittingErrorMsgBox = NULL;
    }

    QMessageBox::critical(
        this, "Unmounting failed",
        QString("Some mounts can't be unmounted. Make sure that they are "
                "not used by other programs. You can also try to unmount "
                "them directly from your OS."));

    mAppQuittingStatus = false;
    return;
  }

  if (processActive == false) {
    // no running widget - bye bye - quitting at last
    saveQueueFile();
    saveSchedulerFile();
    stopDaemonAndWait();
    QApplication::quit();
  } else {
    // something still running we check again a bit later then
    QTimer::singleShot(200, Qt::CoarseTimer, this, SLOT(quitApp()));
    ++mQuitInfoDelay;
  }
}

void MainWindow::rcloneGetVersion() {
  bool firstTime = mFirstTime;
  mFirstTime = false;

  QProcess *p = new QProcess();

  QObject::connect(
      p,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      this, [=](int code, QProcess::ExitStatus) {
        if (code == 0) {
          QString version = p->readAllStandardOutput().trimmed();

          // first line of the output, used in the status bar
          const QString rclone_info1 = version.section('\n', 0, 0).trimmed();

          const RcloneVersion rcloneVersion =
              RcloneVersion::fromVersionOutput(version);

          // save current version no in settings
          auto settings = GetSettings();
          settings->setValue("Settings/rcloneVersion",
                             rcloneVersion.toString());

          // rclone older than the minimum (or unrecognised output): block
          // everything else that normally follows a successful version check
          const QString versionProblem = rcloneVersionProblem(version);
          if (!versionProblem.isEmpty()) {
            p->deleteLater();
            showUnsupportedRcloneDialog(versionProblem);
            return;
          }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 1)
          QStringList lines = version.split("\n", Qt::SkipEmptyParts);
#else
          QStringList lines = version.split("\n", QString::SkipEmptyParts);
#endif

          QString rclone_info2;
          QString rclone_info3;

          int counter = 0;
          foreach (QString line, lines) {
            line = line.trimmed();
            if (counter == 1)
              rclone_info2 = line.replace("- ", "");
            if (counter == 2)
              rclone_info3 = line.replace("- ", "");
            counter++;
          };

          QFileInfo appBundlePath;
#ifdef Q_OS_MACOS
          if (IsPortableMode()) {

            QFileInfo applicationPath = qApp->applicationFilePath();
            QFileInfo MacOSPath = applicationPath.dir().path();
            QFileInfo ContentsPath = MacOSPath.dir().path();
            appBundlePath = ContentsPath.dir().path();

            mStatusMessage->setText(rclone_info1 + ", " + rclone_info2 + ", " +
                                    rclone_info3);

            mStatusMessage->setToolTip(
                rclone_info1 + " in " +
                QDir::toNativeSeparators(GetRclone().replace(
                    appBundlePath.fileName() + "/Contents/MacOS/../../../",
                    "")) +
                ", " + rclone_info2 + ", " + rclone_info3);

          } else {

            mStatusMessage->setText(rclone_info1 + ", " + rclone_info2 + ", " +
                                    rclone_info3);

            mStatusMessage->setToolTip(
                rclone_info1 + " in " + QDir::toNativeSeparators(GetRclone()) +
                ", " + rclone_info2 + ", " + rclone_info3);
          }
#else
#ifdef Q_OS_WIN
          mStatusMessage->setText(rclone_info1 + ", " +
                                  rclone_info2 + ", " + rclone_info3);


          mStatusMessage->setToolTip(rclone_info1 + " in " +
                                  QDir::toNativeSeparators(GetRclone()) + ", " +
                                  rclone_info2 + ", " + rclone_info3);

#else
          if (IsPortableMode()) {
            QString xdg_config_home = qgetenv("XDG_CONFIG_HOME");
            QString appImageConfigFolder = xdg_config_home.right(xdg_config_home.length()-xdg_config_home.lastIndexOf("/"));

            mStatusMessage->setText(rclone_info1 + ", " +
                                  rclone_info2 + ", " + rclone_info3);

            mStatusMessage->setToolTip(rclone_info1 + " in " +
                                  QDir::toNativeSeparators(GetRclone().replace(appImageConfigFolder + "/..",  "")) + ", " +
                                  rclone_info2 + ", " + rclone_info3);


          } else {
            mStatusMessage->setText(rclone_info1 + ", " +
                                  rclone_info2 + ", " + rclone_info3);

            mStatusMessage->setToolTip(rclone_info1 + " in " +
                                  QDir::toNativeSeparators(GetRclone()) + ", " +
                                  rclone_info2 + ", " + rclone_info3);


         }
#endif
#endif

          mDaemon->start();
          rcloneListRemotes();
          startUpdateChecks(rcloneVersion);
        } else {
          if (p->error() != QProcess::FailedToStart) {
            if (getConfigPassword(p)) {
              rcloneGetVersion();
            } else {
              close();
            }
            p->deleteLater();
            return;
          }

          if (firstTime) {
            if (p->error() == QProcess::FailedToStart) {
              QMessageBox::information(
                  this, "Error",
                  "Wrong rclone executable or rclone not found!\nPlease select "
                  "its location in next dialog.");
            } else {
              QMessageBox::information(this, "Error",
                                       "Cannot check rclone version!\nPlease "
                                       "verify rclone location.");
            }
            emit ui.preferences->trigger();
          }
        }

        p->deleteLater();
      });

  QObject::connect(
      p, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        QString errorString =
            QMetaEnum::fromType<QProcess::ProcessError>().valueToKey(error);

        QMessageBox::information(
            this, "Error",
            "Cannot start rclone\n\n Error: " + errorString +
                "\n\nPlease verify rclone excecutable location.");
        emit ui.preferences->trigger();
      });

  ApplyRcloneEnvironment(p);
  p->start(GetRclone(),
           QStringList() << "version"
                         << "--ask-password=false",
           QIODevice::ReadOnly);
}

void MainWindow::showUnsupportedRcloneDialog(const QString &problem) {
  // "Download rclone" shows the dialog again, the other buttons end it
  for (;;) {
    QMessageBox box(QMessageBox::Critical, "Unsupported rclone version",
                    problem + "\n\nrclone path: " + GetRclone(),
                    QMessageBox::NoButton, this);
    QPushButton *preferences =
        box.addButton("Preferences...", QMessageBox::AcceptRole);
    QPushButton *download =
        box.addButton("Download rclone", QMessageBox::ActionRole);
    QPushButton *quit = box.addButton("Quit", QMessageBox::RejectRole);
    box.setEscapeButton(quit);
    box.exec();

    if (box.clickedButton() == download) {
      QDesktopServices::openUrl(QUrl("https://rclone.org/downloads/"));
      continue;
    }
    if (box.clickedButton() == preferences) {
      // accepting Preferences runs rcloneGetVersion() again
      emit ui.preferences->trigger();
    } else if (canClose()) {
      // quit even when "close to tray" is set (close() would only hide)
      quitApp();
    }
    return;
  }
}

void MainWindow::startUpdateChecks(const RcloneVersion &rcloneVersion) {
  auto settings = GetSettings();
  const QDate today = QDate::currentDate();
  const QString todayText = today.toString(Qt::ISODate);

  // rclone (any repository chosen in Preferences)
  if (!(settings->contains("Settings/checkRcloneUpdates"))) {
    settings->setValue("Settings/checkRcloneUpdates", true);
  }
  if (settings->value("Settings/checkRcloneUpdates").toBool() &&
      updateCheckDue(
          settings->value("Settings/lastRcloneUpdateCheck").toString(),
          today)) {
    const QString slug =
        settings->value("Settings/rcloneUpdateRepo", "rclone/rclone")
            .toString()
            .trimmed();
    if (!isValidRepoSlug(slug)) {
      qWarning().noquote()
          << "rclone update check skipped: invalid repository" << slug;
    } else {
      settings->setValue("Settings/lastRcloneUpdateCheck", todayText);
      mRcloneUpdateChecker->check(slug, rcloneVersion);
    }
  }

  // Rclone Browser itself
  if (!(settings->contains("Settings/checkRcloneBrowserUpdates"))) {
    settings->setValue("Settings/checkRcloneBrowserUpdates", true);
  }
  if (settings->value("Settings/checkRcloneBrowserUpdates").toBool() &&
      updateCheckDue(
          settings->value("Settings/lastRcloneBrowserUpdateCheck").toString(),
          today)) {
    const RcloneVersion current =
        RcloneVersion::fromTag(QString::fromLatin1(RCLONE_BROWSER_VERSION));
    if (!current.valid) {
      qWarning().noquote()
          << "Rclone Browser update check skipped: version"
          << QString::fromLatin1(RCLONE_BROWSER_VERSION)
          << "is not major.minor.patch";
    } else {
      settings->setValue("Settings/lastRcloneBrowserUpdateCheck", todayText);
      mBrowserUpdateChecker->check(QString::fromLatin1(kBrowserUpdateRepo),
                                   current);
    }
  }
}

namespace {

bool daemonRunning() {
  const QPointer<RcloneDaemon> daemon(global.daemon);
  return daemon && daemon->state() == RcloneDaemon::State::Running;
}

// Runs the dialog modally; true when the remote list may have changed: the
// remote was created or updated, or the flow started and was left (a partial
// remote whose cleanup failed must still show up in the list).
bool runRemoteConfigDialog(QWidget *parent, RemoteConfigDialog::Mode mode,
                           const QString &name) {
  auto ctx = std::make_shared<QPointer<QObject>>();
  RemoteConfigDialog dialog(
      daemonRcCall(QPointer<RcloneDaemon>(global.daemon), ctx), mode, name,
      parent);
  *ctx = &dialog;
  const bool accepted = dialog.exec() == QDialog::Accepted;
  return accepted || dialog.flowStarted();
}

} // namespace

void MainWindow::openRemoteConfig() {
  if (!daemonRunning()) {
    // no private rcd: the terminal flow is the fallback
    rcloneConfig();
    return;
  }
  if (runRemoteConfigDialog(this, RemoteConfigDialog::Mode::Create,
                            QString())) {
    rcloneListRemotes();
  }
}

void MainWindow::editRemote(const QString &name) {
  if (runRemoteConfigDialog(this, RemoteConfigDialog::Mode::Update, name)) {
    rcloneListRemotes();
  }
}

void MainWindow::showRemotesMenu(const QPoint &pos) {
  if (QListWidgetItem *item = ui.remotes->itemAt(pos)) {
    ui.remotes->setCurrentItem(item);
  }
  // The name is read now: the menu runs a nested event loop, and a remote
  // list refresh meanwhile deletes the items.
  const QList<QListWidgetItem *> selected = ui.remotes->selectedItems();
  const QString remoteName =
      selected.isEmpty() ? QString() : selected.first()->text();

  QMenu menu(this);
  QAction *edit = menu.addAction(QStringLiteral(u"Edit remote\u2026"));
  edit->setObjectName("actionEditRemote");
  edit->setEnabled(!remoteName.isEmpty() && daemonRunning());
  QAction *advanced = menu.addAction(QStringLiteral(u"Advanced (terminal)\u2026"));
  advanced->setObjectName("actionAdvancedConfig");

  QAction *chosen = menu.exec(ui.remotes->viewport()->mapToGlobal(pos));
  if (chosen == edit && !remoteName.isEmpty()) {
    editRemote(remoteName);
  } else if (chosen == advanced) {
    rcloneConfig();
  }
}

void MainWindow::rcloneConfig() {

#if defined(Q_OS_WIN32) && (QT_VERSION < QT_VERSION_CHECK(5, 7, 0))
  QProcess::startDetached(GetRclone(), QStringList()
                                           << "config" << GetRcloneConf());
  return;
#else

  QProcess *p = new QProcess(this);

  QObject::connect(p,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, [=](int, QProcess::ExitStatus) {
                     // the config may have changed even on a non-zero exit
                     emit rcloneListRemotes();
                     p->deleteLater();
                   });
#endif

#if defined(Q_OS_WIN32)
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
  p->setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NEW_CONSOLE;
        args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
      });
  p->setProgram(GetRclone());
  p->setArguments(QStringList() << "config" << GetRcloneConf());
#endif

#elif defined(Q_OS_MACOS)
  // unique file in the per-user temp dir instead of the shared /tmp path
  QTemporaryFile tmp(QDir::tempPath() + "/rclone_config_XXXXXX.command");
  tmp.setAutoRemove(false);
  if (tmp.open()) {
    QTextStream(&tmp) << "#!/bin/sh\n"
                      << rcloneConfigShellCommand(GetRclone(), GetRcloneConf())
                      << "\n";
    tmp.close();
    tmp.setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser |
                       QFileDevice::ExeUser | QFileDevice::ReadGroup |
                       QFileDevice::ExeGroup | QFileDevice::ReadOther |
                       QFileDevice::ExeOther);
  }
  p->setProgram("open");
  p->setArguments(QStringList() << tmp.fileName());
#else
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  QString terminalPath;
  for (const QString &name : terminalCandidates(env)) {
    QFileInfo info(name);
    if (info.isAbsolute()) {
      // $TERMINAL may hold a full path: use it as is when it is executable
      if (info.isFile() && info.isExecutable()) {
        terminalPath = name;
      }
    } else {
      terminalPath = QStandardPaths::findExecutable(name);
    }
    if (!terminalPath.isEmpty()) {
      break;
    }
  }
  if (terminalPath.isEmpty()) {
    QMessageBox::critical(this, "Error",
                          "Not sure how to launch terminal!\n"
                          "Please set path to terminal executable in "
                          "$TERMINAL environment variable.",
                          QMessageBox::Ok);
    return;
  }

  TerminalLaunch launch = buildTerminalLaunch(
      terminalPath, rcloneConfigShellCommand(GetRclone(), GetRcloneConf()));
  p->setProgram(launch.program);
  p->setArguments(launch.args);
#endif

#if !defined(Q_OS_WIN32) || (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
  ApplyRcloneEnvironment(p, RcloneEnvMode::Interactive);
  p->start(QIODevice::NotOpen);
#endif
}

void MainWindow::rcloneListRemotes() {

  ui.remotes->clear();

  auto settings = GetSettings();
  QString mIconsLayout = settings->value("Settings/iconsLayout").toString();

  QProcess *p = new QProcess();

  QObject::connect(
      p,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      this, [=](int code, QProcess::ExitStatus) {
        std::optional<QVector<RemoteInfo>> remotes;
        if (code == 0) {
          remotes = parseListRemotesJson(p->readAllStandardOutput());
        }
        if (remotes) {
          QStyle *style = qApp->style();

          // sort remotes by name in plain character-code order (case
          // sensitive, uppercase before lowercase), like rclone listremotes
          std::sort(remotes->begin(), remotes->end(),
                    [](const RemoteInfo &a, const RemoteInfo &b) {
                      return a.name.compare(b.name, Qt::CaseSensitive) < 0;
                    });

          auto settings = GetSettings();
          bool darkModeIni = settings->value("Settings/darkModeIni").toBool();
          QString iconSize = settings->value("Settings/iconSize").toString();
          QString iconsColour =
              settings->value("Settings/iconsColour").toString();

          for (const RemoteInfo &ri : *remotes) {
            QString name = ri.name;
            QString type = ri.type;
            QString tooltip = "type: " + type + "\n\nname: " + name;
            if (!ri.description.isEmpty()) {
              tooltip += "\n\ndescription: " + ri.description;
            }

            QString img_add = "";
            int size;

            // medium scale by default
            double darkModeIconScale = 1.333;
            double lightModeiconScale = 2;
            // to avoid "variable not used" compiler error
            if (darkModeIconScale == lightModeiconScale) {
            };

            // icons scale from preferences (number, or old letter values)
            lightModeiconScale = RemotesIconScaleFromSetting(iconSize);
            darkModeIconScale = lightModeiconScale;

            // disable scaling - all is fusion now
            // let's leave scaling logic for now
            darkModeIconScale = lightModeiconScale;

#if !defined(Q_OS_MACOS)
            // _inv only for dark mode
            // we use darkModeIni to apply mode active at startup
            if (darkModeIni) {
              if (iconsColour == "white") {
                img_add = "_inv";
              } else {
                img_add = "";
              }
            } else {
              img_add = "";
            }
#if defined(Q_OS_WIN)
            // on Windows dark theme changes PM_ListViewIconSize size
            // so we have to adjust
            if (darkModeIni) {
              size = darkModeIconScale *
                     style->pixelMetric(QStyle::PM_ListViewIconSize);
            } else {
              size = lightModeiconScale *
                     style->pixelMetric(QStyle::PM_ListViewIconSize);
            }
#else
             // for Linux/BSD PM_ListViewIconSize stays the same
             size = lightModeiconScale * style->pixelMetric(QStyle::PM_ListViewIconSize);
#endif
#else
             QString sysInfo = QSysInfo::productVersion();
             // dark mode on older macOS
             if (sysInfo == "10.9" ||
                 sysInfo == "10.10" ||
                 sysInfo == "10.11" ||
                 sysInfo == "10.12" ||
                 sysInfo == "10.13") {

               // on older macOS we also have to adjust icon size per mode
               if (darkModeIni) {
                 size = darkModeIconScale * style->pixelMetric(QStyle::PM_ListViewIconSize);
                 if (iconsColour == "white") {
                   img_add = "_inv";
                 } else {
                   img_add = "";
                 }
               } else {
                 size = lightModeiconScale * style->pixelMetric(QStyle::PM_ListViewIconSize);
                 img_add = "";
               }

             } else {
               // for macOS > 10.13 native dark mode does not change IconSize base
               size = 1.5 * lightModeiconScale * style->pixelMetric(QStyle::PM_ListViewIconSize);
               if (iconsColour == "white") {
                  img_add = "_inv";
               } else {
                   img_add = "";
               }
             }
#endif
            ui.remotes->setIconSize(QSize(size, size));

            if (mIconsLayout == "tiles") {
              ui.remotes->setViewMode(QListWidget::IconMode);
              // disable drag and drop
              ui.remotes->setMovement(QListView::Static);
              // always adjust icons after the window is resized
              ui.remotes->setResizeMode(QListView::Adjust);
              ui.remotes->setWrapping(true);
              ui.remotes->setGridSize(QSize(size + 20, size + 40));
              ui.remotes->setSpacing(10);
              ui.remotes->setTextElideMode(Qt::ElideMiddle);
            }
            if (mIconsLayout == "longlist") {
              ui.remotes->setViewMode(QListWidget::ListMode);
              ui.remotes->setResizeMode(QListView::Adjust);
              ui.remotes->setWrapping(false);
              ui.remotes->setGridSize(QSize(size + 8, size + 8));
            }
            if (mIconsLayout == "list") {
              ui.remotes->setViewMode(QListWidget::ListMode);
              ui.remotes->setResizeMode(QListView::Adjust);
              ui.remotes->setWrapping(true);
              ui.remotes->setGridSize(QSize(size + 100, size + 20));
              ui.remotes->setSpacing(10);
            }

            QString path = ":media/images/remotes_icons/" +
                           type.replace(' ', '_') + img_add + ".png";
            QIcon icon(QFile(path).exists()
                           ? path
                           : ":media/images/remotes_icons/unknown" + img_add +
                                 ".png");

            QListWidgetItem *item = new QListWidgetItem(icon, name);
            item->setData(Qt::UserRole, type);
            item->setToolTip(tooltip);
            ui.remotes->addItem(item);
          }
        } else {
          if (p->error() != QProcess::FailedToStart) {
            if (getConfigPassword(p)) {
              rcloneListRemotes();
            }
          }
        }
        p->deleteLater();
        ui.open->setEnabled(false);
      });

  QObject::connect(
      p, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        QString errorString =
            QMetaEnum::fromType<QProcess::ProcessError>().valueToKey(error);

        QMessageBox::information(
            this, "Error",
            "Cannot start rclone\n\n Error: " + errorString +
                "\n\nPlease verify rclone excecutable location.");
        emit ui.preferences->trigger();
      });

  ApplyRcloneEnvironment(p);
  p->start(GetRclone(),
           QStringList() << "listremotes" << GetRcloneConf()
                         << GetDefaultOptionsList("defaultRcloneOptions")
                         << "--long"
                         << "--json"
                         << "--ask-password=false",
           QIODevice::ReadOnly);
}

void MainWindow::stopDaemonAndWait() {
  auto active = [this]() {
    return mDaemon->state() == RcloneDaemon::State::Starting ||
           mDaemon->state() == RcloneDaemon::State::Running;
  };
  if (!active()) {
    return;
  }

  QEventLoop loop;
  // The loop is the connection context, so Qt also drops the connection when
  // the loop is destroyed; disconnect explicitly so it never outlives it.
  const QMetaObject::Connection conn = QObject::connect(
      mDaemon, &RcloneDaemon::stateChanged, &loop,
      [&loop](RcloneDaemon::State state) {
        if (state == RcloneDaemon::State::Stopped ||
            state == RcloneDaemon::State::Failed) {
          loop.quit();
        }
      });
  QTimer::singleShot(4000, &loop, &QEventLoop::quit);
  mDaemon->stop();
  if (active()) {
    loop.exec();
  }
  QObject::disconnect(conn);
}

void MainWindow::unlockDaemonSilently() {
  const QString password = GetRclonePassword();
  if (mDaemon->state() != RcloneDaemon::State::Running || password.isEmpty()) {
    return;
  }
  mDaemon->client()->call("config/unlock",
                          QJsonObject{{"configPassword", password}}, this,
                          [](const RcReply &) {});
}

void MainWindow::ensureRcUnlocked() {
  if (mRcUnlockBusy) {
    return;
  }
  mRcUnlockBusy = true;
  promptRcUnlock(false);
}

void MainWindow::promptRcUnlock(bool wrongPassword) {
  bool ok = false;
  const QString password = QInputDialog::getText(
      this, qApp->applicationDisplayName(), configPasswordLabel(wrongPassword),
      QLineEdit::Password, QString(), &ok);
  if (!ok) {
    mRcUnlockBusy = false;
    return;
  }
  SetRclonePassword(password);

  // a locked config asks again, any other error ends the loop
  auto failed = [this](const QString &error) {
    const ConfigLoadProblem problem =
        classifyConfigLoadError(1, error.toUtf8());
    if (problem == ConfigLoadProblem::NeedsPassword ||
        problem == ConfigLoadProblem::WrongPassword) {
      SetRclonePassword("");
      promptRcUnlock(true);
      return;
    }
    mRcUnlockBusy = false;
    statusBar()->showMessage("rclone backend: " + error);
  };

  mDaemon->client()->call(
      "config/unlock", QJsonObject{{"configPassword", password}}, this,
      [this, failed](const RcReply &unlocked) {
        if (!unlocked.ok) {
          failed(unlocked.error);
          return;
        }
        // R17: unlock is ok even for a wrong password, listremotes tells
        mDaemon->client()->call(
            "config/listremotes", QJsonObject(), this,
            [this, failed](const RcReply &verified) {
              if (!verified.ok) {
                failed(verified.error);
                return;
              }
              mRcUnlockBusy = false;
              // same code path as the Refresh button of each remote view
              for (int i = 0; i < ui.tabs->count(); i++) {
                auto remoteTab = qobject_cast<RemoteWidget *>(ui.tabs->widget(i));
                if (!remoteTab) {
                  continue;
                }
                // a disabled refresh means the view has no usable selection
                // (the handler needs a selected row): reload its root instead
                QAction *refresh = remoteTab->findChild<QAction *>("refresh");
                if (refresh && refresh->isEnabled()) {
                  refresh->trigger();
                } else if (auto *model = remoteTab->findChild<ItemModel *>()) {
                  const QModelIndex root = model->index(0, 0, QModelIndex());
                  if (root.isValid()) {
                    model->refresh(root);
                  }
                }
              }
            },
            30000);
      },
      30000);
}

bool MainWindow::getConfigPassword(QProcess *p) {
  const QByteArray stderrBytes = p->readAllStandardError();
  const ConfigLoadProblem problem =
      classifyConfigLoadError(p->exitCode(), stderrBytes);

  switch (problem) {
  case ConfigLoadProblem::NeedsPassword:
  case ConfigLoadProblem::WrongPassword: {
    if (problem == ConfigLoadProblem::WrongPassword) {
      SetRclonePassword("");
    }
    bool ok;
    QString password = QInputDialog::getText(
        this, qApp->applicationDisplayName(),
        configPasswordLabel(problem == ConfigLoadProblem::WrongPassword),
        QLineEdit::Password, QString(), &ok);
    if (ok) {
      SetRclonePassword(password);
      // a daemon that is already running has not seen this password yet
      unlockDaemonSilently();
      return true;
    }
    return false;
  }
  case ConfigLoadProblem::PasswordCommandFailed:
    QMessageBox::warning(this, qApp->applicationDisplayName(),
                         "RCLONE_PASSWORD_COMMAND failed:\n" +
                             lastRcloneStderrMessage(stderrBytes));
    return false;
  case ConfigLoadProblem::None:
  case ConfigLoadProblem::Other:
    break;
  }

  if (QString::fromUtf8(stderrBytes).trimmed().indexOf(
          "unknown command \"listremotes\"") > 0) {
    QMessageBox::critical(this, qApp->applicationDisplayName(),
                          "It seems rclone version you are using is too "
                          "old.\nPlease upgrade to the latest version");
    return false;
  }
  return false;
}

bool MainWindow::canClose() {
  if (mJobCount == 0) {
    return true;
  }

  bool wasVisible = isVisible();

  ui.tabs->setCurrentIndex(1);
  showNormal();

  int button = QMessageBox::question(
      this, "Rclone Browser",
      QString("There are %1 job(s) running.\n"
              "\nDo you want to stop them and quit?")
          .arg(mJobCount),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (!wasVisible) {
    hide();
  }

  if (button == QMessageBox::Yes) {
    // make sure terminated job is not removed from the queue
    // we make close process aware that it is quitting
    mAppQuittingStatus = true;

    int widgetsCount = ui.jobs->count();
    for (int i = widgetsCount - 2; i >= 0; i = i - 2) {
      QWidget *widget = ui.jobs->itemAt(i)->widget();
      if (auto mount = qobject_cast<MountWidget *>(widget)) {
        if (mount->isRunning) {
          emit mount->cancel();
        } else {
          emit mount->closed();
        }

      } else if (auto transfer = qobject_cast<JobWidget *>(widget)) {
        if (transfer->isRunning) {
          emit transfer->cancel();
        } else {
          emit transfer->closed();
        }
      } else if (auto stream = qobject_cast<StreamWidget *>(widget)) {
        if (stream->isRunning) {
          emit stream->cancel();
        } else {
          emit stream->closed();
        }
      }
    }
    return true;
  }
  return false;
}

void MainWindow::closeEvent(QCloseEvent *ev) {
  if (mCloseToTray && isVisible()) {
#ifdef Q_OS_MACOS
    osxHideDockIcon();
#endif
    mSystemTray.show();
    hide();
    ev->ignore();
    return;
  }

  if (canClose()) {
    // quitApp() will wait for everything to finish
    quitApp();
    ev->ignore();
  } else {
    ev->ignore();
  }
}

void MainWindow::addTransfer(const QString &message, const QString &source,
                             const QString &dest, const QStringList &args,
                             const QString &uniqueId,
                             const QString &transferMode,
                             const QString &requestId) {

  QProcess *transfer = new QProcess(this);
  transfer->setProcessChannelMode(QProcess::MergedChannels);

  // The app owns the logging flags and the environment (I-1).
  const TransferArgs ta = PrepareTransferArgs(args);

  auto widget = new JobWidget(transfer, message, ta.args, source, dest,
                              uniqueId, transferMode, requestId);
  widget->noteIgnoredArgs(ta.ignored);
  if (!ta.logFile.isEmpty()) {
    widget->setLogFile(ta.logFile);
  }

  auto line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  //!!!  QObject::connect(  widget, &JobWidget::finished
  QObject::connect(
      widget, &JobWidget::finished, this,
      [=](const QString &info, const QString &jobFinalStatus) {
        QMutexLocker locker(&mMutex);

        if (mNotifyFinishedTransfers) {
          mLastFinished = widget;
#if defined(Q_OS_WIN)
          mSystemTray.showMessage(
              "Rclone Browser - transfer " + jobFinalStatus, info,
              QIcon(":media/images/program_icons/rclone-browser512.png"));
#else
#if defined(Q_OS_MACOS)
          MacOsNotification::Display(
              "Rclone Browser - transfer " + jobFinalStatus, info);
#else
          mSystemTray.showMessage("Rclone Browser - transfer " + jobFinalStatus,
                                  info, QSystemTrayIcon::Information);
#endif
#endif
        }

        if (mSoundNotif) {
          // play notification sound
#ifdef RB_HAVE_MULTIMEDIA
          if (mNotificationSound.source().isEmpty()) {
            mNotificationSound.setSource(
                QUrl("qrc:/media/sounds/notification-sound.wav"));
          }
          mNotificationSound.play();
#endif
        }

        --mTransferJobCount;

        if (mTransferJobCount == 0) {

      // allow entering sleep
#if defined(Q_OS_WIN)
          SetThreadExecutionState(ES_CONTINUOUS);
#endif
#if defined(Q_OS_MACOS)
          mMacOsPowerSaving->resumePowerSaving();
#endif
          // run custom script
          auto settings = GetSettings();
          QString transferOffScript =
              settings->value("Settings/transferOffScript").toString();

          bool jobLastFinishedScriptRun =
              settings->value("Settings/jobLastFinishedScriptRun", false)
                  .toBool();
          if (jobLastFinishedScriptRun) {

            if (!transferOffScript.isEmpty()) {
              runScript(transferOffScript);
            }
          }
        }

        if (--mJobCount == 0) {
          ui.tabs->setTabText(1, "Jobs");
        } else {
          ui.tabs->setTabText(1, QString("Jobs (%1)").arg(mJobCount));
        }

        ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
        ui.buttonCleanNotRunning->setEnabled(mJobCount !=
                                             (ui.jobs->count() - 2) / 2);

        // transfer finished - remove darkGreen from tasks list
        auto transfer = qobject_cast<JobWidget *>(widget);
        for (int k = 0; k < ui.tasksListWidget->count(); k = k + 1) {
          JobOptionsListWidgetItem *item =
              static_cast<JobOptionsListWidgetItem *>(
                  ui.tasksListWidget->item(k));
          JobOptions *jo = item->GetData();
          if (transfer->getUniqueID() == jo->uniqueId.toString()) {

            ui.tasksListWidget->item(k)->setBackground(QBrush());
            break;
          }
        }

        // if job finished try to run next one from the queue
        // only when not quitting, queue is active and there are tasks in the
        // queue and there is no running other task

        if (!mAppQuittingStatus && mQueueStatus &&
            ui.queueListWidget->count() > 0) {
          //            ui.queueListWidget->count() > 0 && mTransferJobCount ==
          //            0) {

          auto QueueRunningTask = ui.queueListWidget->item(0);

          JobOptionsListWidgetItem *item =
              static_cast<JobOptionsListWidgetItem *>(QueueRunningTask);
          JobOptions *jo = item->GetData();

          // check if finished task the same as running from the queue
          auto transfer = qobject_cast<JobWidget *>(widget);

          // we also have to check requestId - to distinguish between the same
          // task triggered by queue/scheduler and by user directly if yes we
          // try to run next one
          if ((transfer->getUniqueID() == jo->uniqueId.toString()) &&
              (transfer->getRequestId() == item->GetRequestId())) {
            ui.queueListWidget->takeItem(0);
            --mQueueCount;

            // queue is still running, even if mQueueCount is 0
            if (mQueueCount == 0) {
              mQueueTaskRunning = false;
              ui.tabs->setTabText(3,
                                  QString("Queue (%1)>>(0)").arg(mQueueCount));
              // run queueScript
              auto settings = GetSettings();
              bool queueScriptRun =
                  settings->value("Settings/queueScriptRun", false).toBool();

              if (queueScriptRun) {
                QString queueScript =
                    settings->value("Settings/queueScript").toString();
                if (!queueScript.isEmpty()) {
                  runScript(queueScript);
                }
              }
            }

            // if there is still task to run
            if ((ui.queueListWidget->count()) > 0) {

              auto nextTask = ui.queueListWidget->item(0);
              // check if task is not already running (user could run it
              // manually)

              bool isAlreadyRunning = false;

              JobOptionsListWidgetItem *item =
                  static_cast<JobOptionsListWidgetItem *>(nextTask);
              JobOptions *jo = item->GetData();

              int widgetsCount = ui.jobs->count();
              for (int j = widgetsCount - 2; j >= 0; j = j - 2) {
                QWidget *widget = ui.jobs->itemAt(j)->widget();
                if (auto transfer = qobject_cast<JobWidget *>(widget)) {
                  if ((transfer->getUniqueID() == jo->uniqueId.toString()) &&
                      (transfer->isRunning)) {
                    isAlreadyRunning = true;
                    break;
                  }
                }
              }

              // if the same task not running and nothing else running
              if (!isAlreadyRunning && mTransferJobCount == 0) {
                mQueueTaskRunning = true;
                runItem(item, "queue", item->GetRequestId());

                ui.tabs->setTabText(
                    3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));

                ui.queueListWidget->item(0)->setBackground(Qt::darkGreen);
              } else {

                ui.tabs->setTabText(
                    3, QString("Queue (%1)>>(0)").arg(mQueueCount));

                mQueueTaskRunning = false;
              }
            }
            saveQueueFile();
          } else {
            // finished task was not one from the queue
            // queue is active and have some tasks and if there is nothing else
            // running we start top task from the queue
            if (!mQueueTaskRunning) {
              auto nextTask = ui.queueListWidget->item(0);

              JobOptionsListWidgetItem *item =
                  static_cast<JobOptionsListWidgetItem *>(nextTask);

              if (mTransferJobCount == 0) {

                mQueueTaskRunning = true;
                runItem(item, "queue", item->GetRequestId());
                ui.queueListWidget->item(0)->setBackground(Qt::darkGreen);
                ui.tabs->setTabText(
                    3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
              }

            } else {

              // mQueueTaskRunning = false;
              ui.tabs->setTabText(
                  3, QString("Queue (%1)>>(1)").arg(mQueueCount - 1));
            }
          }
        }

        setTasksButtons();

        // notify schedulers about finished task
        int schedulersCount = ui.schedulers->count();
        for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
          QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();
          if (auto scheduler =
                  qobject_cast<SchedulerWidget *>(schedulerWidget)) {
            // jobFinalStatus = stopped, error, finished

            if (transfer->getRequestId() ==
                scheduler->getSchedulerRequestId()) {
              mRunningSchedulersCount--;
              ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                         .arg(mSchedulersCount)
                                         .arg(mRunningSchedulersCount));

              scheduler->updateTaskStatus(transfer->getRequestId(),
                                          jobFinalStatus);
            }
          }
        }

        // if queue was stopped task terminated but still in the queue
        // then scheduled task is "in the queue" not "stopped"
        // we notify schedulers what is still in the queue

        if (mQueueStatus == false) {
          int itemsCount = ui.queueListWidget->count();
          if (itemsCount > 0) {
            JobOptionsListWidgetItem *item_queue =
                static_cast<JobOptionsListWidgetItem *>(
                    ui.queueListWidget->item(0));

            int schedulersCount = ui.schedulers->count();
            for (int j = schedulersCount - 2; j >= 0; j = j - 2) {
              QWidget *schedulerWidget = ui.schedulers->itemAt(j)->widget();

              if (auto scheduler =
                      qobject_cast<SchedulerWidget *>(schedulerWidget)) {
                // jobFinalStatus = stopped, error, finished
                scheduler->updateTaskStatus(item_queue->GetRequestId(),
                                            "in the queue");

                if (transfer->getRequestId() ==
                        scheduler->getSchedulerRequestId() &&
                    transfer->getRequestId() == item_queue->GetRequestId()) {
                  mRunningSchedulersCount++;
                  ui.tabs->setTabText(4, QString("Scheduler (%1)>>(%2)")
                                             .arg(mSchedulersCount)
                                             .arg(mRunningSchedulersCount));
                }
              }
            }
          }
        }

        // job status changed so we have to sort jobs list

        sortJobs();
      });

  QObject::connect(widget, &JobWidget::closed, this, [=]() {
    if (widget == mLastFinished) {
      mLastFinished = nullptr;
    }

    ui.jobs->removeWidget(widget);
    ui.jobs->removeWidget(line);
    widget->deleteLater();
    delete line;

    int _jobsCount = (ui.jobs->count() - 2) / 2;
    ui.buttonSortByTime->setEnabled(_jobsCount > 1);
    ui.buttonSortByStatus->setEnabled(_jobsCount > 1);

    //    sortJobs();

    if (ui.jobs->count() == 2) {
      ui.noJobsAvailable->show();
    }
    ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
    ui.buttonCleanNotRunning->setEnabled(mJobCount !=
                                         (ui.jobs->count() - 2) / 2);
  });

  if (ui.jobs->count() == 2) {
    ui.noJobsAvailable->hide();
  }

  ui.jobs->insertWidget(0, widget);
  ui.jobs->insertWidget(1, line);
  ++mTransferJobCount;

  int _jobsCount = (ui.jobs->count() - 2) / 2;
  ui.buttonSortByTime->setEnabled(_jobsCount > 1);
  ui.buttonSortByStatus->setEnabled(_jobsCount > 1);

  sortJobs();

  // prevent OS sleep when transfer running
#if defined(Q_OS_WIN)
  SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
                          ES_AWAYMODE_REQUIRED);
#endif
#if defined(Q_OS_MACOS)
  mMacOsPowerSaving->suspendPowerSaving();
#endif

  // run custom script
  auto settings = GetSettings();
  QString transferOnScript =
      settings->value("Settings/transferOnScript").toString();

  bool jobStartScriptRun =
      settings->value("Settings/jobStartScriptRun", false).toBool();

  if (jobStartScriptRun) {

    if (!transferOnScript.isEmpty()) {
      runScript(transferOnScript);
    }
  }

  ui.tabs->setTabText(1, QString("Jobs (%1)").arg(++mJobCount));

  ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
  ui.buttonCleanNotRunning->setEnabled(mJobCount != (ui.jobs->count() - 2) / 2);

  ApplyRcloneEnvironment(transfer);
  transfer->start(GetRclone(), ta.args + GetRcloneConf(),
                  QIODevice::ReadOnly);

  ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
  ui.buttonCleanNotRunning->setEnabled(mJobCount != (ui.jobs->count() - 2) / 2);

  // job status changed so we have to sort jobs list
  sortJobs();
}

void MainWindow::addNewMount(const QString &remote, const QString &folder,
                             const QString &remoteType, const QStringList &args,
                             const QString &script, const QString &uniqueId,
                             const QString &info) {

  if (remoteType == "") {
  }

  QStringList argsFinal = args;

  QProcess *mount = new QProcess(this);
  mount->setProcessChannelMode(QProcess::MergedChannels);

  if (ui.jobs->count() == 2) {
    ui.noJobsAvailable->hide();
  }

  ui.tabs->setTabText(1, QString("Jobs (%1)").arg(++mJobCount));

  // get default mount options
  auto settings = GetSettings();
  QString opt = settings->value("Settings/mount").toString();

  if (!opt.isEmpty()) {
    // split on spaces but not if inside quotes e.g. --option-1 --option-2="arg1
    // arg2" --option-3 arg3 should generate "--option-1" "--option-2=\"arg1
    // arg2\"" "--option-3" "arg3"
    for (QString arg : opt.split(QRegularExpression(" (?=[^\"]*(\"[^\"]*\"[^\"]*)*$)"))) {
      if (!arg.isEmpty()) {
        argsFinal << arg.replace("\"", "");
      }
    }
  }

  argsFinal << GetRcloneConf();

  // the rc credentials go through the environment, never through an argv
  const MountLaunch ml = buildMountLaunch(
      argsFinal, RcloneEnvironment(RcloneEnvMode::Parsed),
      generateRcCredentials());

  auto widget =
      new MountWidget(mount, remote, folder, ml.args, script, uniqueId, info);
  widget->setRcCredentials(ml.creds, ml.rcEnabled);

  auto line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  ui.jobs->insertWidget(0, widget);
  ui.jobs->insertWidget(1, line);

  int _jobsCount = (ui.jobs->count() - 2) / 2;
  ui.buttonSortByTime->setEnabled(_jobsCount > 1);
  ui.buttonSortByStatus->setEnabled(_jobsCount > 1);

  QObject::connect(widget, &MountWidget::finished, this, [=]() {
    if (--mJobCount == 0) {
      ui.tabs->setTabText(1, "Jobs");
    } else {
      ui.tabs->setTabText(1, QString("Jobs (%1)").arg(mJobCount));
    }

    ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
    ui.buttonCleanNotRunning->setEnabled(mJobCount !=
                                         (ui.jobs->count() - 2) / 2);

    // mount finished - we remove darkGreen from task list
    auto mount = qobject_cast<MountWidget *>(widget);
    for (int k = 0; k < ui.tasksListWidget->count(); k = k + 1) {
      JobOptionsListWidgetItem *item =
          static_cast<JobOptionsListWidgetItem *>(ui.tasksListWidget->item(k));
      JobOptions *jo = item->GetData();
      if (mount->getUniqueID() == jo->uniqueId.toString()) {

        ui.tasksListWidget->item(k)->setBackground(QBrush());
      }
    }
    setTasksButtons();
    sortJobs();
  });

  QObject::connect(widget, &MountWidget::closed, this, [=]() {
    ui.jobs->removeWidget(widget);
    ui.jobs->removeWidget(line);
    widget->deleteLater();
    delete line;

    int _jobsCount = (ui.jobs->count() - 2) / 2;
    ui.buttonSortByTime->setEnabled(_jobsCount > 1);
    ui.buttonSortByStatus->setEnabled(_jobsCount > 1);

    if (ui.jobs->count() == 2) {
      ui.noJobsAvailable->show();
    }
    ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
    ui.buttonCleanNotRunning->setEnabled(mJobCount !=
                                         (ui.jobs->count() - 2) / 2);

    setTasksButtons();
  });

  mount->setProcessEnvironment(ml.env);
  mount->start(GetRclone(), ml.args, QIODevice::ReadOnly);

  ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
  ui.buttonCleanNotRunning->setEnabled(mJobCount != (ui.jobs->count() - 2) / 2);
  sortJobs();
}

void MainWindow::addStream(const QString &remote, const QString &stream,
                           const QString &remoteType) {
  auto player = new QProcess();
  auto rclone = new QProcess();
  rclone->setStandardOutputProcess(player);

  QObject::connect(
      player,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      this, [=](int status, QProcess::ExitStatus) {
        player->deleteLater();
        if (status != 0 && player->error() == QProcess::FailedToStart) {
          QMessageBox::critical(
              this, "Error",
              QString("Failed to start '%1' player process").arg(stream));
          auto settings = GetSettings();
          settings->remove("Settings/streamConfirmed");
        }
      });

  ui.tabs->setTabText(1, QString("Jobs (%1)").arg(++mJobCount));

  ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
  ui.buttonCleanNotRunning->setEnabled(mJobCount != (ui.jobs->count() - 2) / 2);

  auto settings = GetSettings();
  QString opt = settings->value("Settings/mount").toString();
  QString driveSharedMode = settings->value("Settings/remoteMode").toString();

  QStringList args;

  args << "cat";

  args << remote;

  if (remoteType == "drive") {

    if (driveSharedMode == "shared") {
      args << "--drive-shared-with-me";
    }

    if (driveSharedMode == "trash") {
      args << "--drive-trashed-only";
    }
  };

  args << GetDefaultOptionsList("defaultRcloneOptions");

  args << GetRcloneConf();

  auto widget = new StreamWidget(rclone, player, remote, stream, args);

  auto line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  QObject::connect(widget, &StreamWidget::finished, this, [=]() {
    if (--mJobCount == 0) {
      ui.tabs->setTabText(1, "Jobs");
    } else {
      ui.tabs->setTabText(1, QString("Jobs (%1)").arg(mJobCount));
    }

    ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
    ui.buttonCleanNotRunning->setEnabled(mJobCount !=
                                         (ui.jobs->count() - 2) / 2);
    sortJobs();
  });

  QObject::connect(widget, &StreamWidget::closed, this, [=]() {
    ui.jobs->removeWidget(widget);
    ui.jobs->removeWidget(line);
    widget->deleteLater();
    delete line;
    if (ui.jobs->count() == 2) {
      ui.noJobsAvailable->show();
    }

    int _jobsCount = (ui.jobs->count() - 2) / 2;
    ui.buttonSortByTime->setEnabled(_jobsCount > 1);
    ui.buttonSortByStatus->setEnabled(_jobsCount > 1);

    ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
    ui.buttonCleanNotRunning->setEnabled(mJobCount !=
                                         (ui.jobs->count() - 2) / 2);
  });

  if (ui.jobs->count() == 2) {
    ui.noJobsAvailable->hide();
  }

  ui.jobs->insertWidget(0, widget);
  ui.jobs->insertWidget(1, line);

  int _jobsCount = (ui.jobs->count() - 2) / 2;
  ui.buttonSortByTime->setEnabled(_jobsCount > 1);
  ui.buttonSortByStatus->setEnabled(_jobsCount > 1);

  QStringList streamPrefsList;

  for (QString arg : stream.split(QRegularExpression(" (?=[^\"]*(\"[^\"]*\"[^\"]*)*$)"))) {
    if (!arg.isEmpty()) {
      streamPrefsList << arg.replace("\"", "");
    }
  }

  QString streamCmd = streamPrefsList.takeAt(0);
  QStringList streamArgs = streamPrefsList;

  player->start(streamCmd, streamArgs, QProcess::ReadOnly);

  ApplyRcloneEnvironment(rclone);
  rclone->start(GetRclone(), QStringList() << args, QProcess::WriteOnly);

  ui.buttonStopAllJobs->setEnabled(mTransferJobCount != 0);
  ui.buttonCleanNotRunning->setEnabled(mJobCount != (ui.jobs->count() - 2) / 2);

  sortJobs();
}

void MainWindow::slotCloseTab(int index) {

  // only when last remote tab is closed return to remote list tab
  // only when closing current tab
  if (ui.tabs->currentIndex() == index) {

    if (ui.tabs->count() == 6) {
      if (index == 5) {
        ui.tabs->setCurrentIndex(0);
      }
    } else {

      if (ui.tabs->count() == 7) {
        ui.tabs->setCurrentIndex(5);
      }
    }
  }
}

void MainWindow::sortJobs() {

  //  bool mJobsTimeSortOrder = false;
  //  bool mJobsStatusSortOrder = false;
  //  QString mJobsSort = "byDate";

  //  QMutexLocker locker(&mMutex);

  QMutexLocker locker(&mJobsSortMutex);

  // don't sort when quitting or stopping all transfers
  if (mAppQuittingStatus) {
    return;
  }
  if (mDoNotSort) {
    return;
  }

  int widgetsCount = ui.jobs->count();
  int move = widgetsCount > 2 ? widgetsCount - 4 : 0;
  QDateTime dt;
  QDateTime widgetStartDateTime;
  QString widgetStatus = 0;
  QString ws;

  for (int i = 0; i < (widgetsCount - 2) / 2 - 1; i = i + 1) {

    for (int j = widgetsCount - 4; j >= i * 2; j = j - 2) {

      QWidget *widget = ui.jobs->itemAt(j)->widget();

      if (auto transfer = qobject_cast<JobWidget *>(widget)) {
        widgetStartDateTime = transfer->getStartDateTime();
        widgetStatus = transfer->getStatus();

      } else if (auto mount = qobject_cast<MountWidget *>(widget)) {
        widgetStartDateTime = mount->getStartDateTime();
        widgetStatus = mount->getStatus();

      } else if (auto stream = qobject_cast<StreamWidget *>(widget)) {
        widgetStartDateTime = stream->getStartDateTime();
        widgetStatus = stream->getStatus();
      }

      if (mJobsSort == "byDate") {
        if (j == widgetsCount - 4) {
          move = j;
          dt = widgetStartDateTime;
        } else {
          if (mJobsTimeSortOrder) {
            if (dt > widgetStartDateTime) {
              move = j;
              dt = widgetStartDateTime;
            }
          } else {
            if (dt < widgetStartDateTime) {
              move = j;
              dt = widgetStartDateTime;
            }
          }
        }
      }

      if (mJobsSort == "byStatus") {

        if (j == widgetsCount - 4) {
          move = j;
          ws = widgetStatus;
        } else {
          if (mJobsStatusSortOrder) {
            if (ws > widgetStatus) {
              move = j;
              ws = widgetStatus;
            }
          } else {
            if (ws < widgetStatus) {
              move = j;
              ws = widgetStatus;
            }
          }
        }
      }

    } // for (int j

    // move to top
    QWidget *widget = ui.jobs->itemAt(move)->widget();
    QWidget *widget_line = ui.jobs->itemAt(move + 1)->widget();
    auto line = qobject_cast<QFrame *>(widget_line);

    if (auto transfer = qobject_cast<JobWidget *>(widget)) {
      ui.jobs->removeWidget(transfer);
      ui.jobs->removeWidget(line);
      ui.jobs->insertWidget(i * 2, transfer);
      ui.jobs->insertWidget(i * 2 + 1, line);
    } else if (auto mount = qobject_cast<MountWidget *>(widget)) {
      ui.jobs->removeWidget(mount);
      ui.jobs->removeWidget(line);
      ui.jobs->insertWidget(i * 2, mount);
      ui.jobs->insertWidget(i * 2 + 1, line);
    } else if (auto stream = qobject_cast<StreamWidget *>(widget)) {
      ui.jobs->removeWidget(stream);
      ui.jobs->removeWidget(line);
      ui.jobs->insertWidget(i * 2, stream);
      ui.jobs->insertWidget(i * 2 + 1, line);
    } else {
      //
      break;
    }
  } //  for (int i

  return;
}
