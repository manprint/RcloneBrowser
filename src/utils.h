#pragma once

#include "pch.h"

std::unique_ptr<QSettings> GetSettings();

void ReadSettings(QSettings *settings, QObject *widget);
void WriteSettings(QSettings *settings, QObject *widget);

bool IsPortableMode();

QString GetRclone();
void SetRclone(const QString &rclone);

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
