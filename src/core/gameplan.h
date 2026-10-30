// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "launchoptions.h"
#include "accounts.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>

namespace mc {
struct GamePlatform {
    QString os, arch, version;
    static GamePlatform current();
};
struct GameFile {
    QString path, sha1;
    QUrl url;
    qint64 size = 0;
    bool executable = false;
};
struct GamePlan {
    QList<GameFile> files;
    QStringList classpath, natives;
    QString assetIndex, loggingArgument;
    int javaMajor = 0;
};
struct LaunchCommand { QString executable, directory; QStringList arguments; QString memoryNote; QStringList redactionSecrets; bool online=false; };
bool safeRelativePath(const QString &path);
QString checkedPath(const QString &root, const QString &relative);
bool rulesAllow(const QJsonArray &rules, const GamePlatform &platform);
GameFile gameFile(const QJsonObject &descriptor, const QString &path);
GamePlan gamePlan(const QJsonObject &metadata, const QString &instanceId, const GamePlatform &platform);
QString offlineUuid(const QString &name);
QString defaultInstanceName(const QString &version, const QString &loader, const QStringList &existing);
LaunchCommand offlineCommand(const QJsonObject &metadata, const GamePlan &plan, const QString &root,
    const QString &instanceId, const QString &gameDirectory, const QString &java, const QString &player, int memoryMiB,
    const GamePlatform &platform,const LaunchOptions &options={});
LaunchCommand gameCommand(const QJsonObject &metadata,const GamePlan &plan,const QString &root,
    const QString &instanceId,const QString &gameDirectory,const QString &java,const GameIdentity &identity,
    int memoryMiB,const GamePlatform &platform,const LaunchOptions &options={});
void extractNatives(const QString &archive, const QString &directory);
}
