#pragma once
#include <QPointer>
#include <QtWidgets>
#include <functional>
#include <memory>

#include "config_flow.h"
#include "rclone_daemon.h"

// The RcCallFn of the dialog inside MainWindow: calls go to the daemon's
// RcClient with `*ctx` (the dialog, bound after construction) as the call
// context. While `*ctx` is unbound or gone nothing is called and `done` never
// runs; while the daemon is gone or not Running the call fails asynchronously
// (never from inside the call) with the error "rclone daemon is not running".
RcCallFn daemonRcCall(QPointer<RcloneDaemon> daemon,
                      std::shared_ptr<QPointer<QObject>> ctx);

// Native remote configuration: creates a remote (page 1 asks for a name and a
// provider) or edits one (starts at the first question) through ConfigFlow, one
// question per page. Every rc call goes through the injected RcCallFn, which
// must reply asynchronously. Cancel and close are idempotent: the first one
// cancels the flow (a Create leaves no partial remote) and the dialog closes
// once the flow reports `finished`, after 10 s at the latest.
class RemoteConfigDialog : public QDialog {
  Q_OBJECT

public:
  using Mode = ConfigFlow::Mode;

  // `existingName` is only used in Update mode.
  RemoteConfigDialog(RcCallFn call, Mode mode, const QString &existingName,
                     QWidget *parent = nullptr);

  // Test hooks. With showErrors false a failure is not shown in a
  // QMessageBox (which would block); the dialog rejects as usual. The opener
  // replaces QDesktopServices::openUrl for the OAuth link.
  void setShowErrors(bool show);
  void setUrlOpener(std::function<void(const QUrl &)> opener);

  void reject() override;

  // True once the flow was started (Update, or Create past page 1): rclone may
  // have written the remote even when the dialog was not accepted.
  bool flowStarted() const { return mFlowStarted; }

private:
  void startCreateFlow();
  void loadProviders();
  void loadRemoteNames();
  void fillProviders();
  void updateNameError();
  void showQuestion(const ConfigOption &option, const QString &error);
  void showOAuth(const QUrl &url);
  void onFinished(bool ok, const QString &message);
  void onNext();
  void requestCancel();
  void updateButtons();
  QVariant editorValue() const;
  bool editorEmpty() const;
  QString selectedProvider() const;
  void setBusy(bool busy);

  RcCallFn mCall;
  Mode mMode;
  QString mExistingName;
  ConfigFlow *mFlow;

  QStackedWidget *mStack;
  QWidget *mPage1 = nullptr;
  QWidget *mQuestionPage;
  QWidget *mOAuthPage;

  // page 1
  QLineEdit *mName = nullptr;
  QLineEdit *mFilter = nullptr;
  QListWidget *mProviders = nullptr;
  QLabel *mNameError = nullptr;
  QLabel *mProvidersError = nullptr;
  QVector<ProviderInfo> mProviderInfos;
  QSet<QString> mExistingNames;

  // question page
  QLabel *mTitle;
  QLabel *mHelp;
  QLabel *mError;
  QVBoxLayout *mEditorLayout;
  QWidget *mEditor = nullptr;
  ConfigOption mOption;
  bool mHasQuestion = false;

  // OAuth page
  QLabel *mOAuthLink;

  QPushButton *mNext;
  QPushButton *mCancel;
  QTimer *mCancelTimer;

  std::function<void(const QUrl &)> mOpenUrl;
  bool mShowErrors = true;
  bool mFlowStarted = false;
  bool mFinished = false;
  bool mCancelRequested = false;
  bool mBusy = false; // waiting for the next signal of the flow
};
