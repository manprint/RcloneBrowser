#pragma once

#include "pch.h"

#include "serve_manager.h"

// "Serve...": asks how to serve `fs` (rclone serve). Built in code. The fields a
// serve type does not take are disabled and cleared (see serveTypeRules), so
// params() never carries a key rclone would reject; OK needs an address, a
// port for ftp, a user name and password for sftp, and a user name with any
// password.
class ServeDialog : public QDialog {
  Q_OBJECT

public:
  // `types` is what serve/types returned; empty gives the built-in list.
  explicit ServeDialog(const QString &fs, const QStringList &types,
                       QWidget *parent = nullptr);

  // type, addr and user are trimmed, pass is kept as typed, fs as given.
  ServeParams params() const;
  // Replaces the type list and keeps the selection when it is still there.
  void setTypes(const QStringList &types);

public slots:
  // Whether the rclone behind the rc has the webdav_webui option block; it
  // gates OK while webdav-webui is selected.
  void setWebuiSupport(ServeManager::WebuiSupport support);
  // The text of a failed capability check (ServeManager::webuiCheckError); it
  // is shown while the support is Unknown. Empty clears it.
  void setWebuiCheckFailed(const QString &text);

public:
  // Saves the webdav-webui options (never the secret) when that type is
  // selected, then closes with Accepted.
  void accept() override;

private:
  QString type() const;
  void buildWebuiGroup();
  void loadWebuiSettings();
  void saveWebuiSettings() const;
  WebuiOptions webuiOptions() const;
  void applyRules();
  void updateWarning();
  void updateOk();

  QString mFs;
  QString mLastDefaultAddr;
  QComboBox *mType;
  QLineEdit *mAddr;
  QLineEdit *mUser;
  QLineEdit *mPass;
  QCheckBox *mReadOnly;
  QLabel *mHint;
  QLabel *mWarning;
  QLabel *mProblem;
  QDialogButtonBox *mButtons;

  ServeManager::WebuiSupport mWebuiSupport = ServeManager::WebuiSupport::Unknown;
  QString mWebuiCheckFailed;
  QGroupBox *mWebuiGroup = nullptr;
  QToolButton *mWebuiAdvancedToggle = nullptr;
  QWidget *mWebuiAdvanced = nullptr;
  QLabel *mWebuiUnsupported = nullptr;
  QComboBox *mVfsCacheMode = nullptr;
  QCheckBox *mAllowDelete = nullptr;
  QCheckBox *mSearch = nullptr;
  QSpinBox *mSearchMaxDepth = nullptr;
  QSpinBox *mSearchMaxResults = nullptr;
  QLineEdit *mSearchTimeout = nullptr;
  QLineEdit *mPublinkPaths = nullptr;
  QLineEdit *mSecret = nullptr;
  QLineEdit *mTokenExpiry = nullptr;
  QCheckBox *mResume = nullptr;
  QLineEdit *mResumeDir = nullptr;
  QLineEdit *mResumeRetention = nullptr;
  QSpinBox *mPlayerMaxRetry = nullptr;
  QSpinBox *mPlayerReconnectInterval = nullptr;
  QCheckBox *mDiskStats = nullptr;
  QCheckBox *mUploadSettings = nullptr;
  QLineEdit *mEditMaxSize = nullptr;
  QCheckBox *mDirCount = nullptr;
  QSpinBox *mMultiZipMax = nullptr;
};
