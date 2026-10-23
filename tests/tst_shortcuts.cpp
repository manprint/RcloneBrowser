#include <QtTest>
#include <QtWidgets>

#include <map>
#include <memory>

#include "global.h"
#include "icon_cache.h"
#include "main_window.h"
#include "rb_test_support.h"
#include "remote_widget.h"
#include "utils.h"

// Keyboard shortcuts (#94): the keys of the remote view, the tab and filter
// keys of the main window, and the proof that no two of them are the same.
// Neither widget starts rclone here: the remote type "internetarchive" lists
// nothing and MainWindow runs with /bin/false. No test triggers Ctrl+R,
// Ctrl+Return, Ctrl+E or Ctrl+Shift+Return (a listing, task dialogs).
class TestShortcuts : public QObject {
  Q_OBJECT

  struct KeyOwner {
    QKeySequence key;
    QString owner;
  };

  static QString ownerName(const QObject *o) {
    QString n = o->objectName();
    if (n.isEmpty()) {
      if (auto *a = qobject_cast<const QAction *>(o)) {
        n = a->text();
      }
    }
    return QString::fromUtf8(o->metaObject()->className()) + ':' + n;
  }

  // "Alt+D: a and b" for every key that has more than one owner
  static QStringList duplicates(const QVector<KeyOwner> &all) {
    std::map<QString, QStringList> byKey;
    for (const KeyOwner &e : all) {
      byKey[e.key.toString(QKeySequence::PortableText)] << e.owner;
    }
    QStringList out;
    for (const auto &kv : byKey) {
      if (kv.second.size() > 1) {
        out << kv.first + QStringLiteral(": ") +
                   kv.second.join(QStringLiteral(" and "));
      }
    }
    return out;
  }

  // the one action of `w` with this key (nullptr when none or several)
  static QAction *byKey(QWidget *w, const QString &keys) {
    const QKeySequence want(keys, QKeySequence::PortableText);
    QAction *found = nullptr;
    int n = 0;
    const auto acts = w->actions();
    for (QAction *a : acts) {
      if (a->shortcut() == want) {
        found = a;
        ++n;
      }
    }
    return n == 1 ? found : nullptr;
  }

  static QString keyText(const QAction *a) {
    return a->shortcut().toString(QKeySequence::PortableText);
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    // a private HOME and /bin/false as rclone, as in the snapshot test: the
    // MainWindow constructor must find no tasks of other tests and start no
    // rclone or modal box
    QVERIFY(QDir().mkpath(rbTestDir().path() + QStringLiteral("/home")));
    qputenv("HOME", (rbTestDir().path() + QStringLiteral("/home")).toUtf8());
    SetRclone(QStringLiteral("/bin/false"));
    GetSettings()->setValue(QStringLiteral("Settings/rclone"),
                            QStringLiteral("/bin/false"));
  }

  // T-SC-REMOTE-CONTEXT
  void T_SC_REMOTE_CONTEXT() {
    IconCache icons;
    QTabWidget tabs;
    tabs.addTab(new QWidget, QStringLiteral("first"));
    auto *rw = new RemoteWidget(&icons, QStringLiteral("ia"),
                                QStringLiteral("internetarchive"), nullptr);
    tabs.addTab(rw, QStringLiteral("ia"));

    // the map of keys (exact: an action of the view with a key must be listed)
    const std::map<QString, QString> expected = {
        {"refresh", "F5"},        {"mkdir", "F7"},
        {"rename", "F2"},         {"purge", "Del"},
        {"upload", "Alt+U"},      {"download", "Alt+D"},
        {"actionGoToPath", "Ctrl+L"}, {"getInfo", "Alt+I"},
        {"getSize", "Alt+S"},     {"getTree", "Alt+T"},
        {"link", "Alt+L"},        {"actionNewMount", "Alt+M"},
        {"stream", "Alt+V"},      {"move", "F6"},
        {"actionCopyTo", "Shift+F5"}, {"actionServe", "Alt+E"},
        {"actionUploadFromUrl", ""}, {"mount", ""}};

    // every key is a window shortcut owned by the remote widget itself, and no
    // action outside the map has one
    QStringList bad;
    QStringList unexpected;
    const auto actions = rw->findChildren<QAction *>();
    for (QAction *a : actions) {
      if (a->shortcut().isEmpty()) {
        continue;
      }
      if (a->shortcutContext() != Qt::WindowShortcut ||
          !a->associatedObjects().contains(static_cast<QObject *>(rw))) {
        bad << ownerName(a);
      }
      const auto it = expected.find(a->objectName());
      if (it == expected.end() || it->second.isEmpty()) {
        unexpected << ownerName(a);
      }
    }
    QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QStringLiteral(", "))));
    QVERIFY2(unexpected.isEmpty(),
             qPrintable(unexpected.join(QStringLiteral(", "))));

    for (const auto &kv : expected) {
      auto *a = rw->findChild<QAction *>(kv.first);
      QVERIFY2(a != nullptr, qPrintable(kv.first));
      QCOMPARE(keyText(a), kv.second);
    }

    // Ctrl+W closes through the tab widget's own signal
    QShortcut *closeKey = nullptr;
    const auto shortcuts = rw->findChildren<QShortcut *>();
    for (QShortcut *sc : shortcuts) {
      if (sc->key() == QKeySequence(QKeySequence::Close)) {
        closeKey = sc;
      }
    }
    QVERIFY(closeKey != nullptr);
    QSignalSpy spy(&tabs, &QTabWidget::tabCloseRequested);
    QCOMPARE(tabs.indexOf(rw), 1);
    emit closeKey->activated();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toInt(), 1);
    QCOMPARE(tabs.count(), 2);  // the signal's slots remove the tab, not the key

    // no tab widget around: nothing to close, nothing to crash
    auto *loose = new RemoteWidget(&icons, QStringLiteral("ia2"),
                                   QStringLiteral("internetarchive"), nullptr);
    QShortcut *looseKey = nullptr;
    const auto looseShortcuts = loose->findChildren<QShortcut *>();
    for (QShortcut *sc : looseShortcuts) {
      if (sc->key() == QKeySequence(QKeySequence::Close)) {
        looseKey = sc;
      }
    }
    QVERIFY(looseKey != nullptr);
    emit looseKey->activated();
    QCOMPARE(spy.count(), 0);
    delete loose;

    // a tab widget around but not the widget's page (index -1): nothing is
    // emitted, so MainWindow's slots never see index -1
    auto *orphan = new RemoteWidget(&icons, QStringLiteral("ia3"),
                                    QStringLiteral("internetarchive"), &tabs);
    QShortcut *orphanKey = nullptr;
    const auto orphanShortcuts = orphan->findChildren<QShortcut *>();
    for (QShortcut *sc : orphanShortcuts) {
      if (sc->key() == QKeySequence(QKeySequence::Close)) {
        orphanKey = sc;
      }
    }
    QVERIFY(orphanKey != nullptr);
    QCOMPARE(tabs.indexOf(orphan), -1);
    emit orphanKey->activated();
    QCOMPARE(spy.count(), 0);
    QCOMPARE(tabs.count(), 2);
    delete orphan;
  }

  // T-SC-UNIQUE
  void T_SC_UNIQUE() {
    IconCache icons;
    QTabWidget tabs;
    tabs.addTab(new QWidget, QStringLiteral("first"));
    auto *rw = new RemoteWidget(&icons, QStringLiteral("ia"),
                                QStringLiteral("internetarchive"), nullptr);
    tabs.addTab(rw, QStringLiteral("ia"));
    MainWindow w;

    QVector<KeyOwner> all;
    auto addActions = [&all](QWidget *root) {
      const auto actions = root->findChildren<QAction *>();
      for (QAction *a : actions) {
        const auto keys = a->shortcuts();
        for (const QKeySequence &k : keys) {
          all.append({k, QStringLiteral("action ") + ownerName(a)});
        }
      }
    };
    addActions(rw);
    addActions(&w);

    // Close (every binding of the platform) and the Esc of the path field
    bool hasClose = false;
    const auto shortcuts = rw->findChildren<QShortcut *>();
    for (QShortcut *sc : shortcuts) {
      const auto keys = sc->keys();
      for (const QKeySequence &k : keys) {
        all.append({k, QStringLiteral("shortcut ") + ownerName(sc)});
      }
      if (keys == QKeySequence::keyBindings(QKeySequence::Close)) {
        hasClose = true;
      }
    }
    QVERIFY(hasClose);

    // the menu bar mnemonics
    QSet<QString> mnemonics;
    const auto menus = w.menuBar()->actions();
    for (QAction *a : menus) {
      const QKeySequence m = QKeySequence::mnemonic(a->text());
      if (!m.isEmpty()) {
        all.append({m, QStringLiteral("menu ") + a->text()});
        mnemonics.insert(m.toString(QKeySequence::PortableText));
      }
    }
    QVERIFY(mnemonics.contains(QStringLiteral("Alt+F")));
    QVERIFY(mnemonics.contains(QStringLiteral("Alt+H")));

    // buttons: a default action gives the button no key of its own
    QStringList buttonKeys;
    auto addButtons = [&all, &buttonKeys](QWidget *root) {
      const auto buttons = root->findChildren<QAbstractButton *>();
      for (QAbstractButton *b : buttons) {
        if (!b->shortcut().isEmpty()) {
          const QString what = ownerName(b) + ' ' +
                               b->shortcut().toString(QKeySequence::PortableText);
          buttonKeys << what;
          all.append({b->shortcut(), QStringLiteral("button ") + ownerName(b)});
        }
      }
    };
    addButtons(rw);
    addButtons(&w);
    QVERIFY2(buttonKeys.isEmpty(), qPrintable(buttonKeys.join(QStringLiteral(", "))));

    QVERIFY2(all.size() > 30, qPrintable(QString::number(all.size())));
    const QStringList dup = duplicates(all);
    QVERIFY2(dup.isEmpty(), qPrintable(dup.join(QStringLiteral("; "))));

    // the check itself: a planted copy of an existing entry is reported
    QVector<KeyOwner> planted = all;
    planted.append({all.first().key, QStringLiteral("planted")});
    QVERIFY(!duplicates(planted).isEmpty());
  }

  // T-SC-MAINWINDOW
  void T_SC_MAINWINDOW() {
    MainWindow w;
    auto *tabs = w.findChild<QTabWidget *>(QStringLiteral("tabs"));
    auto *tasksFilter = w.findChild<QLineEdit *>(QStringLiteral("tasksFilter"));
    auto *jobsFilter = w.findChild<QLineEdit *>(QStringLiteral("jobsFilter"));
    QVERIFY(tabs != nullptr && tasksFilter != nullptr && jobsFilter != nullptr);
    QVERIFY(w.findChild<QToolButton *>(QStringLiteral("refresh")) != nullptr);

    QAction *tab[5] = {};
    for (int n = 0; n < 5; ++n) {
      tab[n] = byKey(&w, QStringLiteral("Ctrl+%1").arg(n + 1));
      QVERIFY2(tab[n] != nullptr, qPrintable(QString::number(n + 1)));
    }
    QAction *find = byKey(&w, QStringLiteral("Ctrl+F"));
    QVERIFY(find != nullptr);
    QVERIFY(byKey(&w, QStringLiteral("Ctrl+R")) != nullptr);

    // the Tasks tab keys belong to the actions of its buttons, not the window
    const struct {
      const char *name;
      const char *keys;
    } taskKeys[] = {{"actionRun", "Ctrl+Return"},
                    {"actionEdit", "Ctrl+E"},
                    {"actionDryRun", "Ctrl+Shift+Return"}};
    for (const auto &k : taskKeys) {
      auto *a = w.findChild<QAction *>(QString::fromLatin1(k.name));
      QVERIFY2(a != nullptr, k.name);
      QCOMPARE(keyText(a), QString::fromLatin1(k.keys));
      QVERIFY2(!w.actions().contains(a), k.name);
      // only widgets of the Tasks page carry them (not tabs or the central
      // widget, which would fire the key on every tab)
      const auto owners = a->associatedObjects();
      int widgets = 0;
      for (QObject *o : owners) {
        if (auto *wid = qobject_cast<QWidget *>(o)) {
          ++widgets;
          QVERIFY2(tabs->widget(2)->isAncestorOf(wid),
                   qPrintable(QString::fromLatin1(k.name) + ' ' +
                              ownerName(wid)));
        }
      }
      QVERIFY2(widgets > 0, k.name);
    }

    tab[1]->trigger();
    QCOMPARE(tabs->currentIndex(), 1);
    tab[2]->trigger();
    QCOMPARE(tabs->currentIndex(), 2);
    tab[0]->trigger();
    QCOMPARE(tabs->currentIndex(), 0);

    // Ctrl+F: the filter of the Tasks and the Jobs tab (its text selected),
    // nothing elsewhere
    QVERIFY(w.focusWidget() != tasksFilter);
    tasksFilter->setText(QStringLiteral("abc"));
    tasksFilter->deselect();
    tabs->setCurrentIndex(2);
    // the page switch must not have put the focus in the filter already
    auto *tasksList =
        w.findChild<QListWidget *>(QStringLiteral("tasksListWidget"));
    QVERIFY(tasksList != nullptr);
    tasksList->setFocus();
    QVERIFY(w.focusWidget() != tasksFilter);
    find->trigger();
    QCOMPARE(w.focusWidget(), static_cast<QWidget *>(tasksFilter));
    QCOMPARE(tasksFilter->selectedText(), QStringLiteral("abc"));
    tasksFilter->clear();
    jobsFilter->setText(QStringLiteral("xyz"));
    jobsFilter->deselect();
    tabs->setCurrentIndex(1);
    auto *jobsArea = w.findChild<QScrollArea *>(QStringLiteral("jobsArea"));
    QVERIFY(jobsArea != nullptr);
    jobsArea->setFocus();
    QVERIFY(w.focusWidget() != jobsFilter);
    find->trigger();
    QCOMPARE(w.focusWidget(), static_cast<QWidget *>(jobsFilter));
    QCOMPARE(jobsFilter->selectedText(), QStringLiteral("xyz"));
    jobsFilter->clear();
    // on the other tabs the focus stays where it was, in neither filter
    auto *queueList =
        w.findChild<QListWidget *>(QStringLiteral("queueListWidget"));
    auto *remotes = w.findChild<QListWidget *>(QStringLiteral("remotes"));
    QVERIFY(queueList != nullptr && remotes != nullptr);
    tabs->setCurrentIndex(3);
    queueList->setFocus();
    find->trigger();
    QCOMPARE(w.focusWidget(), static_cast<QWidget *>(queueList));
    tabs->setCurrentIndex(0);
    remotes->setFocus();
    find->trigger();
    QCOMPARE(w.focusWidget(), static_cast<QWidget *>(remotes));

    // a tab hidden in Preferences is not entered
    tabs->setCurrentIndex(0);
    tabs->setTabVisible(3, false);
    tab[3]->trigger();
    QCOMPARE(tabs->currentIndex(), 0);
    tabs->setTabVisible(3, true);
    tab[3]->trigger();
    QCOMPARE(tabs->currentIndex(), 3);
  }

  // T-SC-SERVE-BUTTON
  void T_SC_SERVE_BUTTON() {
    // the Serve button sits next to Stream in the button row and follows the
    // actionServe default action, in every button style
    struct RestoreStyle {
      QVariant value;
      ~RestoreStyle() {
        auto s = GetSettings();
        if (value.isValid()) {
          s->setValue(QStringLiteral("Settings/buttonStyle"), value);
        } else {
          s->remove(QStringLiteral("Settings/buttonStyle"));
        }
      }
    } restore{GetSettings()->value(QStringLiteral("Settings/buttonStyle"))};

    const QStringList styles = {QStringLiteral("textandicon"),
                                QStringLiteral("textonly"),
                                QStringLiteral("icononly")};
    for (const QString &style : styles) {
      const QString tag = QStringLiteral("buttonStyle=") + style;
      GetSettings()->setValue(QStringLiteral("Settings/buttonStyle"), style);

      IconCache icons;
      std::unique_ptr<RemoteWidget> rw(new RemoteWidget(
          &icons, QStringLiteral("ia"), QStringLiteral("internetarchive"),
          nullptr));
      auto *serve = rw->findChild<QToolButton *>(QStringLiteral("buttonServe"));
      auto *stream =
          rw->findChild<QToolButton *>(QStringLiteral("buttonStream"));
      QVERIFY2(serve != nullptr && stream != nullptr, qPrintable(tag));
      QAction *actionServe = rw->findChild<QAction *>(QStringLiteral("actionServe"));
      QVERIFY2(actionServe != nullptr, qPrintable(tag));

      QVERIFY2(serve->defaultAction() == actionServe, qPrintable(tag));
      QVERIFY2(serve->parentWidget() == stream->parentWidget(), qPrintable(tag));

      // same layout, Serve right after Stream
      QLayout *lay = stream->parentWidget()->layout();
      QVERIFY2(lay != nullptr, qPrintable(tag));
      auto indexIn = [lay](QWidget *w) {
        for (int i = 0; i < lay->count(); ++i) {
          if (lay->itemAt(i)->widget() == w) {
            return i;
          }
        }
        return -1;
      };
      QVERIFY2(indexIn(stream) >= 0 && indexIn(serve) >= 0, qPrintable(tag));
      QVERIFY2(indexIn(serve) == indexIn(stream) + 1, qPrintable(tag));

      // same look as Stream
      QVERIFY2(serve->toolButtonStyle() == stream->toolButtonStyle(),
               qPrintable(tag));
      QCOMPARE(serve->iconSize(), stream->iconSize());
      QCOMPARE(serve->minimumWidth(), stream->minimumWidth());

      // no daemon in the test: the action, and the button with it, is disabled
      QVERIFY2(!serve->isEnabled(), qPrintable(tag));
    }
  }
};

QTEST_MAIN(TestShortcuts)
#include "tst_shortcuts.moc"
