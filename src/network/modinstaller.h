// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/mods.h"
#include "core/resources.h"
#include "core/packs.h"
#include "gameinstaller.h"
class QNetworkAccessManager;
namespace mc {
struct ModPlanEdge {QString from,to,versionId,type;};
struct ModPlan {
    Instance target;
    QList<ResourceFile> files;
    QString rootProjectId,displayTargetName;
    QList<ResourceFile> resolved;
    QList<ModPlanEdge> edges;
    QList<ModPlan> optionalPlans;
    QStringList optionalProjects,requiredProjects;
    QMap<QString,QString> optionalErrors;
    QStringList reusedProjects;
    QMap<QString,QString> reusedFiles,disabledFiles,enableFiles;
    QMap<QString,QString> replacementVersions;
    QStringList activeProjects,demandedProjects;
    QMap<QString,QString> projectNames,replacementProjects;
    QMap<QString,QUrl> projectIcons;
    QMap<QString,QByteArray> localProjectIcons;
    QStringList skippedProjects;
    QStringList replacements,notes;
    QMap<QString,QString> disableCandidates; // filename -> explicit conflict explanation
    QStringList disableFiles; // user-authorized only
    QJsonObject originalManifest;
    QJsonObject expectedFiles;
    QString error;
};
struct ModInstallResult { QString error; QStringList installed,enabled,warnings; };
// A downloaded, verified resource committed together with its supporting mods.
struct AttachedPack { QString source; PackKind kind=PackKind::Shader; ResourceProject project; ResourceFile file; QByteArray icon; };
ModPlan planModInstall(const Instance &target,const ResourceFile &file,const CancelFlag &cancel,QNetworkAccessManager *network=nullptr);
ModPlan expandedModPlan(const ModPlan &plan);
ModPlan selectModDependencies(const ModPlan &plan,const QSet<QString> &selectedProjects,const QSet<QString> &redownloadProjects={},const QSet<QString> &enableProjects={});
ModInstallResult applyModPlan(const ModPlan &plan,const CancelFlag &cancel,const InstallProgress &progress={},QNetworkAccessManager *network=nullptr,const AttachedPack *pack=nullptr);
}
