#pragma once

#include <QString>
#include <QStringList>

// Text that a user pastes into a shell to run the same argv the app starts
// (the "copy command" buttons and the Tasks "Copy command" action).
//
// Known limits of the Windows form: cmd.exe still expands %VAR% inside double
// quotes (so an argument with '%' is quoted, which does not stop the expansion),
// an embedded '"' followed by a cmd.exe metacharacter is not safe in cmd.exe,
// and PowerShell needs the call operator '&' in front of a quoted program path.
enum class Shell { Posix, Windows };

// POSIX sh: '' for an empty string; unchanged when every character is one of
// [A-Za-z0-9_@%+=:,./-]; otherwise wrapped in single quotes with every ' written
// as '\''.
QString quoteArgPosix(const QString &a);

// CommandLineToArgvW rules: unchanged when non-empty and free of space, tab,
// '"', line breaks and the cmd.exe metacharacters & | < > ^ ( ) % !; otherwise
// wrapped in double quotes with every '"' written as \" and the backslashes
// before a '"' or before the closing quote doubled.
QString quoteArgWindows(const QString &a);

// The program and every argument quoted for `shell` (the program like an
// argument), joined by single spaces.
QString commandLineForClipboard(const QString &program,
                                const QStringList &args, Shell shell);

// Windows on Q_OS_WIN, otherwise Posix.
Shell nativeShell();
