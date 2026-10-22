#include <QtTest>

#include "upload_headers.h"

class TestUploadHeaders : public QObject {
  Q_OBJECT

private slots:
  // T_HDR_PARSE: "Name: value" lines, trimmed, split at the first colon; a bad
  // line rejects the whole text.
  void T_HDR_PARSE() {
    const auto two =
        parseUploadHeaders(QString("Cache-Control: no-cache\n\nX-Foo: a:b"));
    QVERIFY(two.has_value());
    QCOMPARE(*two, (QStringList{"Cache-Control: no-cache", "X-Foo: a:b"}));

    QVERIFY(!parseUploadHeaders(QString("bad header")).has_value());
    QVERIFY(!parseUploadHeaders(QString("Bad Name: x")).has_value());
    QVERIFY(!parseUploadHeaders(QString(": x")).has_value());
    // one bad line rejects the good ones
    QVERIFY(!parseUploadHeaders(QString("A: 1\nbad")).has_value());

    const auto trimmed = parseUploadHeaders(QString("  X-A :  v  \r\n"));
    QVERIFY(trimmed.has_value());
    QCOMPARE(*trimmed, (QStringList{"X-A: v"}));

    const auto empty = parseUploadHeaders(QString(""));
    QVERIFY(empty.has_value());
    QVERIFY(empty->isEmpty());
    const auto blank = parseUploadHeaders(QString(" \n "));
    QVERIFY(blank.has_value());
    QVERIFY(blank->isEmpty());

    // an empty value is allowed
    const auto noValue = parseUploadHeaders(QString("X-Empty:"));
    QVERIFY(noValue.has_value());
    QCOMPARE(*noValue, (QStringList{"X-Empty: "}));

    QCOMPARE(headerUploadArgs(QStringList{"A: 1", "B: 2"}),
             (QStringList{"--header-upload", "A: 1", "--header-upload",
                          "B: 2"}));
    QVERIFY(headerUploadArgs(QStringList()).isEmpty());
  }

  // T_TAG_PARSE: "key=value" lines, at most 10, no duplicate key.
  void T_TAG_PARSE() {
    const auto two = parseS3Tags(QString("proj=rb\nenv=test"));
    QVERIFY(two.has_value());
    QCOMPARE(two->size(), qsizetype(2));
    QCOMPARE(two->at(0).key, QString("proj"));
    QCOMPARE(two->at(0).value, QString("rb"));
    QCOMPARE(two->at(1).key, QString("env"));
    QCOMPARE(two->at(1).value, QString("test"));

    QVERIFY(!parseS3Tags(QString("novalue")).has_value());
    QVERIFY(!parseS3Tags(QString("=x")).has_value());

    // the S3 limit: 10 tags pass, 11 do not
    QStringList lines;
    for (int i = 1; i <= 10; ++i) {
      lines << QString("k%1=v").arg(i);
    }
    const auto ten = parseS3Tags(lines.join('\n'));
    QVERIFY(ten.has_value());
    QCOMPARE(ten->size(), qsizetype(10));
    lines << QString("k11=v");
    QVERIFY(!parseS3Tags(lines.join('\n')).has_value());

    const auto emptyValue = parseS3Tags(QString("k="));
    QVERIFY(emptyValue.has_value());
    QCOMPARE(emptyValue->size(), qsizetype(1));
    QCOMPARE(emptyValue->at(0).key, QString("k"));
    QCOMPARE(emptyValue->at(0).value, QString());

    const auto equals = parseS3Tags(QString("a=b=c"));
    QVERIFY(equals.has_value());
    QCOMPARE(equals->at(0).key, QString("a"));
    QCOMPARE(equals->at(0).value, QString("b=c"));

    const auto none = parseS3Tags(QString(""));
    QVERIFY(none.has_value());
    QVERIFY(none->isEmpty());

    QVERIFY(!parseS3Tags(QString("a=1\na=2")).has_value());
  }

  // T_TAG_HEADER: the X-Amz-Tagging value is a percent-encoded query string.
  void T_TAG_HEADER() {
    QCOMPARE(s3TaggingHeader(QVector<S3Tag>{{"proj", "rb"}, {"a b", "c&d"}}),
             QString("X-Amz-Tagging: proj=rb&a%20b=c%26d"));
    QCOMPARE(s3TaggingHeader(QVector<S3Tag>{{"proj", "rb"}, {"env", "test"}}),
             QString("X-Amz-Tagging: proj=rb&env=test"));
    QCOMPARE(s3TaggingHeader(QVector<S3Tag>{{"k", "v=x"}}),
             QString("X-Amz-Tagging: k=v%3Dx"));
  }
};

QTEST_GUILESS_MAIN(TestUploadHeaders)
#include "tst_upload_headers.moc"
