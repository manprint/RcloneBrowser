#include <QtTest>

#include "list_filter.h"

class TestListFilter : public QObject {
  Q_OBJECT

private slots:
  // T-FILTER-MATCH: every token in some field, any case, empty matches all.
  void T_FILTER_MATCH() {
    const QStringList one{QString("Copy gd:Photos")};
    const QStringList two{QString("Copy gd:Photos"), QString("to /x")};
    QVERIFY(filterMatches(QString(""), one));
    QVERIFY(filterMatches(QString("gd photos"), two));
    QVERIFY(!filterMatches(QString("gd backup"), one));
    QVERIFY(filterMatches(QString("PHO"), QStringList{QString("photos")}));

    QVERIFY(filterMatches(QString("   "), QStringList{QString("x")}));
    QVERIFY(filterMatches(QString(""), QStringList()));
    QVERIFY(!filterMatches(QString("a"), QStringList()));
    QVERIFY(filterMatches(QString("gd  PHOTOS\t"), one));
    // the tokens may be spread over two fields
    QVERIFY(filterMatches(QString("gd x"), two));
    QVERIFY(filterMatches(QString::fromUtf8("\xC3\x89"),
                          QStringList{QString::fromUtf8("\xC3\xA9")}));
  }
};

QTEST_GUILESS_MAIN(TestListFilter)
#include "tst_list_filter.moc"
