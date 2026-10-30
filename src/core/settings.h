// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "launchoptions.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

namespace mc {
struct Settings {
    QStringList gameRoots;
    QString selectedInstance;
    QString modInstallTarget;
    QJsonObject legacyInstanceJava; // Pending migration only; never a runtime policy.
    QJsonArray javaCatalog;
    QString microsoftClientId; // Public developer configuration, never a client secret.
    QString offlinePlayer = QStringLiteral("Player");
    int memoryMiB = 2048;
    QStringList javaFolders;
    QString appearance = QStringLiteral("system");
    QString language = QStringLiteral("system");
    QString downloadSource = QStringLiteral("auto");
    QString theme = QStringLiteral("forest");
    bool exitAfterLaunch = true;
    QJsonObject isolationOverrides;
    QStringList favoriteInstances;
    QJsonObject instanceNames;
    QJsonObject coverOverrides;
};

class SettingsStore {
public:
    explicit SettingsStore(QString directory);
    Settings load(QString *error = nullptr) const;
    bool save(const Settings &settings, QString *error = nullptr) const;
    QString directory() const { return m_directory; }
private:
    QString m_directory;
};
QString defaultGameRoot();
QString instanceConfigPath(const QString &instancePath);
QJsonObject loadInstanceJavaChoice(const QString &instancePath, QString *error = nullptr);
bool saveInstanceJavaChoice(const QString &instancePath, const QJsonObject &choice, QString *error = nullptr);
LaunchOptions loadInstanceLaunchOptions(const QString &instancePath,int defaultMemoryMiB=2048,QString *error=nullptr);
bool saveInstanceRuntimeSettings(const QString &instancePath,const QJsonObject &java,const LaunchOptions &launch,QString *error=nullptr);
QStringList migrateInstanceJavaChoices(Settings &settings, const QStringList &instancePaths);
}
