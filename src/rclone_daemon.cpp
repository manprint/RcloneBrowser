#include "rclone_daemon.h"

#include "rclone_log.h"
#include "rclone_process.h"
#include "utils.h"

#ifdef Q_OS_LINUX
#include <signal.h>
#include <sys/prctl.h>
#endif

namespace {

const int kStartTimeoutMs = 10000;
const int kQuitTimeoutMs = 2000;
const int kKillDelayMs = 3000;

} // namespace

RcloneDaemon::RcloneDaemon(QObject *parent)
    : QObject(parent), mClient(new RcClient(this)) {
  mStartTimer.setSingleShot(true);
  mStartTimer.setInterval(kStartTimeoutMs);
  connect(&mStartTimer, &QTimer::timeout, this,
          &RcloneDaemon::handleStartTimeout);
  mKillTimer.setSingleShot(true);
  mKillTimer.setInterval(kKillDelayMs);
  connect(&mKillTimer, &QTimer::timeout, this,
          &RcloneDaemon::handleKillTimeout);
}

RcloneDaemon::~RcloneDaemon() {
  mStartTimer.stop();
  mKillTimer.stop();
  if (!mProcess) {
    return;
  }
  mProcess->disconnect(this);
  if (mProcess->state() != QProcess::NotRunning) {
    mProcess->terminate();
    if (!mProcess->waitForFinished(3000)) {
      mProcess->kill();
      mProcess->waitForFinished(1000);
    }
  }
}

RcloneDaemon::State RcloneDaemon::state() const { return mState; }

int RcloneDaemon::port() const {
  return mState == State::Running ? mPort : 0;
}

QString RcloneDaemon::lastError() const { return mLastError; }

RcClient *RcloneDaemon::client() { return mClient; }

qint64 RcloneDaemon::processId() const {
  return mProcess ? mProcess->processId() : 0;
}

bool RcloneDaemon::lastStopWasKill() const { return mLastStopWasKill; }

RcCredentials RcloneDaemon::credentials() const { return mCredentials; }

void RcloneDaemon::start() {
  if (mState != State::Stopped && mState != State::Failed) {
    return;
  }
  discardProcess();
  mPort = 0;
  mLastError.clear();
  mLastLogError.clear();
  mStderrBuffer.clear();
  mStopping = false;
  mLastStopWasKill = false;
  mCredentials = generateRcCredentials();

  QProcessEnvironment env = RcloneEnvironment(RcloneEnvMode::Parsed);
  const QStringList names = env.keys();
  for (const QString &name : names) {
    // no RCLONE_RC_NO_AUTH, _HTPASSWD, _WEB_GUI ... from the user's environment
    if (name.startsWith(QLatin1String("RCLONE_RC_"), Qt::CaseInsensitive)) {
      env.remove(name);
    }
  }
  applyRcCredentials(env, mCredentials);

  QStringList args = {QStringLiteral("rcd"), QStringLiteral("--rc-addr"),
                      QStringLiteral("127.0.0.1:0"),
                      QStringLiteral("--use-json-log"),
                      QStringLiteral("--ask-password=false")};
  args << GetRcloneConf();

  QProcess *process = new QProcess(this);
  mProcess = process;
  process->setProcessChannelMode(QProcess::SeparateChannels);
  process->setProcessEnvironment(env);
  process->setProgram(GetRclone());
  process->setArguments(args);
#ifdef Q_OS_LINUX
  // rcd must not outlive an app that dies without running destructors.
  process->setChildProcessModifier([]() { ::prctl(PR_SET_PDEATHSIG, SIGTERM); });
#endif

  connect(process, &QProcess::readyReadStandardError, this,
          [this, process]() { handleStderr(process); });
  connect(process, &QProcess::readyReadStandardOutput, this,
          [process]() { process->readAllStandardOutput(); });
  connect(process, &QProcess::errorOccurred, this,
          [this, process](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart || process != mProcess ||
                (mState != State::Starting && mState != State::Running)) {
              return;
            }
            mStartTimer.stop();
            mKillTimer.stop();
            fail(QStringLiteral("rcd failed to start: ") +
                 process->errorString());
          });
  connect(process, &QProcess::finished, this,
          [this, process](int exitCode, QProcess::ExitStatus) {
            handleFinished(process, exitCode);
          });

  setState(State::Starting);
  mStartTimer.start();
  process->start();
}

void RcloneDaemon::stop() {
  if (mState != State::Starting && mState != State::Running) {
    return;
  }
  if (mStopping) {
    return;
  }
  mStopping = true;
  if (mState == State::Starting) {
    mLastStopWasKill = true;
    mStartTimer.stop();
    if (mProcess) {
      mProcess->kill();
    }
    return;
  }
  mLastStopWasKill = false;
  mClient->call(QStringLiteral("core/quit"), QJsonObject(), this,
                [](const RcReply &) {}, kQuitTimeoutMs);
  mKillTimer.start();
}

void RcloneDaemon::setState(State state) {
  if (mState == state) {
    return;
  }
  mState = state;
  if (state != State::Running) {
    mPort = 0;
    mClient->clearEndpoint();
  }
  emit stateChanged(state);
}

void RcloneDaemon::fail(const QString &message) {
  mLastError = message;
  setState(State::Failed);
  emit failed(message);
}

// Releases the process of an earlier start; it is not running any more.
void RcloneDaemon::discardProcess() {
  if (!mProcess) {
    return;
  }
  mProcess->disconnect(this);
  if (mProcess->state() != QProcess::NotRunning) {
    mProcess->kill();
    mProcess->waitForFinished(1000);
  }
  mProcess->deleteLater();
  mProcess = nullptr;
}

void RcloneDaemon::handleStderr(QProcess *process) {
  if (process != mProcess) {
    return;
  }
  mStderrBuffer.append(process->readAllStandardError());
  const bool detectPort = mState == State::Starting;
  qsizetype end;
  while ((end = mStderrBuffer.indexOf('\n')) >= 0) {
    const QByteArray line = mStderrBuffer.left(end);
    mStderrBuffer.remove(0, end + 1);
    handleLine(line, detectPort && mState == State::Starting);
  }
}

void RcloneDaemon::handleLine(const QByteArray &line, bool detectPort) {
  const RcloneLogEvent event = parseRcloneLogLine(line);
  if (event.kind == RcloneLogEvent::Kind::Log &&
      (event.level == RcloneLogLevel::Error ||
       event.level == RcloneLogLevel::Critical)) {
    mLastLogError = event.msg;
  }
  if (!detectPort) {
    return;
  }
  const std::optional<int> port = parseRcServingPort(event.msg);
  if (!port) {
    return;
  }
  mPort = *port;
  mStartTimer.stop();
  mClient->setEndpoint(
      QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(mPort)), mCredentials);
  setState(State::Running);
}

void RcloneDaemon::handleFinished(QProcess *process, int exitCode) {
  if (process != mProcess ||
      (mState != State::Starting && mState != State::Running)) {
    return;
  }
  mStartTimer.stop();
  mKillTimer.stop();

  // lines that arrived with the exit: only used for lastError
  mStderrBuffer.append(process->readAllStandardError());
  qsizetype end;
  while ((end = mStderrBuffer.indexOf('\n')) >= 0) {
    const QByteArray line = mStderrBuffer.left(end);
    mStderrBuffer.remove(0, end + 1);
    handleLine(line, false);
  }
  if (!mStderrBuffer.isEmpty()) {
    handleLine(mStderrBuffer, false);
    mStderrBuffer.clear();
  }

  if (mStopping) {
    setState(State::Stopped);
    return;
  }
  fail(mLastLogError.isEmpty()
           ? QStringLiteral("rcd exited (code %1)").arg(exitCode)
           : mLastLogError);
}

void RcloneDaemon::handleStartTimeout() {
  if (mState != State::Starting || !mProcess) {
    return;
  }
  // The exit of this process is expected: do not let handleFinished see it.
  mProcess->disconnect(this);
  mProcess->kill();
  mProcess->waitForFinished(2000);
  fail(QStringLiteral("rcd did not report its port"));
}

void RcloneDaemon::handleKillTimeout() {
  if (mProcess && mProcess->state() != QProcess::NotRunning) {
    mLastStopWasKill = true;
    mProcess->kill();
  }
}
