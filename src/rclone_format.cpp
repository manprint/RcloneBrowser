#include "rclone_format.h"

#include <limits>

QString formatBytes(qint64 n) {
  if (n < 0) {
    return QStringLiteral("-");
  }
  if (n < 1024) {
    return QString::number(n) + QStringLiteral(" B");
  }
  static const char *const units[] = {"KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
  double v = double(n) / 1024.0;
  int u = 0;
  while (v >= 1024.0 && u < 5) {
    v /= 1024.0;
    ++u;
  }
  return QString::number(v, 'f', 3) + QLatin1Char(' ') +
         QLatin1String(units[u]);
}

QString formatRate(double bps) {
  return formatBytes(qRound64(bps)) + QStringLiteral("/s");
}

QString formatDuration(std::optional<qint64> seconds) {
  if (!seconds || *seconds < 0) {
    return QStringLiteral("-");
  }
  qint64 s = *seconds;
  const qint64 d = s / 86400;
  s %= 86400;
  const qint64 h = s / 3600;
  s %= 3600;
  const qint64 m = s / 60;
  s %= 60;
  QString r;
  if (d > 0) {
    r += QString::number(d) + QLatin1Char('d');
  }
  if (d > 0 || h > 0) {
    r += QString::number(h) + QLatin1Char('h');
  }
  if (d > 0 || h > 0 || m > 0) {
    r += QString::number(m) + QLatin1Char('m');
  }
  r += QString::number(s) + QLatin1Char('s');
  return r;
}

QString formatElapsed(double seconds) {
  return formatDuration(qint64(seconds));
}

QString formatNiceSize(qint64 size) {
  if (size < 0) {
    return QString();
  }
  if (size < 10) {
    return QString::number(size);
  }
  // Largest unit with size >= 10 * base. quint64 keeps 10 * 2^60 from overflowing.
  static const char prefix[] = " KMGTPE";
  const quint64 u = quint64(size);
  for (int i = int(sizeof(prefix)) - 2; i >= 0; --i) {
    const quint64 base = quint64(1) << (i * 10);
    if (u >= 10 * base) {
      return QStringLiteral("%1 %2")
          .arg(u / base)
          .arg(QChar(prefix[i]))
          .trimmed();
    }
  }
  return QString::number(size);
}

QString formatPercent(qint64 part, qint64 total) {
  if (total <= 0) {
    return QStringLiteral("-");
  }
  qint64 pct;
  if (part >= total) {
    pct = 100;
  } else if (part <= 0) {
    pct = 0;
  } else if (part > std::numeric_limits<qint64>::max() / 100) {
    pct = qint64(double(part) * 100.0 / double(total));
  } else {
    pct = part * 100 / total;
  }
  return QString::number(qBound(qint64(0), pct, qint64(100))) + QLatin1Char('%');
}
