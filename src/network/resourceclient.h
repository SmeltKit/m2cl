// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/resources.h"
#include <QCryptographicHash>
#include <QCache>
#include <QSet>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSaveFile>
#include <memory>

namespace mc {
class ResourceClient : public QObject {
    Q_OBJECT
public:
    explicit ResourceClient(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr, int metadataTimeoutMs = 15000);
    void setResourceType(const QString &type){const auto next=(type=="shader"||type=="resourcepack")?type:QString("mod");if(next==m_provider.projectType)return;invalidateSearch();invalidateVersions();m_provider.projectType=next;m_tagsLoaded=false;m_tagsLoading=false;++m_tagGeneration;m_tags.categories.clear();}
    void search(const QString &text, const QString &version, const QString &loader, const QString &category = {}, const QString &sort = "relevance", int offset = 0);
    void tags(bool refresh = false);
    void icon(const QUrl &url);
    void projectIcons(const QStringList &projectIds);
    void invalidateSearch();
    void invalidateVersions();
    void versions(const QString &project, const QString &version, const QString &loader);
    void project(const QString &project);
    void download(const ResourceFile &file, const QString &directory);
    void cancelDownload();
    bool downloading() const { return !m_download.isNull(); }
signals:
    void tagsLoading();
    void iconReady(const QUrl &url, const QByteArray &bytes);
    void projectIconReady(const QString &projectId, const QUrl &url);
    void tagsReady(const mc::ResourceTags &tags);
    void tagsFailed(const QString &message);
    void searchPageReady(int total, int offset);
    void projectsReady(const QList<mc::ResourceProject> &projects);
    void versionsReady(const QList<mc::ResourceFile> &files);
    void projectReady(const mc::ResourceProject &project);
    void projectFailed(const QString &message);
    void searchFailed(const QString &message);
    void versionsFailed(const QString &message);
    void downloadFailed(const QString &message);
    void failed(const QString &message);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &path);
private:
    int m_tagGeneration=0;
    QNetworkRequest request(const QUrl &url) const;
    QNetworkReply *metadataReply(const QUrl &url);
    void json(const QUrl &url, bool versions);
    void reportDownloadError(const QString &message);
    QNetworkAccessManager m_network;
    QNetworkAccessManager *m_manager;
    int m_metadataTimeoutMs;
    ModrinthProvider m_provider;
    quint64 m_searchGeneration = 0, m_versionGeneration = 0, m_projectGeneration = 0;
    bool m_tagsLoading = false, m_tagsLoaded = false;
    ResourceTags m_tags;
    QCache<QString,QByteArray> m_iconCache{8192};
    QSet<QUrl> m_iconPending;
    QPointer<QNetworkReply> m_download;
    std::unique_ptr<QSaveFile> m_file;
    std::unique_ptr<QCryptographicHash> m_hash;
    QString m_writeError;
    qint64 m_received = 0;
};
}
