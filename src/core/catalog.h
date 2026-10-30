// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QStringList>

namespace mc {
struct Instance {
    QString id;
    QString path;
    QString root;
    QString version;
    QString loader;
    QString loaderVersion,loaderProblem;
    QString optifineVersion,optifineSha512,optifineForgeVersion,optifineFile;
    QString gameDirectory;
    int javaMajor = 0;
    bool isolated = false;
    bool supportsMods() const { return loaderProblem.isEmpty()&&(loader=="fabric"||loader=="quilt"||loader=="forge"||loader=="neoforge"); }
    QString loaderName() const;
};
struct CatalogResult {
    QList<Instance> instances;
    QStringList warnings;
};
QMap<QString,QString> gameLoaderComponents(const QJsonObject &metadata);
CatalogResult scanInstances(const QStringList &roots, const QJsonObject &isolationOverrides = {});
int requiredJavaMajor(const QString &version);
}
