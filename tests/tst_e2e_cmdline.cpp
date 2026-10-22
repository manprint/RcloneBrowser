#include <QtTest>
#include <QtWidgets>

#include "job_widget.h"
#include "rb_test_support.h"
#include "rclone_process.h"
#include "utils.h"

namespace {

// The inverse of quoteArgPosix, for the only forms it emits: bare words and
// '...' words with the '\'' escape for a quote (same as in tst_copy_sites).
QStringList parsePosix(const QString &s) {
  QStringList out;
  QString cur;
  bool inWord = false;
  bool inQuote = false;
  for (qsizetype i = 0; i < s.size(); ++i) {
    const QChar c = s.at(i);
    if (inQuote) {
      if (c == QLatin1Char('\'')) {
        inQuote = false;
      } else {
        cur += c;
      }
    } else if (c == QLatin1Char('\'')) {
      inQuote = true;
      inWord = true;
    } else if (c == QLatin1Char('\\') && i + 1 < s.size() &&
               s.at(i + 1) == QLatin1Char('\'')) {
      cur += QLatin1Char('\'');
      inWord = true;
      ++i;
    } else if (c == QLatin1Char(' ')) {
      if (inWord) {
        out << cur;
        cur.clear();
        inWord = false;
      }
    } else {
      cur += c;
      inWord = true;
    }
  }
  if (inWord) {
    out << cur;
  }
  return out;
}

} // namespace

class TestE2eCmdline : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    SetRclone(rbRclone());
  }

  void cleanupTestCase() { SetRcloneConf(QString()); }

  // T-E2E-CMD-ROUNDTRIP (#226): the text of the copy button of a job widget,
  // run through sh, does what the job does. The directory names and the config
  // path hold a space and an apostrophe, so a plain join(" ") cannot pass.
  void T_E2E_CMD_ROUNDTRIP() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString srcDir = tmp.filePath(QStringLiteral("src dir's"));
    const QString dstDir = tmp.filePath(QStringLiteral("dst dir"));
    const QString confDir = tmp.filePath(QStringLiteral("conf dir"));
    QVERIFY(QDir().mkpath(srcDir));
    QVERIFY(QDir().mkpath(confDir));

    const auto write = [](const QString &path, const QByteArray &data) {
      QFile f(path);
      return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
    };
    QVERIFY(write(srcDir + QStringLiteral("/a b.txt"), "content of a b"));
    QVERIFY(write(srcDir + QStringLiteral("/it's.txt"), "content of it's"));
    // I-9: an empty temporary config, never the host one
    const QString confPath = confDir + QStringLiteral("/rclone.conf");
    QVERIFY(write(confPath, QByteArray()));
    SetRcloneConf(confPath);

    const TransferArgs ta =
        PrepareTransferArgs({"copy", srcDir, dstDir},
                            RcloneEnvironment(RcloneEnvMode::Parsed));
    QObject holder;
    auto *proc = new QProcess(&holder); // never started
    JobWidget w(proc, QStringLiteral("info"), ta.args, srcDir, dstDir,
                QStringLiteral("uid"), QStringLiteral("copy"),
                QStringLiteral("req"));
    auto *copy = w.findChild<QToolButton *>(QStringLiteral("copy"));
    QVERIFY(copy);
    QGuiApplication::clipboard()->clear();
    copy->click();
    const QString text = QGuiApplication::clipboard()->text();
    QVERIFY(!text.isEmpty());
    QVERIFY2(text.contains(QStringLiteral("'\\''")), qPrintable(text));
    // the exact argv addTransfer starts: program, the transfer args, then
    // --config last
    const QStringList expectedArgv =
        QStringList{QDir::toNativeSeparators(GetRclone())} + ta.args +
        QStringList{QStringLiteral("--config"), confPath};
    QCOMPARE(parsePosix(text), expectedArgv);

    QProcess sh;
    sh.setProcessChannelMode(QProcess::MergedChannels);
    // I-9: whatever the outer environment holds, rclone reads the temp config
    QProcessEnvironment env = RcloneEnvironment(RcloneEnvMode::Parsed);
    env.insert(QStringLiteral("RCLONE_CONFIG"), confPath);
    sh.setProcessEnvironment(env);
    sh.start(QStringLiteral("sh"), {QStringLiteral("-c"), text});
    QVERIFY(sh.waitForFinished(60000));
    const QString output = QString::fromUtf8(sh.readAll());
    QCOMPARE(sh.exitStatus(), QProcess::NormalExit);
    QVERIFY2(sh.exitCode() == 0, qPrintable(text + QLatin1Char('\n') + output));

    const auto read = [](const QString &path) {
      QFile f(path);
      return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<missing>");
    };
    QCOMPARE(read(dstDir + QStringLiteral("/a b.txt")),
             QByteArray("content of a b"));
    QCOMPARE(read(dstDir + QStringLiteral("/it's.txt")),
             QByteArray("content of it's"));
  }
};

QTEST_MAIN(TestE2eCmdline)
#include "tst_e2e_cmdline.moc"
