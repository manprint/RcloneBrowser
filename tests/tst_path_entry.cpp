#include <QtTest>

#include "path_entry.h"

class TestPathEntry : public QObject {
  Q_OBJECT

private slots:
  // T-PATH-NORMALIZE: trim, prefix, separators; "" is nothing to open.
  void T_PATH_NORMALIZE() {
    QCOMPARE(normalizeEnteredPath("  a//b/ ", "r", false), QString("a/b"));
    QCOMPARE(normalizeEnteredPath("r:/a//b/", "r", false), QString("/a/b"));
    QCOMPARE(normalizeEnteredPath("/", "r", false), QString("/"));
    QCOMPARE(normalizeEnteredPath("//", "r", false), QString("/"));
    QCOMPARE(normalizeEnteredPath("", "r", false), QString(""));
    QCOMPARE(normalizeEnteredPath("  ", "r", false), QString(""));
    QCOMPARE(normalizeEnteredPath("/tmp/x/../y", "r", true),
             QString("/tmp/y"));
    QCOMPARE(normalizeEnteredPath("s:a", "r", false), QString("s:a"));
    // trim first, then the prefix, no second trim
    QCOMPARE(normalizeEnteredPath("  r:a", "r", false), QString("a"));
    QCOMPARE(normalizeEnteredPath("r: a", "r", false), QString(" a"));
    // a bare drive of a local remote is its root
    QCOMPARE(normalizeEnteredPath("C:", "r", true), QString("C:/"));
#ifdef Q_OS_WIN
    QCOMPARE(normalizeEnteredPath("r:C:\\x\\..\\y", "r", true),
             QString("C:/y"));
    QCOMPARE(normalizeEnteredPath("c:\\", "r", true), QString("c:/"));
#endif
  }

  // T-PATH-PREFIX: only "<remote>:" at the start, case-sensitive.
  void T_PATH_PREFIX() {
    QCOMPARE(stripRemotePrefix("r:a/b", "r"), QString("a/b"));
    QCOMPARE(stripRemotePrefix("s:a", "r"), QString("s:a"));
    QCOMPARE(stripRemotePrefix("r:", "r"), QString(""));
    QCOMPARE(stripRemotePrefix("rr:a", "r"), QString("rr:a"));
    QCOMPARE(stripRemotePrefix("r:a", ""), QString("r:a"));
  }

  // T-PATH-ROOTLESS: the remotes that need a typed first path.
  void T_PATH_ROOTLESS() {
    QVERIFY(remoteTypeNeedsPathEntry("internetarchive"));
    QVERIFY(!remoteTypeNeedsPathEntry("drive"));
    QVERIFY(!remoteTypeNeedsPathEntry("local"));
    QVERIFY(!remoteTypeNeedsPathEntry(""));
  }
};

QTEST_GUILESS_MAIN(TestPathEntry)
#include "tst_path_entry.moc"
