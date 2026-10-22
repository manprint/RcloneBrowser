#include <QtTest>

#include "remote_actions.h"

class TestRemoteActions : public QObject {
  Q_OBJECT

private slots:
  // T-ACT-COPYTO (#199): the verb, the source and the destination, as typed
  // (spaces stay inside one argument).
  void T_ACT_COPYTO() {
    QCOMPARE(buildCopytoArgs("r:a b.txt", "/x/y z.txt"),
             (QStringList{"copyto", "r:a b.txt", "/x/y z.txt"}));
  }

  // T-ACT-URL (#199): only http and https URLs with a host.
  void T_ACT_URL() {
    const QStringList valid = {
        "https://a.b/c",
        "http://127.0.0.1:8/x",
        "HTTP://A.B/c",
        // the URL of the report: literal brackets in the path (QUrl accepts
        // them in its default TolerantMode)
        "https://gdriveindex.mrdeveloper.workers.dev/1:/"
        "Looop.Lapeta.2022.1080p.NF.10bit.DDP.5.1.x265.%5BHashMiner%5D/"
        "Looop.Lapeta.2022.1080p.NF.10bit.DDP.5.1.x265.[HashMiner].mkv"};
    for (const QString &s : valid) {
      QVERIFY2(isHttpUrl(s), qPrintable(s));
    }
    const QStringList invalid = {"ftp://a",   "a.b/c",    "",
                                 "http://",   "http:///x", "javascript:alert(1)"};
    for (const QString &s : invalid) {
      QVERIFY2(!isHttpUrl(s), qPrintable(s));
    }
  }

  // T-ACT-COPYURL (#199): the auto form, the named form and --no-clobber.
  void T_ACT_COPYURL() {
    const QString url = "http://h/f.txt";
    // the auto form: the destination is a directory
    QCOMPARE(buildCopyurlArgs(url, "r:d", "", false),
             (QStringList{"copyurl", url, "r:d/", "--auto-filename",
                          "--print-filename"}));
    // the base of a remote stays "r:" ("r:/" is the root of sftp and local)
    QCOMPARE(buildCopyurlArgs(url, "r:", "", false),
             (QStringList{"copyurl", url, "r:", "--auto-filename",
                          "--print-filename"}));
    QCOMPARE(buildCopyurlArgs(url, "r:d/", "", false),
             (QStringList{"copyurl", url, "r:d/", "--auto-filename",
                          "--print-filename"}));
    QCOMPARE(buildCopyurlArgs(url, "/tmp/dl", "", false),
             (QStringList{"copyurl", url, "/tmp/dl/", "--auto-filename",
                          "--print-filename"}));
    // the named form: the destination is the file
    QCOMPARE(buildCopyurlArgs(url, "r:d", "n.txt", false),
             (QStringList{"copyurl", url, "r:d/n.txt"}));
    QCOMPARE(buildCopyurlArgs(url, "r:", "n.txt", false),
             (QStringList{"copyurl", url, "r:n.txt"}));
    // --no-clobber is the last item of both forms
    QCOMPARE(buildCopyurlArgs(url, "r:d", "", true),
             (QStringList{"copyurl", url, "r:d/", "--auto-filename",
                          "--print-filename", "--no-clobber"}));
    QCOMPARE(buildCopyurlArgs(url, "r:d", "n.txt", true),
             (QStringList{"copyurl", url, "r:d/n.txt", "--no-clobber"}));
  }

  // T-ACT-JOIN (#199): one separator between the directory and the name.
  void T_ACT_JOIN() {
    QCOMPARE(joinRclonePath("r:", "a"), QString("r:a"));
    QCOMPARE(joinRclonePath("r:a", "b"), QString("r:a/b"));
    QCOMPARE(joinRclonePath("r:a/", "b"), QString("r:a/b"));
    QCOMPARE(joinRclonePath("/x", "y"), QString("/x/y"));
    QCOMPARE(joinRclonePath("loc:/", "b.txt"), QString("loc:/b.txt"));
  }
};

QTEST_GUILESS_MAIN(TestRemoteActions)
#include "tst_remote_actions.moc"
