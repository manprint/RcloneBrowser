#include <QtTest>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#include "command_line.h"

namespace {

// Reference for CommandLineToArgvW (argv[0] has its own rules: no backslash
// processing). The newer ucrt rule that `""` inside quotes is a literal quote is
// deliberately not implemented: the quoter never emits it.
QStringList parseWin(const QString &s) {
  QStringList out;
  qsizetype i = 0;
  const qsizetype n = s.size();
  const auto ws = [](QChar c) { return c == ' ' || c == '\t'; };
  while (i < n && ws(s[i])) {
    ++i;
  }
  {
    QString a; // argv[0]
    if (i < n && s[i] == '"') {
      ++i;
      while (i < n && s[i] != '"') {
        a += s[i++];
      }
      if (i < n) {
        ++i;
      }
    } else {
      while (i < n && !ws(s[i])) {
        a += s[i++];
      }
    }
    out << a;
  }
  for (;;) {
    while (i < n && ws(s[i])) {
      ++i;
    }
    if (i >= n) {
      break;
    }
    QString a;
    bool inQuote = false;
    while (i < n) {
      if (s[i] == '\\') {
        qsizetype k = 0;
        while (i < n && s[i] == '\\') {
          ++k;
          ++i;
        }
        if (i < n && s[i] == '"') {
          a += QString(k / 2, QLatin1Char('\\'));
          if (k % 2) {
            a += QLatin1Char('"');
            ++i;
          } // even: the quote toggles on the next pass
        } else {
          a += QString(k, QLatin1Char('\\'));
        }
        continue;
      }
      if (s[i] == '"') {
        inQuote = !inQuote;
        ++i;
        continue;
      }
      if (!inQuote && ws(s[i])) {
        break;
      }
      a += s[i++];
    }
    out << a; // an empty token ("") yields an empty argument
  }
  return out;
}

#ifdef Q_OS_WIN
QStringList systemParse(const QString &line) {
  int argc = 0;
  LPWSTR *argv = CommandLineToArgvW(reinterpret_cast<LPCWSTR>(line.utf16()), &argc);
  QStringList out;
  for (int i = 0; i < argc; ++i) {
    out << QString::fromWCharArray(argv[i]);
  }
  LocalFree(argv);
  return out;
}
#endif

} // namespace

class TestCommandLine : public QObject {
  Q_OBJECT

private slots:
  // T-CMD-POSIX
  void T_CMD_POSIX() {
    QCOMPARE(quoteArgPosix(QStringLiteral("abc")), QStringLiteral("abc"));
    QCOMPARE(quoteArgPosix(QString()), QStringLiteral("''"));
    QCOMPARE(quoteArgPosix(QStringLiteral("a b")), QStringLiteral("'a b'"));
    QCOMPARE(quoteArgPosix(QStringLiteral("it's")),
             QStringLiteral("'it'\\''s'"));
    QCOMPARE(quoteArgPosix(QStringLiteral("--exclude")),
             QStringLiteral("--exclude"));
    QCOMPARE(quoteArgPosix(QStringLiteral("._*")), QStringLiteral("'._*'"));
    // every character the shell treats specially forces the quotes
    for (const QString &x :
         {QStringLiteral("$HOME"), QStringLiteral("~x"), QStringLiteral("a`b"),
          QStringLiteral("a\\b"), QStringLiteral("a;b"), QStringLiteral("a&b"),
          QStringLiteral("a|b"), QStringLiteral("a!b"), QStringLiteral("a\nb"),
          QStringLiteral("*"), QStringLiteral("#x"), QStringLiteral("a\tb"),
          QStringLiteral("a\"b"), QStringLiteral("a<b"), QStringLiteral("a>b"),
          QStringLiteral("a(b"), QStringLiteral("a)b"), QStringLiteral("a?b"),
          QStringLiteral("a[b"), QStringLiteral("a{b")}) {
      QCOMPARE(quoteArgPosix(x), QLatin1Char('\'') + x + QLatin1Char('\''));
    }
    QCOMPARE(commandLineForClipboard(QStringLiteral("/usr/bin/rclone"),
                                     {QStringLiteral("copy"),
                                      QStringLiteral("a b"), QString()},
                                     Shell::Posix),
             QStringLiteral("/usr/bin/rclone copy 'a b' ''"));
  }

  // T-CMD-WIN
  void T_CMD_WIN() {
    QCOMPARE(quoteArgWindows(QStringLiteral(
                 "r:\\Downloads\\Redump - Sega - Dreamcast\\")),
             QStringLiteral(
                 "\"r:\\Downloads\\Redump - Sega - Dreamcast\\\\\""));
    QCOMPARE(quoteArgWindows(QStringLiteral("a\"b")),
             QStringLiteral("\"a\\\"b\""));
    QCOMPARE(quoteArgWindows(QStringLiteral("C:\\x\\y")),
             QStringLiteral("C:\\x\\y"));
    QCOMPARE(quoteArgWindows(QStringLiteral("a&b")),
             QStringLiteral("\"a&b\""));
    QCOMPARE(quoteArgWindows(QString()), QStringLiteral("\"\""));
    // every cmd.exe metacharacter forces the quotes (the round trip through
    // parseWin does not model cmd.exe, so each one is pinned here)
    for (const QString &x :
         {QStringLiteral("a|b"), QStringLiteral("a<b"), QStringLiteral("a>b"),
          QStringLiteral("a^b"), QStringLiteral("a(b"), QStringLiteral("a)b"),
          QStringLiteral("100%"), QStringLiteral("a!b"), QStringLiteral("a\nb"),
          QStringLiteral("a\rb"), QStringLiteral("a\tb")}) {
      QCOMPARE(quoteArgWindows(x), QLatin1Char('"') + x + QLatin1Char('"'));
    }
  }

  // T-CMD-ISSUE226: the argv of the report, parsed back as CommandLineToArgvW
  // does, equals the argv.
  void T_CMD_ISSUE226() {
    // the reference parser first, so a parser bug cannot mask a quoter bug
    QCOMPARE(parseWin(QStringLiteral("p.exe a \"b c\" \"d\\\\\"")),
             (QStringList{"p.exe", "a", "b c", "d\\"}));
    QCOMPARE(parseWin(QStringLiteral("\"C:\\x y\\p.exe\" \"\" \"a\\\"b\"")),
             (QStringList{"C:\\x y\\p.exe", "", "a\"b"}));
    QCOMPARE(parseWin(QStringLiteral("p x\\\\y \"\\\\\\\"q\"")),
             (QStringList{"p", "x\\\\y", "\\\"q"}));

    const QString program = QStringLiteral("N:\\rclone\\rclone.exe");
    QStringList args = QString::fromLatin1(
                           "--config N:/rclone/rclone.conf move --verbose "
                           "--transfers 4 --checkers 8 --contimeout 60s "
                           "--timeout 300s --retries 3 --low-level-retries 10 "
                           "--stats 1s --stats-file-name-length 0 --fast-list")
                           .split(QLatin1Char(' '));
    args << QStringLiteral("r:\\Downloads\\Jdownloader\\Redump - Sega - "
                           "Dreamcast\\")
         << QStringLiteral("MediaDB:00 - Online Media/Giochi/Rom & Bios "
                           "Sets/Sega Dreamcast/");
    const QString line = commandLineForClipboard(program, args, Shell::Windows);
    QVERIFY2(line.contains(QStringLiteral("\"r:\\Downloads\\Jdownloader\\Redump "
                                          "- Sega - Dreamcast\\\\\"")),
             qPrintable(line));
    QCOMPARE(parseWin(line), QStringList{program} + args);
#ifdef Q_OS_WIN
    QCOMPARE(systemParse(line), QStringList{program} + args);
#endif

    // corpus: empty, spaces, tabs, quotes, backslash runs, metacharacters
    const QStringList corpus = {
        QString(),
        QStringLiteral(" "),
        QStringLiteral("a b"),
        QStringLiteral("a\tb"),
        QStringLiteral("\""),
        QStringLiteral("a\"b"),
        QStringLiteral("\"quoted\""),
        QStringLiteral("a\\"),
        QStringLiteral("a b\\"),
        QStringLiteral("a b\\\\"),
        QStringLiteral("a\\\"b"),
        QStringLiteral("a \\\"b"),
        QStringLiteral("a \\\\\"b"),
        QStringLiteral("\\\\"),
        QStringLiteral("C:\\x y\\"),
        QStringLiteral("a&b"),
        QStringLiteral("100%"),
        QStringLiteral("x|y<z>w^v(u)!"),
        QStringLiteral("dir [x] (y)/\u00f1\"quote'.txt"),
    };
    for (const QString &x : corpus) {
      const QString one =
          commandLineForClipboard(program, {x}, Shell::Windows);
      QCOMPARE(parseWin(one), (QStringList{program, x}));
#ifdef Q_OS_WIN
      QCOMPARE(systemParse(one), (QStringList{program, x}));
#endif
    }
  }
};

QTEST_GUILESS_MAIN(TestCommandLine)
#include "tst_command_line.moc"
