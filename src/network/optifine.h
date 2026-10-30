// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "gameinstaller.h"
#include <QJsonObject>
#include <QLockFile>
namespace mc {
QJsonArray parseOptifineBuilds(const QByteArray &html, const QString &game);
QJsonArray parseBmclOptifineBuilds(const QByteArray &json, const QString &game);
VersionList fetchOptifineBuilds(const QString &game, const CancelFlag &cancelled, const QString &source = "official");
QJsonObject inspectOptifine(const QString &jar);
QString optifineCompatibility(const QJsonObject &build, const QString &game, const QString &loader, const QString &loaderVersion);
// Prepared output remains outside mods until commit. A failed metadata save restores the old file.
class OptifineChange {
public:
    ~OptifineChange();
    void prepare(const InstallRequest &request, QJsonObject &metadata, const QString &java,
                 const CancelFlag &cancelled, const InstallProgress &progress);
    void commitFiles();
    void finish() { m_finished=true; }
private:
    std::unique_ptr<QLockFile> m_lock;
    QString m_target,m_oldTarget,m_stage,m_backup,m_oldHash;
    bool m_finished=false,m_changed=false,m_added=false;
};
void verifyOptifine(const QJsonObject &metadata,const QString &root,const QString &gameDirectory);
}
