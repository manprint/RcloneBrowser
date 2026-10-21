#pragma once

#include "pch.h"
#include "ui_job_widget.h"

class TransferMonitor;

class JobWidget : public QWidget {
  Q_OBJECT

public:
  JobWidget(QProcess *process, const QString &info, const QStringList &args,
            const QString &source, const QString &dest, const QString &uniqueID,
            const QString &transferMode, const QString &requestId,
            QWidget *parent = nullptr);
  ~JobWidget();

  void showDetails();
  bool isRunning = true;
  QDateTime getStartDateTime();
  QString getStatus();

  TransferMonitor *monitor() const { return mMonitor; }
  // Writes a rendered text log of the job (rclone --log-file). Call before the
  // process starts.
  void setLogFile(const QString &path);
  // Tells the user which logging options the app dropped from the command.
  void noteIgnoredArgs(const QStringList &ignored);

public slots:
  void cancel();
  QString getUniqueID();
  QString getRequestId();
  QString getTransferMode();

signals:
  void finished(const QString &info, const QString &jobFinalStatus);
  void closed();

private:
  Ui::JobWidget ui;

  QProcess *mProcess;
  TransferMonitor *mMonitor = nullptr;
  QFile *mLogFile = nullptr;

  QStringList mArgs;
  QHash<QString, QLabel *> mActive;

  QString mUniqueID = "";
  QString mTransferMode = "";
  QString mRequestId = "";
  QString mJobFinalStatus = "";

  // 0 - running, 1 - finished, 2 - error
  QString mStatus = "0_transfer_running";

  QDateTime mStartDateTime = QDateTime::currentDateTime();
  QDateTime mFinishDateTime;
  void updateProgress(const QString &name, int value, const QString &toolTip);
  void updateStartFinishInfo();
  void appendOutputLine(const QString &text);
  void consumeProcessOutput(bool final);
  void removeProgressRow(QLabel *label);
  void clearProgressRows();
};
