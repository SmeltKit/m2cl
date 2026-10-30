// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QMap>
#include <QStringList>
#include <QUrl>

namespace mc {
struct ResourceProject {
    QString id, name, description, slug, author, body; qint64 downloads = 0;
    QStringList gameVersions, categories, loaders;
    QString clientSide, serverSide;
    QUrl iconUrl;
};
struct ResourceTag { QString id, label; };
struct ResourceTags { QList<ResourceTag> categories, loaders; QStringList gameVersions; QMap<QString, QString> gameVersionTypes; };
QString gameVersionChannel(const QString &id, const QString &type = {});
QString resourceDownloadCount(qint64 count);
QString resourceTagLabel(const QString &id);
QString resourceVersionFamily(const QString &version);
QStringList resourceVersionFamilies(const QStringList &versions);
QString resourceVersionOverview(const QStringList &versions, int limit = 4);
QString resourceVersionSummary(const QStringList &versions, int limit = 4);
struct ResourceDependency { QString projectId, versionId, type; };
struct ResourceFile {
    QString versionId, versionName, versionNumber, fileName, sha512, sha1;
    QString projectId;
    QUrl url;
    qint64 size = 0;
    QStringList requiredDependencies, gameVersions, loaders;
    QString releaseType, published;
    QList<ResourceDependency> dependencies;
};
class ResourceProvider {
public:
    virtual ~ResourceProvider() = default;
    virtual QString name() const = 0;
    virtual QUrl searchUrl(const QString &query, const QString &gameVersion, const QString &loader, const QString &category = {}, const QString &sort = "relevance", int offset = 0) const = 0;
    virtual QUrl versionsUrl(const QString &project, const QString &gameVersion, const QString &loader) const = 0;
};
class ModrinthProvider final : public ResourceProvider {
public:
    QString projectType="mod";
    QString name() const override { return QStringLiteral("Modrinth"); }
    QUrl searchUrl(const QString &query, const QString &gameVersion, const QString &loader, const QString &category = {}, const QString &sort = "relevance", int offset = 0) const override;
    QUrl versionsUrl(const QString &project, const QString &gameVersion, const QString &loader) const override;
    static QList<ResourceProject> projects(const QJsonObject &object);
    static ResourceProject project(const QJsonObject &object);
    static QList<ResourceFile> files(const QJsonArray &array, bool packs=false);
    static QList<ResourceTag> categories(const QJsonArray &array,const QString &type="mod");
    static QList<ResourceTag> loaders(const QJsonArray &array);
    static QStringList gameVersions(const QJsonArray &array);
    static bool compatible(const ResourceFile &file, const QString &version, const QString &loader);
};
bool safeModFileName(const QString &fileName);
}
Q_DECLARE_METATYPE(mc::ResourceProject)
Q_DECLARE_METATYPE(QList<mc::ResourceProject>)
Q_DECLARE_METATYPE(mc::ResourceFile)
Q_DECLARE_METATYPE(QList<mc::ResourceFile>)

Q_DECLARE_METATYPE(mc::ResourceTags)
