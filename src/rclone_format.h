#pragma once
#include <QtCore>
#include <optional>

QString formatBytes(qint64 n);                         // "0 B", "1.500 KiB", "-" when negative
QString formatRate(double bps);                        // formatBytes(round(bps)) + "/s"
QString formatDuration(std::optional<qint64> seconds); // "1h2m3s", "-" for nullopt/negative
QString formatElapsed(double seconds);                 // formatDuration(qint64(seconds))
QString formatPercent(qint64 part, qint64 total);      // "-" when total <= 0, else 0..100 "%"
