#include "rclone_process.h"

#include "utils.h"

const QStringList &RcloneLoggingEnvVars() {
  static const QStringList vars = {
      "RCLONE_VERBOSE",
      "RCLONE_LOG_LEVEL",
      "RCLONE_LOG_FILE",
      "RCLONE_LOG_FILE_COMPRESS",
      "RCLONE_LOG_FILE_MAX_AGE",
      "RCLONE_LOG_FILE_MAX_BACKUPS",
      "RCLONE_LOG_FILE_MAX_SIZE",
      "RCLONE_LOG_FORMAT",
      "RCLONE_LOG_SYSTEMD",
      "RCLONE_USE_JSON_LOG",
      "RCLONE_STATS",
      "RCLONE_STATS_LOG_LEVEL",
      "RCLONE_STATS_ONE_LINE",
      "RCLONE_STATS_ONE_LINE_DATE",
      "RCLONE_STATS_ONE_LINE_DATE_FORMAT",
      "RCLONE_STATS_FILE_NAME_LENGTH",
      "RCLONE_PROGRESS",
      "RCLONE_PROGRESS_TERMINAL_TITLE",
      "RCLONE_QUIET",
      "RCLONE_SYSLOG",
      "RCLONE_SYSLOG_FACILITY",
      "RCLONE_COLOR",
  };
  return vars;
}

QProcessEnvironment RcloneEnvironment(RcloneEnvMode mode,
                                      const QProcessEnvironment &base) {
  QProcessEnvironment env = base;
  if (mode == RcloneEnvMode::Parsed) {
    const QStringList &vars = RcloneLoggingEnvVars();
    for (const QString &name : vars) {
      env.remove(name);
    }
  }
  const QString password = GetRclonePassword();
  if (!password.isEmpty()) {
    env.insert("RCLONE_CONFIG_PASS", password);
  }
  return env;
}

void ApplyRcloneEnvironment(QProcess *p, RcloneEnvMode mode) {
  p->setProcessEnvironment(RcloneEnvironment(mode));
}

namespace {

const QSet<QString> &boolFlags() {
  static const QSet<QString> flags = {
      "-q",
      "--quiet",
      "-P",
      "--progress",
      "--progress-terminal-title",
      "--use-json-log",
      "--stats-one-line",
      "--stats-one-line-date",
      "--log-systemd",
      "--syslog",
      "--log-file-compress",
  };
  return flags;
}

const QSet<QString> &valueFlags() {
  static const QSet<QString> flags = {
      "--log-level",
      "--log-file",
      "--log-format",
      "--stats",
      "--stats-log-level",
      "--stats-file-name-length",
      "--stats-one-line-date-format",
      "--syslog-facility",
      "--log-file-max-age",
      "--log-file-max-backups",
      "--log-file-max-size",
      "--color",
  };
  return flags;
}

// -v, -vv, -vvv: returns the number of v, 0 when `token` is not one of them.
int shortVerbosity(const QString &token) {
  if (token == "-v") {
    return 1;
  }
  if (token == "-vv") {
    return 2;
  }
  if (token == "-vvv") {
    return 3;
  }
  return 0;
}

} // namespace

TransferArgs PrepareTransferArgs(const QStringList &args,
                                 const QProcessEnvironment &env) {
  TransferArgs ta;
  QStringList kept;
  QStringList tail; // "--" and everything after it, verbatim
  int verbosity = 0;
  QString level;
  bool levelGiven = false;
  bool logFileGiven = false;

  for (int i = 0; i < args.size(); ++i) {
    const QString &tok = args.at(i);
    if (tok == "--") {
      tail = args.mid(i);
      break;
    }

    const int eq = tok.startsWith("--") ? tok.indexOf('=') : -1;
    const QString name = eq > 0 ? tok.left(eq) : tok;
    const QString inlineValue = eq > 0 ? tok.mid(eq + 1) : QString();

    if (boolFlags().contains(name)) {
      ta.ignored << tok;
      continue;
    }

    if (const int n = shortVerbosity(tok)) {
      verbosity += n;
      ta.ignored << tok;
      continue;
    }
    if (tok == "--verbose") {
      verbosity += 1;
      ta.ignored << tok;
      continue;
    }
    if (name == "--verbose" && eq > 0) {
      verbosity = qMax(verbosity, inlineValue.toInt());
      ta.ignored << tok;
      continue;
    }

    if (valueFlags().contains(name)) {
      QString value;
      bool haveValue = false;
      if (eq > 0) {
        value = inlineValue;
        haveValue = true;
        ta.ignored << tok;
      } else if (i + 1 < args.size()) {
        value = args.at(i + 1);
        haveValue = true;
        ta.ignored << (tok + " " + value);
        ++i;
      } else {
        ta.ignored << tok;
      }
      if (haveValue) {
        if (name == "--log-level") {
          level = value.toUpper();
          levelGiven = true;
        } else if (name == "--log-file") {
          ta.logFile = value;
          logFileGiven = true;
        }
      }
      continue;
    }

    kept << tok;
  }

  if (!logFileGiven) {
    ta.logFile = env.value("RCLONE_LOG_FILE");
  }
  if (!levelGiven) {
    level = env.value("RCLONE_LOG_LEVEL").toUpper();
  }
  ta.debug = verbosity >= 2 || level == "DEBUG";

  ta.args = kept;
  ta.args << "--use-json-log" << (ta.debug ? "-vv" : "-v") << "--stats"
          << "1s"
          << "--stats-log-level"
          << "NOTICE"
          << "--stats-file-name-length"
          << "0"
          << "--color"
          << "NEVER";
  ta.args << tail;
  return ta;
}

ConfigLoadProblem classifyConfigLoadError(int exitCode,
                                          const QByteArray &stderrBytes) {
  if (exitCode == 0) {
    return ConfigLoadProblem::None;
  }
  if (stderrBytes.contains("password command failed")) {
    return ConfigLoadProblem::PasswordCommandFailed;
  }
  if (stderrBytes.contains("most likely wrong password") ||
      stderrBytes.contains("using RCLONE_CONFIG_PASS env password")) {
    return ConfigLoadProblem::WrongPassword;
  }
  if (stderrBytes.contains("not allowed to ask for password")) {
    return ConfigLoadProblem::NeedsPassword;
  }
  return ConfigLoadProblem::Other;
}
