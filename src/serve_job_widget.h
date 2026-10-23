#pragma once

#include "pch.h"
#include "serve_manager.h"

class QLabel;
class QLineEdit;
class QToolButton;

// One `rclone serve` in the Jobs list (like MountWidget for mounts). The daemon
// is the source of truth: the owner (MainWindow) drives the state with the
// mark*() methods. The widget holds ServeParams, password included, only in
// memory (I-2) and never logs them.
class ServeJobWidget : public QWidget {
  Q_OBJECT

public:
  enum class State { Starting, Serving, Finished, Error };

  // displayType: "webdav-webui" for that flavor, else entry.type. Empty
  // params.type = adopted serve (no Restart).
  ServeJobWidget(const ServeEntry &entry, const QString &displayType,
                 const ServeParams &params, QWidget *parent = nullptr);

  bool isRunning = true; // State Starting or Serving (MountWidget convention)
  State state() const;
  QDateTime getStartDateTime();
  // "0_zserve_starting" | "0_zserve_serving" | "1_zserve_finished" |
  // "1_zserve_error"
  QString getStatus();
  // The texts the Jobs filter searches, read live.
  QStringList filterFields() const; // {displayType, entry.fs, serveUrl(entry)}
  QString serveId() const;          // "" while Starting after a restart
  ServeParams params() const;
  bool canRestart() const; // !params.type.isEmpty()
  void adoptParams(const ServeParams &p, const QString &displayType);
  // Finished/Error -> Starting, only when canRestart(); the id is cleared and
  // isRunning is true. Any other state: no-op.
  void markStarting();
  // Starting/Serving -> Serving with e (new id/addr, start time now). Serving
  // with the same id only updates the address, without a signal. Finished and
  // Error: no-op.
  void markRunning(const ServeEntry &e);
  // Starting/Serving -> Finished, emits finished() once; else no-op.
  void markFinished(const QString &reason);
  // Starting -> Error, emits finished(); else no-op.
  void markStartFailed(const QString &message);

public slots:
  // Serving: emit stopRequested(id), disable, text "  Stopping".
  // Finished/Error: emit closed(). Starting (or already stopping): no-op.
  void cancel();

signals:
  void stopRequested(const QString &id);
  void restartRequested();
  void openUrlRequested(const QUrl &url);
  void finished();
  void closed();

private:
  void applyState();
  void applyEntry();
  void applyDisplayType();
  void updateStartFinishInfo();
  void showMessage(const QString &text);
  QString iconPath(const QString &name) const;

  ServeEntry mEntry;
  QString mDisplayType;
  ServeParams mParams;
  State mState = State::Serving;
  bool mStopping = false;
  bool mWhiteIcons = false;
  QDateTime mStartDateTime = QDateTime::currentDateTime();
  QDateTime mFinishDateTime;

  QToolButton *mShowDetails;
  QLineEdit *mInfo;
  QToolButton *mCopyUrl;
  QToolButton *mOpenBrowser;
  QToolButton *mRestart;
  QToolButton *mCancel;
  QWidget *mDetails;
  QLineEdit *mType;
  QLineEdit *mSource;
  QLineEdit *mUrl;
  QLineEdit *mAddress;
  QLineEdit *mStartFinishInfo;
  QLabel *mMessage;
};
