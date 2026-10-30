// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include <QLocale>
#include "i18n.h"
#include "resources.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QHash>
#include <QRegularExpression>
#include <QVersionNumber>
#include <QDateTime>
#include <algorithm>
#include <QSet>

namespace mc {
QString gameVersionChannel(const QString &id, const QString &type) {
    // The manifest labels April Fools releases as snapshots (some as releases).
    // Keep explicit exceptions rather than classifying every April 1 build as a joke.
    static const QSet<QString> special{"15w14a", "1.rv-pre1", "3d shareware v1.34",
        "20w14infinite", "22w13oneblockatatime", "23w13a_or_b", "24w14potato", "25w14craftmine", "26w14a"};
    if (special.contains(id.toLower())) return "special";
    if (type == "old_alpha" || type == "old_beta") return "historical";
    if (type == "release" || (type.isEmpty() && QRegularExpression("^\\d+(?:\\.\\d+)+$").match(id).hasMatch())) return "release";
    return "test";
}

static QString compactArray(const QString &value) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact));
}
QUrl ModrinthProvider::searchUrl(const QString &query, const QString &version, const QString &loader, const QString &category, const QString &sort, int offset) const {
    QUrl url("https://api.modrinth.com/v2/search");
    QUrlQuery parameters;
    parameters.addQueryItem("query", query);
    parameters.addQueryItem("limit", "20");
    parameters.addQueryItem("offset", QString::number(qMax(0, offset)));
    parameters.addQueryItem("index", QStringList{"relevance", "downloads", "follows", "newest", "updated"}.contains(sort) ? sort : "relevance");
    QJsonArray facets{QJsonArray{QStringLiteral("project_type:")+projectType}};
    if (!version.isEmpty()) facets.append(QJsonArray{QStringLiteral("versions:") + version});
    if (!loader.isEmpty() && loader != "vanilla") facets.append(QJsonArray{QStringLiteral("categories:") + loader});
    if (!category.isEmpty()) facets.append(QJsonArray{QStringLiteral("categories:") + category});
    parameters.addQueryItem("facets", QString::fromUtf8(QJsonDocument(facets).toJson(QJsonDocument::Compact)));
    url.setQuery(parameters);
    return url;
}
QUrl ModrinthProvider::versionsUrl(const QString &project, const QString &version, const QString &loader) const {
    QUrl url("https://api.modrinth.com/v2/project/" + QString::fromLatin1(QUrl::toPercentEncoding(project)) + "/version");
    QUrlQuery parameters;
    parameters.addQueryItem("include_changelog", "false");
    if (!version.isEmpty()) parameters.addQueryItem("game_versions", compactArray(version));
    if (!loader.isEmpty() && loader != "vanilla") parameters.addQueryItem("loaders", loader == "quilt" ? QStringLiteral("[\"quilt\",\"fabric\"]") : compactArray(loader));
    url.setQuery(parameters);
    return url;
}
QList<ResourceProject> ModrinthProvider::projects(const QJsonObject &object) {
    QList<ResourceProject> results;
    for (const auto &item : object.value("hits").toArray()) {
        const auto data = item.toObject();
        const auto id = data.value("project_id").toString();
        if (id.isEmpty()) continue;
        ResourceProject project;
        project.id = id;project.slug=data.value("slug").toString(id);project.author=data.value("author").toString(); project.name = data.value("title").toString(); project.description = data.value("description").toString();
        project.iconUrl = QUrl(data.value("icon_url").toString());
        project.downloads = static_cast<qint64>(data.value("downloads").toDouble());
        for (const auto &v : data.value("versions").toArray()) project.gameVersions.append(v.toString());
        for (const auto &c : data.value("categories").toArray()) project.categories.append(c.toString());
        for (const auto &c : data.value("display_categories").toArray()) if (!project.categories.contains(c.toString())) project.categories.append(c.toString());
        for (const auto &c : project.categories) if (QStringList{"forge", "fabric", "neoforge", "quilt", "liteloader", "risugamis-modloader"}.contains(c)) project.loaders.append(c);
        project.clientSide = data.value("client_side").toString(); project.serverSide = data.value("server_side").toString();
        results.append(project);
    }
    return results;
}
bool safeModFileName(const QString &name) {
    return !name.isEmpty() && name.endsWith(".jar", Qt::CaseInsensitive)
        && !name.contains('/') && !name.contains('\\') && !name.contains(':')
        && !name.contains(QChar::Null) && !name.contains(QChar('\n')) && !name.contains(QChar('\r'));
}
ResourceProject ModrinthProvider::project(const QJsonObject &data) {
    auto entry = data;
    entry.insert("project_id", data.value("id"));
    entry.insert("versions", data.value("game_versions"));
    const auto parsed = projects({{"hits", QJsonArray{entry}}});
    if (parsed.isEmpty()) return {};
    auto result = parsed.first(); result.body = data.value("body").toString();
    result.loaders.clear();
    for (const auto &loader : data.value("loaders").toArray()) result.loaders.append(loader.toString());
    return result;
}
QList<ResourceFile> ModrinthProvider::files(const QJsonArray &array,bool packs) {
    QList<ResourceFile> results;
    for (const auto &entry : array) {
        const auto version = entry.toObject();
        const auto rawFiles = version.value("files").toArray();
        if (rawFiles.isEmpty()) continue;
        auto selected = rawFiles.first().toObject();
        for (const auto &raw : rawFiles) if (raw.toObject().value("primary").toBool()) { selected = raw.toObject(); break; }
        ResourceFile file;
        for (const auto &v : version.value("game_versions").toArray()) file.gameVersions.append(v.toString());
        for (const auto &v : version.value("loaders").toArray()) file.loaders.append(v.toString());
        file.releaseType = version.value("version_type").toString();file.published=version.value("date_published").toString();
        file.projectId=version.value("project_id").toString();file.versionId = version.value("id").toString();
        file.versionName = version.value("name").toString();
        file.versionNumber = version.value("version_number").toString();
        file.fileName = selected.value("filename").toString();
        file.url = QUrl(selected.value("url").toString());
        file.size = static_cast<qint64>(selected.value("size").toDouble());
        file.sha512 = selected.value("hashes").toObject().value("sha512").toString();
        file.sha1 = selected.value("hashes").toObject().value("sha1").toString();
        for (const auto &dependency : version.value("dependencies").toArray()) {
            const auto dep = dependency.toObject();file.dependencies.append({dep.value("project_id").toString(),dep.value("version_id").toString(),dep.value("dependency_type").toString()});
            if (dep.value("dependency_type").toString() != "required") continue;
            const auto id = dep.value("project_id").toString(dep.value("version_id").toString());
            if (!id.isEmpty()) file.requiredDependencies.append(id);
        }
        if ((packs?(!file.fileName.contains('/')&&!file.fileName.contains('\\')&&!file.fileName.contains(':')&&file.fileName.endsWith(".zip",Qt::CaseInsensitive)):safeModFileName(file.fileName)) && file.url.scheme() == "https" && file.size > 0 && (!file.sha512.isEmpty() || !file.sha1.isEmpty()))
            results.append(file);
    }
    std::stable_sort(results.begin(),results.end(),[](const ResourceFile&a,const ResourceFile&b){const auto ad=QDateTime::fromString(a.published,Qt::ISODate),bd=QDateTime::fromString(b.published,Qt::ISODate);if(ad.isValid()!=bd.isValid())return ad.isValid();return ad.isValid()&&ad>bd;});
    return results;
}
QString resourceDownloadCount(qint64 count) {
    if(currentLanguage().startsWith("en")){
        const auto value=qMax(qint64(0),count);
        if(value>=1000000000)return QLocale().toString(value/1000000000.,'f',1)+"B";
        if(value>=1000000)return QLocale().toString(value/1000000.,'f',1)+"M";
        if(value>=1000)return QLocale().toString(value/1000.,'f',1)+"K";
        return QLocale().toString(value);
    }
    if(count>=100000000)return QString::number(count/100000000.,'f',2)+QCoreApplication::translate("M2CL", " 亿");
    if(count>=10000)return QString::number(count/10000.,'f',1)+QCoreApplication::translate("M2CL", " 万");
    return QString::number(qMax(qint64(0),count));
}
QString resourceTagLabel(const QString &id) {
    const QHash<QString, QString> labels{
        {"modloader", QCoreApplication::translate("M2CL", "旧版 ModLoader")}, {"release", QCoreApplication::translate("M2CL", "正式版")}, {"beta", QCoreApplication::translate("M2CL", "测试版")}, {"alpha", QCoreApplication::translate("M2CL", "预览版")},
        {"client", QCoreApplication::translate("M2CL", "客户端")}, {"server", QCoreApplication::translate("M2CL", "服务端")}, {"required", QCoreApplication::translate("M2CL", "必需")}, {"optional", QCoreApplication::translate("M2CL", "可选")}, {"unsupported", QCoreApplication::translate("M2CL", "不支持")},
        {"adventure", QCoreApplication::translate("M2CL", "冒险")}, {"cursed", QCoreApplication::translate("M2CL", "趣味与整活")}, {"decoration", QCoreApplication::translate("M2CL", "装饰与建筑")}, {"economy", QCoreApplication::translate("M2CL", "经济")},
        {"equipment", QCoreApplication::translate("M2CL", "装备")}, {"food", QCoreApplication::translate("M2CL", "食物")}, {"game-mechanics", QCoreApplication::translate("M2CL", "游戏机制")}, {"library", QCoreApplication::translate("M2CL", "前置与类库")},
        {"magic", QCoreApplication::translate("M2CL", "魔法")}, {"management", QCoreApplication::translate("M2CL", "管理")}, {"minigame", QCoreApplication::translate("M2CL", "小游戏")}, {"mobs", QCoreApplication::translate("M2CL", "生物")},
        {"optimization", QCoreApplication::translate("M2CL", "性能优化")}, {"social", QCoreApplication::translate("M2CL", "社交")}, {"storage", QCoreApplication::translate("M2CL", "存储")}, {"technology", QCoreApplication::translate("M2CL", "科技")},
        {"transportation", QCoreApplication::translate("M2CL", "交通")}, {"utility", QCoreApplication::translate("M2CL", "实用工具")}, {"worldgen", QCoreApplication::translate("M2CL", "世界生成")},
        {"forge", "Forge"}, {"fabric", "Fabric"}, {"neoforge", "NeoForge"}, {"quilt", "Quilt"}, {"liteloader", "LiteLoader"}
    };
    return labels.value(id, id);
}
QString resourceVersionFamily(const QString &version) {
    static const QRegularExpression stable("^(\\d+)\\.(\\d+)(?:\\.\\d+)*$");
    const auto match=stable.match(version);
    if(match.hasMatch()) return match.captured(1)+"."+match.captured(2);
    if(version.startsWith("b") || version.startsWith("a") || version.startsWith("inf") || version.startsWith("rd-") || version.startsWith("c0")) return QCoreApplication::translate("M2CL", "早期版本");
    if(version.contains("w") || version.contains("pre",Qt::CaseInsensitive) || version.contains("rc",Qt::CaseInsensitive) || version.contains("snapshot",Qt::CaseInsensitive)) return QCoreApplication::translate("M2CL", "快照 / 预览");
    return QCoreApplication::translate("M2CL", "其他版本");
}
QStringList resourceVersionFamilies(const QStringList &versions) {
    QStringList result;
    for(const auto &v:versions) { const auto family=resourceVersionFamily(v);if(!result.contains(family)) result.append(family); }
    std::stable_sort(result.begin(),result.end(),[](const QString &a,const QString &b){
        const auto av=QVersionNumber::fromString(a),bv=QVersionNumber::fromString(b);
        if(av.isNull()!=bv.isNull())return !av.isNull();
        return !av.isNull() && QVersionNumber::compare(av,bv)>0;
    });
    return result;
}
QString resourceVersionOverview(const QStringList &versions,int limit) {
    const auto families=resourceVersionFamilies(versions);if(families.isEmpty())return QCoreApplication::translate("M2CL", "未标注");
    return families.mid(0,limit).join("、")+(families.size()>limit?QCoreApplication::translate("M2CL", " 等 %1 个系列").arg(families.size()):QString());
}
QString resourceVersionSummary(const QStringList &versions, int limit) {
    if (versions.isEmpty()) return QCoreApplication::translate("M2CL", "未标注");
    return versions.mid(0, limit).join("、") + (versions.size() > limit ? QCoreApplication::translate("M2CL", " 等 %1 个版本").arg(versions.size()) : QString());
}
QList<ResourceTag> ModrinthProvider::categories(const QJsonArray &array,const QString &type) {
    QList<ResourceTag> result; QSet<QString> seen;
    for (const auto &entry : array) {
        const auto o = entry.toObject(); const auto id = o.value("name").toString();
        if (o.value("project_type") != type || id.isEmpty() || seen.contains(id)) continue;
        seen.insert(id); result.append({id, resourceTagLabel(id)});
    }
    return result;
}
QList<ResourceTag> ModrinthProvider::loaders(const QJsonArray &array) {
    QList<ResourceTag> result; QSet<QString> seen;
    for (const auto &entry : array) {
        const auto o = entry.toObject(); const auto id = o.value("name").toString();
        if (!o.value("supported_project_types").toArray().contains("mod") || id.isEmpty() || seen.contains(id)) continue;
        seen.insert(id); result.append({id, resourceTagLabel(id)});
    }
    const QStringList common{"forge", "neoforge", "fabric", "quilt"};
    std::stable_sort(result.begin(), result.end(), [&common](const ResourceTag &a, const ResourceTag &b) {
        const auto ai = common.indexOf(a.id), bi = common.indexOf(b.id);
        return (ai < 0 ? common.size() : ai) < (bi < 0 ? common.size() : bi);
    });
    return result;
}
QStringList ModrinthProvider::gameVersions(const QJsonArray &array) {
    QStringList result;
    for (const auto &entry : array) { const auto o = entry.toObject(); const auto id = o.value("version").toString(); if (!id.isEmpty() && !result.contains(id)) result.append(id); }
    return result;
}
bool ModrinthProvider::compatible(const ResourceFile &file, const QString &version, const QString &loader) {
    return !version.isEmpty() && QStringList{"forge","neoforge","fabric","quilt"}.contains(loader) && file.gameVersions.contains(version) && (file.loaders.contains(loader) || (loader == "quilt" && file.loaders.contains("fabric")));
}

}
