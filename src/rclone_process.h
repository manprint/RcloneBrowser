#pragma once
#include <QtCore>

// How a rclone child process is going to be used.
//  Parsed:      the app parses its --use-json-log output; the user's logging
//               and progress environment variables are removed (I-2).
//  Interactive: the process runs in a terminal; the environment is kept as is.
enum class RcloneEnvMode { Parsed, Interactive };

// Exactly the I-2 variables removed in Parsed mode.
const QStringList &RcloneLoggingEnvVars();

// Copies `base`; Parsed mode removes RcloneLoggingEnvVars(). In both modes a
// non-empty GetRclonePassword() is inserted as RCLONE_CONFIG_PASS.
QProcessEnvironment RcloneEnvironment(
    RcloneEnvMode mode = RcloneEnvMode::Parsed,
    const QProcessEnvironment &base = QProcessEnvironment::systemEnvironment());

// p->setProcessEnvironment(RcloneEnvironment(mode)).
void ApplyRcloneEnvironment(QProcess *p,
                            RcloneEnvMode mode = RcloneEnvMode::Parsed);

struct TransferArgs {
  QStringList args;    // user args without logging flags + the app-owned flags
  QString logFile;     // --log-file value, or RCLONE_LOG_FILE, or empty
  bool debug = false;  // -vv requested (verbosity >= 2 or log level DEBUG)
  QStringList ignored; // dropped tokens, as typed ("--x value" for pairs)
};

// Strips every logging/progress flag from `args` and appends the flags the app
// owns: --use-json-log, -v/-vv, --stats 1s, --stats-log-level NOTICE,
// --stats-file-name-length 0, --color NEVER. Tokens after "--" are untouched.
TransferArgs PrepareTransferArgs(
    const QStringList &args,
    const QProcessEnvironment &env = QProcessEnvironment::systemEnvironment());

// Why rclone could not load its configuration file (R8, rclone 1.75).
enum class ConfigLoadProblem {
  None,
  NeedsPassword,
  WrongPassword,
  PasswordCommandFailed,
  Other
};

// Classifies the stderr of a failed rclone run. Case-sensitive substring
// match, so JSON-log and text stderr give the same result. Rules in order:
// exit 0 -> None; "password command failed" -> PasswordCommandFailed;
// "most likely wrong password" or "using RCLONE_CONFIG_PASS env password" ->
// WrongPassword; "not allowed to ask for password" -> NeedsPassword;
// anything else -> Other.
ConfigLoadProblem classifyConfigLoadError(int exitCode,
                                          const QByteArray &stderrBytes);
