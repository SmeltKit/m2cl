// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "networkerror.h"
#include "modinstaller.h"
#include "core/gameplan.h"
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <QSet>
#include <algorithm>
#include <stdexcept>
namespace mc {
namespace {
void fail(const QString&s){throw std::runtime_error(s.toUtf8().constData());}
void check(const CancelFlag&c){if(c&&c->load())fail(QCoreApplication::translate("M2CL", "安装已取消"));}
QJsonDocument json(QNetworkAccessManager &net,const QUrl&url,const CancelFlag&flag){check(flag);auto b=fetchInstallData(url,flag,{},&net);QJsonParseError e;auto d=QJsonDocument::fromJson(b,&e);if(e.error!=QJsonParseError::NoError)fail(QCoreApplication::translate("M2CL", "模组元数据无效"));return d;}
QJsonObject descriptor(const ResourceFile &f){QJsonArray deps;for(auto d:f.dependencies)deps.append(QJsonObject{{"projectId",d.projectId},{"versionId",d.versionId},{"type",d.type}});return {{"projectId",f.projectId},{"versionId",f.versionId},{"name",f.versionName},{"versionNumber",f.versionNumber},{"fileName",f.fileName},{"sha512",f.sha512},{"sha1",f.sha1},{"required",QJsonArray::fromStringList(f.requiredDependencies)},{"dependencies",deps}};}
QByteArray hashFile(const QString&path,const ResourceFile&f){if(!QFileInfo::exists(path)||QFileInfo(path).size()!=f.size)return {};QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};QCryptographicHash h(f.sha512.isEmpty()?QCryptographicHash::Sha1:QCryptographicHash::Sha512);h.addData(&file);return h.result().toHex();}
bool same(const QString&path,const ResourceFile&f){return hashFile(path,f)==(f.sha512.isEmpty()?f.sha1:f.sha512).toLatin1().toLower();}
QJsonObject localSnapshot(const Instance &target){QJsonObject snapshot;for(const auto &mod:localMods(target))snapshot.insert(mod.fileName,mod.sha512);return snapshot;}
QJsonObject versionsFromHashes(QNetworkAccessManager &network,const QStringList &hashes,const CancelFlag &cancel){
    QJsonObject result;
    for(int offset=0;offset<hashes.size();offset+=200){
        check(cancel);QNetworkRequest request(QUrl("https://api.modrinth.com/v2/version_files"));request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");request.setRawHeader("User-Agent","m2cl/0.1.0");request.setTransferTimeout(15000);
        auto reply=network.post(request,QJsonDocument(QJsonObject{{"hashes",QJsonArray::fromStringList(hashes.mid(offset,200))},{"algorithm","sha512"}}).toJson(QJsonDocument::Compact));QEventLoop loop;QTimer poll,deadline;QByteArray bytes;QString error;
        poll.setInterval(50);deadline.setSingleShot(true);QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(cancel&&cancel->load())reply->abort();});QObject::connect(&deadline,&QTimer::timeout,reply,[reply]{reply->setProperty("networkTimedOut",true);reply->abort();});QObject::connect(reply,&QNetworkReply::readyRead,&loop,[&]{bytes+=reply->readAll();if(bytes.size()>8*1024*1024)reply->abort();});QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);poll.start();deadline.start(15000);if(!reply->isFinished())loop.exec();bytes+=reply->readAll();error=networkReplyError(reply);reply->deleteLater();check(cancel);
        if(!error.isEmpty()||bytes.size()>8*1024*1024)fail(QCoreApplication::translate("M2CL", "无法识别本地模组来源，请重试；尚未安装文件：%1").arg(error));auto doc=QJsonDocument::fromJson(bytes);if(!doc.isObject())fail(QCoreApplication::translate("M2CL", "本地模组来源查询返回无效元数据"));const auto object=doc.object();for(auto it=object.begin();it!=object.end();++it)result.insert(it.key(),it.value());
    }return result;
}
QByteArray stream(QNetworkAccessManager &net,const ResourceFile &file,const QString &dest,const CancelFlag &cancel,const InstallProgress &progress){
    if(file.url.scheme()!="https"||file.url.host().isEmpty()||!file.url.userInfo().isEmpty()||file.size<=0||file.size>512LL*1024*1024||(!file.sha512.isEmpty()?file.sha512.size()!=128:file.sha1.size()!=40))fail("Invalid mod download metadata");
    QSaveFile out(dest);if(!out.open(QIODevice::WriteOnly))fail(out.errorString());QNetworkRequest request(file.url);request.setRawHeader("User-Agent","m2cl/0.1.0");request.setTransferTimeout(30000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);auto reply=net.get(request);QEventLoop loop;QTimer timer;timer.setInterval(50);QCryptographicHash hash(file.sha512.isEmpty()?QCryptographicHash::Sha1:QCryptographicHash::Sha512);qint64 received=0;QString error;
    auto consume=[&]{const auto problem=networkReplyError(reply);if(!problem.isEmpty()){error=problem;reply->abort();return;}auto data=reply->readAll();if(data.size()>file.size-received){error="Mod exceeds advertised size";reply->abort();return;}if(out.write(data)!=data.size()){error=out.errorString();reply->abort();return;}received+=data.size();hash.addData(data);if(progress)progress(QStringLiteral("%1 · %2 / %3 MiB").arg(file.fileName).arg(received/1048576.,0,'f',1).arg(file.size/1048576.,0,'f',1),int(received*1000/file.size));};
    QObject::connect(reply,&QNetworkReply::readyRead,&loop,consume);QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);QObject::connect(&timer,&QTimer::timeout,&loop,[&]{if(cancel&&cancel->load())reply->abort();});timer.start();if(!reply->isFinished())loop.exec();if(error.isEmpty()&&reply->error()==QNetworkReply::NoError)consume();if(error.isEmpty())error=networkReplyError(reply,reply->error()==QNetworkReply::OperationCanceledError&&!(cancel&&cancel->load()));reply->deleteLater();check(cancel);const auto expected=(file.sha512.isEmpty()?file.sha1:file.sha512).toLatin1().toLower();if(error.isEmpty()&&(received!=file.size||hash.result().toHex()!=expected))error=QCoreApplication::translate("M2CL", "文件大小或哈希校验失败");if(!error.isEmpty())fail(error);if(!out.commit())fail(out.errorString());return hash.result().toHex();
}
}
static ModPlan planModInstallImpl(const Instance &target,const ResourceFile &requested,const CancelFlag &cancel,QNetworkAccessManager *manager,bool inspectOptional){
    ModPlan plan;plan.target=target;try{if(!target.supportsMods()||target.path.isEmpty()||target.gameDirectory.isEmpty())fail(target.loaderProblem.isEmpty()?QCoreApplication::translate("M2CL", "请选择使用单一主加载器的实例"):target.loaderProblem);QNetworkAccessManager owned;auto &net=manager?*manager:owned;plan.originalManifest=modManifest(target.gameDirectory);QJsonArray installed,available;const auto localInventory=localMods(target);QStringList hashes;QHash<QString,ResourceFile> recognized;
        const bool renderingInstall=QStringList{"YL57xq9U","AANobbMI","iris","sodium"}.contains(requested.projectId);
        if(renderingInstall&&!target.optifineVersion.isEmpty())fail(QCoreApplication::translate("M2CL", "此实例已安装 OptiFine 版本组件。请先在“管理版本组件”中移除 OptiFine，再安装 Iris / Sodium。"));
        if(renderingInstall)for(const auto &mod:localInventory)if(mod.enabled&&mod.ids.contains("optifine"))plan.disableCandidates.insert(mod.fileName,QCoreApplication::translate("M2CL", "OptiFine 与 Iris / Sodium 不兼容；停用后保留原文件"));
        if(QStringList{"YL57xq9U","iris"}.contains(requested.projectId))for(const auto &mod:localInventory)if(mod.enabled&&mod.ids.contains("optifabric"))plan.disableCandidates.insert(mod.fileName,QCoreApplication::translate("M2CL", "OptiFabric 与 Iris 不兼容；停用后保留原文件"));
        for(const auto &mod:localInventory){bool cached=false;for(auto value:plan.originalManifest.value("files").toArray()){auto old=value.toObject();const auto name=old.value("fileName").toString();if((name==mod.fileName||(!mod.enabled&&name+".disabled"==mod.fileName))&&old.value("sha512")==mod.sha512&&!old.value("projectId").toString().isEmpty()){old.insert("fileName",mod.fileName);old.insert("enabled",mod.enabled);available.append(old);if(mod.enabled)installed.append(old);cached=true;break;}}if(!cached)hashes.append(mod.sha512);}
        QHash<QString,QString> presentIds;for(const auto &mod:localInventory)if(mod.enabled&&mod.known&&!plan.disableCandidates.contains(mod.fileName)){const auto environment=modEnvironmentProblem(mod,target);if(!environment.isEmpty())fail(mod.name+" · "+environment);for(const auto &other:localInventory)if(other.enabled&&other.fileName!=mod.fileName&&!plan.disableCandidates.contains(other.fileName)){const auto duplicate=duplicateModProblem(mod,other);if(!duplicate.isEmpty())fail(duplicate);}}
        hashes.removeDuplicates();const auto identified=versionsFromHashes(net,hashes,cancel);
        for(const auto &mod:localInventory)if(identified.contains(mod.sha512)){
            auto version=identified.value(mod.sha512).toObject();QJsonArray matching;for(auto raw:version.value("files").toArray())if(raw.toObject().value("hashes").toObject().value("sha512")==mod.sha512)matching.append(raw);version.insert("files",matching);const auto files=ModrinthProvider::files({version});
            if(files.isEmpty()||files.first().sha512!=mod.sha512||files.first().size!=QFileInfo(checkedPath(target.gameDirectory,"mods/"+mod.fileName)).size())continue;
            const auto file=files.first();recognized.insert(mod.sha512,file);auto record=descriptor(file);record.insert("fileName",mod.fileName);record.insert("enabled",mod.enabled);available.append(record);if(mod.enabled)installed.append(record);
        }
        for(const auto &hash:hashes)if(!recognized.contains(hash))plan.notes.append(QCoreApplication::translate("M2CL", "有本地文件未被 Modrinth 收录或无法识别来源；仍会检查 JAR 元数据，无法确认的前置不会按已满足处理。"));plan.notes.removeDuplicates();
        QHash<QString,ResourceFile> chosen;QHash<QString,QString> exact;QList<ResourceDependency> incompatible;QSet<QString> processed;
        auto require=[&](const ResourceFile &candidate,bool pinned){
            if(candidate.projectId.isEmpty()||candidate.versionId.isEmpty()||!safeModFileName(candidate.fileName)||!ModrinthProvider::compatible(candidate,target.version,target.loader))fail(QCoreApplication::translate("M2CL", "文件不适合目标实例：%1").arg(candidate.fileName));
            const auto old=chosen.value(candidate.projectId);if(!old.versionId.isEmpty()&&old.versionId!=candidate.versionId){if(!pinned || exact.contains(candidate.projectId))fail(QCoreApplication::translate("M2CL", "必需依赖版本冲突：%1").arg(candidate.projectId));}
            if(pinned)exact.insert(candidate.projectId,candidate.versionId);chosen.insert(candidate.projectId,candidate);
        };
        auto root=requested;if(root.projectId.isEmpty()&&!root.versionId.isEmpty()){auto fs=ModrinthProvider::files({json(net,QUrl("https://api.modrinth.com/v2/version/"+QString::fromLatin1(QUrl::toPercentEncoding(root.versionId))),cancel).object()});if(fs.isEmpty())fail("Root version not found");root=fs.first();}require(root,true);
        for(int iteration=0;iteration<256;++iteration){QString id;for(auto it=chosen.begin();it!=chosen.end();++it)if(!processed.contains(it.value().versionId)){id=it.key();break;}if(id.isEmpty())break;auto file=chosen.value(id);processed.insert(file.versionId);if(chosen.size()>128)fail(QCoreApplication::translate("M2CL", "依赖图超过处理上限"));
            for(auto dependency:file.dependencies){check(cancel);if(dependency.type=="incompatible"){if(dependency.projectId.isEmpty()&&dependency.versionId.isEmpty())fail("Unresolved incompatible dependency");incompatible.append(dependency);continue;}if(dependency.type!="required")continue;ResourceFile dep;bool pinned=!dependency.versionId.isEmpty();
                if(pinned){auto fs=ModrinthProvider::files({json(net,QUrl("https://api.modrinth.com/v2/version/"+QString::fromLatin1(QUrl::toPercentEncoding(dependency.versionId))),cancel).object()});if(fs.isEmpty())fail("Required version unavailable");dep=fs.first();if(!dependency.projectId.isEmpty()&&dep.projectId!=dependency.projectId)fail("Dependency project mismatch");}
                else{if(dependency.projectId.isEmpty())fail(QCoreApplication::translate("M2CL", "必需依赖缺少可解析标识"));if(chosen.contains(dependency.projectId))continue;auto d=json(net,ModrinthProvider().versionsUrl(dependency.projectId,target.version,target.loader),cancel);auto fs=ModrinthProvider::files(d.array());for(auto f:fs){if(!ModrinthProvider::compatible(f,target.version,target.loader))continue;for(auto value:installed){auto old=value.toObject();if(old.value("projectId")==f.projectId&&old.value("versionId")==f.versionId&&same(checkedPath(target.gameDirectory,"mods/"+old.value("fileName").toString()),f)){dep=f;break;}}if(!dep.versionId.isEmpty())break;}if(dep.versionId.isEmpty())for(auto f:fs){if(!ModrinthProvider::compatible(f,target.version,target.loader))continue;for(const auto &value:available){const auto old=value.toObject();if(old.value("enabled").toBool()||old.value("projectId")!=f.projectId||old.value("versionId")!=f.versionId)continue;const auto name=old.value("fileName").toString();for(const auto &local:localInventory)if(local.fileName==name&&local.known&&modEnvironmentProblem(local,target).isEmpty()&&same(checkedPath(target.gameDirectory,"mods/"+name),f)){dep=f;break;}if(!dep.versionId.isEmpty())break;}if(!dep.versionId.isEmpty())break;}if(dep.versionId.isEmpty())for(auto f:fs)if(f.releaseType=="release"&&ModrinthProvider::compatible(f,target.version,target.loader)){dep=f;break;}if(dep.versionId.isEmpty())fail(QCoreApplication::translate("M2CL", "必需依赖没有兼容正式版：%1").arg(dependency.projectId));}
                if(chosen.contains(dep.projectId)&&chosen.value(dep.projectId).versionId==dep.versionId)continue;require(dep,pinned);
            }
            if(iteration==255)fail("Dependency resolution did not converge");
        }
        QSet<QString> reachable;QStringList queue{root.projectId};while(!queue.isEmpty()){auto id=queue.takeFirst();if(reachable.contains(id))continue;reachable.insert(id);for(auto d:chosen.value(id).dependencies)if(d.type=="required"){auto child=d.projectId;if(child.isEmpty())for(auto f:chosen)if(f.versionId==d.versionId)child=f.projectId;if(!child.isEmpty())queue.append(child);}}
        for(auto id:chosen.keys())if(!reachable.contains(id))chosen.remove(id);incompatible.clear();for(auto f:chosen)for(auto d:f.dependencies)if(d.type=="incompatible")incompatible.append(d);
        plan.rootProjectId=root.projectId;plan.resolved=chosen.values();
        std::sort(plan.resolved.begin(),plan.resolved.end(),[](const ResourceFile&a,const ResourceFile&b){return a.projectId<b.projectId;});
        for(const auto &file:plan.resolved)for(const auto &dependency:file.dependencies){auto child=dependency.projectId;if(child.isEmpty())for(const auto &candidate:plan.resolved)if(candidate.versionId==dependency.versionId)child=candidate.projectId;plan.edges.append({file.projectId,child,dependency.versionId,dependency.type});}
        const auto allInstalled=available;QJsonArray effectiveInstalled;for(auto value:installed){const auto old=value.toObject();if(chosen.contains(old.value("projectId").toString())&&chosen.value(old.value("projectId").toString()).versionId!=old.value("versionId").toString())continue;effectiveInstalled.append(value);}installed=effectiveInstalled;
        for(auto value:installed){auto old=value.toObject();for(auto raw:old.value("dependencies").toArray()){auto dep=raw.toObject();if(dep.value("type")!="incompatible")continue;for(auto f:chosen)if((dep.value("projectId").toString().isEmpty()||dep.value("projectId")==f.projectId)&&(dep.value("versionId").toString().isEmpty()||dep.value("versionId")==f.versionId))fail(QCoreApplication::translate("M2CL", "已安装的 %1 声明与此项目不兼容").arg(old.value("name").toString()));}}
        for(const auto &dep:incompatible){for(auto f:chosen)if((dep.projectId.isEmpty()||f.projectId==dep.projectId)&&(dep.versionId.isEmpty()||f.versionId==dep.versionId))fail(QCoreApplication::translate("M2CL", "所选依赖之间存在已声明冲突"));for(auto v:installed){auto o=v.toObject();if((dep.projectId.isEmpty()||o.value("projectId")==dep.projectId)&&(dep.versionId.isEmpty()||o.value("versionId")==dep.versionId))fail(QCoreApplication::translate("M2CL", "与已安装项目存在已声明冲突：%1").arg(o.value("name").toString()));}}
        QSet<QString> plannedNames;for(auto f:chosen){if(plannedNames.contains(f.fileName.toCaseFolded()))fail("Dependency files use duplicate filenames");plannedNames.insert(f.fileName.toCaseFolded());auto dest=checkedPath(target.gameDirectory,"mods/"+f.fileName);if(QFileInfo::exists(dest)&&!same(dest,f)){bool identifiedOld=false;for(const auto &value:allInstalled){const auto old=value.toObject();if(old.value("projectId")==f.projectId&&old.value("fileName")==f.fileName&&old.value("sha512")==QString::fromLatin1(fileSha512(dest)))identifiedOld=true;}if(!identifiedOld)fail(QCoreApplication::translate("M2CL", "同名文件已存在且内容不同：%1").arg(f.fileName));}
            for(auto value:allInstalled){auto old=value.toObject();if(old.value("projectId")!=f.projectId)continue;auto name=old.value("fileName").toString();auto path=checkedPath(target.gameDirectory,"mods/"+name);if(QFileInfo::exists(path)){if(same(path,f))continue;auto actual=QString::fromLatin1(fileSha512(path));if(!old.value("sha512").toString().isEmpty()&&old.value("sha512")!=actual)fail(QCoreApplication::translate("M2CL", "旧文件已被修改，无法自动替换：%1").arg(name));plan.replacements.append(name);plan.replacementProjects.insert(name,f.projectId);QString oldVersion=old.value("versionNumber").toString();for(const auto &local:localInventory)if(local.fileName==name)oldVersion=local.version;plan.replacementVersions.insert(name,oldVersion);plan.expectedFiles.insert(name,actual);}}
            QString localName;for(const auto &mod:localInventory)if(mod.enabled&&(f.sha512.isEmpty()?same(checkedPath(target.gameDirectory,"mods/"+mod.fileName),f):mod.sha512==f.sha512&&QFileInfo(checkedPath(target.gameDirectory,"mods/"+mod.fileName)).size()==f.size)){localName=mod.fileName;break;}
            if(localName.isEmpty()){plan.files.append(f);for(const auto &mod:localInventory)if(!mod.enabled&&mod.known&&modEnvironmentProblem(mod,target).isEmpty()&&same(checkedPath(target.gameDirectory,"mods/"+mod.fileName),f)){plan.disabledFiles.insert(f.projectId,mod.fileName);break;}}else{plan.reusedProjects.append(f.projectId);plan.reusedFiles.insert(f.projectId,localName);}
        }
        // Reuse the already inspected archive metadata; icon extraction runs in the planner worker.
        auto localFiles=plan.reusedFiles;for(auto it=plan.disabledFiles.begin();it!=plan.disabledFiles.end();++it)localFiles.insert(it.key(),it.value());
        for(auto it=localFiles.begin();it!=localFiles.end();++it)for(const auto &mod:localInventory)if(mod.fileName==it.value()){
            const auto bytes=localModIcon(checkedPath(target.gameDirectory,"mods/"+mod.fileName),mod);
            if(!bytes.isEmpty())plan.localProjectIcons.insert(it.key(),bytes);break;
        }
        std::sort(plan.files.begin(),plan.files.end(),[](const ResourceFile&a,const ResourceFile&b){return a.projectId<b.projectId;});
        if(chosen.size()>1)plan.notes.append(QCoreApplication::translate("M2CL", "主模组与必需依赖共 %1 项").arg(chosen.size()));
        // Respect local Mod IDs, including untracked files, rather than silently installing another copy.
        plan.expectedFiles={};for(const auto &mod:localInventory)plan.expectedFiles.insert(mod.fileName,mod.sha512);if(localSnapshot(target)!=plan.expectedFiles)fail(QCoreApplication::translate("M2CL", "解析依赖期间模组目录已改变，请重新生成计划"));
        // Project titles and icons are cosmetic; a missing title must not invalidate an install.
        QStringList projectIds;for(const auto &file:plan.resolved)projectIds.append(file.projectId);for(const auto &edge:plan.edges)if(!edge.to.isEmpty())projectIds.append(edge.to);projectIds.removeDuplicates();
        for(int offset=0;offset<projectIds.size();offset+=100){try{QUrl url("https://api.modrinth.com/v2/projects");QUrlQuery query;query.addQueryItem("ids",QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(projectIds.mid(offset,100))).toJson(QJsonDocument::Compact)));url.setQuery(query);for(const auto &raw:json(net,url,cancel).array()){const auto project=raw.toObject();const auto id=project.value("id").toString();if(!id.isEmpty()){plan.projectNames.insert(id,project.value("title").toString());plan.projectIcons.insert(id,QUrl(project.value("icon_url").toString()));}}}catch(const std::exception &){check(cancel);}}
        if(inspectOptional){
            QSet<QString> attempted;int expandedCount=plan.resolved.size();
            for(const auto &edge:plan.edges)if(edge.type=="optional"){
                const auto key=edge.to.isEmpty()?edge.versionId:edge.to;if(key.isEmpty()||attempted.contains(key)||chosen.contains(edge.to))continue;attempted.insert(key);if(attempted.size()>16){plan.notes.append(QCoreApplication::translate("M2CL", "可选项目较多，仅预查前 16 项，其余可从模组市场单独添加。"));break;}
                try{
                    QList<ResourceFile> candidates;if(!edge.versionId.isEmpty())candidates=ModrinthProvider::files({json(net,QUrl("https://api.modrinth.com/v2/version/"+QString::fromLatin1(QUrl::toPercentEncoding(edge.versionId))),cancel).object()});else candidates=ModrinthProvider::files(json(net,ModrinthProvider().versionsUrl(edge.to,target.version,target.loader),cancel).array());
                    ResourceFile candidate;for(const auto &file:candidates)if(ModrinthProvider::compatible(file,target.version,target.loader)&&(!edge.versionId.isEmpty()||file.releaseType=="release")){candidate=file;break;}
                    if(candidate.projectId.isEmpty()||(!edge.to.isEmpty()&&candidate.projectId!=edge.to))fail(QCoreApplication::translate("M2CL", "没有可用的匹配版本"));auto optional=planModInstallImpl(target,candidate,cancel,&net,false);if(!optional.error.isEmpty())fail(optional.error);
                    for(const auto &file:optional.resolved)for(const auto &base:plan.resolved)if(file.projectId==base.projectId&&file.versionId!=base.versionId)fail(QCoreApplication::translate("M2CL", "与必需前置要求不同构建，需单独选择版本"));
                    for(const auto &prior:plan.optionalPlans)for(const auto &file:optional.resolved)for(const auto &other:prior.resolved)if(file.projectId==other.projectId&&file.versionId!=other.versionId)fail(QCoreApplication::translate("M2CL", "与另一个可选方案要求不同构建，需单独选择版本"));
                    if(expandedCount+optional.resolved.size()>128)fail(QCoreApplication::translate("M2CL", "可选依赖预查超过项目上限"));expandedCount+=optional.resolved.size();plan.optionalPlans.append(optional);
                }catch(const std::exception &error){check(cancel);plan.optionalErrors.insert(key,QString::fromUtf8(error.what()));}
            }
            if(localSnapshot(target)!=plan.expectedFiles)fail(QCoreApplication::translate("M2CL", "预查可选前置期间模组目录已改变，请重新生成计划"));
        }
        const auto local=localMods(target);for(const auto &mod:local)if(mod.enabled&&!mod.known)plan.notes.append(QCoreApplication::translate("M2CL", "已有文件无法完整识别：%1；暂存校验时无法确认将停止安装").arg(mod.fileName));
    }catch(const std::exception&e){plan.error=QString::fromUtf8(e.what());}return plan;
}
ModPlan planModInstall(const Instance &target,const ResourceFile &file,const CancelFlag &cancel,QNetworkAccessManager *manager){return planModInstallImpl(target,file,cancel,manager,true);}
ModPlan expandedModPlan(const ModPlan &plan){
    auto expanded=plan;expanded.optionalPlans.clear();if(expanded.requiredProjects.isEmpty())for(const auto &file:plan.resolved)expanded.requiredProjects.append(file.projectId);
    QSet<QString> files,resolved;for(const auto &file:expanded.files)files.insert(file.projectId);for(const auto &file:expanded.resolved)resolved.insert(file.projectId);
    for(const auto &optional:plan.optionalPlans){
        expanded.optionalProjects.append(optional.rootProjectId);
        for(auto &edge:expanded.edges)if(edge.type=="optional"&&edge.to.isEmpty())for(const auto &file:optional.resolved)if(edge.versionId==file.versionId)edge.to=file.projectId;
        for(const auto &file:optional.files)if(!files.contains(file.projectId)){expanded.files.append(file);files.insert(file.projectId);}
        for(const auto &file:optional.resolved)if(!resolved.contains(file.projectId)){expanded.resolved.append(file);resolved.insert(file.projectId);}
        expanded.edges.append(optional.edges);expanded.reusedProjects.append(optional.reusedProjects);expanded.reusedProjects.removeDuplicates();
        for(auto it=optional.reusedFiles.begin();it!=optional.reusedFiles.end();++it)expanded.reusedFiles.insert(it.key(),it.value());
        for(auto it=optional.disabledFiles.begin();it!=optional.disabledFiles.end();++it)expanded.disabledFiles.insert(it.key(),it.value());
        for(const auto &name:optional.replacements)if(!expanded.replacements.contains(name))expanded.replacements.append(name);
        for(auto it=optional.replacementProjects.begin();it!=optional.replacementProjects.end();++it)expanded.replacementProjects.insert(it.key(),it.value());
        for(auto it=optional.replacementVersions.begin();it!=optional.replacementVersions.end();++it)expanded.replacementVersions.insert(it.key(),it.value());
        for(auto it=optional.projectNames.begin();it!=optional.projectNames.end();++it)expanded.projectNames.insert(it.key(),it.value());
        for(auto it=optional.localProjectIcons.begin();it!=optional.localProjectIcons.end();++it)expanded.localProjectIcons.insert(it.key(),it.value());
        for(auto it=optional.projectIcons.begin();it!=optional.projectIcons.end();++it)expanded.projectIcons.insert(it.key(),it.value());
    }
    return expanded;
}
ModPlan selectModDependencies(const ModPlan &input,const QSet<QString> &selectedProjects,const QSet<QString> &redownloadProjects,const QSet<QString> &enableProjects){
    const auto plan=expandedModPlan(input);auto selected=plan;selected.files.clear();selected.skippedProjects.clear();selected.enableFiles.clear();selected.activeProjects.clear();selected.demandedProjects.clear();
    QSet<QString> allowed{plan.rootProjectId};for(const auto &id:plan.reusedProjects)allowed.insert(id);allowed.unite(selectedProjects);
    QSet<QString> active,demanded;QStringList queue{plan.rootProjectId};
    while(!queue.isEmpty()){const auto id=queue.takeFirst();if(active.contains(id))continue;active.insert(id);for(const auto &edge:plan.edges)if(edge.from==id&&(edge.type=="required"||edge.type=="optional")&&!edge.to.isEmpty()){demanded.insert(edge.to);if(allowed.contains(edge.to)&&(edge.type=="required"||selectedProjects.contains(edge.to)||plan.reusedProjects.contains(edge.to)))queue.append(edge.to);}}
    selected.activeProjects=active.values();selected.demandedProjects=demanded.values();
    for(const auto &file:plan.files)if(active.contains(file.projectId)){
        if(plan.disabledFiles.contains(file.projectId)&&enableProjects.contains(file.projectId))selected.enableFiles.insert(file.projectId,plan.disabledFiles.value(file.projectId));else selected.files.append(file);
    }else if(file.projectId!=plan.rootProjectId&&demanded.contains(file.projectId))selected.skippedProjects.append(file.projectId);
    QSet<QString> requiredTargets;for(const auto &edge:plan.edges)if(active.contains(edge.from)&&edge.type=="required")requiredTargets.insert(edge.to);for(int i=selected.skippedProjects.size()-1;i>=0;--i)if(!requiredTargets.contains(selected.skippedProjects.at(i)))selected.skippedProjects.removeAt(i);
    for(const auto &edge:plan.edges)if(active.contains(edge.from)&&edge.type=="incompatible")for(const auto &file:plan.resolved)if(active.contains(file.projectId)&&(edge.to.isEmpty()||edge.to==file.projectId)&&(edge.versionId.isEmpty()||edge.versionId==file.versionId))selected.error=QCoreApplication::translate("M2CL", "所选可选组件与安装计划存在已声明冲突");
    for(const auto &name:plan.replacements)if(!active.contains(plan.replacementProjects.value(name)))selected.replacements.removeAll(name);
    for(const auto &id:plan.reusedProjects)if(!active.contains(id)){selected.reusedProjects.removeAll(id);selected.reusedFiles.remove(id);}
    for(const auto &file:plan.resolved)if(active.contains(file.projectId)&&redownloadProjects.contains(file.projectId)&&plan.reusedFiles.contains(file.projectId)){
        const auto oldName=plan.reusedFiles.value(file.projectId);selected.files.append(file);selected.reusedProjects.removeAll(file.projectId);selected.reusedFiles.remove(file.projectId);if(!selected.replacements.contains(oldName))selected.replacements.append(oldName);selected.replacementProjects.insert(oldName,file.projectId);
    }
    for(auto it=selected.enableFiles.begin();it!=selected.enableFiles.end();++it){if(!selected.replacements.contains(it.value()))selected.replacements.append(it.value());selected.replacementProjects.insert(it.value(),it.key());}
    // Choosing a fresh download of an identified disabled copy also replaces that copy.
    for(const auto &file:selected.files)if(plan.disabledFiles.contains(file.projectId)){const auto old=plan.disabledFiles.value(file.projectId);if(!selected.replacements.contains(old))selected.replacements.append(old);selected.replacementProjects.insert(old,file.projectId);}
    return selected;
}
ModInstallResult applyModPlan(const ModPlan &plan,const CancelFlag &cancel,const InstallProgress &progress,QNetworkAccessManager *manager,const AttachedPack *pack){
    ModInstallResult result;try{if(!plan.error.isEmpty())fail(plan.error);for(auto it=plan.disableCandidates.begin();it!=plan.disableCandidates.end();++it)if(!plan.disableFiles.contains(it.key()))fail(QCoreApplication::translate("M2CL", "需要先确认停用冲突组件：%1").arg(it.key()));for(const auto &name:plan.disableFiles)if(!plan.disableCandidates.contains(name)||!plan.expectedFiles.contains(name)||QFileInfo::exists(checkedPath(plan.target.gameDirectory,"mods/"+name+".disabled")))fail(QCoreApplication::translate("M2CL", "无法安全停用：%1").arg(name));for(const auto &file:plan.disableFiles+plan.replacements)if(isVersionComponentFile(plan.target,file))fail(QCoreApplication::translate("M2CL", "无法通过普通模组操作替换 OptiFine，请使用“管理版本组件”。"));check(cancel);auto mods=checkedPath(plan.target.gameDirectory,"mods");if(!QDir().mkpath(mods))fail("Cannot create mods directory");QLockFile lock(checkedPath(plan.target.gameDirectory,"mods/.m2cl-install.lock"));if(!lock.tryLock())fail(QCoreApplication::translate("M2CL", "该实例已有模组操作正在进行"));
        if(modManifest(plan.target.gameDirectory)!=plan.originalManifest||localSnapshot(plan.target)!=plan.expectedFiles)fail(QCoreApplication::translate("M2CL", "模组目录已改变，请重新生成安装计划"));for(auto it=plan.expectedFiles.begin();it!=plan.expectedFiles.end();++it){auto path=checkedPath(plan.target.gameDirectory,"mods/"+it.key());if(!QFileInfo::exists(path)||QString::fromLatin1(fileSha512(path))!=it.value().toString())fail(QCoreApplication::translate("M2CL", "本地文件已改变，请重新检查：%1").arg(it.key()));}
        QTemporaryDir stage(QDir(plan.target.gameDirectory).filePath(".m2cl-mod-install-XXXXXX"));if(!stage.isValid())fail("Cannot create staging directory");QNetworkAccessManager owned;auto &net=manager?*manager:owned;
        QList<ResourceFile> additions=plan.files;for(const auto &file:plan.resolved)if(plan.enableFiles.contains(file.projectId)){auto enabled=file;const auto old=plan.enableFiles.value(file.projectId);if(!old.endsWith(".jar.disabled"))fail("Invalid disabled mod filename");enabled.fileName=old.left(old.size()-9);if(!safeModFileName(enabled.fileName))fail("Invalid enabled mod filename");additions.append(enabled);}
        QSet<QString> additionNames;for(const auto &file:additions){if(additionNames.contains(file.fileName.toCaseFolded()))fail(QCoreApplication::translate("M2CL", "所选文件名重复：%1").arg(file.fileName));additionNames.insert(file.fileName.toCaseFolded());if(QFileInfo::exists(checkedPath(plan.target.gameDirectory,"mods/"+file.fileName))&&!plan.replacements.contains(file.fileName))fail(QCoreApplication::translate("M2CL", "目标文件名已占用：%1").arg(file.fileName));}
        QHash<QString,LocalMod> staged;QSet<QString> newIds;for(const auto &f:additions){check(cancel);auto path=QDir(stage.path()).filePath(f.fileName);if(plan.enableFiles.contains(f.projectId)){const auto source=checkedPath(plan.target.gameDirectory,"mods/"+plan.enableFiles.value(f.projectId));if(!same(source,f)||!QFile::copy(source,path)||!same(path,f))fail(QCoreApplication::translate("M2CL", "已禁用文件发生变化，未启用：%1").arg(f.fileName));}else stream(net,f,path,cancel,progress);auto m=inspectMod(path,plan.target.loader);if(!m.known)fail(QCoreApplication::translate("M2CL", "无法核对下载文件的模组元数据：%1 · %2").arg(f.fileName,m.problem));auto env=modEnvironmentProblem(m,plan.target);if(!env.isEmpty())fail(env);for(const auto &other:staged){const auto duplicate=duplicateModProblem(m,other);if(!duplicate.isEmpty())fail(duplicate);}staged.insert(f.fileName,m);}
        for(auto m:localMods(plan.target))if(m.enabled&&!plan.replacements.contains(m.fileName)&&!plan.disableFiles.contains(m.fileName))for(const auto &other:staged){const auto duplicate=duplicateModProblem(m,other);if(!duplicate.isEmpty())fail(duplicate);}
        QList<LocalMod> combined;for(auto m:localMods(plan.target))if(m.enabled&&!plan.replacements.contains(m.fileName)&&!plan.disableFiles.contains(m.fileName))combined.append(m);for(auto m:staged)combined.append(m);
        const auto problems=checkModSet(plan.target,combined,true,!plan.skippedProjects.isEmpty());if(!problems.errors.isEmpty())fail(problems.errors.join('\n'));result.warnings=problems.warnings;
        check(cancel);if(modManifest(plan.target.gameDirectory)!=plan.originalManifest||localSnapshot(plan.target)!=plan.expectedFiles)fail(QCoreApplication::translate("M2CL", "下载期间模组目录已改变，请重新生成安装计划"));for(auto it=plan.expectedFiles.begin();it!=plan.expectedFiles.end();++it)if(QString::fromLatin1(fileSha512(checkedPath(plan.target.gameDirectory,"mods/"+it.key())))!=it.value().toString())fail("Local mod changed during download");QStringList added,moved,disabled;const auto backupRoot=checkedPath(plan.target.gameDirectory,".m2cl-mod-backups/"+QUuid::createUuid().toString(QUuid::WithoutBraces));if(!plan.replacements.isEmpty()&&!QDir().mkpath(backupRoot))fail("Cannot prepare mod backup");
        QString packPath,metaPath; QByteArray previousMeta; bool packAdded=false,metaWritten=false,metaExisted=false;
        std::unique_ptr<QLockFile> packLock;
        if(pack){
            const auto inspected=inspectPack(pack->source,pack->kind);
            if(!inspected.problem.isEmpty())fail(inspected.problem);
            if(inspected.directory||inspected.fileName!=pack->file.fileName||inspected.sha512!=pack->file.sha512)fail(QCoreApplication::translate("M2CL", "光影包身份已改变，请重新下载"));
            const auto folder=checkedPath(plan.target.gameDirectory,packFolder(pack->kind));
            if(!QDir().mkpath(folder))fail(QCoreApplication::translate("M2CL", "无法创建资源目录"));
            packLock=std::make_unique<QLockFile>(folder+"/.m2cl-pack.lock");
            if(!packLock->tryLock())fail(QCoreApplication::translate("M2CL", "资源目录正在使用"));
            packPath=checkedPath(folder,inspected.fileName);
            if(QFileInfo::exists(packPath)&&QString::fromLatin1(fileSha512(packPath))!=inspected.sha512)fail(QCoreApplication::translate("M2CL", "已有同名但内容不同的资源，未修改模组"));
            metaPath=checkedPath(plan.target.gameDirectory,".m2cl-pack-metadata/"+inspected.sha512+".json");
            QFile old(metaPath);metaExisted=old.exists();
            if(metaExisted){if(!old.open(QIODevice::ReadOnly))fail(QCoreApplication::translate("M2CL", "无法读取已有资源信息"));previousMeta=old.readAll();}
        }
        try{for(const auto &name:plan.disableFiles){if(!QFile::rename(checkedPath(plan.target.gameDirectory,"mods/"+name),checkedPath(plan.target.gameDirectory,"mods/"+name+".disabled")))fail("Cannot disable conflicting component");disabled.append(name);}
            for(auto name:plan.replacements){auto source=checkedPath(plan.target.gameDirectory,"mods/"+name),backup=QDir(backupRoot).filePath(name);if(!QFile::rename(source,backup))fail("Cannot back up old mod");moved.append(name);}
            for(auto f:additions){auto dest=checkedPath(plan.target.gameDirectory,"mods/"+f.fileName);if(QFileInfo::exists(dest)||!QFile::rename(QDir(stage.path()).filePath(f.fileName),dest))fail("Cannot commit mod files");added.append(f.fileName);}
            auto entries=plan.originalManifest.value("files").toArray();QJsonArray next;for(auto v:entries){auto old=v.toObject();if(!plan.replacements.contains(old.value("fileName").toString())&&!plan.replacements.contains(old.value("fileName").toString()+".disabled")&&!plan.reusedFiles.values().contains(old.value("fileName").toString()))next.append(old);}
            for(const auto &f:plan.resolved)if(plan.reusedFiles.contains(f.projectId)){auto observed=descriptor(f);const auto name=plan.reusedFiles.value(f.projectId);observed.insert("fileName",name);observed.insert("sha512",plan.expectedFiles.value(name));observed.insert("observedLocal",true);next.append(observed);}
            for(auto f:additions){auto o=descriptor(f);o.insert("sha512",staged.value(f.fileName).sha512);o.insert("modId",staged.value(f.fileName).id);if(f.projectId==plan.rootProjectId&&!plan.skippedProjects.isEmpty())o.insert("skippedDependencyProjects",QJsonArray::fromStringList(plan.skippedProjects));next.append(o);}
            check(cancel);
            if(pack){
                if(!QFileInfo::exists(packPath)){
                    if(!QFile::copy(pack->source,packPath))fail(QCoreApplication::translate("M2CL", "无法提交资源包"));
                    packAdded=true;
                    if(QString::fromLatin1(fileSha512(packPath))!=pack->file.sha512)fail(QCoreApplication::translate("M2CL", "资源包复制校验失败"));
                }
                const auto error=savePackMetadata(plan.target,pack->kind,pack->project,pack->file,pack->icon);
                if(!error.isEmpty())fail(error);
                metaWritten=true;check(cancel);
            }
            QSaveFile manifest(checkedPath(plan.target.gameDirectory,"mods/.m2cl-mods.json"));auto bytes=QJsonDocument(QJsonObject{{"schemaVersion",1},{"files",next}}).toJson();if(!manifest.open(QIODevice::WriteOnly)||manifest.write(bytes)!=bytes.size()||!manifest.commit())fail("Cannot commit mod inventory");
        }catch(...){
            QStringList recoveryFailures;
            if(packAdded&&!QFile::remove(packPath))recoveryFailures.append(packPath);
            if(metaWritten){
                if(metaExisted){QSaveFile restore(metaPath);if(!restore.open(QIODevice::WriteOnly)||restore.write(previousMeta)!=previousMeta.size()||!restore.commit())recoveryFailures.append(metaPath);}
                else if(!QFile::remove(metaPath))recoveryFailures.append(metaPath);
            }
            for(const auto &name:disabled)if(!QFile::rename(checkedPath(plan.target.gameDirectory,"mods/"+name+".disabled"),checkedPath(plan.target.gameDirectory,"mods/"+name)))recoveryFailures.append(name);
            for(const auto &name:added)if(!QFile::remove(checkedPath(plan.target.gameDirectory,"mods/"+name)))recoveryFailures.append(name);
            for(const auto &name:moved)if(!QFile::rename(QDir(backupRoot).filePath(name),checkedPath(plan.target.gameDirectory,"mods/"+name)))recoveryFailures.append(QDir(backupRoot).filePath(name));
            if(!recoveryFailures.isEmpty())fail(QCoreApplication::translate("M2CL", "安装失败，以下文件恢复失败，请检查：\n")+recoveryFailures.join('\n'));
            throw;
        }
        result.installed=added;for(const auto &file:additions)if(plan.enableFiles.contains(file.projectId))result.enabled.append(file.fileName);if(progress)progress(QCoreApplication::translate("M2CL", "已按所选计划安装到 %1").arg(plan.target.id),1000);
    }catch(const std::exception&e){result.error=QString::fromUtf8(e.what());}return result;
}
}
