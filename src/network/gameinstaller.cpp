// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "networkerror.h"
#include "gameinstaller.h"
#include "forgeinstaller.h"
#include "optifine.h"
#include "downloadsource.h"
#include "offlineskin.h"
#include "core/java.h"
#include "core/catalog.h"
#include "core/memory.h"
#include "core/mods.h"
#include <QRegularExpression>
#include <QSet>
#include <QVersionNumber>
#include <algorithm>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTimer>
#include <stdexcept>

namespace mc {
namespace {
const QString versionsUrl = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
const QString runtimeUrl = "https://piston-meta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json";
void fail(const QString &message) { throw std::runtime_error(message.toUtf8().constData()); }
QJsonObject object(const QByteArray &bytes) {
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) fail(QCoreApplication::translate("M2CL", "官方元数据格式无效"));
    return doc.object();
}
QByteArray read(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32 * 1024 * 1024) fail(QCoreApplication::translate("M2CL", "无法读取元数据：%1").arg(path));
    return file.readAll();
}
void write(const QString &path, const QByteArray &bytes) {
    if (!QDir().mkpath(QFileInfo(path).path())) fail(QCoreApplication::translate("M2CL", "无法创建目录：%1").arg(path));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) fail(QCoreApplication::translate("M2CL", "无法保存：%1").arg(path));
}
class Worker {
public:
    QNetworkAccessManager ownedNetwork;
    QNetworkAccessManager &network;
    CancelFlag cancelled;
    InstallProgress progress;
    QString source;
    Worker(CancelFlag flag, InstallProgress callback = {}, QNetworkAccessManager *manager = nullptr, const QString &downloadSource = "official")
        : network(manager ? *manager : ownedNetwork), cancelled(std::move(flag)), progress(std::move(callback)), source(resolveDownloadSource(downloadSource)) {}
    void check() const { if (cancelled->load()) fail(QCoreApplication::translate("M2CL", "安装已取消，可重新操作以继续")); }
    void report(const QString &message, int value = -1) { check(); if (progress) progress(message, value); }
    QNetworkRequest request(const QUrl &url) {
        if (url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty()) fail(QCoreApplication::translate("M2CL", "下载地址无效"));
        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", "m2cl/0.1.0");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(30000);
        return req;
    }
    QByteArray fetch(const QUrl &url, const QString &sha1 = {}) {
        QString error; QStringList errors;
        const auto candidates = downloadCandidates(url, source);
        for (int attempt = 0; attempt < candidates.size(); ++attempt) {
            check();
            QEventLoop loop;
            QTimer timer; QElapsedTimer elapsed;elapsed.start();
            QByteArray bytes;
            const auto candidate = candidates.at(attempt);
            report(QStringLiteral("%1%2 · %3").arg(attempt ? QCoreApplication::translate("M2CL", "切换备用源：") : QCoreApplication::translate("M2CL", "请求："), downloadSourceLabel(candidate), candidate.host()));
            check();auto reply = network.get(request(candidate));
            QObject::connect(&timer, &QTimer::timeout, &loop, [&] { if (cancelled->load() || elapsed.elapsed()>60000) reply->abort(); });
            QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&] {
                bytes += reply->readAll();
                if (bytes.size() > 32 * 1024 * 1024) reply->abort();
            });
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            timer.start(100); loop.exec();
            bytes += reply->readAll();
            error = networkReplyError(reply,elapsed.elapsed()>60000||(reply->error()==QNetworkReply::OperationCanceledError&&!cancelled->load()));
            reply->deleteLater(); check();
            if (bytes.size() > 32 * 1024 * 1024) error = QCoreApplication::translate("M2CL", "元数据超过大小限制");
            if (error.isEmpty() && !sha1.isEmpty() && QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex() != sha1.toLatin1())
                error = QCoreApplication::translate("M2CL", "元数据 SHA-1 校验失败");
            if (error.isEmpty()) return bytes;
            errors.append(downloadSourceLabel(candidate)+" · "+candidate.host()+"："+error);
        }
        fail(QCoreApplication::translate("M2CL", "请求失败：\n%1").arg(errors.join("\n"))); return {};
    }
    bool valid(const QString &path, const GameFile &file) {
        check(); QFile input(path);
        if (!input.open(QIODevice::ReadOnly) || input.size() != file.size) return false;
        QCryptographicHash hash(QCryptographicHash::Sha1);
        while (!input.atEnd()) { check(); hash.addData(input.read(1024 * 1024)); }
        return hash.result().toHex() == file.sha1.toLatin1();
    }
    void files(const QList<GameFile> &files, const QString &root, const QString &stage) {
        // Eight streaming requests at a time; all file I/O runs outside the GUI thread.
        int done = 0;
        for (int base = 0; base < files.size(); base += 8) {
            check();
            QEventLoop loop; QTimer timer;
            struct Transfer {
                GameFile file; QString path, error; QList<QUrl> candidates; QElapsedTimer elapsed; int attempts = 0; qint64 received = 0;
                std::unique_ptr<QSaveFile> output;
                std::unique_ptr<QCryptographicHash> hash;
                QNetworkReply *reply = nullptr;
            };
            QList<std::shared_ptr<Transfer>> transfers;
            int pending = 0; QString fatal;
            std::function<void(std::shared_ptr<Transfer>)> start;
            start = [&](std::shared_ptr<Transfer> t) {
                ++t->attempts; t->elapsed.start(); t->received = 0; t->error.clear();
                t->output = std::make_unique<QSaveFile>(t->path);
                t->hash = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha1);
                if (!QDir().mkpath(QFileInfo(t->path).path()) || !t->output->open(QIODevice::WriteOnly)) {
                    fatal = QCoreApplication::translate("M2CL", "无法保存文件：%1").arg(t->path); loop.quit(); return;
                }
                const auto candidate = t->candidates.at(t->attempts - 1);
                if(progress)progress(QStringLiteral("%1%2 · %3").arg(t->attempts > 1 ? QCoreApplication::translate("M2CL", "切换备用源：") : stage + " · ", downloadSourceLabel(candidate), candidate.host()),-1);
                if(cancelled->load()){fatal=QCoreApplication::translate("M2CL", "安装已取消，可重新操作以继续");loop.quit();return;}
                t->reply = network.get(request(candidate));
                auto consume = [t] {
                    const auto problem=networkReplyError(t->reply);if(!problem.isEmpty()){t->error=problem;t->reply->abort();return;}
                    const auto data = t->reply->readAll();
                    if (data.size() > t->file.size - t->received) t->error = QCoreApplication::translate("M2CL", "文件超过预期大小");
                    else if (t->output->write(data) != data.size()) t->error = t->output->errorString();
                    else { t->received += data.size(); t->hash->addData(data); }
                    if (!t->error.isEmpty()) t->reply->abort();
                };
                QObject::connect(t->reply, &QNetworkReply::readyRead, &loop, consume);
                QObject::connect(t->reply, &QNetworkReply::finished, &loop, [&, t, consume] {
                    if (t->error.isEmpty()) consume();
                    auto reply = t->reply;
                    if (t->error.isEmpty())t->error=networkReplyError(reply,t->elapsed.elapsed()>120000||(reply->error()==QNetworkReply::OperationCanceledError&&!cancelled->load()));
                    if (t->error.isEmpty() && (t->received != t->file.size || t->hash->result().toHex() != t->file.sha1.toLatin1()))
                        t->error = QCoreApplication::translate("M2CL", "文件大小或 SHA-1 校验失败");
                    t->reply = nullptr; reply->deleteLater();
                    if (t->error.isEmpty() && !t->output->commit()) t->error = t->output->errorString();
                    if (!t->error.isEmpty()) {
                        t->output->cancelWriting();
                        if (!cancelled->load() && t->attempts < t->candidates.size()) { start(t); return; }
                        fatal = QStringLiteral("%1：%2").arg(t->file.path + " · " + downloadSourceLabel(t->candidates.at(t->attempts-1)), t->error); loop.quit(); return;
                    }
                    if (t->file.executable) QFile::setPermissions(t->path, QFile::permissions(t->path) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
                    ++done; --pending;
                    if (progress) progress(QStringLiteral("%1 · %2 / %3").arg(stage).arg(done).arg(files.size()), files.isEmpty() ? 1000 : done * 1000 / files.size());
                    if (!pending) loop.quit();
                });
            };
            for (int i = base; i < qMin(base + 8, int(files.size())); ++i) {
                const auto &f = files.at(i); const auto path = checkedPath(root, f.path);
                if (valid(path, f)) {
                    if (f.executable) QFile::setPermissions(path, QFile::permissions(path) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
                    ++done; continue;
                }
                auto t = std::make_shared<Transfer>(); t->file = f; t->path = path; t->candidates = downloadCandidates(f.url, source); transfers.append(t); ++pending;
            }
            for (const auto &t : transfers) { start(t); if (!fatal.isEmpty()) break; }
            QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
                if (cancelled->load()) { fatal = QCoreApplication::translate("M2CL", "安装已取消，可重新操作以继续"); loop.quit(); return; }
                for(const auto &t:transfers)if(t->reply && t->elapsed.elapsed()>120000)t->reply->abort();
            });
            if (pending && fatal.isEmpty()) { timer.start(100); loop.exec(); }
            for (const auto &t : transfers) if (t->reply) { t->reply->disconnect(&loop); t->reply->abort(); t->reply->deleteLater(); t->reply = nullptr; }
            check(); if (!fatal.isEmpty()) fail(fatal);
            report(QStringLiteral("%1 · %2 / %3").arg(stage).arg(done).arg(files.size()), files.isEmpty() ? 1000 : done * 1000 / files.size());
        }
    }
    JavaRuntime java(const InstallRequest &req, const QJsonObject &metadata, const GamePlan &plan, const GamePlatform &platform) {
        report(QCoreApplication::translate("M2CL", "正在匹配 Java %1…").arg(plan.javaMajor));
        auto roots = defaultJavaRoots({req.root}); roots.append(req.javaRoots);
        if (!req.java.isEmpty()) roots.prepend(req.java);

        const auto managedRoot = checkedPath(req.root, "runtime/linjian");
        auto complete = [&](const JavaRuntime &j) {
            if (!j.executable.startsWith(managedRoot + "/")) return true;
            const auto parts = QDir(managedRoot).relativeFilePath(j.executable).split('/');
            return parts.size() >= 3 && QFileInfo::exists(checkedPath(managedRoot, parts.at(0) + "/" + parts.at(1) + "/.linjian-complete"));
        };
        if (req.javaChoice.value("mode") == "fixed" && req.java.isEmpty()) fail(QCoreApplication::translate("M2CL", "实例固定 Java 尚未选择可执行文件，请修改 Java 设置"));
        if (!req.java.isEmpty()) {
            const auto j = probeJava(req.java, cancelled); check();
            if (!j.major || !complete(j)) fail(QCoreApplication::translate("M2CL", "固定的 Java 无法运行或安装未完成，请在实例 Java 设置中重新选择"));
            if(j.major<plan.javaMajor&&!req.javaChoice.value("allowBelowRecommended").toBool())fail(QCoreApplication::translate("M2CL", "所选 Java %1 低于游戏默认 Java %2；可在实例设置中主动启用低版本强制使用").arg(j.major).arg(plan.javaMajor));
            const auto pin = javaPin(j);
            for (const auto &key : {"runtimeVersion", "vendor", "vendorVersion", "arch"}) {
                const auto expected = req.javaChoice.value(key).toString();
                if (!expected.isEmpty() && pin.value(key).toString() != expected) fail(QCoreApplication::translate("M2CL", "固定 Java 的构建 / 发行版或架构已变化，请重新确认实例 Java 设置"));
            }
            return j;
        }
        const auto runtimes = discoverJava(roots, cancelled); check();
        for (const auto &j : runtimes) if (complete(j) && j.major == plan.javaMajor && j.architecture == platform.arch) return j;
        const auto component = metadata.value("javaVersion").toObject().value("component").toString();
        if (component.isEmpty()) fail(QCoreApplication::translate("M2CL", "没有匹配的 Java %1，请在设置中添加 Java 目录").arg(plan.javaMajor));
        QString key;
        if (platform.os == "osx") key = platform.arch == "aarch64" ? "mac-os-arm64" : "mac-os";
        else if (platform.os == "windows") key = platform.arch == "aarch64" ? "windows-arm64" : platform.arch == "x86" ? "windows-x86" : "windows-x64";
        else if (platform.arch == "x86_64" || platform.arch == "x86") key = platform.arch == "x86" ? "linux-i386" : "linux";
        Worker runtimeWorker(cancelled,progress,&network,"official");
        const auto choices = object(runtimeWorker.fetch(QUrl(runtimeUrl))).value(key).toObject().value(component).toArray();
        if (choices.isEmpty()) fail(QCoreApplication::translate("M2CL", "官方未提供此平台的 Java %1，请在设置中添加兼容 Java").arg(plan.javaMajor));
        report(QCoreApplication::translate("M2CL", "正在下载官方 Java %1…").arg(plan.javaMajor));
        const auto descriptor = choices.first().toObject().value("manifest").toObject();
        const auto manifest = object(runtimeWorker.fetch(QUrl(descriptor.value("url").toString()), descriptor.value("sha1").toString())).value("files").toObject();
        const auto runtimeRoot = checkedPath(req.root, "runtime/linjian/" + component + "/" + key);
        QList<GameFile> downloads; QString executable;
        for (auto it = manifest.begin(); it != manifest.end(); ++it) {
            checkedPath(runtimeRoot, it.key());
            const auto entry = it.value().toObject();
            if (entry.value("type") == "file") {
                auto f = gameFile(entry.value("downloads").toObject().value("raw").toObject(), it.key());
                f.executable = entry.value("executable").toBool(); downloads.append(f);
                if (it.key() == "bin/java" || it.key() == "bin/java.exe" || it.key().endsWith("/Home/bin/java")) executable = checkedPath(runtimeRoot, it.key());
            } else if (entry.value("type") == "directory") QDir().mkpath(checkedPath(runtimeRoot, it.key()));
        }
        runtimeWorker.files(downloads, runtimeRoot, QCoreApplication::translate("M2CL", "下载 Java"));
        // Materialize links as copies, resolving only files inside the verified runtime tree.
        for (int pass = 0; pass < 16; ++pass) {
            bool unresolved = false;
            for (auto it = manifest.begin(); it != manifest.end(); ++it) {
                const auto entry = it.value().toObject(); if (entry.value("type") != "link") continue;
                const auto target = QDir::cleanPath(QFileInfo(it.key()).path() + "/" + entry.value("target").toString());
                const auto source = checkedPath(runtimeRoot, target), destination = checkedPath(runtimeRoot, it.key());
                if (QFileInfo::exists(destination)) continue;
                if (!manifest.contains(target) || !QFileInfo(source).isFile()) { unresolved = true; continue; }
                write(destination, read(source)); QFile::setPermissions(destination, QFile::permissions(source));
            }
            if (!unresolved) break;
            if (pass == 15) fail(QCoreApplication::translate("M2CL", "Java 运行时链接无效"));
        }
        const auto installed = discoverJava({executable}, cancelled); check();
        if (installed.isEmpty() || installed.first().major != plan.javaMajor || installed.first().architecture != platform.arch)
            fail(QCoreApplication::translate("M2CL", "下载的 Java 无法运行或版本 / 架构不匹配"));
        write(checkedPath(runtimeRoot, ".linjian-complete"), "verified\n");
        return installed.first();
    }
};
}
QByteArray fetchInstallData(const QUrl &url, const CancelFlag &cancelled, const QString &sha1, QNetworkAccessManager *network, const QString &source, const InstallProgress &progress) {
    Worker worker(cancelled, progress, network, source); return worker.fetch(url, sha1);
}
void downloadVerifiedGameFiles(const QList<GameFile> &files, const QString &root, const CancelFlag &cancelled, const InstallProgress &progress, QNetworkAccessManager *network, const QString &source) {
    Worker worker(cancelled, progress, network, source); worker.files(files, root, QCoreApplication::translate("M2CL", "下载文件"));
}
QJsonObject mergeGameMetadata(const QJsonObject &base,const QJsonObject &child){
    auto result=base;
    for(auto it=child.begin();it!=child.end();++it)if(it.key()!="libraries"&&it.key()!="arguments"&&it.key()!="inheritsFrom")result.insert(it.key(),it.value());
    QJsonArray libraries;QSet<QString> replaced;
    auto key=[](const QJsonObject &lib){auto parts=lib.value("name").toString().split(':');if(parts.size()>2)parts.removeAt(2);return parts.join(':');};
    for(const auto &v:child.value("libraries").toArray()){libraries.append(v);replaced.insert(key(v.toObject()));}
    for(const auto &v:base.value("libraries").toArray())if(!replaced.contains(key(v.toObject())))libraries.append(v);
    result.insert("libraries",libraries);auto arguments=base.value("arguments").toObject();
    const auto childArgs=child.value("arguments").toObject();for(auto it=childArgs.begin();it!=childArgs.end();++it){auto values=arguments.value(it.key()).toArray();for(const auto &v:it.value().toArray())values.append(v);arguments.insert(it.key(),values);}result.insert("arguments",arguments);
    result.insert("_baseId",base.value("_baseId").toString(base.value("id").toString()));result.remove("inheritsFrom");return result;
}
VersionList fetchFabricVersions(const QString &gameVersion,const CancelFlag &cancelled,const QString &source){
    try{Worker worker(cancelled,{},nullptr,source);auto doc=QJsonDocument::fromJson(worker.fetch(QUrl("https://meta.fabricmc.net/v2/versions/loader/"+QString::fromLatin1(QUrl::toPercentEncoding(gameVersion)))));if(!doc.isArray())fail(QCoreApplication::translate("M2CL", "Fabric 版本列表无效"));return {doc.array(),{}};}catch(const std::exception &e){return {{},QString::fromUtf8(e.what())};}
}
VersionList fetchQuiltVersions(const QString &gameVersion, const CancelFlag &cancelled, const QString &source) {
    try {
        Worker worker(cancelled,{},nullptr,source);
        const auto doc=QJsonDocument::fromJson(worker.fetch(QUrl("https://meta.quiltmc.org/v3/versions/loader/"+QString::fromLatin1(QUrl::toPercentEncoding(gameVersion)))));
        if(!doc.isArray())fail(QCoreApplication::translate("M2CL", "Quilt 版本列表无效"));
        QList<QJsonObject> entries;
        for(const auto &value:doc.array()) {
            auto entry=value.toObject();auto loader=entry.value("loader").toObject();
            const auto version=loader.value("version").toString();if(version.isEmpty())continue;
            // Quilt Meta has no Fabric-style stable field.
            loader.insert("stable",QRegularExpression("^\\d+\\.\\d+\\.\\d+$").match(version).hasMatch());
            entry.insert("loader",loader);entries.append(entry);
        }
        std::stable_sort(entries.begin(),entries.end(),[](const QJsonObject &a,const QJsonObject &b){
            const auto x=a.value("loader").toObject(),y=b.value("loader").toObject();
            if(x.value("stable")!=y.value("stable"))return x.value("stable").toBool();
            return QVersionNumber::fromString(x.value("version").toString())>QVersionNumber::fromString(y.value("version").toString());
        });
        QJsonArray result;for(const auto &entry:entries)result.append(entry);return {result,{}};
    }catch(const std::exception &e){return {{},QString::fromUtf8(e.what())};}
}
VersionList fetchGameVersions(const CancelFlag &cancelled, const QString &source) {
    try { Worker worker(cancelled,{},nullptr,source); return {object(worker.fetch(QUrl(versionsUrl))).value("versions").toArray(), {}}; }
    catch (const std::exception &e) { return {{}, QString::fromUtf8(e.what())}; }
}
InstallResult prepareGame(const InstallRequest &input, const CancelFlag &cancelled, const InstallProgress &progress, QNetworkAccessManager *network) {
    auto req=input;req.downloadSource=resolveDownloadSource(req.downloadSource);
    InstallResult result;
    try {
        auto launch=req.launchOptions;launch.maxMemoryMiB=req.memoryMiB;const auto launchError=validateLaunchOptions(launch);if(!launchError.isEmpty())fail(launchError);
        Worker worker(cancelled, progress, network, req.downloadSource);
        OptifineChange optifineChange;
        if (!safeRelativePath(req.id) || req.id.contains('/')) fail(QCoreApplication::translate("M2CL", "实例 ID 无效"));
        const auto path = checkedPath(req.root, "versions/" + req.id + "/" + req.id + ".json");
        const auto originalMetadata=QFileInfo::exists(path)?read(path):QByteArray();
        QJsonObject metadata;
        if (!req.versionEntry.isEmpty()) {
            worker.report(QCoreApplication::translate("M2CL", "正在获取版本元数据…"));
            const auto bytes = worker.fetch(QUrl(req.versionEntry.value("url").toString()), req.versionEntry.value("sha1").toString());
            metadata = object(bytes);
            if (metadata.value("id").toString() != req.version) fail(QCoreApplication::translate("M2CL", "版本元数据 ID 不匹配"));
            // Preserve the original game ID; the directory ID is launcher-owned.
            // Cache verified vanilla metadata for interrupted-install recovery; never overwrite an existing instance here.
            if(!QFileInfo::exists(path))write(path,QJsonDocument(metadata).toJson());
        } else metadata = object(read(path));
        QSet<QString> parents;
        std::function<QJsonObject(QJsonObject)> resolve=[&](QJsonObject own){
            const auto parent=own.value("inheritsFrom").toString();if(parent.isEmpty())return own;
            if(!safeRelativePath(parent)||parent.contains('/')||parents.contains(parent)||parents.size()>=16)fail(QCoreApplication::translate("M2CL", "版本继承无效或存在循环"));parents.insert(parent);
            const auto parentPath=checkedPath(req.root,"versions/"+parent+"/"+parent+".json");QJsonObject base;
            if(QFileInfo::exists(parentPath))base=object(read(parentPath));else{
                const auto versions=object(worker.fetch(QUrl(versionsUrl))).value("versions").toArray();
                for(const auto &value:versions){auto entry=value.toObject();if(entry.value("id")==parent){base=object(worker.fetch(QUrl(entry.value("url").toString()),entry.value("sha1").toString()));break;}}
                if(base.value("id")!=parent)fail(QCoreApplication::translate("M2CL", "找不到继承的游戏版本：%1").arg(parent));
            }
            return mergeGameMetadata(resolve(base),own);
        };
        metadata=resolve(metadata);
        const auto components=gameLoaderComponents(metadata);
        if(components.size()>1)fail(QCoreApplication::translate("M2CL", "实例含多个主加载器，请先修复版本组件"));
        QString currentLoader=components.isEmpty()?QString("vanilla"):components.firstKey();
        if(!metadata.value("_m2clOptifine").isObject())for(const auto &v:metadata.value("libraries").toArray())if(v.toObject().value("name").toString().startsWith("optifine:"))fail(QCoreApplication::translate("M2CL", "外部 OptiFine 版本请重新创建受管理的实例，不能直接覆盖启动配置"));
        if(metadata.contains("_m2clOptifine")&&!req.loader.isEmpty())fail(QCoreApplication::translate("M2CL", "请先移除 OptiFine，再升级主加载器并重新匹配 OptiFine"));
        if(req.optifineAction.isEmpty())verifyOptifine(metadata,req.root,req.directory);
        const QStringList supported{"vanilla","fabric","quilt","forge","neoforge"};
        if(!supported.contains(currentLoader)||(!req.loader.isEmpty()&&!supported.contains(req.loader)))fail(QCoreApplication::translate("M2CL", "暂不支持此加载器的安装或启动"));
        if(!req.loader.isEmpty()&&currentLoader!="vanilla"&&req.loader!=currentLoader)fail(QCoreApplication::translate("M2CL", "不能直接更换已有实例的主加载器，请创建独立实例"));
        if(req.loader=="forge"||req.loader=="neoforge"){
            auto base=metadata.value("_m2clVanilla").toObject();if(base.isEmpty()){if(currentLoader!="vanilla")fail(QCoreApplication::translate("M2CL", "外部加载器实例请先导入原版配置，不能覆盖升级"));base=metadata;}
            GamePlan requirements;requirements.javaMajor=base.value("javaVersion").toObject().value("majorVersion").toInt(requiredJavaMajor(base.value("id").toString()));
            const auto java=worker.java(req,base,requirements,GamePlatform::current());
            const auto profile=installForgeClient(req,base,java.executable,cancelled,progress,network);
            metadata=mergeGameMetadata(base,profile);metadata.insert("_m2clVanilla",base);currentLoader=req.loader;
        }
        if(currentLoader=="forge"||currentLoader=="neoforge")verifyForgeOutputs(metadata,req.root);
        if(req.loader=="fabric"||req.loader=="quilt"){
            const bool quilt=req.loader=="quilt";
            const auto title=quilt?QString("Quilt"):QString("Fabric");
            if(req.loaderVersion.isEmpty())fail(QCoreApplication::translate("M2CL", "请选择 %1 Loader 版本").arg(title));
            const auto baseVersion=metadata.value("_baseId").toString(metadata.value("id").toString());
            const auto profile=object(worker.fetch(QUrl((quilt?QString("https://meta.quiltmc.org/v3/versions/loader/"):QString("https://meta.fabricmc.net/v2/versions/loader/"))+QString::fromLatin1(QUrl::toPercentEncoding(baseVersion))+"/"+QString::fromLatin1(QUrl::toPercentEncoding(req.loaderVersion))+"/profile/json")));
            if(profile.value("inheritsFrom")!=baseVersion||profile.value("mainClass")!=(quilt?"org.quiltmc.loader.impl.launch.knot.KnotClient":"net.fabricmc.loader.impl.launch.knot.KnotClient") || gameLoaderComponents(profile)!=QMap<QString,QString>{{req.loader,req.loaderVersion}})fail(QCoreApplication::translate("M2CL", "%1 官方启动配置不匹配").arg(title));
            // Rebuild from the original vanilla document when upgrading, so old loader libraries/arguments disappear.
            auto base=metadata.value("_m2clVanilla").toObject();if(base.isEmpty()){if(currentLoader!="vanilla")fail(QCoreApplication::translate("M2CL", "外部 %1 实例请保留原版本，当前仅支持升级由 M²CL 安装的加载器").arg(title));base=metadata;}
            metadata=mergeGameMetadata(base,profile);metadata.insert("_m2clVanilla",base);currentLoader=req.loader;
        }
        // Fabric Maven entries do not always include the standard downloads object.
        QJsonArray normalized;for(const auto &value:metadata.value("libraries").toArray()){
            auto lib=value.toObject();if(lib.value("downloads").toObject().isEmpty()){
                const auto parts=lib.value("name").toString().split(':');if(parts.size()<3||parts.size()>4)fail(QCoreApplication::translate("M2CL", "无效的 Maven 库名称"));
                auto group=parts[0];group.replace('.','/');const auto relative=group+"/"+parts[1]+"/"+parts[2]+"/"+parts[1]+"-"+parts[2]+(parts.size()==4?"-"+parts[3]:QString())+".jar";
                if(!safeRelativePath(relative))fail(QCoreApplication::translate("M2CL", "无效的 Maven 库路径"));const QUrl baseUrl(lib.value("url").toString("https://libraries.minecraft.net/"));const auto url=baseUrl.resolved(QUrl(relative));
                QString hash=lib.value("sha1").toString();qint64 size=lib.value("size").toDouble(-1);
                if(hash.isEmpty())hash=QString::fromLatin1(worker.fetch(QUrl(url.toString()+".sha1"))).trimmed().section(' ',0,0);
                if(!QRegularExpression("^[a-fA-F0-9]{40}$").match(hash).hasMatch())fail(QCoreApplication::translate("M2CL", "Maven 校验值无效"));
                if(size<0){const auto bytes=worker.fetch(url,hash.toLower());size=bytes.size();write(checkedPath(req.root,"libraries/"+relative),bytes);}
                lib.insert("downloads",QJsonObject{{"artifact",QJsonObject{{"path",relative},{"url",url.toString()},{"sha1",hash.toLower()},{"size",size}}}});
            }normalized.append(lib);
        }metadata.insert("libraries",normalized);
        auto platform = GamePlatform::current();
        GamePlan requirements; requirements.javaMajor = metadata.value("javaVersion").toObject().value("majorVersion").toInt(requiredJavaMajor(metadata.value("_baseId").toString(metadata.value("id").toString())));
        if (requirements.javaMajor <= 0) fail(QCoreApplication::translate("M2CL", "无法识别此版本的 Java 要求"));

        const auto java = worker.java(req, metadata, requirements, platform);
        platform.arch = java.architecture; auto plan = gamePlan(!req.optifineAction.isEmpty()&&metadata.contains("_m2clOptifine")?metadata.value("_m2clOptifine").toObject().value("base").toObject():metadata, req.id, platform);
        worker.files(plan.files, req.root, QCoreApplication::translate("M2CL", "下载游戏与依赖"));
        const auto index = object(read(checkedPath(req.root, "assets/indexes/" + plan.assetIndex + ".json")));
        const auto objects = index.value("objects").toObject(); QList<GameFile> assets;
        for (auto it = objects.begin(); it != objects.end(); ++it) {
            const auto entry = it.value().toObject(); const auto hash = entry.value("hash").toString();
            assets.append(gameFile({{"sha1", hash}, {"size", entry.value("size")}, {"url", "https://resources.download.minecraft.net/" + hash.left(2) + "/" + hash}}, "assets/objects/" + hash.left(2) + "/" + hash));
        }
        worker.files(assets, req.root, QCoreApplication::translate("M2CL", "下载资源"));
        if (!QDir().mkpath(req.directory)) fail(QCoreApplication::translate("M2CL", "无法创建独立游戏目录"));
        if (index.value("virtual").toBool() || index.value("map_to_resources").toBool()) {
            for (auto it = objects.begin(); it != objects.end(); ++it) {
                worker.check(); const auto hash = it.value().toObject().value("hash").toString();
                const auto source = checkedPath(req.root, "assets/objects/" + hash.left(2) + "/" + hash);
                const auto targetRoot = index.value("map_to_resources").toBool() ? QDir(req.directory).filePath("resources") : checkedPath(req.root, "assets/virtual/" + plan.assetIndex);
                const auto destination = checkedPath(targetRoot, it.key());
                if (!QFileInfo::exists(destination)) write(destination, read(source));
            }
        }
        worker.report(QCoreApplication::translate("M2CL", "正在准备原生库…"));
        const auto natives = checkedPath(req.root, "versions/" + req.id + "/natives");
        // Clear stale natives left by a prior Java architecture selection.
        if (QFileInfo::exists(natives) && !QDir(natives).removeRecursively()) fail(QCoreApplication::translate("M2CL", "无法清理原生库目录"));
        QDir().mkpath(natives);
        for (const auto &archive : plan.natives) { worker.check(); extractNatives(checkedPath(req.root, archive), natives); }
        if(!req.optifineAction.isEmpty()) {
            optifineChange.prepare(req,metadata,java.executable,cancelled,progress);
            plan=gamePlan(metadata,req.id,platform);
            optifineChange.commitFiles();
            verifyOptifine(metadata,req.root,req.directory);
        }
        // Re-sample after downloads and extraction, using the actual selected Java architecture.
        int memory=req.memoryMiB;QString memoryNote;
        if(req.launchOptions.autoMemory){
            const auto recommendation=recommendMemory(req.memorySampler?req.memorySampler():systemMemory(),memoryContext(req.version,req.directory,currentLoader,java.architecture));
            if(!recommendation.valid())fail(recommendation.error);
            memory=recommendation.maximumMiB;memoryNote=QCoreApplication::translate("M2CL", "自动最大堆 %1").arg(memorySize(memory));
            if(!recommendation.warning.isEmpty())memoryNote+=QStringLiteral(" · ")+recommendation.warning;
            if(req.launchOptions.minMemoryMiB>memory)fail(QCoreApplication::translate("M2CL", "初始内存高于本次自动上限 %1，请调整实例内存设置").arg(memorySize(memory)));
        }else memoryNote=QCoreApplication::translate("M2CL", "手动最大堆 %1").arg(memorySize(memory));
        if(currentLoader!="vanilla"||metadata.contains("_m2clOptifine")){
            Instance target;target.root=req.root;target.path=QFileInfo(path).path();target.gameDirectory=req.directory;target.version=metadata.value("_baseId").toString(req.version);target.loader=currentLoader;
            target.loaderVersion=gameLoaderComponents(metadata).value(currentLoader);
            const auto of=metadata.value("_m2clOptifine").toObject();target.optifineFile=of.value("modFile").toString();target.optifineVersion=of.value("version").toString();target.optifineSha512=of.value("sha512").toString();target.optifineForgeVersion=of.value("forge").toString();
            const auto problems=checkModSet(target,localMods(target));result.warnings=problems.warnings;if(!problems.errors.isEmpty())fail(problems.errors.join('\n'));
        }
        auto identity=req.identity;if(identity.name.isEmpty())identity.name=req.player;
        result.command = gameCommand(metadata, plan, req.root, req.id, req.directory, java.executable, identity, memory, platform,req.launchOptions);result.command.memoryNote=memoryNote;
        const auto skinArguments=prepareOfflineSkin(req,java,cancelled,progress,network);
        for(int n=skinArguments.size()-1;n>=0;--n)result.command.arguments.prepend(skinArguments.at(n));
        worker.check();if(!originalMetadata.isEmpty()&&read(path)!=originalMetadata)fail(QCoreApplication::translate("M2CL", "安装过程中版本配置已被修改，请重新确认"));
        write(path,QJsonDocument(metadata).toJson());optifineChange.finish();
        result.instancePath = checkedPath(req.root, "versions/" + req.id); result.java = java.executable; result.javaRuntime = java;
        if(progress)progress(QCoreApplication::translate("M2CL", "安装完成，文件校验通过"),1000);
    } catch (const std::exception &e) { result.error = QString::fromUtf8(e.what()); }
    return result;
}
}
