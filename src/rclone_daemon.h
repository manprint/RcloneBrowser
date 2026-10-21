#pragma once
#include <QtCore>

#include "rc_client.h"

// One private `rclone rcd` on 127.0.0.1 with environment-only credentials.
// Stopped -> Starting -> Running; Failed when rcd cannot start, never reports
// its port or exits by itself. start() leaves Failed/Stopped again.
class RcloneDaemon : public QObject {
  Q_OBJECT

public:
  enum class State { Stopped, Starting, Running, Failed };
  Q_ENUM(State)

  explicit RcloneDaemon(QObject *parent = nullptr);
  // Emits nothing. terminate(), waits up to 3 s, then kill().
  ~RcloneDaemon() override;

  // No-op unless Stopped or Failed.
  void start();
  // Starting: kill. Running: core/quit (2 s timeout), kill after 3 s. The
  // state stays Running until the process has exited. No-op otherwise.
  void stop();

  State state() const;
  int port() const; // 0 unless Running
  QString lastError() const;
  RcClient *client(); // endpoint set while Running
  qint64 processId() const; // 0 without a running process

  // Test hooks.
  bool lastStopWasKill() const;
  RcCredentials credentials() const; // of the current or last start

signals:
  void stateChanged(RcloneDaemon::State state);
  void failed(const QString &message);

private:
  void setState(State state);
  void fail(const QString &message);
  void discardProcess();
  void handleStderr(QProcess *process);
  void handleLine(const QByteArray &line, bool detectPort);
  void handleFinished(QProcess *process, int exitCode);
  void handleStartTimeout();
  void handleKillTimeout();

  RcClient *mClient;
  QProcess *mProcess = nullptr;
  State mState = State::Stopped;
  int mPort = 0;
  QString mLastError;
  QString mLastLogError; // msg of the last Error/Critical log event
  QByteArray mStderrBuffer;
  RcCredentials mCredentials;
  bool mStopping = false;
  bool mLastStopWasKill = false;
  QTimer mStartTimer;
  QTimer mKillTimer;
};
