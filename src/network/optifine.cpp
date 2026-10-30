// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "optifine.h"
#include "downloadsource.h"
#include "core/catalog.h"
#include "core/mods.h"
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QCollator>
#include <algorithm>
#include <QUuid>
#include <stdexcept>

namespace mc {
namespace {
void fail(const QString &text) { throw std::runtime_error(text.toUtf8().constData()); }
QByteArray read(const QString &path) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>64*1024*1024)fail(QCoreApplication::translate("M2CL", "无法读取 OptiFine 文件：%1").arg(path));return f.readAll();
}
void save(const QString &path,const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).path());QSaveFile f(path);
    if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())fail(QCoreApplication::translate("M2CL", "无法保存 OptiFine 文件：%1").arg(path));
}
QString hash(const QString &path) { return QString::fromLatin1(QCryptographicHash::hash(read(path),QCryptographicHash::Sha256).toHex()); }
void check(const CancelFlag &flag) { if(flag->load())fail(QCoreApplication::translate("M2CL", "OptiFine 安装已取消")); }
// An attempt has no retry. BMCLAPI may redirect to an HTTPS OpenBMCLAPI node.
QByteArray fetch(const QUrl &url,const CancelFlag &flag,const InstallProgress &progress = {}) {
    if(url.scheme()!="https"||!QStringList{"optifine.net","bmclapi2.bangbang93.com"}.contains(url.host())||!url.userInfo().isEmpty())fail(QCoreApplication::translate("M2CL", "OptiFine 下载地址无效"));
    return fetchInstallData(url,flag,{},nullptr,"official",progress);
}
QUrl mirrorPackage(const QJsonObject &build) {
    const auto v=build.value("version").toString();
    const auto match=QRegularExpression("^(HD_[A-Z])_([A-Za-z0-9]+)(?:_(pre[0-9]+))?$").match(v);
    if(!match.hasMatch())fail(QCoreApplication::translate("M2CL", "OptiFine 构建名称无效"));
    const auto type=match.captured(1)+(match.captured(3).isEmpty()?QString():"_"+match.captured(2));
    const auto patch=match.captured(3).isEmpty()?match.captured(2):match.captured(3);
    return QUrl("https://bmclapi2.bangbang93.com/optifine/"+QString::fromLatin1(QUrl::toPercentEncoding(build.value("game").toString()))+"/"+type+"/"+patch);
}
QJsonObject localLibrary(const QString &root,const QString &name,const QString &relative,const QByteArray &bytes,QJsonArray &files) {
    const auto path=checkedPath(root,"libraries/"+relative);save(path,bytes);
    const auto sha=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha1).toHex());
    files.append(QJsonObject{{"path","libraries/"+relative},{"sha256",hash(path)}});
    return {{"name",name},{"downloads",QJsonObject{{"artifact",QJsonObject{{"path",relative},{"url","https://optifine.net/downloads"},{"sha1",sha},{"size",bytes.size()}}}}}};
}
// Read ConstantValue fields from the class file; filenames are not trusted as identity.
QMap<QString,QString> constants(const QByteArray &data) {
    int pos=0;auto number=[&](int size)->quint32 {if(pos+size>data.size())fail(QCoreApplication::translate("M2CL", "OptiFine 类文件截断"));quint32 v=0;for(int i=0;i<size;++i)v=(v<<8)|quint8(data[pos++]);return v;};
    if(number(4)!=0xcafebabe)fail(QCoreApplication::translate("M2CL", "OptiFine 类文件无效"));number(2);number(2);
    const int count=number(2);QMap<int,QString> utf;QMap<int,int> strings;
    for(int i=1;i<count;++i){const int tag=number(1);switch(tag){
    case 1:{const int len=number(2);if(pos+len>data.size())fail("Invalid constant pool");utf[i]=QString::fromUtf8(data.mid(pos,len));pos+=len;break;}
    case 8:strings[i]=number(2);break;
    case 3:case 4:case 9:case 10:case 11:case 12:case 17:case 18:number(4);break;
    case 5:case 6:number(4);number(4);++i;break;
    case 7:case 16:case 19:case 20:number(2);break;
    case 15:number(1);number(2);break;
    default:fail("Invalid constant pool tag");}}
    number(2);number(2);number(2);const int interfaces=number(2);for(int i=0;i<interfaces;++i)number(2);
    QMap<QString,QString> result;const int fields=number(2);
    for(int i=0;i<fields;++i){number(2);const auto name=utf.value(number(2));number(2);const int attrs=number(2);
        for(int a=0;a<attrs;++a){const auto attr=utf.value(number(2));const quint32 len=number(4);if(len>quint32(data.size()-pos))fail("Invalid class attribute");
            if(attr=="ConstantValue"&&len==2)result[name]=utf.value(strings.value(number(2)));else pos+=int(len);}}
    return result;
}
}
QJsonArray parseOptifineBuilds(const QByteArray &html,const QString &game) {
    QJsonArray result;QSet<QString> seen;const auto text=QString::fromUtf8(html);
    QRegularExpression rows("<tr\\b[^>]*>(.*?)</tr>",QRegularExpression::DotMatchesEverythingOption|QRegularExpression::CaseInsensitiveOption);
    auto matches=rows.globalMatch(text);
    while(matches.hasNext()) {
        const auto row=matches.next().captured(1);
        const auto file=QRegularExpression("adloadx\\?f=((?:preview_)?OptiFine_[A-Za-z0-9._]+\\.jar)").match(row).captured(1);
        const auto id=QRegularExpression("^(preview_)?OptiFine_(.+?)_(HD_[A-Z]_[A-Za-z0-9_]+)\\.jar$").match(file);
        if(!id.hasMatch()||id.captured(2)!=game||seen.contains(file))continue;
        const auto forge=QRegularExpression("class=['\"]colForge['\"][^>]*>\\s*Forge\\s+([^<]+)").match(row).captured(1).trimmed();
        result.append(QJsonObject{{"file",file},{"game",game},{"version",id.captured(3)},{"preview",!id.captured(1).isEmpty()},
            {"forge",forge},{"page","https://optifine.net/adloadx?f="+file}});seen.insert(file);
    }
    return result;
}
QJsonArray parseBmclOptifineBuilds(const QByteArray &bytes,const QString &game) {
    const auto doc=QJsonDocument::fromJson(bytes);if(!doc.isArray())fail(QCoreApplication::translate("M2CL", "BMCLAPI OptiFine 列表格式无效"));
    QJsonArray result;QSet<QString> seen;
    for(const auto &value:doc.array()) {
        const auto item=value.toObject();if(item.value("mcversion")!=game)continue;
        const auto version=item.value("type").toString()+"_"+item.value("patch").toString();
        if(!QRegularExpression("^HD_[A-Z]_[A-Za-z0-9_]+$").match(version).hasMatch()||seen.contains(version))continue;
        const bool preview=version.contains("_pre");const auto file=(preview?"preview_OptiFine_":"OptiFine_")+game+"_"+version+".jar";
        if(item.value("filename")!=file)continue;
        auto forge=item.value("forge").toString();forge.remove(QRegularExpression("^Forge\\s+"));
        result.append(QJsonObject{{"file",file},{"game",game},{"version",version},{"preview",preview},{"forge",forge.trimmed()},{"listSource","BMCLAPI"},{"page","https://optifine.net/adloadx?f="+file}});seen.insert(version);
    }
    QList<QJsonObject> ordered;for(const auto &v:result)ordered.append(v.toObject());
    QCollator collator;collator.setNumericMode(true);
    std::stable_sort(ordered.begin(),ordered.end(),[&collator](const QJsonObject &a,const QJsonObject &b){return collator.compare(a.value("version").toString(),b.value("version").toString())>0;});
    QJsonArray out;for(const auto &v:ordered)out.append(v);return out;
}
VersionList fetchOptifineBuilds(const QString &game,const CancelFlag &flag,const QString &source) {
    const QStringList providers=downloadSourceOrder(source);
    QStringList errors;
    for(const auto &provider:providers)try {
        check(flag);const auto bytes=fetch(provider=="official"?QUrl("https://optifine.net/downloads"):QUrl("https://bmclapi2.bangbang93.com/optifine/"+QString::fromLatin1(QUrl::toPercentEncoding(game))),flag);
        if(provider=="official"&&!bytes.contains("adloadx?f="))fail(QCoreApplication::translate("M2CL", "OptiFine 官网列表格式无效"));
        return {provider=="official"?parseOptifineBuilds(bytes,game):parseBmclOptifineBuilds(bytes,game),{}};
    } catch(const std::exception &e){errors.append(QString::fromUtf8(e.what()));if(flag->load())break;}
    return {{},errors.join("\n")};
}
QJsonObject inspectOptifine(const QString &jar) {
    if(!jarEntry(jar,"optifine/Patcher.class").startsWith(QByteArray::fromHex("cafebabe")))fail(QCoreApplication::translate("M2CL", "此文件没有受支持的 OptiFine 补丁器"));
    for(const auto &entry:{"net/optifine/Config.class","notch/net/optifine/Config.class","Config.class"}) {
        const auto data=jarEntry(jar,entry);if(data.isEmpty())continue;const auto fields=constants(data);
        const auto game=fields.value("MC_VERSION"),version=fields.value("OF_EDITION")+"_"+fields.value("OF_RELEASE");
        if(game.isEmpty()||fields.value("OF_RELEASE").isEmpty()||!safeRelativePath(game)||!safeRelativePath(version)||game.contains('/')||version.contains('/'))continue;
        return {{"game",game},{"version",version}};
    }
    fail(QCoreApplication::translate("M2CL", "无法读取 OptiFine 内部版本信息"));return {};
}
QString optifineCompatibility(const QJsonObject &build,const QString &game,const QString &loader,const QString &loaderVersion) {
    if(build.value("game")!=game)return QCoreApplication::translate("M2CL", "OptiFine 与 Minecraft 版本不一致");
    if(loader=="vanilla")return {};
    if(loader!="forge")return QCoreApplication::translate("M2CL", "当前仅接入原版 / Forge 的 OptiFine 安装；请勿与 Fabric、Quilt、NeoForge 或 Iris 混装");
    const auto forge=build.value("forge").toString();
    if(!QRegularExpression("^\\d+\\.\\d+\\.\\d+(?:\\.\\d+)?$").match(forge).hasMatch())return QCoreApplication::translate("M2CL", "官网未明确标注可匹配的 Forge 构建，此组合暂不安装");
    if(forge!=loaderVersion)return QCoreApplication::translate("M2CL", "官网标注 Forge %1，当前为 %2。请先使用匹配的 Forge 构建；不会自动替换加载器。").arg(forge,loaderVersion);
    return {};
}
OptifineChange::~OptifineChange() {
    if(!m_finished){if(m_added)QFile::remove(m_target);if(m_changed&&!m_backup.isEmpty())QFile::rename(m_backup,m_oldTarget);}
    else if(!m_backup.isEmpty())QFile::remove(m_backup);
    if(!m_stage.isEmpty())QFile::remove(m_stage);
}
void OptifineChange::prepare(const InstallRequest &req,QJsonObject &metadata,const QString &java,const CancelFlag &flag,const InstallProgress &progress) {
    if(req.optifineAction.isEmpty())return;
    check(flag);if(!QDir().mkpath(checkedPath(req.directory,"mods")))fail(QCoreApplication::translate("M2CL", "无法创建模组目录"));
    m_lock=std::make_unique<QLockFile>(checkedPath(req.directory,"mods/.m2cl-install.lock"));
    if(!m_lock->tryLock())fail(QCoreApplication::translate("M2CL", "该实例已有模组或版本组件操作正在进行"));
    const auto old=metadata.value("_m2clOptifine").toObject();
    if(req.optifineAction=="remove") {
        if(old.isEmpty()||old.value("base").toObject().isEmpty())fail(QCoreApplication::translate("M2CL", "此 OptiFine 不是由 M²CL 安装，无法自动还原版本配置"));
    } else if(req.optifineAction!="install")return;
    if(!old.isEmpty()) {
        const auto path=old.value("modFile").toString();
        if(!path.isEmpty()){m_oldTarget=checkedPath(req.directory,"mods/"+path);m_oldHash=old.value("sha256").toString();if(QFileInfo::exists(m_oldTarget)&&hash(m_oldTarget)!=m_oldHash)fail(QCoreApplication::translate("M2CL", "已有 OptiFine 文件已被外部修改，请先核对"));}
        metadata=old.value("base").toObject();if(metadata.isEmpty())fail("Missing OptiFine base metadata");
    }
    if(req.optifineAction=="remove")return;
    const auto components=gameLoaderComponents(metadata);const auto loader=components.isEmpty()?QString("vanilla"):components.firstKey();
    const auto problem=optifineCompatibility(req.optifineBuild,req.version,loader,components.value(loader));if(!problem.isEmpty())fail(problem);
    Instance instance;instance.gameDirectory=req.directory;instance.loader=loader;
    for(const auto &mod:localMods(instance))if(mod.enabled) {
        for(const auto &id:mod.ids)if(QStringList{"iris","sodium","oculus","optifabric"}.contains(id))fail(QCoreApplication::translate("M2CL", "请先在模组管理中停用 %1，再安装 OptiFine").arg(mod.name));
        if(mod.ids.contains("optifine")&&QDir(req.directory).filePath("mods/"+mod.fileName)!=m_oldTarget)fail(QCoreApplication::translate("M2CL", "已存在手动安装的 OptiFine，请先移除，避免重复加载"));
    }
    check(flag);QByteArray bytes;
    if(!req.optifineLocalFile.isEmpty()) { if(progress)progress(QCoreApplication::translate("M2CL", "读取本地 OptiFine 安装包…"),-1); bytes=read(req.optifineLocalFile); }
    else {
        const QStringList providers=downloadSourceOrder(req.downloadSource);
        QStringList errors;
        for(const auto &provider:providers)try {
            check(flag);if(progress)progress(QCoreApplication::translate("M2CL", "%1OptiFine 安装包 · %2").arg(errors.isEmpty()?QCoreApplication::translate("M2CL", "下载 "):QCoreApplication::translate("M2CL", "切换备用源下载 "),provider=="official"?QCoreApplication::translate("M2CL", "官方源"):"BMCLAPI"),-1);
            if(provider=="bmclapi")bytes=fetch(mirrorPackage(req.optifineBuild),flag,progress);
            else {
                const auto page=fetch(QUrl(req.optifineBuild.value("page").toString()),flag,progress);
                const auto match=QRegularExpression("href=['\"](downloadx\\?[^'\"]+)['\"]").match(QString::fromUtf8(page));
                if(!match.hasMatch())fail(QCoreApplication::translate("M2CL", "官方页面没有下载链接，请通过项目官网查看"));
                const QUrl url("https://optifine.net/"+match.captured(1).replace("&amp;","&"));
                if(QUrlQuery(url).queryItemValue("f")!=req.optifineBuild.value("file").toString())fail("OptiFine download identity mismatch");
                bytes=fetch(url,flag,progress);
            }
            QTemporaryDir checkDir;const auto checkJar=checkDir.path()+"/check.jar";save(checkJar,bytes);const auto found=inspectOptifine(checkJar);
            if(found.value("game")!=req.version||found.value("version")!=req.optifineBuild.value("version"))fail(QCoreApplication::translate("M2CL", "OptiFine 安装包版本不匹配"));
            break;
        } catch(const std::exception &e){bytes.clear();errors.append(QString::fromUtf8(e.what()));if(flag->load())break;}
        if(bytes.isEmpty())fail(errors.join("\n")+QCoreApplication::translate("M2CL", "\n可切换下载源或选择本地安装包。"));
    }
    QTemporaryDir work;if(!work.isValid())fail("Cannot create OptiFine workspace");
    const auto installer=work.path()+"/installer.jar";save(installer,bytes);const auto identity=inspectOptifine(installer);
    if(identity.value("game")!=req.version||identity.value("version")!=req.optifineBuild.value("version"))fail(QCoreApplication::translate("M2CL", "OptiFine 安装包内部版本与所选构建不一致"));
    QJsonObject component=req.optifineBuild;component.insert("base",metadata);component.insert("sha256",hash(installer));component.insert("sha512",QString::fromLatin1(fileSha512(installer)));component.insert("mode",loader);
    if(loader=="forge") {
        const auto file="OptiFine_"+req.version+"_"+identity.value("version").toString()+".jar";
        m_target=checkedPath(req.directory,"mods/"+file);
        if(QFileInfo::exists(m_target)&&m_target!=m_oldTarget)fail(QCoreApplication::translate("M2CL", "目标文件已存在，请先检查手动安装的副本"));
        m_stage=checkedPath(req.directory,"mods/.m2cl-optifine-"+QUuid::createUuid().toString(QUuid::Id128)+".tmp");save(m_stage,bytes);component.insert("modFile",file);
    } else {
        if(progress)progress(QCoreApplication::translate("M2CL", "生成 OptiFine 客户端补丁…"),-1);
        const auto client=checkedPath(req.root,"versions/"+req.id+"/"+req.id+".jar"),patched=work.path()+"/patched.jar";
        QProcess process;process.setWorkingDirectory(work.path());process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(java,{"-Djava.awt.headless=true","-cp",installer,"optifine.Patcher",client,installer,patched});
        if(!process.waitForStarted(10000))fail(QCoreApplication::translate("M2CL", "无法启动 OptiFine 补丁器"));
        QElapsedTimer elapsed;elapsed.start();QByteArray tail;
        while(process.state()!=QProcess::NotRunning){process.waitForFinished(100);tail=(tail+process.readAll()).right(8192);if(flag->load()||elapsed.elapsed()>120000){process.kill();process.waitForFinished(5000);fail(QCoreApplication::translate("M2CL", "OptiFine 补丁已取消或超时"));}}
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)fail(QCoreApplication::translate("M2CL", "OptiFine 补丁失败：%1").arg(QString::fromUtf8(tail)));
        if(!jarEntry(patched,"net/optifine/Config.class").startsWith(QByteArray::fromHex("cafebabe"))&&!jarEntry(patched,"notch/net/optifine/Config.class").startsWith(QByteArray::fromHex("cafebabe"))&&!jarEntry(patched,"Config.class").startsWith(QByteArray::fromHex("cafebabe")))fail(QCoreApplication::translate("M2CL", "OptiFine 补丁输出无效"));
        auto wrapperVersion=QString::fromUtf8(jarEntry(installer,"launchwrapper-of.txt")).trimmed();
        if(!QRegularExpression("^\\d+\\.\\d+$").match(wrapperVersion).hasMatch())fail(QCoreApplication::translate("M2CL", "此旧版 OptiFine 启动包装器尚未接入"));
        const auto wrapper=jarEntry(installer,"launchwrapper-of-"+wrapperVersion+".jar");if(wrapper.isEmpty())fail("Missing OptiFine launchwrapper");
        // Content-addressed private libraries keep older instances and failed upgrades intact.
        const auto key=component.value("sha256").toString();QJsonArray files,libraries=metadata.value("libraries").toArray();
        libraries.prepend(localLibrary(req.root,"optifine:launchwrapper-of:"+wrapperVersion,"m2cl-optifine/"+key+"/launchwrapper.jar",wrapper,files));
        libraries.prepend(localLibrary(req.root,"optifine:OptiFine:"+req.version+"_"+identity.value("version").toString(),"m2cl-optifine/"+key+"/OptiFine.jar",read(patched),files));
        metadata.insert("libraries",libraries);metadata.insert("mainClass","net.minecraft.launchwrapper.Launch");
        if(metadata.contains("minecraftArguments"))metadata.insert("minecraftArguments",metadata.value("minecraftArguments").toString()+" --tweakClass optifine.OptiFineTweaker");
        else {auto args=metadata.value("arguments").toObject();auto game=args.value("game").toArray();game.append("--tweakClass");game.append("optifine.OptiFineTweaker");args.insert("game",game);metadata.insert("arguments",args);}
        component.insert("files",files);
    }
    check(flag);metadata.insert("_m2clOptifine",component);
}
void OptifineChange::commitFiles() {
    if(!m_oldTarget.isEmpty()&&QFileInfo::exists(m_oldTarget)) {
        if(hash(m_oldTarget)!=m_oldHash)fail(QCoreApplication::translate("M2CL", "安装过程中 OptiFine 文件发生变化，请重新确认"));
        m_backup=m_oldTarget+".m2cl-"+QUuid::createUuid().toString(QUuid::Id128)+".rollback";
        if(!QFile::rename(m_oldTarget,m_backup))fail(QCoreApplication::translate("M2CL", "无法替换已有 OptiFine"));m_changed=true;
    }
    if(!m_stage.isEmpty()){if(!QFile::rename(m_stage,m_target))fail(QCoreApplication::translate("M2CL", "无法写入 OptiFine，请检查重复文件或目录权限"));m_added=true;}
}
void verifyOptifine(const QJsonObject &metadata,const QString &root,const QString &directory) {
    const auto component=metadata.value("_m2clOptifine").toObject();if(component.isEmpty())return;
    const auto components=gameLoaderComponents(metadata);const auto loader=components.isEmpty()?QString("vanilla"):components.firstKey();
    const auto problem=optifineCompatibility(component,metadata.value("_baseId").toString(metadata.value("id").toString()),loader,components.value(loader));if(!problem.isEmpty())fail(problem);
    const auto modFile=component.value("modFile").toString();if(!modFile.isEmpty()&&hash(checkedPath(directory,"mods/"+modFile))!=component.value("sha256").toString())fail(QCoreApplication::translate("M2CL", "OptiFine 文件缺失或已变化，请从版本组件重新安装"));
    for(const auto &v:component.value("files").toArray()){const auto f=v.toObject();if(hash(checkedPath(root,f.value("path").toString()))!=f.value("sha256").toString())fail(QCoreApplication::translate("M2CL", "OptiFine 库文件已变化，请重新安装"));}
}
}
