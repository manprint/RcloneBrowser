#include <QtTest>

#include "rb_test_support.h"
#include "utils.h"

class TestUtils : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-UTIL-SPLIT
  void T_UTIL_SPLIT() {
    {
      auto settings = GetSettings();
      settings->setValue("Settings/defaultRcloneOptions",
                         R"(--a --b="x y" --c z)");
      settings->sync();
    }
    QCOMPARE(GetDefaultOptionsList("defaultRcloneOptions"),
             QStringList({"--a", "--b=x y", "--c", "z"}));
  }

  // T-UTIL-EMPTY
  void T_UTIL_EMPTY() {
    QVERIFY(GetDefaultOptionsList("unsetOptionsKey").isEmpty());
  }

  // T-UTIL-PORTABLE
  void T_UTIL_PORTABLE() { QVERIFY(!IsPortableMode()); }

  // T-UTIL-PORTABLE-XDG: on Linux the portable ini is
  // $XDG_CONFIG_HOME/rclone-browser/rclone-browser.ini, and only with an
  // absolute XDG_CONFIG_HOME. Unset, it was looked for in "/rclone-browser";
  // relative (the XDG spec says to ignore it), below the current folder.
  void T_UTIL_PORTABLE_XDG() {
#if defined(Q_OS_LINUX)
    const QByteArray savedXdg = qgetenv("XDG_CONFIG_HOME");
    const bool hadAppImage = qEnvironmentVariableIsSet("APPIMAGE");
    const QByteArray savedAppImage = qgetenv("APPIMAGE");
    const QString savedCwd = QDir::currentPath();
    const auto restore = qScopeGuard([&] {
      qputenv("XDG_CONFIG_HOME", savedXdg);
      if (hadAppImage) {
        qputenv("APPIMAGE", savedAppImage);
      } else {
        qunsetenv("APPIMAGE");
      }
      QDir::setCurrent(savedCwd);
    });
    qunsetenv("APPIMAGE");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("rel/rclone-browser")));
    QFile ini(dir.path() + QStringLiteral("/rel/rclone-browser/rclone-browser.ini"));
    QVERIFY(ini.open(QIODevice::WriteOnly));
    ini.close();
    QVERIFY(QDir::setCurrent(dir.path()));

    qunsetenv("XDG_CONFIG_HOME");
    QVERIFY(!IsPortableMode());
    qputenv("XDG_CONFIG_HOME", "rel");
    QVERIFY(!IsPortableMode());
    qputenv("XDG_CONFIG_HOME", (dir.path() + QStringLiteral("/rel")).toUtf8());
    QVERIFY(IsPortableMode());
#else
    // QSKIP with one argument is a C++20 extension for clang -Werror
    // (macOS, FreeBSD, OpenBSD); this is what the macro expands to
    QTest::qSkip("the XDG portable ini is Linux only",
                 __FILE__, __LINE__);
    return;
#endif
  }

  // T-UTIL-STREAM-CMD: the player command of a stream. "$file_name" is
  // replaced after the split, so a name with spaces or quotes stays one
  // argument; a blank command gives nothing (no program to start).
  void T_UTIL_STREAM_CMD() {
    QCOMPARE(SplitStreamCommand("vlc -", "a b.mkv"),
             (QStringList{"vlc", "-"}));
    QCOMPARE(SplitStreamCommand("mpv --title=$file_name  -", "my \"film\".mkv"),
             (QStringList{"mpv", "--title=my \"film\".mkv", "-"}));
    QCOMPARE(SplitStreamCommand(R"("/opt/my player/p" --x="a b" -)", "f"),
             (QStringList{"/opt/my player/p", "--x=a b", "-"}));
    QVERIFY(SplitStreamCommand("", "f").isEmpty());
    QVERIFY(SplitStreamCommand("   ", "f").isEmpty());
  }

  // T-UTIL-RCLONE-CMD: the command line shown in the transfer dialog; any
  // number of arguments (two used to read past the end of the list), the
  // arguments with a space quoted.
  void T_UTIL_RCLONE_CMD() {
    SetRclone("/opt/rclone");
    SetRcloneConf(QString());
    // the program is shown with the separators of the platform (a backslash
    // on Windows)
    const QString rc = QDir::toNativeSeparators(QStringLiteral("/opt/rclone"));
    QCOMPARE(GetRcloneCmd({}), (QStringList{rc}));
    QCOMPARE(GetRcloneCmd({"lsd"}), (QStringList{rc, "lsd"}));
    QCOMPARE(GetRcloneCmd({"lsd", "a b:"}),
             (QStringList{rc, "lsd", "\"a b:\""}));
    QCOMPARE(GetRcloneCmd({"copy", "a b:", "c:", "-v"}),
             (QStringList{rc, "copy", "\"a b:\"", "c:", "-v"}));
  }

  void cleanupTestCase() {}
};

QTEST_GUILESS_MAIN(TestUtils)
#include "tst_utils.moc"
