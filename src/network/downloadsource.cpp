// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "downloadsource.h"
#include <QTimeZone>

namespace mc {
QString resolveDownloadSource(const QString &source,const QByteArray &timeZoneId){
    if(QStringList{"official","bmclapi","auto-mirror-first","auto-official-first"}.contains(source))return source;
    const auto zone=timeZoneId.isEmpty()?QTimeZone::systemTimeZoneId():timeZoneId;
    return zone.toLower()=="asia/shanghai"?QString("auto-mirror-first"):QString("auto-official-first");
}
QStringList downloadSourceOrder(const QString &source){
    const auto resolved=resolveDownloadSource(source);
    if(resolved=="official")return {"official"};
    if(resolved=="bmclapi")return {"bmclapi"};
    return resolved=="auto-mirror-first"?QStringList{"bmclapi","official"}:QStringList{"official","bmclapi"};
}
QString automaticDownloadSourceLabel(const QByteArray &zone){
    return resolveDownloadSource("auto",zone)=="auto-mirror-first"?QCoreApplication::translate("M2CL", "自动（镜像优先）"):QCoreApplication::translate("M2CL", "自动（官方优先）");
}
QList<QUrl> downloadCandidates(const QUrl &official, const QString &source) {
    QUrl mirror;
    if (official.scheme() == "https" && official.userInfo().isEmpty()) {
        const auto host = official.host();
        QString prefix;
        bool mapped = false;
        if (host == "piston-meta.mojang.com" || host == "piston-data.mojang.com" || host == "launchermeta.mojang.com" || host == "launcher.mojang.com") mapped = true;
        else if (host == "resources.download.minecraft.net") { prefix = "/assets"; mapped = true; }
        else if (host == "libraries.minecraft.net") { prefix = "/libraries"; mapped = true; }
        else if (host == "maven.minecraftforge.net" || host == "maven.fabricmc.net") { prefix = "/maven"; mapped = true; }
        else if (host == "meta.fabricmc.net") { prefix = "/fabric-meta"; mapped = true; }
        else if (host == "authlib-injector.yushi.moe") { prefix = "/mirrors/authlib-injector"; mapped = true; }
        if (host == "files.minecraftforge.net" && official.path().startsWith("/maven/")) mapped = true;
        if (mapped) { mirror = official; mirror.setHost("bmclapi2.bangbang93.com"); mirror.setPort(-1); mirror.setPath(prefix + official.path()); }
    }
    if (mirror.isEmpty()) return {official};
    QList<QUrl> result;for(const auto &provider:downloadSourceOrder(source))result.append(provider=="bmclapi"?mirror:official);
    return result;
}
QString downloadSourceLabel(const QUrl &url) {
    return url.host() == "bmclapi2.bangbang93.com" ? QStringLiteral("BMCLAPI") : QCoreApplication::translate("M2CL", "官方源");
}
}
