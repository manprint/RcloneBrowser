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

// Terminal names to try, in order: $TERMINAL (when set and not empty), then
// gnome-terminal, konsole, xfce4-terminal, xterm and x-terminal-emulator.
// These are names, not paths.
QStringList terminalCandidates(const QProcessEnvironment &env);

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
