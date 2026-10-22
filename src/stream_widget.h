#pragma once

#include "pch.h"
#include "ui_stream_widget.h"

class StreamWidget : public QWidget {
  Q_OBJECT

public:
  StreamWidget(QProcess *rclone, QProcess *player, const QString &remote,
               const QString &stream, const QStringList &args,
               QWidget *parent = nullptr);
  ~StreamWidget();
  bool isRunning = true;
  QDateTime getStartDateTime();
  QString getStatus();
  // The texts the Jobs filter searches (#21), read live.
  QStringList filterFields() const;

public slots:
  void cancel();

signals:
  void finished();
  void closed();

private:
  Ui::StreamWidget ui;

  QProcess *mRclone;
  QProcess *mPlayer;

  // the command as the user would type it in the native shell
  QString mCommand;

  // 0 - running, 1 - finished, 2 - error
  QString mStatus = "0_stream_streaming";

  QDateTime mStartDateTime = QDateTime::currentDateTime();
  QDateTime mFinishDateTime;
  void updateStartFinishInfo();
};
