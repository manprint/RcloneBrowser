#include "config_flow.h"

#include <algorithm>

namespace {

const int kStepTimeoutMs = 30000;
const int kOAuthTimeoutMs = 900000;
const int kOAuthStatusTimeoutMs = 5000;
const int kOAuthPollMs = 500;
const int kMaxSteps = 500;

QString str(const QJsonObject &o, const char *key) {
  return o.value(QLatin1String(key)).toString();
}

bool flag(const QJsonObject &o, const char *key) {
  return o.value(QLatin1String(key)).toBool();
}

QStringList remoteNames(const RcReply &reply) {
  QStringList names;
  const QJsonArray remotes = reply.result.value(QStringLiteral("remotes")).toArray();
  for (const QJsonValue &v : remotes) {
    names << v.toString();
  }
  return names;
}

} // namespace

ConfigOption configOptionFromJson(const QJsonObject &o) {
  ConfigOption option;
  option.name = str(o, "Name");
  option.help = str(o, "Help");
  option.type = str(o, "Type");
  option.defaultStr = str(o, "DefaultStr");
  option.required = flag(o, "Required");
  option.isPassword = flag(o, "IsPassword");
  option.exclusive = flag(o, "Exclusive");
  option.advanced = flag(o, "Advanced");
  const QJsonArray examples = o.value(QStringLiteral("Examples")).toArray();
  for (const QJsonValue &v : examples) {
    const QJsonObject e = v.toObject();
    option.examples.append(
        qMakePair(e.value(QStringLiteral("Value")).toVariant().toString(),
                  str(e, "Help")));
  }
  return option;
}

ConfigStep parseConfigStep(const QJsonObject &reply) {
  ConfigStep step;
  step.state = str(reply, "State");
  step.error = str(reply, "Error");
  step.result = str(reply, "Result");
  step.done = step.state.isEmpty();
  const QJsonValue option = reply.value(QStringLiteral("Option"));
  if (option.isObject()) {
    step.option = configOptionFromJson(option.toObject());
  }
  return step;
}

QVector<ProviderInfo> parseProviders(const QJsonObject &reply) {
  QVector<ProviderInfo> providers;
  const QJsonArray array = reply.value(QStringLiteral("providers")).toArray();
  for (const QJsonValue &v : array) {
    const QJsonObject o = v.toObject();
    ProviderInfo p;
    p.name = str(o, "Name");
    p.description = str(o, "Description");
    p.prefix = str(o, "Prefix");
    const QJsonValue hide = o.value(QStringLiteral("Hide"));
    p.hide = hide.isBool() ? hide.toBool() : hide.toInt() != 0;
    providers.append(p);
  }
  std::stable_sort(providers.begin(), providers.end(),
                   [](const ProviderInfo &a, const ProviderInfo &b) {
                     return a.description.compare(b.description,
                                                  Qt::CaseInsensitive) < 0;
                   });
  return providers;
}

QString configAnswerText(const ConfigOption &o, const QVariant &v) {
  if (o.type == QLatin1String("bool")) {
    return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  }
  return v.toString();
}

bool isValidRemoteName(const QString &name) {
  static const QRegularExpression re(
      QRegularExpression::anchoredPattern(
          QStringLiteral("[\\w.+@](?:[\\w.+@ -]*[\\w.+@-])?")),
      QRegularExpression::UseUnicodePropertiesOption);
  return re.match(name).hasMatch();
}

ConfigFlow::ConfigFlow(RcCallFn call, QObject *parent)
    : QObject(parent), mCall(std::move(call)), mPollTimer(new QTimer(this)) {
  mPollTimer->setInterval(kOAuthPollMs);
  connect(mPollTimer, &QTimer::timeout, this, &ConfigFlow::pollOAuth);
}

bool ConfigFlow::isOAuthRunning() const { return mOAuthRunning; }

void ConfigFlow::startCreate(const QString &name, const QString &type) {
  start(Mode::Create, name, type);
}

void ConfigFlow::startUpdate(const QString &name) {
  start(Mode::Update, name, QString());
}

void ConfigFlow::start(Mode mode, const QString &name, const QString &type) {
  if (mStarted) {
    qWarning("ConfigFlow: already started");
    return;
  }
  mStarted = true;
  mMode = mode;
  mName = name;
  mType = type;
  // The listing records whether the remote is new (cleanup) and rejects a
  // Create over an existing remote.
  mListing = true;
  const QPointer<ConfigFlow> self(this);
  mCall(QStringLiteral("config/listremotes"), QJsonObject(),
        [self](const RcReply &reply) {
          if (!self || self->mFinished) {
            return;
          }
          self->mListing = false;
          self->handleListRemotes(reply);
        },
        kStepTimeoutMs);
}

void ConfigFlow::handleListRemotes(const RcReply &reply) {
  if (mCancelRequested) {
    finish(false, QStringLiteral("cancelled"));
    return;
  }
  if (!reply.ok) {
    finish(false, reply.error);
    return;
  }
  if (mMode == Mode::Create) {
    if (remoteNames(reply).contains(mName)) {
      finish(false, QStringLiteral("remote %1 already exists").arg(mName));
      return;
    }
    mAbsentAtStart = true;
  }

  QJsonObject opt;
  opt.insert(QStringLiteral("nonInteractive"), true);
  opt.insert(QStringLiteral("all"), true);
  QJsonObject params;
  params.insert(QStringLiteral("name"), mName);
  if (mMode == Mode::Create) {
    params.insert(QStringLiteral("type"), mType);
  }
  params.insert(QStringLiteral("parameters"), QJsonObject());
  params.insert(QStringLiteral("opt"), opt);
  mStepInFlight = true;
  const QPointer<ConfigFlow> self(this);
  mCall(mMode == Mode::Create ? QStringLiteral("config/create")
                              : QStringLiteral("config/update"),
        params,
        [self](const RcReply &r) {
          if (!self || self->mFinished) {
            return;
          }
          self->handleStepReply(r, false);
        },
        kStepTimeoutMs);
}

void ConfigFlow::answer(const QVariant &value) {
  if (mFinished || !mQuestionOutstanding) {
    qWarning("ConfigFlow::answer: no question is outstanding");
    return;
  }
  mQuestionOutstanding = false;
  const ConfigOption option = mOption;
  sendAnswer(option, mState, configAnswerText(option, value));
}

// R18: only answers that differ from the default are echoed in `parameters`;
// that keeps passwords obscured and the section free of default keys.
void ConfigFlow::sendAnswer(const ConfigOption &option, const QString &state,
                            const QString &text) {
  QString result = text;
  if (mMode == Mode::Update && option.isPassword && result.isEmpty()) {
    result = option.defaultStr;
  }
  if (!option.name.startsWith(QLatin1String("config_")) &&
      result != option.defaultStr &&
      (!option.isPassword || !result.isEmpty())) {
    mParams.insert(option.name, result);
  } else {
    mParams.remove(option.name);
  }

  QJsonObject opt;
  opt.insert(QStringLiteral("nonInteractive"), true);
  opt.insert(QStringLiteral("continue"), true);
  opt.insert(QStringLiteral("state"), state);
  opt.insert(QStringLiteral("result"), result);
  QJsonObject params;
  params.insert(QStringLiteral("name"), mName);
  if (mMode == Mode::Create) {
    params.insert(QStringLiteral("type"), mType);
  }
  params.insert(QStringLiteral("parameters"), mParams);
  params.insert(QStringLiteral("opt"), opt);

  const bool oauth =
      option.name == QLatin1String("config_is_local") && result == QLatin1String("true");
  mStepInFlight = true;
  if (oauth) {
    mOAuthRunning = true;
    mOAuthUrlEmitted = false;
    mPollTimer->start();
  }
  const QPointer<ConfigFlow> self(this);
  mCall(mMode == Mode::Create ? QStringLiteral("config/create")
                              : QStringLiteral("config/update"),
        params,
        [self, oauth](const RcReply &r) {
          if (!self || self->mFinished) {
            return;
          }
          self->handleStepReply(r, oauth);
        },
        oauth ? kOAuthTimeoutMs : kStepTimeoutMs);
}

void ConfigFlow::handleStepReply(const RcReply &reply, bool oauth) {
  mStepInFlight = false;
  if (oauth) {
    mOAuthRunning = false;
    mPollTimer->stop();
  }
  if (mCleaning) {
    return;
  }
  if (mCancelRequested) {
    cleanup(QStringLiteral("cancelled"));
    return;
  }
  if (!reply.ok) {
    cleanup(reply.error);
    return;
  }
  handleStep(parseConfigStep(reply.result));
}

void ConfigFlow::handleStep(const ConfigStep &step) {
  if (step.done) {
    finish(true, QString());
    return;
  }
  if (++mSteps > kMaxSteps || !step.option) {
    stopLoop(step.state);
    return;
  }
  const ConfigOption &option = *step.option;
  const bool autoAnswer =
      option.name == QLatin1String("config_fs_advanced") || option.advanced;
  if (autoAnswer) {
    if (mAutoStates.contains(step.state)) {
      stopLoop(step.state);
      return;
    }
    mAutoStates.insert(step.state);
    sendAnswer(option, step.state,
               option.name == QLatin1String("config_fs_advanced")
                   ? QStringLiteral("false")
                   : option.defaultStr);
    return;
  }
  mOption = option;
  mState = step.state;
  mQuestionOutstanding = true;
  emit question(option, step.error);
}

void ConfigFlow::stopLoop(const QString &state) {
  cleanup(QStringLiteral("rclone config did not finish (state %1)").arg(state));
}

void ConfigFlow::pollOAuth() {
  if (mFinished || !mOAuthRunning || mCancelRequested || mPollInFlight) {
    return;
  }
  mPollInFlight = true;
  const QPointer<ConfigFlow> self(this);
  mCall(QStringLiteral("config/oauthstatus"), QJsonObject(),
        [self](const RcReply &reply) {
          if (!self || self->mFinished) {
            return;
          }
          self->mPollInFlight = false;
          if (!self->mOAuthRunning || self->mCancelRequested ||
              self->mOAuthUrlEmitted || !reply.ok) {
            return;
          }
          const QString url =
              reply.result.value(QStringLiteral("authUrl")).toString();
          if (!url.isEmpty()) {
            self->mOAuthUrlEmitted = true;
            emit self->oauthUrl(QUrl(url));
          }
        },
        kOAuthStatusTimeoutMs);
}

void ConfigFlow::cancel() {
  if (mFinished || mCancelRequested || mCleaning) {
    return;
  }
  mCancelRequested = true;
  mPollTimer->stop();
  if (!mStarted) {
    finish(false, QStringLiteral("cancelled"));
    return;
  }
  if (mOAuthRunning) {
    // The blocked call fails, which leads to the cleanup.
    mCall(QStringLiteral("config/oauthstop"), QJsonObject(),
          [](const RcReply &) {}, kOAuthStatusTimeoutMs);
    return;
  }
  if (mListing || mStepInFlight) {
    return; // the reply leads to the cleanup
  }
  mQuestionOutstanding = false;
  cleanup(QStringLiteral("cancelled"));
}

// Create mode removes what a failed or cancelled flow left behind; errors of
// the cleanup calls are ignored.
void ConfigFlow::cleanup(const QString &message) {
  mCleaning = true;
  mQuestionOutstanding = false;
  mPollTimer->stop();
  if (mMode != Mode::Create || !mAbsentAtStart) {
    finish(false, message);
    return;
  }
  const QPointer<ConfigFlow> self(this);
  mCall(QStringLiteral("config/listremotes"), QJsonObject(),
        [self, message](const RcReply &listed) {
          if (!self || self->mFinished) {
            return;
          }
          if (!listed.ok || !remoteNames(listed).contains(self->mName)) {
            self->finish(false, message);
            return;
          }
          QJsonObject params;
          params.insert(QStringLiteral("name"), self->mName);
          self->mCall(QStringLiteral("config/delete"), params,
                      [self, message](const RcReply &) {
                        if (!self || self->mFinished) {
                          return;
                        }
                        self->finish(false, message);
                      },
                      kStepTimeoutMs);
        },
        kStepTimeoutMs);
}

void ConfigFlow::finish(bool ok, const QString &message) {
  if (mFinished) {
    return;
  }
  mFinished = true;
  mPollTimer->stop();
  mQuestionOutstanding = false;
  mOAuthRunning = false;
  emit finished(ok, message);
}
