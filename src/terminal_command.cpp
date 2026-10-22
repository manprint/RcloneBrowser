#include "terminal_command.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>

QString shellQuote(const QString &s) {
  QString out = s;
  // close the quote, emit an escaped quote, reopen the quote
  out.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
  return QStringLiteral("'") + out + QStringLiteral("'");
}

QStringList terminalCandidates(const QProcessEnvironment &env,
                               const QString &preferred) {
  QStringList candidates;
  auto add = [&candidates](const QString &name) {
    if (!name.isEmpty() && !candidates.contains(name)) {
      candidates << name;
    }
  };
  add(preferred.trimmed());
  add(env.value(QStringLiteral("TERMINAL")).trimmed());
  add(QStringLiteral("gnome-terminal"));
  add(QStringLiteral("konsole"));
  add(QStringLiteral("xfce4-terminal"));
  add(QStringLiteral("xterm"));
  add(QStringLiteral("x-terminal-emulator"));
  return candidates;
}

QString findTerminal(const QStringList &candidates) {
  for (const QString &name : candidates) {
    const QFileInfo info(name);
    if (info.isAbsolute()) {
      // a full path is used as is when it is an executable file
      if (info.isFile() && info.isExecutable()) {
        return name;
      }
    } else {
      const QString found = QStandardPaths::findExecutable(name);
      if (!found.isEmpty()) {
        return found;
      }
    }
  }
  return QString();
}

QString terminalNotFoundMessage() {
  return QCoreApplication::translate(
      "MainWindow",
      "Not sure how to launch terminal!\n"
      "Set \"Terminal command\" in Preferences (a terminal emulator such as "
      "xterm or konsole, or its full path; not a shell), or define the "
      "$TERMINAL environment variable.");
}

TerminalLaunch buildTerminalLaunch(const QString &terminalPath,
                                   const QString &shellCommand) {
  const QString name = QFileInfo(terminalPath).fileName();
  const QStringList shell = {QStringLiteral("sh"), QStringLiteral("-c"),
                             shellCommand};

  TerminalLaunch launch;
  launch.program = terminalPath;
  // gnome-terminal ignores "-e" with a string, so it gets "--wait --" (the
  // launcher then waits for the command, #163/#172/#239)
  if (name == QLatin1String("gnome-terminal")) {
    launch.args = QStringList{QStringLiteral("--wait"), QStringLiteral("--")} +
                  shell;
  } else if (name == QLatin1String("konsole")) {
    launch.args =
        QStringList{QStringLiteral("--nofork"), QStringLiteral("-e")} + shell;
  } else if (name == QLatin1String("xfce4-terminal")) {
    launch.args = QStringList{QStringLiteral("--disable-server"),
                              QStringLiteral("-x")} +
                  shell;
  } else {
    launch.args = QStringList{QStringLiteral("-e")} + shell;
  }
  return launch;
}

QString rcloneConfigShellCommand(const QString &rclone,
                                 const QStringList &confArgs) {
  QString cmd = shellQuote(rclone) + QStringLiteral(" config");
  if (confArgs.size() == 2) {
    cmd += QStringLiteral(" --config ") + shellQuote(confArgs.at(1));
  }
  // the shell's printf needs a literal \n, hence the doubled backslash in C++
  cmd += QStringLiteral("; printf '\\n%s' 'Press Enter to close'; read _");
  return cmd;
}
