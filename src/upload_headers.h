#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

// Upload headers and S3 object tags (13.1): the text of the Extra options tab
// turned into rclone --header-upload values. Qt Core only.

struct S3Tag {
  QString key, value;
};

// Lines "key=value": each line is trimmed and blank lines are skipped. The
// line is split at the first '='; key and value are trimmed, and the value
// may be empty or contain '='.
// nullopt if a non-blank line has no '=', the key is empty, there are more
// than 10 tags (the S3 limit), or a key occurs twice (S3 rejects duplicates).
std::optional<QVector<S3Tag>> parseS3Tags(const QString &text);

// "X-Amz-Tagging: " + join(pct(key) + "=" + pct(value), "&"), percent-encoded
// as a URL query: space -> %20, & -> %26, = -> %3D.
QString s3TaggingHeader(const QVector<S3Tag> &tags);

// Lines "Name: value": each line is trimmed and blank lines are skipped. The
// line is split at the first ':'. The name (trimmed) must be an HTTP token;
// the value (trimmed) may contain ':' and may be empty.
// Returns each header normalized to "name: value". nullopt on any bad line.
// Empty or blank text gives an empty list.
std::optional<QStringList> parseUploadHeaders(const QString &text);

// each header -> "--header-upload", header
QStringList headerUploadArgs(const QStringList &headers);
