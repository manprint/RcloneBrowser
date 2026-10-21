#include <QtTest>
#include <QtWidgets>

#include <memory>

#include "job_widget.h"
#include "rb_test_support.h"
#include "rclone_process.h"

namespace {

// Environment of every case: the real one (with the temporary RCLONE_CONFIG of
// rbInitTestEnvironment(), I-9) polluted with logging variables, then run
// through the app's env factory.
QProcessEnvironment pollutedEnv() {
  QProcessEnvironment base = QProcessEnvironment::systemEnvironment();
  base.insert("RCLONE_VERBOSE", "1");
  base.insert("RCLONE_STATS", "5s");
  base.insert("RCLONE_STATS_ONE_LINE", "true");
  return RcloneEnvironment(RcloneEnvMode::Parsed, base);
}

bool writeFile(const QString &path, qsizetype size) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    return false;
  }
  const QByteArray data(size, 'x');
  return f.write(data) == size;
}

int countFiles(const QString &dir) {
  int n = 0;
  QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    ++n;
  }
  return n;
}

// Owns the widget of one transfer. The QProcess has no parent: JobWidget
// deletes it with deleteLater() when it finishes.
struct Job {
  QPointer<QProcess> proc;
  std::unique_ptr<JobWidget> w;
  TransferArgs ta;

  Job(const QStringList &userArgs, const QProcessEnvironment &env) {
    ta = PrepareTransferArgs(userArgs, env);
    auto *p = new QProcess();
    proc = p;
    p->setProcessChannelMode(QProcess::MergedChannels);
    p->setProcessEnvironment(env);
    w = std::make_unique<JobWidget>(
        p, QStringLiteral("info"), ta.args, QStringLiteral("src"),
        QStringLiteral("dst"), QStringLiteral("uid"), QStringLiteral("copy"),
        QStringLiteral("req"));
  }
  void start() { proc->start(rbRclone(), ta.args); }
  ~Job() {
    if (proc && proc->state() != QProcess::NotRunning) {
      proc->kill();
      proc->waitForFinished(5000);
    }
    w.reset();
  }

  QString text(const char *name) const {
    auto *e = w->findChild<QLineEdit *>(name);
    return e ? e->text() : QString();
  }
  QString tooltip(const char *name) const {
    auto *e = w->findChild<QLineEdit *>(name);
    return e ? e->toolTip() : QString();
  }
  int bars() const { return w->findChildren<QProgressBar *>().size(); }
  QString output() const {
    auto *o = w->findChild<QPlainTextEdit *>("output");
    return o ? o->toPlainText() : QString();
  }
};

// Waits for JobWidget::finished (ceiling 60 s).
bool waitFinished(QSignalSpy &spy) {
  QElapsedTimer t;
  t.start();
  while (spy.count() == 0 && t.elapsed() < 60000) {
    spy.wait(60000 - int(t.elapsed()));
  }
  return spy.count() > 0;
}

int countOccurrences(const QString &text, const QString &needle) {
  return int(text.count(needle));
}

} // namespace

class TestE2eJobWidget : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    QVERIFY(qEnvironmentVariableIsSet("RCLONE_CONFIG"));
  }

  // T-E2E-JOB-STATS
  void T_E2E_JOB_STATS() {
    QTemporaryDir src, dst;
    QVERIFY(src.isValid() && dst.isValid());
    const qsizetype sz = 1536 * 1024;
    const QStringList names = {
        QStringLiteral("a.bin"), QStringLiteral("dir [x] (y)/b.bin"),
        QStringLiteral("c:d.bin"), QStringLiteral("é \"q\".bin")};
    for (const QString &n : names) {
      QVERIFY(writeFile(QDir(src.path()).filePath(n), sz));
    }

    Job job({"copy", src.path(), dst.path(), "--bwlimit", "1M"},
            pollutedEnv());
    QSignalSpy spy(job.w.get(), &JobWidget::finished);
    bool seen = false;
    QString seenSize, seenBw;
    QTimer sampler;
    QObject::connect(&sampler, &QTimer::timeout, this, [&]() {
      if (job.text("size").contains(QStringLiteral("iB")) &&
          job.text("bandwidth").endsWith(QStringLiteral("/s")) &&
          job.bars() >= 1) {
        seen = true;
        seenSize = job.text("size");
        seenBw = job.text("bandwidth");
      }
    });
    sampler.start(200);
    job.start();
    QVERIFY(waitFinished(spy));
    sampler.stop();

    const QString out = job.output();
    QVERIFY2(seen, qPrintable(out));
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("finished"));
    QCOMPARE(job.w->getStatus(), QStringLiteral("1_transfer_finished"));
    QVERIFY2(countOccurrences(out, QStringLiteral("Copied (new)")) >= 4,
             qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral(" DEBUG : ")), qPrintable(out));
    QCOMPARE(countFiles(dst.path()), 4);
  }

  // T-E2E-JOB-ERRORS
  void T_E2E_JOB_ERRORS() {
    QTemporaryDir src, dst;
    QVERIFY(src.isValid() && dst.isValid());
    QVERIFY(writeFile(QDir(src.path()).filePath("a.bin"), 1024));
    QVERIFY(writeFile(QDir(src.path()).filePath("sub/b.bin"), 1024));
    const QString ro = QDir(dst.path()).filePath("ro");
    QVERIFY(QDir().mkpath(ro));
    const QFileDevice::Permissions rwx =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner |
        QFileDevice::ExeOwner | QFileDevice::ReadGroup |
        QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther;
    QVERIFY(QFile::setPermissions(
        ro, QFileDevice::ReadOwner | QFileDevice::ExeOwner |
                QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                QFileDevice::ReadOther | QFileDevice::ExeOther));
    // QTemporaryDir must be able to delete the destination.
    const auto restore = qScopeGuard([&]() {
      QFile::setPermissions(ro, rwx);
    });

    Job job({"copy", src.path(), ro}, pollutedEnv());
    QSignalSpy spy(job.w.get(), &JobWidget::finished);
    job.start();
    QVERIFY(waitFinished(spy));

    const QString out = job.output();
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("error"));
    QCOMPARE(job.w->getStatus(), QStringLiteral("2_transfer_error"));
    const QRegularExpression rx(QStringLiteral("^[1-9]\\d* ") +
                                QChar(0x2014) + QStringLiteral(" "));
    QVERIFY2(rx.match(job.text("errors")).hasMatch(),
             qPrintable(job.text("errors") + "\n" + out));
    QVERIFY2(job.tooltip("errors").contains(QStringLiteral("permission denied")),
             qPrintable(job.tooltip("errors") + "\n" + out));
  }

  // T-E2E-JOB-LOGFILE (#142)
  void T_E2E_JOB_LOGFILE() {
    QTemporaryDir src, dst, logDir;
    QVERIFY(src.isValid() && dst.isValid() && logDir.isValid());
    for (const char *n : {"a.bin", "b.bin", "c.bin"}) {
      QVERIFY(writeFile(QDir(src.path()).filePath(n), 1536 * 1024));
    }
    const QString logPath = QDir(logDir.path()).filePath("t.log");

    Job job({"copy", src.path(), dst.path(), "--bwlimit", "1M", "--log-file",
             logPath},
            pollutedEnv());
    QCOMPARE(job.ta.logFile, logPath);
    // What MainWindow::addTransfer does before start().
    job.w->noteIgnoredArgs(job.ta.ignored);
    if (!job.ta.logFile.isEmpty()) {
      job.w->setLogFile(job.ta.logFile);
    }

    QSignalSpy spy(job.w.get(), &JobWidget::finished);
    bool sizeSeen = false;
    QTimer sampler;
    QObject::connect(&sampler, &QTimer::timeout, this, [&]() {
      if (!job.text("size").isEmpty()) {
        sizeSeen = true;
      }
    });
    sampler.start(200);
    job.start();
    QVERIFY(waitFinished(spy));
    sampler.stop();

    const QString out = job.output();
    QVERIFY2(sizeSeen, qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("ignored options: --log-file")),
             qPrintable(out));

    QFile log(logPath);
    QVERIFY(log.open(QIODevice::ReadOnly));
    const QString logText = QString::fromUtf8(log.readAll());
    QVERIFY2(logText.contains(QStringLiteral("Copied (new)")),
             qPrintable(logText));
    QVERIFY2(logText.contains(QStringLiteral(" NOTICE: ")), qPrintable(logText));
    QVERIFY2(logText.contains(QStringLiteral("# Rclone Browser transfer log")),
             qPrintable(logText));
  }

  // T-E2E-JOB-CANCEL (#117)
  void T_E2E_JOB_CANCEL() {
    QTemporaryDir src, dst;
    QVERIFY(src.isValid() && dst.isValid());
    for (int i = 0; i < 10; ++i) {
      QVERIFY(writeFile(QDir(src.path()).filePath(QString("f%1.bin").arg(i)),
                        1536 * 1024));
    }

    Job job({"copy", src.path(), dst.path(), "--bwlimit", "500K"},
            pollutedEnv());
    QPointer<QProcess> pp = job.proc;
    QSignalSpy spy(job.w.get(), &JobWidget::finished);
    job.start();
    QTRY_VERIFY_WITH_TIMEOUT(!job.text("size").isEmpty(), 30000);

    const qint64 pid = pp ? pp->processId() : 0;
    QVERIFY(pid > 0);
    job.w->cancel();
    QTRY_VERIFY_WITH_TIMEOUT(pp.isNull() || pp->state() == QProcess::NotRunning,
                             5000);
#ifdef Q_OS_LINUX
    QVERIFY(!QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid)));
#endif
    const int before = countFiles(dst.path());
    QTest::qWait(3000);
    QCOMPARE(countFiles(dst.path()), before);
    QCOMPARE(job.w->getStatus(), QStringLiteral("2_transfer_stopped"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("stopped"));
  }
};

QTEST_MAIN(TestE2eJobWidget)
#include "tst_e2e_jobwidget.moc"
