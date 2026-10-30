// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "networkerror.h"
#include "resourceclient.h"
#include "core/gameplan.h"
#include <QDir>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QTimer>

namespace mc {
ResourceClient::ResourceClient(QObject *parent, QNetworkAccessManager *network, int metadataTimeoutMs)
    : QObject(parent), m_network(this), m_manager(network ? network : &m_network), m_metadataTimeoutMs(qMax(1,metadataTimeoutMs)) {}
QNetworkReply *ResourceClient::metadataReply(const QUrl &url) {
    auto req=request(url);req.setTransferTimeout(0);
    auto reply=m_manager->get(req);
    QTimer::singleShot(m_metadataTimeoutMs,reply,[reply]{if(!reply->isFinished()){reply->setProperty("networkTimedOut",true);reply->abort();}});
    return reply;
}
QNetworkRequest ResourceClient::request(const QUrl &url) const {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "m2cl/0.1.0 (desktop development preview)");
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    return request;
}
void ResourceClient::search(const QString &text, const QString &version, const QString &loader, const QString &category, const QString &sort, int offset) {
    json(m_provider.searchUrl(text, version, loader, category, sort, offset), false);
}
void ResourceClient::icon(const QUrl &url) {
    if(url.scheme() != "https" || url.host() != "cdn.modrinth.com") return;
    if(auto bytes=m_iconCache.object(url.toString())) { emit iconReady(url,*bytes); return; }
    if(m_iconPending.contains(url) || m_iconPending.size() >= 40) return;
    m_iconPending.insert(url);auto req=request(url);req.setRawHeader("Accept","image/png,image/jpeg,image/webp");req.setTransferTimeout(15000);
    auto reply=m_manager->get(req);auto bytes=std::make_shared<QByteArray>();
    connect(reply,&QNetworkReply::readyRead,this,[reply,bytes]{
        if(reply->bytesAvailable() > 2*1024*1024-bytes->size()){reply->abort();return;}
        bytes->append(reply->readAll());
    });
    connect(reply,&QNetworkReply::finished,this,[this,reply,url,bytes]{
        m_iconPending.remove(url);reply->deleteLater();
        if(reply->error()!=QNetworkReply::NoError || reply->bytesAvailable()>2*1024*1024-bytes->size()) return;
        bytes->append(reply->readAll());if(bytes->isEmpty()) return;
        m_iconCache.insert(url.toString(),new QByteArray(*bytes),qMax(1,(bytes->size()+1023)/1024));emit iconReady(url,*bytes);
    });
}
void ResourceClient::projectIcons(const QStringList &projectIds) {
    auto ids=projectIds;ids.removeDuplicates();ids.removeAll({});
    for(int start=0;start<ids.size();start+=100){
        QJsonArray batch;for(const auto &id:ids.mid(start,100))batch.append(id);
        QUrl url("https://api.modrinth.com/v2/projects");QUrlQuery query;query.addQueryItem("ids",QString::fromUtf8(QJsonDocument(batch).toJson(QJsonDocument::Compact)));url.setQuery(query);
        auto reply=metadataReply(url);
        connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 size,qint64){if(size>4*1024*1024)reply->abort();});
        connect(reply,&QNetworkReply::finished,this,[this,reply,ids]{
            reply->deleteLater();if(reply->error()!=QNetworkReply::NoError)return;const auto bytes=reply->readAll();if(bytes.size()>4*1024*1024)return;
            const auto document=QJsonDocument::fromJson(bytes);if(!document.isArray())return;
            for(const auto &value:document.array()){const auto object=value.toObject();const auto id=object.value("id").toString();const QUrl url(object.value("icon_url").toString());if(ids.contains(id))emit projectIconReady(id,url);}
        });
    }
}
void ResourceClient::invalidateVersions(){++m_versionGeneration; ++m_projectGeneration;}
void ResourceClient::invalidateSearch() { ++m_searchGeneration; ++m_versionGeneration; ++m_projectGeneration; }
void ResourceClient::tags(bool refresh) {
    if(refresh) m_tagsLoaded = false;
    if (m_tagsLoaded) { emit tagsReady(m_tags); return; }
    if (m_tagsLoading) return;
    m_tagsLoading = true;
    emit tagsLoading();
    struct Batch { ResourceTags tags; int remaining = 3; QStringList errors; };
    const auto generation=m_tagGeneration;const auto type=m_provider.projectType;
    auto batch = std::make_shared<Batch>();
    batch->tags=m_tags;
    for (const auto &kind : {QString("category"), QString("loader"), QString("game_version")}) {
        auto reply = metadataReply(QUrl("https://api.modrinth.com/v2/tag/" + kind));
        connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 n, qint64) { if (n > 8 * 1024 * 1024) reply->abort(); });
        connect(reply, &QNetworkReply::finished, this, [this, reply, batch, kind,generation,type] {
            if(generation!=m_tagGeneration){reply->deleteLater();return;}
            reply->deleteLater(); QJsonParseError parse; const auto bytes = reply->readAll(); const auto data = QJsonDocument::fromJson(bytes, &parse);
            auto problem=networkReplyError(reply);
            if(problem.isEmpty() && (bytes.size()>8*1024*1024 || parse.error!=QJsonParseError::NoError || !data.isArray()))problem=QCoreApplication::translate("M2CL", "网站返回的筛选数据无法读取");
            const auto name=kind=="category"?QCoreApplication::translate("M2CL", "分类"):kind=="loader"?QCoreApplication::translate("M2CL", "加载器"):QCoreApplication::translate("M2CL", "游戏版本");
            if(!problem.isEmpty())batch->errors.append(name+QCoreApplication::translate("M2CL", "加载失败：")+problem);
            else if (kind == "category") batch->tags.categories = ModrinthProvider::categories(data.array(),type);
            else if (kind == "loader") batch->tags.loaders = ModrinthProvider::loaders(data.array());
            else {
                batch->tags.gameVersions = ModrinthProvider::gameVersions(data.array());
                batch->tags.gameVersionTypes.clear();
                for (const auto &entry : data.array()) {
                    const auto tag = entry.toObject();
                    batch->tags.gameVersionTypes.insert(tag.value("version").toString(), tag.value("version_type").toString());
                }
            }
            if (--batch->remaining) return;
            m_tagsLoading = false;
            m_tags = batch->tags;m_tagsLoaded=batch->errors.isEmpty();
            if(m_tagsLoaded || !m_tags.gameVersions.isEmpty() || !m_tags.categories.isEmpty() || !m_tags.loaders.isEmpty())emit tagsReady(m_tags);
            if(!batch->errors.isEmpty())emit tagsFailed(batch->errors.join(QStringLiteral("\n")));
        });
    }
}
void ResourceClient::versions(const QString &project, const QString &version, const QString &loader) {
    json(m_provider.versionsUrl(project, version, loader), true);
}
void ResourceClient::project(const QString &project) {
    const auto generation = ++m_projectGeneration;
    auto reply = metadataReply(QUrl("https://api.modrinth.com/v2/project/" + QString::fromLatin1(QUrl::toPercentEncoding(project))));
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) { if (received > 8 * 1024 * 1024) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater(); if (generation != m_projectGeneration) return;
        const auto bytes = reply->readAll(); QJsonParseError error;
        const auto document = QJsonDocument::fromJson(bytes, &error);
        const auto problem=networkReplyError(reply);
        if (!problem.isEmpty() || bytes.size() > 8 * 1024 * 1024 || error.error != QJsonParseError::NoError || !document.isObject()) { emit projectFailed(QCoreApplication::translate("M2CL", "项目介绍加载失败：")+(problem.isEmpty()?QCoreApplication::translate("M2CL", "网站返回的数据无法读取"):problem)); return; }
        const auto project = ModrinthProvider::project(document.object());
        if (project.id.isEmpty()) { emit projectFailed(QCoreApplication::translate("M2CL", "项目介绍数据不完整。")); return; }
        emit projectReady(project);
    });
}
void ResourceClient::json(const QUrl &url, bool versions) {
    const auto generation = versions ? ++m_versionGeneration : ++m_searchGeneration;
    auto reply = metadataReply(url);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > 8 * 1024 * 1024) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, versions, url]() {
        reply->deleteLater();
        if (generation != (versions ? m_versionGeneration : m_searchGeneration)) return;
        const auto fail=[this,versions](const QString &message){if(versions)emit versionsFailed(message);else emit searchFailed(message);emit failed(message);};
        const auto problem=networkReplyError(reply);
        if (!problem.isEmpty()) {
            fail(QCoreApplication::translate("M2CL", "Modrinth 请求失败：%1").arg(problem));
            return;
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(reply->readAll(), &error);
        if (error.error != QJsonParseError::NoError || (versions ? !document.isArray() : !document.isObject())) {
            fail(QCoreApplication::translate("M2CL", "Modrinth 返回的数据无法读取"));
            return;
        }
        if (versions) {
            auto files=ModrinthProvider::files(document.array(),m_provider.projectType!="mod"); const QUrlQuery query(url);
            const auto gameVersions=QJsonDocument::fromJson(query.queryItemValue("game_versions").toUtf8()).array();
            const auto loaders=QJsonDocument::fromJson(query.queryItemValue("loaders").toUtf8()).array();
            for(int i=files.size()-1;i>=0;--i) {
                bool versionMatch=gameVersions.isEmpty(),loaderMatch=loaders.isEmpty();
                for(const auto &v:gameVersions) if(files.at(i).gameVersions.contains(v.toString())) versionMatch=true;
                for(const auto &l:loaders) if(files.at(i).loaders.contains(l.toString())) loaderMatch=true;
                if(!versionMatch || !loaderMatch) files.removeAt(i);
            }
            emit versionsReady(files);
        }
        else { emit searchPageReady(document.object().value("total_hits").toInt(), document.object().value("offset").toInt()); emit projectsReady(ModrinthProvider::projects(document.object())); }
    });
}
void ResourceClient::reportDownloadError(const QString &message){emit downloadFailed(message);emit failed(message);}
void ResourceClient::download(const ResourceFile &file, const QString &directory) {
    if (downloading()) { reportDownloadError(QCoreApplication::translate("M2CL", "请等待当前下载完成，或先取消下载")); return; }
    if (!(m_provider.projectType=="mod"?safeModFileName(file.fileName):(safeRelativePath(file.fileName)&&!file.fileName.contains('/')&&file.fileName.endsWith(".zip",Qt::CaseInsensitive))) || file.url.scheme() != "https" || file.size <= 0 || (file.sha512.isEmpty() && file.sha1.isEmpty())) {
        reportDownloadError(QCoreApplication::translate("M2CL", "下载文件信息不完整")); return;
    }
    if (!QDir().mkpath(directory)) { reportDownloadError(QCoreApplication::translate("M2CL", "无法创建 Mod 目录")); return; }
    const auto destination = QDir(directory).filePath(file.fileName);
    m_file = std::make_unique<QSaveFile>(destination);
    if (!m_file->open(QIODevice::WriteOnly)) { reportDownloadError(m_file->errorString()); m_file.reset(); return; }
    const bool sha512 = !file.sha512.isEmpty();
    m_hash = std::make_unique<QCryptographicHash>(sha512 ? QCryptographicHash::Sha512 : QCryptographicHash::Sha1);
    m_received = 0;
    m_writeError.clear();
    auto downloadRequest = request(file.url);
    downloadRequest.setRawHeader("Accept", "application/octet-stream");
    auto reply = m_manager->get(downloadRequest);
    m_download = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, file]() {
        const auto problem=networkReplyError(reply);if(!problem.isEmpty()){m_writeError=problem;reply->abort();return;}
        const auto data = reply->readAll();
        if (data.size() > file.size - m_received) {
            m_writeError = QCoreApplication::translate("M2CL", "服务器返回的文件超过预期大小"); reply->abort(); return;
        }
        if (m_file->write(data) != data.size()) {
            m_writeError = m_file->errorString(); reply->abort(); return;
        }
        m_received += data.size();
        m_hash->addData(data);
    });
    connect(reply, &QNetworkReply::downloadProgress, this, &ResourceClient::downloadProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, destination, sha512]() {
        const auto tail = reply->readAll();
        if (!tail.isEmpty() && m_writeError.isEmpty()) {
            if (m_file->write(tail) != tail.size()) m_writeError = m_file->errorString();
            m_received += tail.size();
            m_hash->addData(tail);
        }
        QString error = m_writeError;
        if (error.isEmpty())error=networkReplyError(reply,reply->error()==QNetworkReply::OperationCanceledError&&!reply->property("networkCancelled").toBool());
        if (error.isEmpty() && m_received != file.size) error = QCoreApplication::translate("M2CL", "文件大小校验失败，请重试");
        const auto expected = (sha512 ? file.sha512 : file.sha1).toLatin1().toLower();
        if (error.isEmpty() && m_hash->result().toHex() != expected) error = QCoreApplication::translate("M2CL", "文件哈希校验失败，请重试");
        if (error.isEmpty() && !m_file->commit()) error = m_file->errorString();
        if (!error.isEmpty()) m_file->cancelWriting();
        m_file.reset(); m_hash.reset(); m_download.clear(); reply->deleteLater();
        if (error.isEmpty()) emit downloadFinished(destination); else reportDownloadError(error);
    });
}
void ResourceClient::cancelDownload() { if (m_download){m_download->setProperty("networkCancelled",true);m_download->abort();} }
}
