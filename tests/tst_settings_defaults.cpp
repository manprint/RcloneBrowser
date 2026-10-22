#include <QtTest>
#include <QtWidgets>

#include "job_options.h"
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
    // the confirmation Run and Dry run always had stays on (#144)
    QCOMPARE(dialog.getConfirmTaskRun(), true);
    // an empty iconSize and the first-run value "M" (main.cpp) both give 4.0
    QCOMPARE(dialog.getIconSize(), QString("4.0"));
    QCOMPARE(RemotesIconScaleFromSetting("M"), 4.0);
    // the 3.2 field keeps its default next to the new widgets
    QCOMPARE(dialog.getRcloneUpdateRepo(), QString("rclone/rclone"));
  }

  // T-TASK-FILTER-ARGV (#179): which text combinations keep the plain flags
  // and which go out as ordered --filter rules. It needs no rclone and no
  // MainWindow, and it lives here, in a test every OS builds, so the argv
  // contract is pinned on Linux, macOS and Windows alike.
  void T_TASK_FILTER_ARGV() {
    const auto make = [](const QString &included, const QString &excluded,
                         const QString &filtered) {
      JobOptions jo(false);
      jo.operation = JobOptions::Copy;
      jo.transfers = QStringLiteral("4");
      jo.checkers = QStringLiteral("8");
      jo.connectTimeout = QStringLiteral("60");
      jo.idleTimeout = QStringLiteral("300");
      jo.retries = QStringLiteral("3");
      jo.lowLevelRetries = QStringLiteral("10");
      jo.source = QStringLiteral("/nonexistent/s");
      jo.dest = QStringLiteral("/nonexistent/d");
      jo.included = included;
      jo.excluded = excluded;
      jo.filtered = filtered;
      return jo.getOptions();
    };
    const auto tail = [](const QStringList &argv) {
      const int at = argv.indexOf(QStringLiteral("--low-level-retries"));
      return at < 0 ? QStringList() : argv.mid(at + 2);
    };
    const auto L = [](std::initializer_list<const char *> items) {
      QStringList out;
      for (const char *i : items) {
        out << QString::fromUtf8(i);
      }
      return out;
    };

    // Exclude alone and Include alone: the plain flags, nothing else
    QCOMPARE(tail(make(QString(), QStringLiteral("._*"), QString())),
             L({"--exclude", "._*"}));
    QCOMPARE(tail(make(QStringLiteral("*.txt"), QString(), QString())),
             L({"--include", "*.txt"}));
    // Include and a blank-only Exclude text: the Exclude text has no rule, so
    // the plain flags stay (a blank line is the empty pattern, as before)
    const QStringList blank =
        tail(make(QStringLiteral("*.txt"), QStringLiteral(" \n"), QString()));
    QVERIFY(!blank.contains(QStringLiteral("--filter")));
    QCOMPARE(blank.mid(0, 2), L({"--include", "*.txt"}));
    // a Filter tab alone is passed on as it is, with no catch-all
    QCOMPARE(tail(make(QString(), QString(), QStringLiteral("+ *.jpg"))),
             L({"--filter", "+ *.jpg"}));

    // both: excludes, includes, the Filter tab, then the catch-all
    QCOMPARE(tail(make(QStringLiteral("*.txt"), QStringLiteral("._*"),
                       QStringLiteral("+ *.jpg"))),
             L({"--filter", "- ._*", "--filter", "+ *.txt", "--filter",
                "+ *.jpg", "--filter", "- **"}));
    // blank lines between the rules are no rules
    QCOMPARE(tail(make(QStringLiteral("*.txt\n\n*.md"),
                       QStringLiteral("._*\n"), QString())),
             L({"--filter", "- ._*", "--filter", "+ *.txt", "--filter",
                "+ *.md", "--filter", "- **"}));
  }
};

QTEST_MAIN(TestSettingsDefaults)
#include "tst_settings_defaults.moc"
