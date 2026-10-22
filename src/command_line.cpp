#include "command_line.h"

QString quoteArgPosix(const QString &a) {
  if (a.isEmpty()) {
    return QStringLiteral("''");
  }
  bool plain = true;
  for (const QChar c : a) {
    const ushort u = c.unicode();
    const bool ok = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') ||
                    (u >= '0' && u <= '9') || u == '_' || u == '@' ||
                    u == '%' || u == '+' || u == '=' || u == ':' || u == ',' ||
                    u == '.' || u == '/' || u == '-';
    if (!ok) {
      plain = false;
      break;
    }
  }
  if (plain) {
    return a;
  }
  QString out = a;
  out.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
  return QLatin1Char('\'') + out + QLatin1Char('\'');
}

QString quoteArgWindows(const QString &a) {
  static const QString special = QStringLiteral(" \t\"\n\r&|<>^()%!");
  bool plain = !a.isEmpty();
  for (const QChar c : a) {
    if (special.contains(c)) {
      plain = false;
      break;
    }
  }
  if (plain) {
    return a;
  }
  QString out = QStringLiteral("\"");
  qsizetype backslashes = 0;
  for (const QChar c : a) {
    if (c == QLatin1Char('\\')) {
      ++backslashes;
      continue;
    }
    if (c == QLatin1Char('"')) {
      out += QString(backslashes * 2 + 1, QLatin1Char('\\'));
      out += QLatin1Char('"');
      backslashes = 0;
      continue;
    }
    out += QString(backslashes, QLatin1Char('\\'));
    backslashes = 0;
    out += c;
  }
  // the backslashes before the closing quote would escape it
  out += QString(backslashes * 2, QLatin1Char('\\'));
  out += QLatin1Char('"');
  return out;
}

QString commandLineForClipboard(const QString &program,
                                const QStringList &args, Shell shell) {
  const auto quote = [shell](const QString &a) {
    return shell == Shell::Windows ? quoteArgWindows(a) : quoteArgPosix(a);
  };
  QString line = quote(program);
  for (const QString &a : args) {
    line += QLatin1Char(' ');
    line += quote(a);
  }
  return line;
}

Shell nativeShell() {
#ifdef Q_OS_WIN
  return Shell::Windows;
#else
  return Shell::Posix;
#endif
}
