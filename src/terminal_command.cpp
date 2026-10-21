#include "terminal_command.h"

#include <QFileInfo>

QString shellQuote(const QString &s) {
  QString out = s;
  // close the quote, emit an escaped quote, reopen the quote
  out.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
  return QStringLiteral("'") + out + QStringLiteral("'");
}

QStringList terminalCandidates(const QProcessEnvironment &env) {
  QStringList candidates;
  const QString terminal = env.value(QStringLiteral("TERMINAL"));
  if (!terminal.isEmpty()) {
    candidates << terminal;
  }
  candidates << QStringLiteral("gnome-terminal") << QStringLiteral("konsole")
             << QStringLiteral("xfce4-terminal") << QStringLiteral("xterm")
             << QStringLiteral("x-terminal-emulator");
  return candidates;
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
