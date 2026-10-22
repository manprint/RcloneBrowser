#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

// POSIX shell quoting: the string is wrapped in single quotes and every '
// becomes '\''. An empty string gives ''.
QString shellQuote(const QString &s);

// Program and arguments that start a terminal emulator.
struct TerminalLaunch {
  QString program;
  QStringList args;
};

// Terminal names to try, in order: `preferred` (the Preferences value
// "Terminal command": a program name or a full path, no arguments; skipped
// when blank), $TERMINAL (when set and not empty), then gnome-terminal,
// konsole, xfce4-terminal, xterm and x-terminal-emulator. A repeated entry is
// listed once.
QStringList terminalCandidates(const QProcessEnvironment &env,
                               const QString &preferred = QString());

// The first candidate that can be started: a full path that is an executable
// file, or a name found in PATH. Empty when there is none. A candidate that is
// not found is skipped (the caller's order is the fallback order).
QString findTerminal(const QStringList &candidates);

// The text of the "no terminal found" box. It names the Preferences field and
// $TERMINAL (not $TERM, which is a terminal type, #175).
QString terminalNotFoundMessage();

// Arguments that make the terminal at terminalPath run shellCommand via
// "sh -c". The flags depend on the executable's file name (gnome-terminal,
// konsole and xfce4-terminal each have their own); every other terminal gets
// the common "-e".
TerminalLaunch buildTerminalLaunch(const QString &terminalPath,
                                   const QString &shellCommand);

// Shell command that runs "rclone config" (with "--config <path>" when
// confArgs is {"--config", <path>}) and keeps the terminal open until Enter.
QString rcloneConfigShellCommand(const QString &rclone,
                                 const QStringList &confArgs);
