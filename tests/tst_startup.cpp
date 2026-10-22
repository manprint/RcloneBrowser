#include <QtTest>
#include <QtWidgets>

#include "main_window.h"
#include "preferences_dialog.h"
#include "rb_test_support.h"
#include "terminal_command.h"
#include "utils.h"

namespace {

// Closes whatever modal window a start-up path opens (a blocking box would
// freeze the test) and records the texts of the boxes. The non-blocking
// rcloneProblemBox is the thing under test and is left alone.
struct Answerer {
  QStringList texts;
  int dialogs = 0;
  // accept the dialogs (Preferences) instead of rejecting them
  bool acceptDialogs = false;
  // the password prompt (QInputDialog): rejected unless acceptInput is set
  int inputs = 0;
  bool acceptInput = false;
  QString inputText;
  // called on every other dialog before it is answered
  std::function<void(QDialog *)> onDialog;
  QTimer timer;
  Answerer() {
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, &timer, [this]() {
      QWidget *modal = QApplication::activeModalWidget();
      if (!modal || modal->objectName() == QLatin1String("rcloneProblemBox")) {
        return;
      }
      if (auto *box = qobject_cast<QMessageBox *>(modal)) {
        texts << box->text();
        box->close();
      } else if (auto *input = qobject_cast<QInputDialog *>(modal)) {
        ++inputs;
        if (acceptInput) {
          input->setTextValue(inputText);
          input->accept();
        } else {
          input->reject();
        }
      } else if (auto *dialog = qobject_cast<QDialog *>(modal)) {
        ++dialogs;
        if (onDialog) {
          onDialog(dialog);
        }
        if (acceptDialogs) {
          dialog->accept();
        } else {
          dialog->reject();
        }
      }
    });
    timer.start();
  }
};

// Records that the window received a close event (a quit of the app).
struct CloseWatcher : QObject {
  int closes = 0;
  bool eventFilter(QObject *, QEvent *e) override {
    if (e->type() == QEvent::Close) {
      ++closes;
    }
    return false;
  }
};

// Restores PATH when the test function ends, also after a failed QVERIFY.
struct PathGuard {
  QByteArray saved = qgetenv("PATH");
  ~PathGuard() { qputenv("PATH", saved); }
};

bool writeExecutable(const QString &path, const QByteArray &content) {
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    return false;
  }
  f.write(content);
  f.close();
  return f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                          QFileDevice::ExeOwner);
}

// Restores $TERMINAL when the test function ends.
struct TerminalGuard {
  QByteArray saved = qgetenv("TERMINAL");
  bool had = qEnvironmentVariableIsSet("TERMINAL");
  ~TerminalGuard() {
    if (had) {
      qputenv("TERMINAL", saved);
    } else {
      qunsetenv("TERMINAL");
    }
  }
};

QMessageBox *problemBox(MainWindow &w) {
  return w.findChild<QMessageBox *>(QStringLiteral("rcloneProblemBox"));
}

} // namespace

// #159, #145, #209, #175, #201, #156 and #228: the start-up of the main window
// with a bad rclone path, the terminal order, the tabs and the minimum width.
class TestStartup : public QObject {
  Q_OBJECT

  QString mFakeRclone;    // a valid version, no remotes
  QString mFakeRcloneLoc; // the same with one local remote
  QString mExit1;         // exits 1 without output (not rclone)
  QString mBadInterp;     // executable, but its interpreter does not exist
  QString mNeedPassword;  // `version` asks for the config password

  // The settings the app writes on its first run (main.cpp), plus what keeps
  // the unit tests off the network and out of the notification code.
  static void seed(const QString &rclone,
                   const QString &buttonStyle = QStringLiteral("icononly"),
                   const QString &buttonSize = QStringLiteral("0")) {
    auto settings = GetSettings();
    settings->clear();
    settings->setValue(QStringLiteral("Settings/rclone"), rclone);
    settings->setValue(QStringLiteral("Settings/checkRcloneUpdates"), false);
    settings->setValue(QStringLiteral("Settings/checkRcloneBrowserUpdates"),
                       false);
    settings->setValue(QStringLiteral("Settings/notifyFinishedTransfers"),
                       false);
    settings->setValue(QStringLiteral("Settings/soundNotif"), false);
    settings->setValue(QStringLiteral("Settings/buttonStyle"), buttonStyle);
    settings->setValue(QStringLiteral("Settings/buttonSize"), buttonSize);
    settings->setValue(QStringLiteral("Settings/fontSize"),
                       QStringLiteral("0"));
    settings->setValue(QStringLiteral("Settings/iconSize"),
                       QStringLiteral("M"));
    settings->setValue(QStringLiteral("Settings/iconsLayout"),
                       QStringLiteral("tiles"));
    settings->setValue(QStringLiteral("Settings/iconsColour"),
                       QStringLiteral("black"));
    settings->sync();
    SetRclone(rclone);
  }

  // needPassword: `version` fails like an encrypted config until
  // RCLONE_CONFIG_PASS is set
  static QByteArray fakeScript(const QByteArray &listremotes,
                               bool needPassword = false) {
    return QByteArray("#!/bin/sh\n") +
           (needPassword
                ? QByteArray("if [ \"$1\" = version ] && [ -z "
                             "\"$RCLONE_CONFIG_PASS\" ]; then\n"
                             " echo 'Failed to load config: not allowed to ask "
                             "for password' >&2; exit 1; fi\n")
                : QByteArray()) +
           QByteArray("case \"$1\" in\n"
                      " version) printf 'rclone v1.75.2\\n"
                      "- os/version: ubuntu 24.04 (64 bit)\\n"
                      "- os/kernel: 6.8.0-45-generic (x86_64)\\n"
                      "- os/type: linux\\n- os/arch: amd64\\n"
                      "- go/version: go1.25\\n- go/linking: static\\n"
                      "- go/tags: none\\n';;\n"
                      " listremotes) echo '") +
           listremotes +
           QByteArray("';;\n"
                      " rcd) exit 1;;\n"
                      " *) exit 0;;\n"
                      "esac\n");
  }

  static QLabel *statusLabel(MainWindow &w) {
    const auto labels = w.statusBar()->findChildren<QLabel *>();
    for (QLabel *l : labels) {
      if (!l->text().isEmpty()) {
        return l;
      }
    }
    return nullptr;
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    // A private HOME keeps the tasks of other tests out of the constructor's
    // restore paths (see tst_mainwindow_snapshot).
    QVERIFY(QDir().mkpath(rbTestDir().path() + QStringLiteral("/home")));
    qputenv("HOME", (rbTestDir().path() + QStringLiteral("/home")).toUtf8());

    mFakeRclone = rbTestDir().path() + QStringLiteral("/fake-rclone.sh");
    QVERIFY(writeExecutable(mFakeRclone, fakeScript("[]")));
    mFakeRcloneLoc = rbTestDir().path() + QStringLiteral("/fake-rclone-loc.sh");
    QVERIFY(writeExecutable(
        mFakeRcloneLoc,
        fakeScript("[{\"name\":\"loc\",\"type\":\"local\",\"source\":\"file\","
                   "\"description\":\"\"}]")));
    mExit1 = rbTestDir().path() + QStringLiteral("/exit1.sh");
    QVERIFY(writeExecutable(mExit1, "#!/bin/sh\nexit 1\n"));
    mBadInterp = rbTestDir().path() + QStringLiteral("/badinterp.sh");
    QVERIFY(writeExecutable(mBadInterp, "#!/nonexistent/sh\nexit 0\n"));
    mNeedPassword =
        rbTestDir().path() + QStringLiteral("/fake-rclone-password.sh");
    QVERIFY(writeExecutable(mNeedPassword, fakeScript("[]", true)));
  }

  // T-START-RCLONEPATH (#145, #159, #209): the path check, the editable path
  // field and a main window that stays alive on a bad rclone.
  void T_START_RCLONEPATH() {
    // (a) the helper
    {
      PathGuard guard;
      const QString dir = rbTestDir().path();
      QVERIFY(rcloneExecutableProblem(mFakeRclone).isEmpty());

      const QString missing = dir + QStringLiteral("/no/such/rclone");
      const QString missingText = rcloneExecutableProblem(missing);
      QVERIFY(!missingText.isEmpty());
      QVERIFY2(missingText.contains(missing), qPrintable(missingText));

      // a directory has the executable bit
      const QString dirText = rcloneExecutableProblem(dir);
      QVERIFY(!dirText.isEmpty());
      QVERIFY2(dirText.contains(dir), qPrintable(dirText));

      const QString plain = dir + QStringLiteral("/plain-file");
      QFile f(plain);
      QVERIFY(f.open(QIODevice::WriteOnly));
      f.write("not a program");
      f.close();
      QVERIFY(f.setPermissions(QFileDevice::ReadOwner |
                               QFileDevice::WriteOwner));
      const QString plainText = rcloneExecutableProblem(plain);
      QVERIFY(!plainText.isEmpty());
      QVERIFY2(plainText.contains(plain), qPrintable(plainText));

      QVERIFY(!rcloneExecutableProblem(QString()).isEmpty());
      QVERIFY(!rcloneExecutableProblem(QStringLiteral("  ")).isEmpty());

      // a bare name is looked up in PATH, as QProcess does
      const QString bin = dir + QStringLiteral("/pathbin");
      QVERIFY(QDir().mkpath(bin));
      QVERIFY(writeExecutable(bin + QStringLiteral("/rb-fake-rclone"),
                              "#!/bin/sh\nexit 0\n"));
      qputenv("PATH", bin.toUtf8());
      QVERIFY(rcloneExecutableProblem(QStringLiteral("rb-fake-rclone")).isEmpty());
      const QString bareText =
          rcloneExecutableProblem(QStringLiteral("rb-no-such-rclone"));
      QVERIFY(!bareText.isEmpty());
      QVERIFY2(bareText.contains(QStringLiteral("rb-no-such-rclone")),
               qPrintable(bareText));
    }

    // (b) #209: the path field of Preferences is editable
    {
      seed(mFakeRclone);
      PreferencesDialog dialog;
      auto *edit = dialog.findChild<QLineEdit *>(QStringLiteral("rclone"));
      QVERIFY(edit);
      QVERIFY(edit->isEnabled());
      QVERIFY(!edit->isReadOnly());
      QCOMPARE(edit->text(), QDir::toNativeSeparators(mFakeRclone));
      edit->setText(QStringLiteral("/other/rclone"));
      QCOMPARE(dialog.getRclone(), QStringLiteral("/other/rclone"));
    }

    // (c) a missing file: a box at once, no blocking dialog, one box only.
    // PATH holds no terminal: a Config that reached the terminal search would
    // end in a blocking box, which the answerer records.
    {
      PathGuard guard;
      const QString emptyBin = rbTestDir().path() + QStringLiteral("/emptybin");
      QVERIFY(QDir().mkpath(emptyBin));
      qputenv("PATH", emptyBin.toUtf8());
      Answerer answerer;
      const QString missing =
          rbTestDir().path() + QStringLiteral("/missing/rclone");
      seed(missing);
      MainWindow w;
      QMessageBox *box = problemBox(w);
      QVERIFY2(box, "no rclone problem box right after the constructor");
      QVERIFY2(box->text().contains(missing), qPrintable(box->text()));
      QCOMPARE(box->windowModality(), Qt::WindowModal);
      // the box offers Preferences (#209)
      bool hasPreferences = false;
      const auto buttons = box->findChildren<QPushButton *>();
      for (QPushButton *b : buttons) {
        hasPreferences = hasPreferences ||
                         b->text().contains(QLatin1String("Preferences"));
      }
      QVERIFY(hasPreferences);

      auto *config = w.findChild<QToolButton *>(QStringLiteral("config"));
      QVERIFY(config);
      config->click();
      QCOMPARE(w.findChildren<QMessageBox *>(QStringLiteral("rcloneProblemBox"))
                   .size(),
               1);
      QVERIFY(answerer.texts.isEmpty());
      QCOMPARE(answerer.dialogs, 0);

      // the pre-check of rcloneConfig() itself (the "Advanced (terminal)..."
      // entry and the Windows and macOS branches call it directly, #145)
      QVERIFY(QMetaObject::invokeMethod(&w, "rcloneConfig"));
      QCOMPARE(w.findChildren<QMessageBox *>(QStringLiteral("rcloneProblemBox"))
                   .size(),
               1);
      QVERIFY2(problemBox(w)->text().contains(missing),
               qPrintable(problemBox(w)->text()));
      QVERIFY(answerer.texts.isEmpty());

      // #209: the Preferences button opens Preferences; accepting it with the
      // path still bad shows the problem again (a visible box, not the old one
      // that waits for its deletion)
      answerer.acceptDialogs = true;
      QPushButton *preferences = nullptr;
      for (QPushButton *b : box->findChildren<QPushButton *>()) {
        if (b->text().contains(QLatin1String("Preferences"))) {
          preferences = b;
        }
      }
      QVERIFY(preferences);
      // inside a running event loop, like the app: Qt does not run the
      // deferred delete of the closed box in the nested loop of Preferences
      QEventLoop loop;
      QTimer::singleShot(0, &loop, [&]() {
        preferences->click();
        loop.quit();
      });
      loop.exec();
      QCOMPARE(answerer.dialogs, 1);
      int visibleBoxes = 0;
      for (QMessageBox *b : w.findChildren<QMessageBox *>(
               QStringLiteral("rcloneProblemBox"))) {
        visibleBoxes += b->isVisible() ? 1 : 0;
      }
      QCOMPARE(visibleBoxes, 1);
    }

    // (d) #159: an executable that is not rclone (exit 1, no output) and one
    // that cannot be started (FailedToStart): the window stays and shows the
    // problem instead of quitting, and the problem is logged
    for (const QString &program : {mExit1, QStringLiteral("/bin/false"),
                                   mBadInterp}) {
      Answerer answerer;
      seed(program);
      QTest::ignoreMessage(
          QtWarningMsg,
          QRegularExpression(QStringLiteral("rclone problem:.*") +
                             QRegularExpression::escape(program)));
      MainWindow w;
      QTRY_VERIFY2_WITH_TIMEOUT(problemBox(w) != nullptr,
                                qPrintable(program), 10000);
      const QString text = problemBox(w)->text();
      if (program == mBadInterp) {
        QVERIFY2(text.contains(QStringLiteral("Cannot start rclone")),
                 qPrintable(text));
      } else {
        QVERIFY2(text.contains(QStringLiteral("exit code 1")),
                 qPrintable(text));
      }
      QVERIFY2(answerer.texts.isEmpty(), qPrintable(answerer.texts.join('|')));
    }

    // (e) the config password prompt of the version check: a cancelled prompt
    // still quits (3.3), an entered password runs the check again, and neither
    // shows the problem box
    {
      Answerer answerer;
      seed(mNeedPassword);
      SetRclonePassword(QString());
      MainWindow w;
      CloseWatcher watcher;
      w.installEventFilter(&watcher);
      QTRY_COMPARE_WITH_TIMEOUT(watcher.closes, 1, 10000);
      QCOMPARE(answerer.inputs, 1);
      QVERIFY(!problemBox(w));
      QVERIFY(answerer.texts.isEmpty());
    }
    {
      Answerer answerer;
      answerer.acceptInput = true;
      answerer.inputText = QStringLiteral("secret");
      seed(mNeedPassword);
      SetRclonePassword(QString());
      MainWindow w;
      CloseWatcher watcher;
      w.installEventFilter(&watcher);
      QTRY_VERIFY_WITH_TIMEOUT(statusLabel(w) != nullptr, 10000);
      QCOMPARE(answerer.inputs, 1);
      QCOMPARE(watcher.closes, 0);
      QVERIFY(!problemBox(w));
      SetRclonePassword(QString());
    }
  }

  // T-START-TERMINAL (#175): the order of the terminal candidates and the
  // search for the first one that starts.
  void T_START_TERMINAL() {
    const QStringList defaults = {
        QStringLiteral("gnome-terminal"), QStringLiteral("konsole"),
        QStringLiteral("xfce4-terminal"), QStringLiteral("xterm"),
        QStringLiteral("x-terminal-emulator")};
    QProcessEnvironment env;
    env.insert(QStringLiteral("TERMINAL"), QStringLiteral("/opt/t/envterm"));

    QCOMPARE(terminalCandidates(env, QStringLiteral("myterm")),
             (QStringList{QStringLiteral("myterm"),
                          QStringLiteral("/opt/t/envterm")} +
              defaults));
    // a blank Preferences value is ignored
    QCOMPARE(terminalCandidates(env, QStringLiteral("  ")),
             (QStringList{QStringLiteral("/opt/t/envterm")} + defaults));
    QCOMPARE(terminalCandidates(env),
             (QStringList{QStringLiteral("/opt/t/envterm")} + defaults));
    QCOMPARE(terminalCandidates(QProcessEnvironment(), QString()), defaults);
    // the same value twice is tried once
    QCOMPARE(terminalCandidates(env, QStringLiteral("/opt/t/envterm")),
             (QStringList{QStringLiteral("/opt/t/envterm")} + defaults));

    // findTerminal: only an empty PATH directory, so only full paths count
    PathGuard guard;
    const QString dir = rbTestDir().path();
    const QString emptyBin = dir + QStringLiteral("/emptybin");
    QVERIFY(QDir().mkpath(emptyBin));
    qputenv("PATH", emptyBin.toUtf8());
    const QString preferred = dir + QStringLiteral("/term-preferred");
    const QString fromEnv = dir + QStringLiteral("/term-env");
    QVERIFY(writeExecutable(preferred, "#!/bin/sh\nexit 0\n"));
    QVERIFY(writeExecutable(fromEnv, "#!/bin/sh\nexit 0\n"));
    QProcessEnvironment termEnv;
    termEnv.insert(QStringLiteral("TERMINAL"), fromEnv);
    QCOMPARE(findTerminal(terminalCandidates(termEnv, preferred)), preferred);

    // a preferred file that cannot be executed falls through to $TERMINAL
    const QString notExec = dir + QStringLiteral("/term-not-exec");
    QFile f(notExec);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
    QVERIFY(f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    QCOMPARE(findTerminal(terminalCandidates(termEnv, notExec)), fromEnv);

    // a bare name is found in PATH, and a preferred bare name beats an
    // absolute executable $TERMINAL
    const QString termBin = dir + QStringLiteral("/termbin");
    QVERIFY(QDir().mkpath(termBin));
    const QString bare = termBin + QStringLiteral("/rb-term");
    QVERIFY(writeExecutable(bare, "#!/bin/sh\nexit 0\n"));
    qputenv("PATH", termBin.toUtf8());
    QCOMPARE(findTerminal(QStringList{QStringLiteral("rb-term")}), bare);
    QCOMPARE(findTerminal(terminalCandidates(termEnv, QStringLiteral("rb-term"))),
             bare);
    qputenv("PATH", emptyBin.toUtf8());

    // nothing usable
    QProcessEnvironment none;
    none.insert(QStringLiteral("TERMINAL"), notExec);
    QCOMPARE(findTerminal(terminalCandidates(none, dir + QStringLiteral("/gone"))),
             QString());

    // the box text names the Preferences field and $TERMINAL, not $TERM
    const QString message = terminalNotFoundMessage();
    QVERIFY(message.contains(QStringLiteral("Preferences")));
    QVERIFY(message.contains(QStringLiteral("$TERMINAL")));
    QVERIFY2(!message.contains(QRegularExpression(QStringLiteral(R"(\$TERM\b)"))),
             qPrintable(message));

    // Preferences keeps the value (empty by default)
    seed(mFakeRclone);
    {
      PreferencesDialog dialog;
      QCOMPARE(dialog.getTerminalCommand(), QString());
    }
    GetSettings()->setValue(QStringLiteral("Settings/terminalCommand"),
                            QStringLiteral("xterm"));
    {
      PreferencesDialog dialog;
      QCOMPARE(dialog.getTerminalCommand(), QStringLiteral("xterm"));
    }

    // The whole #175 path through the main window: the Preferences field is
    // saved, Config starts that terminal, and a value that is not found falls
    // back to $TERMINAL with one warning. Both terminals record their argv.
    {
      TerminalGuard terminalGuard;
      const QString prefMarker = dir + QStringLiteral("/term-pref.marker");
      const QString envMarker = dir + QStringLiteral("/term-env.marker");
      QFile::remove(prefMarker);
      QFile::remove(envMarker);
      const QString prefTerm = dir + QStringLiteral("/rb-pref-term");
      const QString envTerm = dir + QStringLiteral("/rb-env-term");
      QVERIFY(writeExecutable(
          prefTerm, "#!/bin/sh\nprintf '%s\\n' \"$@\" > '" +
                        prefMarker.toUtf8() + "'\n"));
      QVERIFY(writeExecutable(
          envTerm, "#!/bin/sh\nprintf '%s\\n' \"$@\" > '" +
                       envMarker.toUtf8() + "'\n"));
      qputenv("TERMINAL", envTerm.toUtf8());

      Answerer answerer;
      seed(mFakeRclone);
      MainWindow w;
      QTRY_VERIFY_WITH_TIMEOUT(statusLabel(w) != nullptr, 10000);

      // 1. the Preferences field, with blanks around the value
      answerer.acceptDialogs = true;
      answerer.onDialog = [&](QDialog *d) {
        auto *edit = d->findChild<QLineEdit *>(QStringLiteral("terminalCommand"));
        if (edit) {
          edit->setText(QStringLiteral("  ") + prefTerm + QStringLiteral("  "));
        }
      };
      auto *preferences = w.findChild<QAction *>(QStringLiteral("preferences"));
      QVERIFY(preferences);
      preferences->trigger();
      QCOMPARE(answerer.dialogs, 1);
      answerer.acceptDialogs = false;
      answerer.onDialog = nullptr;
      QCOMPARE(GetSettings()->value(QStringLiteral("Settings/terminalCommand"))
                   .toString(),
               prefTerm);

      // 2. Config starts it (the fake rcd exits 1: no daemon, so the terminal
      // flow is the fallback of openRemoteConfig)
      auto *config = w.findChild<QToolButton *>(QStringLiteral("config"));
      QVERIFY(config);
      config->click();
      QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(prefMarker).size() > 0, 10000);
      QFile m(prefMarker);
      QVERIFY(m.open(QIODevice::ReadOnly));
      const QString argv = QString::fromUtf8(m.readAll());
      m.close();
      const QStringList lines = argv.split('\n', Qt::SkipEmptyParts);
      QVERIFY2(lines.size() >= 4, qPrintable(argv));
      QCOMPARE(lines.mid(0, 3), (QStringList{QStringLiteral("-e"),
                                             QStringLiteral("sh"),
                                             QStringLiteral("-c")}));
      QVERIFY2(lines.at(3).contains(QStringLiteral(" config")),
               qPrintable(argv));
      QVERIFY(!QFileInfo::exists(envMarker));

      // 3. a value that is not found: one warning, then $TERMINAL
      GetSettings()->setValue(QStringLiteral("Settings/terminalCommand"),
                              dir + QStringLiteral("/gone-term"));
      QTest::ignoreMessage(
          QtWarningMsg,
          QRegularExpression(
              QStringLiteral("terminal command from Preferences not found")));
      config->click();
      QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(envMarker).size() > 0, 10000);
      QVERIFY(answerer.texts.isEmpty());
    }
  }

  void T_START_MINWIDTH_data() {
    QTest::addColumn<QString>("style");
    QTest::addColumn<int>("stage"); // 0 construction, 1 status text, 2 remote
    QTest::addColumn<QString>("size"); // Settings/buttonSize
    QTest::newRow("construction") << "icononly" << 0 << "0";
    QTest::newRow("status label") << "icononly" << 1 << "0";
    QTest::newRow("remote icononly") << "icononly" << 2 << "0";
    QTest::newRow("remote textandicon") << "textandicon" << 2 << "0";
    QTest::newRow("remote textonly") << "textonly" << 2 << "0";
    // the larger icons of Preferences (24, 32, 48, 72, 96 px at 0 to 4)
    QTest::newRow("remote icononly size3") << "icononly" << 2 << "3";
    QTest::newRow("remote icononly size4") << "icononly" << 2 << "4";
    QTest::newRow("remote textandicon size3") << "textandicon" << 2 << "3";
    QTest::newRow("remote textandicon size4") << "textandicon" << 2 << "4";
  }

  // T-START-MINWIDTH (#156, #228): the main window must be able to shrink to
  // 1024 px, with the status text and with a remote tab open.
  void T_START_MINWIDTH() {
    QFETCH(QString, style);
    QFETCH(int, stage);
    QFETCH(QString, size);
    Answerer answerer;
    seed(stage == 2 ? mFakeRcloneLoc : mFakeRclone, style, size);
    MainWindow w;
    auto *tabs = w.findChild<QTabWidget *>(QStringLiteral("tabs"));
    QVERIFY(tabs);

    if (stage >= 1) {
      QTRY_VERIFY_WITH_TIMEOUT(statusLabel(w) != nullptr, 10000);
    }
    if (stage == 2) {
      auto *remotes = w.findChild<QListWidget *>(QStringLiteral("remotes"));
      auto *open = w.findChild<QToolButton *>(QStringLiteral("open"));
      QVERIFY(remotes && open);
      QTRY_COMPARE_WITH_TIMEOUT(remotes->count(), 1, 10000);
      remotes->setCurrentRow(0);
      QVERIFY(open->isEnabled());
      open->click();
      QCOMPARE(tabs->count(), 6);
      // the scroll area around the button row must not take the keyboard focus
      // from the file tree on a click, and must stay transparent
      auto *row = tabs->widget(5)->findChild<QWidget *>(
          QStringLiteral("buttons"), Qt::FindChildrenRecursively);
      QVERIFY(row);
      QScrollArea *area = nullptr;
      for (QWidget *p = row->parentWidget(); p && !area;
           p = p->parentWidget()) {
        area = qobject_cast<QScrollArea *>(p);
      }
      QVERIFY(area);
      QCOMPARE(area->focusPolicy(), Qt::NoFocus);
      QVERIFY(!row->autoFillBackground());
      QVERIFY(!area->viewport()->autoFillBackground());

      // in a window wide enough for the whole row (no scroll bar) the area
      // follows the height of the row when the row's layout changes (a taller
      // button, as with larger icons or another font; the test binary has no
      // icon resources, so the icon size itself changes nothing here)
      tabs->setCurrentIndex(5);
      w.resize(4000, 1200);
      w.show();
      QTRY_COMPARE_WITH_TIMEOUT(area->horizontalScrollBar()->maximum(), 0,
                                5000);
      QTRY_COMPARE_WITH_TIMEOUT(area->height(), row->sizeHint().height(),
                                5000);
      const int before = area->height();
      QToolButton *tall = nullptr;
      for (QToolButton *button : row->findChildren<QToolButton *>()) {
        if (button->isVisibleTo(row)) {
          tall = button;
          break;
        }
      }
      QVERIFY(tall);
      tall->setMinimumHeight(before + 40);
      QTRY_VERIFY_WITH_TIMEOUT(row->sizeHint().height() > before, 5000);
      QTRY_COMPARE_WITH_TIMEOUT(area->horizontalScrollBar()->maximum(), 0,
                                5000);
      QTRY_COMPARE_WITH_TIMEOUT(area->height(), row->sizeHint().height(),
                                5000);
      w.hide();
    }

    QStringList detail;
    detail << QStringLiteral("window %1").arg(w.minimumSizeHint().width());
    detail << QStringLiteral("tabBar %1")
                  .arg(tabs->tabBar()->minimumSizeHint().width());
    for (int i = 0; i < tabs->count(); ++i) {
      detail << QStringLiteral("page%1 %2").arg(i).arg(
          tabs->widget(i)->minimumSizeHint().width());
    }
    detail << QStringLiteral("statusBar %1")
                  .arg(w.statusBar()->minimumSizeHint().width());
    if (QLabel *label = statusLabel(w)) {
      detail << QStringLiteral("statusLabel %1 (%2 chars)")
                    .arg(label->minimumSizeHint().width())
                    .arg(label->text().size());
    }
    const QString line = detail.join(QStringLiteral(", "));
    qInfo().noquote() << "T_START_MINWIDTH" << style << stage << size << line;
    if (stage >= 1) {
      // a working rclone must not show a problem box
      QVERIFY(!problemBox(w));
    }
    QVERIFY2(w.minimumSizeHint().width() <= 1024, qPrintable(line));
  }

  // T-START-TABS (#201): with fresh settings and no remotes (or a bad rclone)
  // the tabs, the lists and the window are enabled.
  void T_START_TABS() {
    for (const QString &rclone :
         {mFakeRclone, rbTestDir().path() + QStringLiteral("/missing/rclone")}) {
      Answerer answerer;
      seed(rclone);
      MainWindow w;
      auto *tabs = w.findChild<QTabWidget *>(QStringLiteral("tabs"));
      QVERIFY(tabs);
      const bool valid = rclone == mFakeRclone;
      if (valid) {
        QTRY_VERIFY_WITH_TIMEOUT(statusLabel(w) != nullptr, 10000);
      }
      // let the start-up finish (the remote list, the problem box)
      QTest::qWait(300);
      if (valid) {
        // a working rclone must not show a problem box
        QVERIFY(!problemBox(w));
      } else {
        QVERIFY(problemBox(w));
      }
      QVERIFY(w.isEnabled());
      QCOMPARE(tabs->count(), 5);
      for (int i = 0; i < 5; ++i) {
        QVERIFY2(tabs->isTabEnabled(i), qPrintable(QString::number(i)));
        QVERIFY2(tabs->isTabVisible(i), qPrintable(QString::number(i)));
        QVERIFY2(tabs->widget(i)->isEnabled(), qPrintable(QString::number(i)));
      }
      for (const char *name : {"tasksListWidget", "queueListWidget"}) {
        auto *list = w.findChild<QListWidget *>(QString::fromLatin1(name));
        QVERIFY2(list, name);
        QVERIFY2(list->isEnabled(), name);
      }
      for (const char *name : {"jobsArea", "buttonPrefs"}) {
        auto *widget = w.findChild<QWidget *>(QString::fromLatin1(name));
        QVERIFY2(widget, name);
        QVERIFY2(widget->isEnabled(), name);
      }
    }
  }
};

QTEST_MAIN(TestStartup)
#include "tst_startup.moc"
