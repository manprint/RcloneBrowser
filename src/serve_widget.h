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

private:
  QString type() const;
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
};
