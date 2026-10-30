// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "catalog.h"
#include <QJsonObject>
#include <QList>
#include <QHash>
namespace mc {
struct ModMetadata {
    QString id,name,version,loader,problem,iconPath;
    QStringList ids,requiredIds;
    QJsonObject requirements,breaks,conflicts;
    QHash<QString,QString> providedVersions;
    QHash<QString,QString> embeddedHashes;
    QList<ModMetadata> components;
    bool known=false;
};
struct LocalMod:ModMetadata {
    QString fileName,sha512,projectId,versionId;
    QMap<QString,ModMetadata> metadata;
    QMap<QString,QString> metadataProblems;
    bool enabled=true,validArchive=false;
};
LocalMod selectModMetadata(const LocalMod &mod,const QString &loader);
struct ModCheck {QStringList errors,warnings;};
ModCheck checkModSet(const Instance &instance,const QList<LocalMod> &mods,bool requireKnown=false,bool allowMissing=false);
QStringList modCompatibilityWarnings(const LocalMod &mod,const QList<LocalMod> &mods);
int modVersionMatches(const QString &range,const QString &version);
QByteArray fileSha512(const QString &path);
QByteArray jarEntry(const QString &archive,const QString &entry,int maxBytes=2*1024*1024);
LocalMod inspectMod(const QString &path,const QString &preferredLoader={});
QByteArray localModIcon(const QString &archive,const ModMetadata &metadata);
QList<LocalMod> localMods(const Instance &instance);
QJsonObject modManifest(const QString &gameDirectory);
QString importLocalMods(const Instance &instance,const QStringList &paths,bool allowUnknown=false);
QString setModEnabled(const Instance &instance,const QString &fileName,bool enabled);
QString removeLocalMod(const Instance &instance,const QString &fileName);
QString setModsEnabled(const Instance &instance,const QStringList &fileNames,bool enabled);
QString removeLocalMods(const Instance &instance,const QStringList &fileNames);
bool isVersionComponentFile(const Instance &instance,const QString &fileName);
QString modEnvironmentProblem(const LocalMod &mod,const Instance &instance);
QString duplicateModProblem(const LocalMod &first,const LocalMod &second);
QString modDependenciesProblem(const LocalMod &mod,const QList<LocalMod> &all,bool allowMissing=false);
}
