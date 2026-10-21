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

  void cleanupTestCase() {}
};

QTEST_GUILESS_MAIN(TestUtils)
#include "tst_utils.moc"
