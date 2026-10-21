#pragma once

#include "pch.h"
#include "rc_client.h"
#include "ui_mount_widget.h"

class MountWidget : public QWidget {
  Q_OBJECT

public:
  MountWidget(QProcess *process, const QString &remote, const QString &folder,
              const QStringList &args, const QString &script,
              const QString &uniqueID, const QString &info,
              QWidget *parent = nullptr);
  ~MountWidget();
  bool isRunning = true;
  QDateTime getStartDateTime();
  QString getStatus();
  // The credentials the mount process got through its environment (I-5) and
  // whether it runs with --rc. Call before the mount prints its rc endpoint.
  void setRcCredentials(const RcCredentials &creds, bool rcEnabled);

public slots:
  void cancel();
  QString getUniqueID();
  QString getUnmountingError();

signals:
  void finished();
  void closed();

private:
  Ui::MountWidget ui;

  bool mScriptRunning = false;
  bool mScriptStarted = false;

  QProcess *mProcess;
  QProcess mScriptProcess;

  QString mUnmountingError = "0";
  QString mRcPort = "0";
  RcCredentials mRcCredentials;
  bool mRcEnabled = false;
  RcClient *mRc;
  QUrl mRcUrl;
  QStringList mArgs;
  QString mUniqueID = "";

  // 0 - running, 1 - finished, 2 - error
  // we add "z" to make mounts listed after transfers
  QString mStatus = "0_zmount_mounted";

  QDateTime mStartDateTime = QDateTime::currentDateTime();
  QDateTime mFinishDateTime;
  void updateStartFinishInfo();
  // the unmount failed with `code` (exit status of umount/fusermount, or an
  // rc error on Windows)
  void showUnmountFailed(const QString &code);
};
