#pragma once
#include <QtCore>
#include <functional>
#include <optional>

#include "rc_client.h"

// One question of rclone's config state machine (the `Option` of a step).
struct ConfigOption {
  QString name, help, type, defaultStr;
  bool required = false, isPassword = false, exclusive = false,
       advanced = false;
  QVector<QPair<QString, QString>> examples; // (Value, Help)
};
Q_DECLARE_METATYPE(ConfigOption)

// Missing keys give defaults; Examples null or missing gives no examples.
ConfigOption configOptionFromJson(const QJsonObject &o);

struct ConfigStep {
  QString state;
  std::optional<ConfigOption> option;
  QString error;
  QString result;
  bool done = false;
};
// done = (State == "").
ConfigStep parseConfigStep(const QJsonObject &reply);

struct ProviderInfo {
  QString name, description, prefix;
  bool hide = false;
};
// Sorted by description, case-insensitive; Hide entries are kept with
// hide = true. Hide is a bool or an int (non-zero = hidden).
QVector<ProviderInfo> parseProviders(const QJsonObject &reply);

// "bool" options give "true"/"false", everything else v.toString().
QString configAnswerText(const ConfigOption &o, const QVariant &v);

// rclone's remote name rule: no leading '-' or space, no trailing space, no
// ':', '/' or '\\'.
bool isValidRemoteName(const QString &name);

using RcCallFn =
    std::function<void(const QString &path, const QJsonObject &params,
                       std::function<void(const RcReply &)> done,
                       int timeoutMs)>;

// Drives `config/create` or `config/update` (nonInteractive) one question at a
// time. Every call goes through the injected RcCallFn, which must reply
// asynchronously. finished() is emitted exactly once; a Create that fails or is
// cancelled deletes the remote it may have left behind.
class ConfigFlow : public QObject {
  Q_OBJECT

public:
  enum class Mode { Create, Update };

  explicit ConfigFlow(RcCallFn call, QObject *parent = nullptr);

  void startCreate(const QString &name, const QString &type);
  void startUpdate(const QString &name);
  // Only valid while a question is outstanding.
  void answer(const QVariant &value);
  void cancel();
  bool isOAuthRunning() const;

signals:
  void question(const ConfigOption &option, const QString &error);
  void oauthUrl(const QUrl &url);
  void finished(bool ok, const QString &message);

private:
  void start(Mode mode, const QString &name, const QString &type);
  void handleListRemotes(const RcReply &reply);
  void sendAnswer(const ConfigOption &option, const QString &state,
                  const QString &text);
  void handleStepReply(const RcReply &reply, bool oauth);
  void handleStep(const ConfigStep &step);
  void stopLoop(const QString &state);
  void pollOAuth();
  void cleanup(const QString &message);
  void finish(bool ok, const QString &message);

  RcCallFn mCall;
  Mode mMode = Mode::Create;
  QString mName, mType;
  QJsonObject mParams;
  QTimer *mPollTimer;

  bool mStarted = false;
  bool mFinished = false;
  bool mCancelRequested = false;
  bool mCleaning = false;
  bool mListing = false;        // the start config/listremotes is in flight
  bool mStepInFlight = false;   // a config/create or config/update call
  bool mAbsentAtStart = false;
  bool mOAuthRunning = false;
  bool mPollInFlight = false;
  bool mOAuthUrlEmitted = false;
  bool mQuestionOutstanding = false;
  ConfigOption mOption; // the outstanding question
  QString mState;       // its State
  int mSteps = 0;
  QSet<QString> mAutoStates;
};
