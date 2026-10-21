#include <QtTest>
#include <QtWidgets>

#include "preferences_dialog.h"
#include "rb_test_support.h"
#include "utils.h"

// The view preferences added by the Nicolaius customizations (4.1) default to
// today's look: tabs and remote icons visible, remotes icon size M (4.0).
class TestSettingsDefaults : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-SET-DEFAULTS
  void T_SET_DEFAULTS() {
    {
      auto settings = GetSettings();
      settings->clear();
      settings->sync();
    }

    PreferencesDialog dialog;
    QCOMPARE(dialog.getHideTasksTab(), false);
    QCOMPARE(dialog.getHideQueueTab(), false);
    QCOMPARE(dialog.getHideSchedulerTab(), false);
    QCOMPARE(dialog.getHideRemoteIcons(), false);
    // an empty iconSize and the first-run value "M" (main.cpp) both give 4.0
    QCOMPARE(dialog.getIconSize(), QString("4.0"));
    QCOMPARE(RemotesIconScaleFromSetting("M"), 4.0);
    // the 3.2 field keeps its default next to the new widgets
    QCOMPARE(dialog.getRcloneUpdateRepo(), QString("rclone/rclone"));
  }
};

QTEST_MAIN(TestSettingsDefaults)
#include "tst_settings_defaults.moc"
