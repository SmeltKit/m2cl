// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "gameinstaller.h"
namespace mc {
// Both providers expose Maven metadata; only matching game releases are offered.
QString forgeMavenPath(const QString &coordinate);
QJsonArray forgeVersionsFromXml(const QByteArray &xml,const QString &loader,const QString &game);
VersionList fetchForgeVersions(const QString &loader,const QString &game,const CancelFlag &cancelled, const QString &source = "official");
QUrl forgeInstallerUrl(const QString &loader,const QString &game,const QString &version);
QJsonObject installForgeClient(const InstallRequest &request,const QJsonObject &vanilla,const QString &java,
                              const CancelFlag &cancelled,const InstallProgress &progress,QNetworkAccessManager *network=nullptr);
void verifyForgeOutputs(const QJsonObject &metadata,const QString &root);
}
