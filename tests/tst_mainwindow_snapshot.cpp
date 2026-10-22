#include <QtTest>
#include <QtWidgets>

#include <algorithm>

#include "main_window.h"
#include "rb_test_support.h"
#include "utils.h"

// T-MW-SNAPSHOT: the widget and action state of a freshly constructed
// MainWindow, serialized and compared with a committed golden text. It guards
// the constructor split (no behavior change). The fixture is Linux-specific:
// MainWindow has Q_OS_MACOS/Q_OS_WIN branches (revision 13).
class TestMainWindowSnapshot : public QObject {
  Q_OBJECT

  // One snapshot entry; the sort key is (name, className, line).
  struct Entry {
    QString name;
    QString cls;
    QString line;
  };

  static bool internalName(const QString &name) {
    return name.startsWith(QLatin1String("qt_")) ||
           name.startsWith(QLatin1String("_q_"));
  }

  // Environment-dependent parts become placeholders, then the text is escaped
  // so that every entry stays on one line.
  static QString field(QString s) {
    s.replace(rbTestDir().path(), QStringLiteral("<TESTDIR>"));
    s.replace(QStringLiteral(RCLONE_BROWSER_VERSION),
              QStringLiteral("<VERSION>"));
    s.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    s.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
    s.replace(QStringLiteral("\r"), QStringLiteral("\\r"));
    s.replace(QStringLiteral("|"), QStringLiteral("\\|"));
    return s;
  }

  static QString flag(bool b) { return b ? QStringLiteral("1") : QStringLiteral("0"); }

  static QString mainWindowSnapshot(QWidget *w) {
    QVector<Entry> widgets;
    const auto children = w->findChildren<QWidget *>();
    for (QWidget *c : children) {
      const QString name = c->objectName();
      if (name.isEmpty() || internalName(name)) {
        continue;
      }
      QString text;
      QString checked = QStringLiteral("-");
      if (auto *b = qobject_cast<QAbstractButton *>(c)) {
        text = b->text();
        if (b->isCheckable()) {
          checked = flag(b->isChecked());
        }
      } else if (auto *l = qobject_cast<QLabel *>(c)) {
        text = l->text();
      } else if (auto *e = qobject_cast<QLineEdit *>(c)) {
        text = e->text();
      }
      const QString cls = QString::fromLatin1(c->metaObject()->className());
      const QString line = QStringLiteral("W|") + field(name) + '|' +
                           field(cls) + '|' + flag(c->isEnabled()) + '|' +
                           flag(c->isVisibleTo(w)) + '|' + field(text) + '|' +
                           field(c->toolTip()) + '|' + checked;
      widgets.append({name, cls, line});
    }
    std::sort(widgets.begin(), widgets.end(),
              [](const Entry &a, const Entry &b) {
                if (a.name != b.name) {
                  return a.name < b.name;
                }
                if (a.cls != b.cls) {
                  return a.cls < b.cls;
                }
                return a.line < b.line;
              });

    QVector<Entry> actions;
    const auto acts = w->findChildren<QAction *>();
    for (QAction *a : acts) {
      const QString name = a->objectName();
      if (name.isEmpty() || internalName(name)) {
        continue;
      }
      const QString line =
          QStringLiteral("A|") + field(name) + '|' + field(a->text()) + '|' +
          flag(a->isEnabled()) + '|' + flag(a->isCheckable()) + '|' +
          flag(a->isChecked()) + '|' +
          field(a->shortcut().toString(QKeySequence::PortableText));
      actions.append({name, QString(), line});
    }
    std::sort(actions.begin(), actions.end(),
              [](const Entry &a, const Entry &b) {
                if (a.name != b.name) {
                  return a.name < b.name;
                }
                return a.line < b.line;
              });

    QString out;
    for (const Entry &e : widgets) {
      out += e.line + '\n';
    }
    for (const Entry &e : actions) {
      out += e.line + '\n';
    }
    return out;
  }

  static QString tabsSnapshot(QTabWidget *tabs) {
    QString out;
    for (int i = 0; i < tabs->count(); ++i) {
      out += QStringLiteral("T|") + QString::number(i) + '|' +
             field(tabs->tabText(i)) + '\n';
    }
    return out;
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    // QStandardPaths test mode keeps AppLocalDataLocation under $HOME/.qttest,
    // which ignores the hermetic settings path and is shared by every test
    // binary. A private HOME keeps tasks.bin, queue.conf and scheduler.conf of
    // other tests out of the constructor's restore paths.
    QVERIFY(QDir().mkpath(rbTestDir().path() + QStringLiteral("/home")));
    qputenv("HOME", (rbTestDir().path() + QStringLiteral("/home")).toUtf8());
    // Revision 13: the same on every runner, and no real rcd from a unit test.
    SetRclone(QStringLiteral("/bin/false"));
    // The constructor re-reads the rclone path from the settings; without the
    // key it searches PATH and, when nothing is found, blocks in a modal box.
    GetSettings()->setValue(QStringLiteral("Settings/rclone"),
                            QStringLiteral("/bin/false"));
  }

  // T-MW-SNAPSHOT
  void T_MW_SNAPSHOT() {
    QString actual;
    {
      MainWindow w;
      auto *tabs = w.findChild<QTabWidget *>(QStringLiteral("tabs"));
      QVERIFY(tabs != nullptr);
      actual = mainWindowSnapshot(&w) + tabsSnapshot(tabs);
    }

    // The destroyed window must not leave an rcd behind.
#ifdef Q_OS_UNIX
    int exitCode = -1;
    for (int i = 0; i < 20 && exitCode != 1; ++i) {
      QProcess p;
      p.start("pgrep", {"-f", "rcd --rc-addr 127.0.0.1:0"});
      QVERIFY(p.waitForFinished(5000));
      exitCode = p.exitCode();
      if (exitCode != 1) {
        QTest::qWait(100);
      }
    }
    QCOMPARE(exitCode, 1);
#endif

    // Always leave the actual text next to the binary (S1 records the fixture
    // by copying this file).
    const QString actualPath = QCoreApplication::applicationDirPath() +
                               QStringLiteral("/mainwindow_snapshot.actual.txt");
    {
      QFile f(actualPath);
      QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Truncate),
               qPrintable(actualPath));
      f.write(actual.toUtf8());
    }

    const QString fixturePath =
        rbFixture(QStringLiteral("synthetic/mainwindow_snapshot.txt"));
    if (qEnvironmentVariable("RB_RECORD_SNAPSHOT") == QLatin1String("1")) {
      QFile f(fixturePath);
      QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Truncate),
               qPrintable(fixturePath));
      f.write(actual.toUtf8());
      QSKIP("recorded");
    }
    QFile fx(fixturePath);
    if (!fx.exists()) {
      QFAIL(qPrintable(QStringLiteral("fixture missing, actual snapshot: ") +
                       actualPath));
    }
    QVERIFY2(fx.open(QIODevice::ReadOnly), qPrintable(fixturePath));
    const QString expected = QString::fromUtf8(fx.readAll());

    const QStringList want = expected.split('\n');
    const QStringList got = actual.split('\n');
    const int n = std::min(want.size(), got.size());
    for (int i = 0; i < n; ++i) {
      QVERIFY2(want.at(i) == got.at(i),
               qPrintable(QStringLiteral("line %1 differs\n  expected: %2\n  "
                                         "actual:   %3")
                              .arg(i + 1)
                              .arg(want.at(i), got.at(i))));
    }
    QCOMPARE(got.size(), want.size());
    QCOMPARE(actual, expected);
  }
};

QTEST_MAIN(TestMainWindowSnapshot)
#include "tst_mainwindow_snapshot.moc"
