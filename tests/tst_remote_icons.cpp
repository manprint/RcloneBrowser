#include <QtTest>

#include "remote_icons.h"

namespace {

QString iconPath(const QString &base, const QString &suffix = QString()) {
  return ":media/images/remotes_icons/" + base + suffix + ".png";
}

struct TypeBase {
  QString type, base;
};

// Hard-coded oracles: never read from remoteIconAliases().
const QList<TypeBase> &identityTable() {
  static const QList<TypeBase> table{
      {"alias", "alias"}, {"azureblob", "azureblob"}, {"b2", "b2"},
      {"box", "box"}, {"cache", "cache"}, {"chunker", "chunker"},
      {"crypt", "crypt"}, {"drive", "drive"}, {"dropbox", "dropbox"},
      {"fichier", "fichier"}, {"ftp", "ftp"}, {"http", "http"},
      {"jottacloud", "jottacloud"}, {"koofr", "koofr"}, {"local", "local"},
      {"mailru", "mailru"}, {"mega", "mega"}, {"memory", "memory"},
      {"onedrive", "onedrive"}, {"opendrive", "opendrive"},
      {"pcloud", "pcloud"}, {"premiumizeme", "premiumizeme"},
      {"putio", "putio"}, {"qingstor", "qingstor"}, {"s3", "s3"},
      {"sftp", "sftp"}, {"sharefile", "sharefile"}, {"sugarsync", "sugarsync"},
      {"swift", "swift"}, {"union", "union"}, {"webdav", "webdav"},
      {"yandex", "yandex"}, {"hubic", "hubic"}, {"teldrive", "teldrive"},
      {"unknown", "unknown"}, {"google cloud storage", "google_cloud_storage"},
      {"google photos", "google_photos"},
      {"amazon cloud drive", "amazon_cloud_drive"},
  };
  return table;
}

} // namespace

class TestRemoteIcons : public QObject {
  Q_OBJECT

private slots:
  // Without the qrc linked every lookup would be "unknown" and T_ICON_EXISTS
  // would pass vacuously: fail the whole binary instead.
  void initTestCase() {
    QVERIFY(QFile::exists(iconPath("unknown")));
    QVERIFY(QFile::exists(iconPath("unknown", "_inv")));
    QVERIFY(QFile::exists(iconPath("teldrive", "_inv")));
  }

  // T-ICON-ALIAS: type -> icon base (identity, aliases, normalization, unknown).
  void T_ICON_ALIAS() {
    for (const auto &row : identityTable()) {
      QVERIFY2(remoteIconBaseName(row.type) == row.base,
               qPrintable(row.type + " -> " + remoteIconBaseName(row.type)));
    }

    const QList<TypeBase> aliases{
        {"gcs", "google_cloud_storage"},
        {"googlecloudstorage", "google_cloud_storage"},
        {"gphotos", "google_photos"},
        {"googlephotos", "google_photos"},
    };
    for (const auto &row : aliases) {
      QVERIFY2(remoteIconBaseName(row.type) == row.base,
               qPrintable(row.type + " -> " + remoteIconBaseName(row.type)));
    }

    QCOMPARE(remoteIconBaseName("  Google Cloud Storage  "),
             QString("google_cloud_storage"));
    QCOMPARE(remoteIconBaseName("DRIVE"), QString("drive"));
    QCOMPARE(remoteIconBaseName("GCS"), QString("google_cloud_storage"));

    const QStringList none{"nosuch", "",        "   ",       "oos",
                           "oracleobjectstorage", "storj",   "tardigrade",
                           "smb",    "azurefiles", "../drive", "a/b",
                           "drive.png", ":drive",
                           // Resolve to an existing icon if the guard in
                           // remoteIconBaseName were missing (QResource cleans
                           // the path): they pin the ^[a-z0-9_]+$ guard.
                           "./drive", "x/../drive", "/drive",
                           "../remotes_icons/drive",
                           "../qbutton_icons/refresh"};
    for (const QString &t : none) {
      QVERIFY2(remoteIconBaseName(t) == QString("unknown"),
               qPrintable("'" + t + "' -> " + remoteIconBaseName(t)));
    }
  }

  // T-ICON-EXISTS: every resolved base has both icon files in the qrc.
  void T_ICON_EXISTS() {
    // exactly the 4 spellings of revision 31, each one reachable
    QCOMPARE(QStringList(remoteIconAliases().keys()),
             (QStringList{"gcs", "googlecloudstorage", "googlephotos",
                          "gphotos"}));
    for (auto it = remoteIconAliases().cbegin();
         it != remoteIconAliases().cend(); ++it) {
      const QString &key = it.key();
      QVERIFY2(key == key.toLower() && !key.contains(' '), qPrintable(key));
      QCOMPARE(remoteIconBaseName(key), it.value());
      QVERIFY2(QFile::exists(iconPath(it.value())), qPrintable(it.value()));
      QVERIFY2(QFile::exists(iconPath(it.value(), "_inv")),
               qPrintable(it.value() + "_inv"));
    }

    for (const auto &row : identityTable()) {
      const QString base = remoteIconBaseName(row.type);
      if (row.type != QString("unknown")) {
        QVERIFY2(base != QString("unknown"), qPrintable(row.type));
      }
      QVERIFY2(QFile::exists(iconPath(base)), qPrintable(base));
      QVERIFY2(QFile::exists(iconPath(base, "_inv")),
               qPrintable(base + "_inv"));
    }
  }
};

QTEST_GUILESS_MAIN(TestRemoteIcons)
#include "tst_remote_icons.moc"
