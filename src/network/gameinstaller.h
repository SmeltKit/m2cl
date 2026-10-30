// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/gameplan.h"
#include "core/java.h"
#include "core/memory.h"
#include <atomic>
#include <functional>
#include <memory>
class QNetworkAccessManager;

namespace mc {
using CancelFlag = std::shared_ptr<std::atomic_bool>;
using InstallProgress = std::function<void(const QString &, int)>;
struct VersionList { QJsonArray versions; QString error; };
struct InstallRequest {
    QString root, id, version, directory, java, player;
    GameIdentity identity; // Ephemeral authenticated launch state; never written to version JSON.
    QJsonObject offlineSkin;
    QString launcherDirectory;
    QString downloadSource = QStringLiteral("auto");
    QString optifineLocalFile;
    QString optifineAction; // install / remove / empty (preserve)
    QJsonObject optifineBuild;
    QString loader, loaderVersion; // Empty keeps the installed component; explicit loader requests install/upgrade it.
    QJsonObject versionEntry, javaChoice;
    QStringList javaRoots;
    int memoryMiB = 2048;
    LaunchOptions launchOptions;
    std::function<MemorySnapshot()> memorySampler;
    bool launch = false;
};
struct InstallResult {
    QString error, instancePath, java;
    QStringList warnings;
    LaunchCommand command;
    JavaRuntime javaRuntime;
};
QByteArray fetchInstallData(const QUrl &url, const CancelFlag &cancelled, const QString &sha1 = {}, QNetworkAccessManager *network = nullptr, const QString &source = "official", const InstallProgress &progress = {});
void downloadVerifiedGameFiles(const QList<GameFile> &files, const QString &root, const CancelFlag &cancelled, const InstallProgress &progress, QNetworkAccessManager *network = nullptr, const QString &source = "official");
VersionList fetchQuiltVersions(const QString &gameVersion, const CancelFlag &cancelled, const QString &source = "official");
VersionList fetchFabricVersions(const QString &gameVersion, const CancelFlag &cancelled, const QString &source = "official");
QJsonObject mergeGameMetadata(const QJsonObject &base, const QJsonObject &child);
VersionList fetchGameVersions(const CancelFlag &cancelled, const QString &source = "official");
InstallResult prepareGame(const InstallRequest &request, const CancelFlag &cancelled, const InstallProgress &progress,
                          QNetworkAccessManager *network = nullptr);
}
