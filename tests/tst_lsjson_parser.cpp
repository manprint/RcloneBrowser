#include <QtTest>

#include "rb_test_support.h"
#include "lsjson_parser.h"
#include "rclone_format.h"
#include "rclone_log.h"

namespace {

QByteArray readBytes(const QString &rel) {
  QFile f(rbFixture(rel));
  if (!f.open(QIODevice::ReadOnly)) {
    qWarning() << "cannot open fixture" << f.fileName();
    return {};
  }
  return f.readAll();
}

// Reads a fixture and splits it on '\n' (a final newline does not add an empty line).
QList<QByteArray> readLines(const QString &rel) {
  QList<QByteArray> lines = readBytes(rel).split('\n');
  if (!lines.isEmpty() && lines.last().isEmpty()) {
    lines.removeLast();
  }
  return lines;
}

struct Parsed {
  QVector<ListEntry> entries;
  int skipped = 0;
};

Parsed parseAll(const QByteArray &data) {
  LsjsonStreamParser p;
  p.feed(data);
  p.finish();
  return {p.takeEntries(), p.skippedLines()};
}

// Feeds data in pieces of `step` bytes.
Parsed parseChunks(const QByteArray &data, qsizetype step) {
  LsjsonStreamParser p;
  for (qsizetype off = 0; off < data.size(); off += step) {
    p.feed(data.mid(off, step));
  }
  p.finish();
  return {p.takeEntries(), p.skippedLines()};
}

// Feeds data as two chunks, split before byte `at`.
Parsed parseSplit(const QByteArray &data, qsizetype at) {
  LsjsonStreamParser p;
  p.feed(data.left(at));
  p.feed(data.mid(at));
  p.finish();
  return {p.takeEntries(), p.skippedLines()};
}

void compareEntries(const QVector<ListEntry> &got, const QVector<ListEntry> &want) {
  QCOMPARE(got.size(), want.size());
  for (qsizetype i = 0; i < want.size(); ++i) {
    QCOMPARE(got.at(i).path, want.at(i).path);
    QCOMPARE(got.at(i).size, want.at(i).size);
    QCOMPARE(got.at(i).isDir, want.at(i).isDir);
  }
}

const ListEntry *findPath(const QVector<ListEntry> &v, const QString &path) {
  for (const ListEntry &e : v) {
    if (e.path == path) {
      return &e;
    }
  }
  return nullptr;
}

// U+00F1 (n with tilde) is built from its code point, so this source file stays ASCII.
QString enye() { return QString(QChar(0x00F1)); }

QString recursiveQuotePath() {
  return QStringLiteral("dir [x] (y)/") + enye() + QStringLiteral("\"quote'.txt");
}

} // namespace

class TestLsjsonParser : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-LSJ-TREE
  void T_LSJ_TREE() {
    const QByteArray data = readBytes("rclone-1.75/lsjson_tree.json");
    QVERIFY(!data.isEmpty());
    const Parsed p = parseAll(data);
    QCOMPARE(p.entries.size(), qsizetype(6));
    QCOMPARE(p.skipped, 0);

    struct Want {
      const char *path;
      bool isDir;
      qint64 size;
    };
    const Want want[] = {
        {"12345 ", true, 4096},
        {"a:b.txt", false, 1},
        {"dir [x] (y)", true, 4096},
        {"file 1.bin", false, 1500000},
        {"file 2.bin", false, 1500000},
        {"file 3.bin", false, 1500000},
    };
    for (const Want &w : want) {
      const QString path = QString::fromUtf8(w.path);
      const ListEntry *e = findPath(p.entries, path);
      QVERIFY2(e != nullptr, w.path);
      QCOMPARE(e->name, path);
      QCOMPARE(e->isDir, w.isDir);
      QCOMPARE(e->size, w.size);
      QVERIFY(e->modTime.isValid());
    }
  }

  // T-LSJ-CHUNKS
  void T_LSJ_CHUNKS() {
    const QByteArray tree = readBytes("rclone-1.75/lsjson_tree.json");
    QVERIFY(!tree.isEmpty());
    const Parsed treeOne = parseAll(tree);
    const Parsed treeChunked = parseChunks(tree, 7);
    compareEntries(treeChunked.entries, treeOne.entries);
    QCOMPARE(treeChunked.skipped, treeOne.skipped);

    const QByteArray rec = readBytes("rclone-1.75/lsjson_recursive.json");
    QVERIFY(!rec.isEmpty());
    const Parsed recOne = parseAll(rec);
    const Parsed recChunked = parseChunks(rec, 7);
    compareEntries(recChunked.entries, recOne.entries);
    QCOMPARE(recChunked.skipped, recOne.skipped);

    // Split between the two bytes of U+00F1 (UTF-8 C3 B1).
    const QByteArray needle = enye().toUtf8();
    const qsizetype at = rec.indexOf(needle);
    QVERIFY(at > 0);
    const Parsed recSplit = parseSplit(rec, at + 1);
    compareEntries(recSplit.entries, recOne.entries);
    QCOMPARE(recSplit.skipped, recOne.skipped);
    const ListEntry *q = findPath(recSplit.entries, recursiveQuotePath());
    QVERIFY2(q != nullptr, "n-tilde path lost after split");
    QCOMPARE(q->path, recursiveQuotePath());
    QVERIFY(q->path.contains(QChar(0x00F1)));
    QCOMPARE(q->size, qint64(2));
  }

  // T-LSJ-RECURSIVE
  void T_LSJ_RECURSIVE() {
    const Parsed p = parseAll(readBytes("rclone-1.75/lsjson_recursive.json"));
    QCOMPARE(p.entries.size(), qsizetype(6));
    QCOMPARE(p.skipped, 0);
    for (const ListEntry &e : p.entries) {
      QVERIFY(!e.isDir);
    }

    const ListEntry *q = findPath(p.entries, recursiveQuotePath());
    QVERIFY2(q != nullptr, "dir [x] (y)/n-tilde\"quote'.txt missing");
    QCOMPARE(q->name, enye() + QStringLiteral("\"quote'.txt"));
    QCOMPARE(q->size, qint64(2));

    const ListEntry *t = findPath(p.entries, QStringLiteral("12345 /trail.txt"));
    QVERIFY2(t != nullptr, "12345 /trail.txt missing");
    QCOMPARE(t->name, QStringLiteral("trail.txt"));
    QCOMPARE(t->size, qint64(1));
  }

  // T-LSJ-VARIANTS
  void T_LSJ_VARIANTS() {
    // Lines (a)..(g) in order; (g) is the empty line.
    const QList<QByteArray> lines = readLines("synthetic/lsjson_variants.jsonl");
    QCOMPARE(lines.size(), qsizetype(7));

    // (a) unknown size
    const std::optional<ListEntry> a = parseLsjsonLine(lines.at(0));
    QVERIFY(a.has_value());
    QCOMPARE(a->size, qint64(-1));
    QCOMPARE(a->name, QStringLiteral("Doc"));
    QVERIFY(a->modTime.isValid());

    // (b) no ModTime
    const std::optional<ListEntry> b = parseLsjsonLine(lines.at(1));
    QVERIFY(b.has_value());
    QVERIFY(!b->modTime.isValid());
    QCOMPARE(b->size, qint64(12));

    // (c) no Name
    QVERIFY(!parseLsjsonLine(lines.at(2)).has_value());
    // (d) "[", (e) "]"
    QVERIFY(!parseLsjsonLine(lines.at(3)).has_value());
    QVERIFY(!parseLsjsonLine(lines.at(4)).has_value());
    // (f) log text
    QVERIFY(!parseLsjsonLine(lines.at(5)).has_value());
    // (g) empty line
    QVERIFY(!parseLsjsonLine(lines.at(6)).has_value());

    // Whole file: 2 entries; only (f) is skipped.
    const Parsed p = parseAll(readBytes("synthetic/lsjson_variants.jsonl"));
    QCOMPARE(p.entries.size(), qsizetype(2));
    QCOMPARE(p.skipped, 1);
  }

  // T-LSJ-MODTEXT
  void T_LSJ_MODTEXT() {
    const QTimeZone utc(QByteArray("UTC"));
    QVERIFY(utc.isValid());
    const QDateTime t = parseRfc3339(QStringLiteral("2026-10-21T17:19:56.651+02:00"));
    QVERIFY(t.isValid());
    QCOMPARE(lsjsonModifiedText(t, utc), QStringLiteral("2026-10-21 15:19:56"));
    QCOMPARE(lsjsonModifiedText(QDateTime(), utc), QString());
  }

  // T-LSJ-MISSING
  void T_LSJ_MISSING() {
    const Parsed p = parseAll(readBytes("rclone-1.75/lsjson_missing.stdout"));
    QCOMPARE(p.entries.size(), qsizetype(0));
    QCOMPARE(p.skipped, 0);

    const QString msg =
        lastRcloneStderrMessage(readBytes("rclone-1.75/lsjson_missing.stderr"));
    QVERIFY(!msg.isEmpty());
    QVERIFY(!msg.at(0).isDigit());
    QVERIFY(msg.contains(QStringLiteral("directory not found")));
  }

  // T-LSJ-REMOTES
  void T_LSJ_REMOTES() {
    const std::optional<QVector<RemoteInfo>> remotes =
        parseListRemotesJson(readBytes("rclone-1.75/listremotes.json"));
    QVERIFY(remotes.has_value());
    QCOMPARE(remotes->size(), qsizetype(2));

    const RemoteInfo *loc = nullptr;
    const RemoteInfo *mem = nullptr;
    for (const RemoteInfo &r : *remotes) {
      if (r.name == QStringLiteral("loc")) {
        loc = &r;
      } else if (r.name == QStringLiteral("mem")) {
        mem = &r;
      }
    }
    QVERIFY(loc != nullptr);
    QVERIFY(mem != nullptr);
    QCOMPARE(loc->type, QStringLiteral("local"));
    QCOMPARE(loc->description, QStringLiteral("test local"));
    QCOMPARE(mem->type, QStringLiteral("memory"));

    QVERIFY(!parseListRemotesJson(QByteArray("{}")).has_value());
    const std::optional<QVector<RemoteInfo>> noType =
        parseListRemotesJson(QByteArray("[{\"name\":\"a\"}]"));
    QVERIFY(noType.has_value());
    QVERIFY(noType->isEmpty());
  }

  // T-FMT-NICESIZE
  void T_FMT_NICESIZE_data() {
    QTest::addColumn<qint64>("value");
    QTest::addColumn<QString>("expected");
    QTest::newRow("negative") << qint64(-1) << QString();
    QTest::newRow("0") << qint64(0) << QStringLiteral("0");
    QTest::newRow("5") << qint64(5) << QStringLiteral("5");
    QTest::newRow("10239") << qint64(10239) << QStringLiteral("10239");
    QTest::newRow("10240") << qint64(10240) << QStringLiteral("10 K");
    QTest::newRow("1GiB") << qint64(1073741824) << QStringLiteral("1024 M");
    QTest::newRow("10GiB") << qint64(10737418240) << QStringLiteral("10 G");
  }
  void T_FMT_NICESIZE() {
    QFETCH(qint64, value);
    QFETCH(QString, expected);
    QCOMPARE(formatNiceSize(value), expected);
  }

  void cleanupTestCase() {}
};

QTEST_GUILESS_MAIN(TestLsjsonParser)
#include "tst_lsjson_parser.moc"
