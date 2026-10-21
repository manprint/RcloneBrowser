#pragma once

#include "pch.h"
#include "ui_progress_dialog.h"

class ProgressDialog : public QDialog {
  Q_OBJECT

public:
  ProgressDialog(const QString &title, const QString &operation,
                 const QString &message, QProcess *process,
                 QWidget *parent = nullptr, bool close = true,
                 bool trim = false, QString toolTip = "",
                 QProcess::ProcessChannelMode channelMode =
                     QProcess::MergedChannels);
  ~ProgressDialog();

  void expand();
  void allowToClose();

signals:
  void outputAvailable(const QString &output) const;
  // the bytes exactly as read from the process (no decoding or trimming)
  void rawOutputAvailable(const QByteArray &raw) const;

private:
  void closeEvent(QCloseEvent *ev) override;

  Ui::ProgressDialog ui;
  int mWidth;
  int mMinimumWidth;
  int mMinimumHeight;
  int mHeight;
  bool mIsRunning = true;
};
