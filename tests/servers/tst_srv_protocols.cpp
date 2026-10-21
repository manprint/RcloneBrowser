#include <QtNetwork>
#include <QtTest>
#include <QtWidgets>

#include <memory>
#include <vector>

#include "config_flow.h"
#include "global.h"
#include "icon_cache.h"
#include "item_model.h"
#include "job_widget.h"
#include "rb_test_support.h"
#include "rc_client.h"
#include "rclone_daemon.h"
#include "rclone_process.h"
#include "transfer_monitor.h"
#include "utils.h"

// Real protocol servers (sftp, ftp, webdav, S3) started by `scripts/dev.sh
// servers` on a per-run --internal Docker network (I-9: no published ports,
// no real account). Outside the harness every function skips. One empty
// temporary config holds every remote the tests create.

namespace {

// One backend: remote type, answers by option name, writable prefix.
struct Row {
  QString row, type;
  QVariantMap answers;
  QString prefix;
};

QVector<Row> rows() {
  return {
      {"sftp",
       "sftp",
       {{"host", "sftp"}, {"user", "rb"}, {"pass", "rbpass"}},
       "upload/"},
      {"ftp",
       "ftp",
       {{"host", "ftp"}, {"user", "rb"}, {"pass", "rbpass"}},
       ""},
      {"webdav",
       "webdav",
       {{"url", "http://webdav/"},
        {"vendor", "other"},
        {"user", "rb"},
        {"pass", "rbpass"}},
       ""},
      {"s3",
       "s3",
       {{"provider", "Minio"},
        {"env_auth", "false"},
        {"access_key_id", "rbadmin"},
        {"secret_access_key", "rbpass1234"},
        {"endpoint", "http://s3:9000"}},
       "rbt/"},
  };
}

void addRows() {
  QTest::addColumn<QString>("row");
  QTest::addColumn<QString>("type");
  QTest::addColumn<QVariantMap>("answers");
  QTest::addColumn<QString>("prefix");
  for (const Row &r : rows()) {
    QTest::newRow(qPrintable(r.row)) << r.row << r.type << r.answers
                                     << r.prefix;
  }
}

QMap<QString, QString> toAnswers(const QVariantMap &m) {
  QMap<QString, QString> out;
  for (auto it = m.cbegin(); it != m.cend(); ++it) {
    out.insert(it.key(), it.value().toString());
  }
  return out;
}

// Non-ASCII name as UTF-8 escapes (the source encoding must not matter).
QString nameUmlaut() { return QString::fromUtf8("\xC3\xBC" "mlaut.txt"); }

bool writeFile(const QString &path, const QByteArray &data) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    return false;
  }
  return f.write(data) == data.size();
}

Item *itemOf(const QModelIndex &i) {
  return static_cast<Item *>(i.internalPointer());
}

// The folder of `root` finished loading (Loading2 -> Ready).
bool isReady(const QModelIndex &root) {
  return itemOf(root)->state == Item::Ready;
}

// IconCache works on Item pointers from its own thread: no child may still
// wait for its icon before the tree is changed or the model is destroyed.
bool iconsSettled(const QModelIndex &root) {
  for (const Item *c : itemOf(root)->childs) {
    if (c->state == Item::LoadingIcon) {
      return false;
    }
  }
  return true;
}

// "name|folder|size" of every child, sorted. Folder sizes are not compared.
QStringList childRecords(const QModelIndex &root) {
  QStringList records;
  for (const Item *c : itemOf(root)->childs) {
    records << (c->isFolder ? QString("%1|1|-").arg(c->name)
                            : QString("%1|0|%2").arg(c->name).arg(c->size));
  }
  records.sort();
  return records;
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

  QString output() const {
    auto *o = w->findChild<QPlainTextEdit *>("output");
    return o ? o->toPlainText() : QString();
  }
};

// Waits until `spy` has a signal (ceiling `timeoutMs`).
bool waitSignal(QSignalSpy &spy, int timeoutMs) {
  QElapsedTimer t;
  t.start();
  while (spy.count() == 0 && t.elapsed() < timeoutMs) {
    spy.wait(int(qMax<qint64>(1, timeoutMs - t.elapsed())));
  }
  return spy.count() > 0;
}

} // namespace

class TestSrvProtocols : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QString mConf;
  QString mObscured; // `rclone obscure rbpass`
  RcloneDaemon *mDaemon = nullptr;

  // Only `scripts/dev.sh servers` sets RB_SRV=1 inside the --internal network.
  static bool inHarness() {
    return qEnvironmentVariable("RB_SRV") == QLatin1String("1");
  }

  // One CLI rclone call on the temporary config (I-9), Parsed environment.
  // `--config` goes first, so `args` may end with `--` and positional values.
  int runRclone(const QStringList &args, QByteArray *out = nullptr,
                QByteArray *err = nullptr, int timeoutMs = 90000) const {
    QProcess p;
    p.setProcessEnvironment(
        RcloneEnvironment(RcloneEnvMode::Parsed,
                          QProcessEnvironment::systemEnvironment()));
    p.start(rbRclone(), QStringList{"--config", mConf} + args);
    p.closeWriteChannel();
    if (!p.waitForFinished(timeoutMs)) {
      p.kill();
      p.waitForFinished(5000);
      return -1;
    }
    if (out) {
      *out = p.readAllStandardOutput();
    }
    if (err) {
      *err = p.readAllStandardError();
    }
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -2;
  }

  // Connection string that reaches the server without a configured remote.
  QString probeFor(const QString &row) const {
    if (row == "sftp") {
      return ":sftp,host=sftp,user=rb,pass=" + mObscured + ":upload";
    }
    if (row == "ftp") {
      return ":ftp,host=ftp,user=rb,pass=" + mObscured + ":";
    }
    if (row == "webdav") {
      return ":webdav,url='http://webdav/',vendor=other,user=rb,pass=" +
             mObscured + ":";
    }
    return ":s3,provider=Minio,access_key_id=rbadmin,"
           "secret_access_key=rbpass1234,endpoint='http://s3:9000':";
  }

  // Retries `rclone lsd` until the server answers; no try starts after 30 s.
  bool waitServer(const QString &row) {
    QElapsedTimer t;
    t.start();
    while (true) {
      const int rc = runRclone({"lsd", probeFor(row), "--contimeout", "2s",
                                "--low-level-retries", "1", "--retries", "1"},
                               nullptr, nullptr, 15000);
      if (rc == 0) {
        return true;
      }
      if (t.elapsed() >= 30000) {
        return false;
      }
      QTest::qWait(1000);
    }
  }

  // Creates mDaemon (published as global.daemon) unless one is running.
  void ensureDaemon() {
    if (mDaemon && mDaemon->state() == RcloneDaemon::State::Running) {
      return;
    }
    global.daemon = nullptr;
    delete mDaemon;
    mDaemon = new RcloneDaemon;
    global.daemon = mDaemon;
    mDaemon->start();
    QTRY_VERIFY2_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Running,
                              qPrintable(mDaemon->lastError()), 10000);
  }

  void stopDaemon() {
    mDaemon->stop();
    QTRY_VERIFY_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Stopped,
                             10000);
  }

  RcCallFn callFn() {
    RcClient *client = mDaemon->client();
    return [this, client](const QString &path, const QJsonObject &params,
                          std::function<void(const RcReply &)> done,
                          int timeoutMs) {
      client->call(path, params, this, std::move(done), timeoutMs);
    };
  }

  // Answers every question from `answers` (by option name), else with the
  // default; a password that is not in the map gets "". Answers are queued.
  static void drive(ConfigFlow *flow, const QMap<QString, QString> &answers) {
    QObject::connect(
        flow, &ConfigFlow::question, flow,
        [flow, answers](const ConfigOption &o, const QString &) {
          QString text;
          if (answers.contains(o.name)) {
            text = answers.value(o.name);
          } else if (!o.isPassword) {
            text = o.defaultStr;
          }
          QMetaObject::invokeMethod(
              flow, [flow, text]() { flow->answer(text); },
              Qt::QueuedConnection);
        });
  }

  // Creates the remote through ConfigFlow; empty string on success, else the
  // reason.
  QString createRemote(const QString &name, const QString &type,
                       const QMap<QString, QString> &answers) {
    ConfigFlow flow(callFn());
    QSignalSpy finished(&flow, &ConfigFlow::finished);
    drive(&flow, answers);
    flow.startCreate(name, type);
    if (!waitSignal(finished, 30000)) {
      return QString("no finished() for %1").arg(name);
    }
    if (!finished.at(0).at(0).toBool()) {
      return finished.at(0).at(1).toString();
    }
    return QString();
  }

  // One rc call through the daemon, waited for.
  void rcCall(const QString &path, const QJsonObject &params,
              RcReply &reply) {
    auto done = std::make_shared<bool>(false);
    auto result = std::make_shared<RcReply>();
    mDaemon->client()->call(path, params, this,
                            [done, result](const RcReply &r) {
                              *result = r;
                              *done = true;
                            },
                            20000);
    QTRY_VERIFY_WITH_TIMEOUT(*done, 25000);
    reply = *result;
  }

  // The calls RemoteWidget makes for a non-Windows remote. rowCount() on a
  // folder in state Unknown is the public call that starts ItemModel::load().
  // The root index is returned by value: QTRY_* may return from this void
  // helper.
  void loadModel(ItemModel &model, const QString &dir, QModelIndex &root) {
    root = model.addRoot("/", dir);
    model.rowCount(root);
    QTRY_VERIFY_WITH_TIMEOUT(isReady(root), 30000);
    QTRY_VERIFY_WITH_TIMEOUT(iconsSettled(root), 20000);
  }

  // `key` of [section] in the temporary config, empty when absent.
  QString confValue(const QString &section, const QString &key) const {
    QFile f(mConf);
    if (!f.open(QIODevice::ReadOnly)) {
      return QString();
    }
    bool in = false;
    const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
    for (const QString &line : lines) {
      if (line.startsWith('[')) {
        in = line.trimmed() == "[" + section + "]";
        continue;
      }
      if (in && line.section(" = ", 0, 0) == key) {
        return line.section(" = ", 1);
      }
    }
    return QString();
  }

  // `rclone reveal` on the temporary config (I-9). An obscured value is
  // base64url and may start with '-': it goes after `--`, never as a flag.
  QString reveal(const QString &obscured) const {
    QByteArray out;
    if (runRclone({"reveal", "--", obscured}, &out, nullptr, 20000) != 0) {
      return QString();
    }
    return QString::fromUtf8(out).trimmed();
  }

  // `lsjson` of `fs` as sorted "Path" entries, folders with a trailing "/".
  // Exit 3 (directory not found) gives an empty list when `missingOk`.
  bool listNames(const QString &fs, bool recursive, QStringList &names,
                 bool missingOk = false) const {
    names.clear();
    QStringList args = {"lsjson", fs, "--no-mimetype"};
    if (recursive) {
      args << "-R";
    }
    QByteArray out, err;
    const int rc = runRclone(args, &out, &err);
    if (rc == 3 && missingOk) {
      return true;
    }
    if (rc != 0) {
      qWarning("lsjson %s: exit %d: %s", qPrintable(fs), rc, err.constData());
      return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(out);
    if (!doc.isArray()) {
      return false;
    }
    for (const QJsonValue &v : doc.array()) {
      const QJsonObject o = v.toObject();
      names << o.value("Path").toString() +
                   (o.value("IsDir").toBool() ? "/" : "");
    }
    names.sort();
    return true;
  }

  // Failures are ignored: a rerun must start clean, not fail on a leftover.
  void purge(const QString &fs) const { runRclone({"purge", fs}); }

  static bool seed(const QDir &d) {
    return writeFile(d.filePath("a b.txt"), "hello\n") &&
           writeFile(d.filePath("[x] (y).txt"), "xy") &&
           writeFile(d.filePath(nameUmlaut()), "uml") &&
           writeFile(d.filePath("sub/c.txt"), "cc");
  }

  static QStringList seededRecords() {
    QStringList expected = {"a b.txt|0|6", "[x] (y).txt|0|2",
                            nameUmlaut() + "|0|3", "sub|1|-"};
    expected.sort();
    return expected;
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    qRegisterMetaType<ConfigOption>();
    qRegisterMetaType<RcloneDaemon::State>();
    if (!inHarness()) {
      return;
    }
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    QVERIFY(mDir.isValid());
    SetRclone(rbRclone());
    // I-9: rcd and every CLI call use this empty temporary config.
    mConf = mDir.filePath("rclone.conf");
    QVERIFY(writeFile(mConf, QByteArray()));
    SetRcloneConf(mConf);
    SetRclonePassword("");

    QByteArray out;
    QCOMPARE(runRclone({"obscure", "rbpass"}, &out, nullptr, 20000), 0);
    mObscured = QString::fromUtf8(out).trimmed();
    QVERIFY(!mObscured.isEmpty());

    for (const Row &r : rows()) {
      QVERIFY2(waitServer(r.row), qPrintable(r.row + " did not answer"));
    }

    ensureDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    for (const Row &r : rows()) {
      const QString err =
          createRemote("rb-" + r.row, r.type, toAnswers(r.answers));
      QVERIFY2(err.isEmpty(), qPrintable(r.row + ": " + err));
    }
    // S3 needs the bucket; the other prefixes exist already.
    QCOMPARE(runRclone({"mkdir", "rb-s3:rbt"}), 0);
  }

  void cleanupTestCase() {
    if (!inHarness()) {
      return;
    }
    global.daemon = nullptr;
    delete mDaemon;
    mDaemon = nullptr;
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
  }

  // T-PROTO-CONFIG: ConfigFlow creates a second set of remotes.
  void T_PROTO_CONFIG_data() { addRows(); }
  void T_PROTO_CONFIG() {
    if (!inHarness()) {
      QSKIP("run through scripts/dev.sh servers");
    }
    QFETCH(QString, row);
    QFETCH(QString, type);
    QFETCH(QVariantMap, answers);

    ensureDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    const QString name = "rbc-" + row;
    const QString err = createRemote(name, type, toAnswers(answers));
    QVERIFY2(err.isEmpty(), qPrintable(err));

    RcReply reply;
    rcCall("config/listremotes", QJsonObject(), reply);
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY2(reply.ok, qPrintable(reply.error));
    QVERIFY(reply.result.value("remotes").toArray().contains(QJsonValue(name)));

    QCOMPARE(confValue(name, "type"), type);
    if (row != "s3") {
      const QString pass = confValue(name, "pass");
      QVERIFY(!pass.isEmpty());
      QVERIFY(pass != "rbpass");
      QCOMPARE(reveal(pass), QString("rbpass"));
    }
  }

  // T-PROTO-LISTING: rc and lsjson listings equal the seeded set.
  void T_PROTO_LISTING_data() { addRows(); }
  void T_PROTO_LISTING() {
    if (!inHarness()) {
      QSKIP("run through scripts/dev.sh servers");
    }
    QFETCH(QString, row);
    QFETCH(QString, prefix);

    const QString remote = "rb-" + row;
    const QString dir = prefix + "rb-list";
    const QString fs = remote + ":" + dir;
    purge(fs);
    const auto cleanup = qScopeGuard([&]() { purge(fs); });

    QTemporaryDir src;
    QVERIFY(src.isValid());
    QVERIFY(seed(QDir(src.path())));
    QCOMPARE(runRclone({"copy", src.path(), fs}), 0);

    ensureDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    QStringList viaRc;
    {
      // rcd is running and the rc path never spawns rclone: any lsjson
      // fallback now fails to start and cannot reproduce the listing.
      SetRclone("/nonexistent/rclone");
      const auto restore = qScopeGuard([]() { SetRclone(rbRclone()); });
      IconCache icons;
      ItemModel model(&icons, remote, nullptr);
      QModelIndex root;
      loadModel(model, dir, root);
      if (QTest::currentTestFailed()) {
        return;
      }
      QVERIFY(model.lastLoadUsedRc());
      viaRc = childRecords(root);
    }
    QCOMPARE(viaRc, seededRecords());

    stopDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    QStringList viaProcess;
    {
      IconCache icons;
      ItemModel model(&icons, remote, nullptr);
      QModelIndex root;
      loadModel(model, dir, root);
      if (QTest::currentTestFailed()) {
        return;
      }
      QVERIFY(!model.lastLoadUsedRc());
      viaProcess = childRecords(root);
    }
    QCOMPARE(viaProcess, viaRc);
  }

  // T-PROTO-TRANSFER: a 3 MiB file up through the job pipeline, down again.
  void T_PROTO_TRANSFER_data() { addRows(); }
  void T_PROTO_TRANSFER() {
    if (!inHarness()) {
      QSKIP("run through scripts/dev.sh servers");
    }
    QFETCH(QString, row);
    QFETCH(QString, prefix);

    const QString fs = "rb-" + row + ":" + prefix + "rb-xfer";
    purge(fs);
    const auto cleanup = qScopeGuard([&]() { purge(fs); });

    constexpr qsizetype kSize = 3145728; // 3 MiB
    std::vector<quint32> words(kSize / qsizetype(sizeof(quint32)));
    QRandomGenerator::global()->fillRange(words.data(), qsizetype(words.size()));
    const QByteArray data(reinterpret_cast<const char *>(words.data()), kSize);
    QCOMPARE(data.size(), kSize);

    QTemporaryDir src, dst;
    QVERIFY(src.isValid() && dst.isValid());
    QVERIFY(writeFile(QDir(src.path()).filePath("big.bin"), data));

    {
      Job job({"copy", src.path(), fs, "--config", mConf},
              RcloneEnvironment(RcloneEnvMode::Parsed,
                                QProcessEnvironment::systemEnvironment()));
      QSignalSpy spy(job.w.get(), &JobWidget::finished);
      job.start();
      QVERIFY2(waitSignal(spy, 90000), qPrintable(job.output()));
      QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("finished"));
      QCOMPARE(job.w->getStatus(), QStringLiteral("1_transfer_finished"));
      QVERIFY(job.w->monitor()->result() == TransferMonitor::Result::Finished);
      QVERIFY2(job.w->monitor()->hasStats(), qPrintable(job.output()));
      QCOMPARE(job.w->monitor()->stats().bytes, qint64(kSize));
    }

    QCOMPARE(runRclone({"copy", fs, dst.path()}), 0);
    QFile f(QDir(dst.path()).filePath("big.bin"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray down = f.readAll();
    QCOMPARE(down.size(), kSize);
    QCOMPARE(QCryptographicHash::hash(down, QCryptographicHash::Sha256),
             QCryptographicHash::hash(data, QCryptographicHash::Sha256));
  }

  // T-PROTO-OPS: mkdir, copyto, moveto, deletefile and purge, each checked
  // with lsjson.
  void T_PROTO_OPS_data() { addRows(); }
  void T_PROTO_OPS() {
    if (!inHarness()) {
      QSKIP("run through scripts/dev.sh servers");
    }
    QFETCH(QString, row);
    QFETCH(QString, prefix);

    const bool s3 = row == "s3";
    const QString parent = "rb-" + row + ":" + prefix;
    const QString base = parent + "rb-ops";
    purge(base);
    const auto cleanup = qScopeGuard([&]() { purge(base); });

    QTemporaryDir src;
    QVERIFY(src.isValid());
    const QString local = QDir(src.path()).filePath("f.txt");
    QVERIFY(writeFile(local, "ops"));

    QStringList names;
    // S3 has no empty directories: mkdir is a no-op there.
    QCOMPARE(runRclone({"mkdir", base + "/d1"}), 0);
    if (!s3) {
      QVERIFY(listNames(base, true, names));
      QCOMPARE(names, QStringList({"d1/"}));
    }

    QCOMPARE(runRclone({"copyto", local, base + "/d1/f1.txt"}), 0);
    QVERIFY(listNames(base, true, names));
    QCOMPARE(names, QStringList({"d1/", "d1/f1.txt"}));

    QCOMPARE(runRclone({"moveto", base + "/d1/f1.txt", base + "/d1/f2.txt"}),
             0);
    QVERIFY(listNames(base, true, names));
    QCOMPARE(names, QStringList({"d1/", "d1/f2.txt"}));

    QCOMPARE(runRclone({"deletefile", base + "/d1/f2.txt"}), 0);
    QVERIFY(listNames(base, true, names, s3));
    QCOMPARE(names, s3 ? QStringList() : QStringList({"d1/"}));

    QCOMPARE(runRclone({"purge", base}), 0);
    // The parent must still exist: only rb-ops is gone.
    QVERIFY(listNames(parent, false, names));
    QVERIFY2(!names.contains("rb-ops/"), qPrintable(names.join(',')));
    if (s3) {
      QVERIFY(listNames("rb-s3:", false, names));
      QVERIFY2(names.contains("rbt/"), qPrintable(names.join(',')));
    }
  }
};

QTEST_MAIN(TestSrvProtocols)
#include "tst_srv_protocols.moc"
