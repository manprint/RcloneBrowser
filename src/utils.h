#pragma once

#include "pch.h"

std::unique_ptr<QSettings> GetSettings();

void ReadSettings(QSettings *settings, QObject *widget);
void WriteSettings(QSettings *settings, QObject *widget);

bool IsPortableMode();

QString GetRclone();
void SetRclone(const QString &rclone);

// Empty when `path` can be started as rclone; else a message that says why and
// names the path (#145, #159). A name without a directory separator is a
// command that QProcess finds through PATH, so it is looked up there. Callers
// pass GetRclone().
QString rcloneExecutableProblem(const QString &path);

QStringList GetRcloneConf();
void SetRcloneConf(const QString &rcloneConf);

QString GetRclonePassword();
void SetRclonePassword(const QString &rclonePassword);

QStringList GetDefaultOptionsList(const QString &settingsOptions);
QStringList GetRemoteModeRcloneOptions();
// remotes icon size setting -> multiplier; accepts a number ("2.5") or
// one of the old letter values (XS, S, M, L, XL, XXL)
double RemotesIconScaleFromSetting(const QString &value);
QStringList GetShowHidden();
QStringList GetRcloneCmd(const QStringList &args);

QDir GetConfigDir(void);
