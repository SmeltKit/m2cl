// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "forgeinstaller.h"
#include "core/mods.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QVersionNumber>
#include <QXmlStreamReader>
#include <algorithm>
#include <stdexcept>
namespace mc {
namespace {
void fail(const QString &s){throw std::runtime_error(s.toUtf8().constData());}
QString repository(const QString &loader){if(loader=="forge")return "https://maven.minecraftforge.net/net/minecraftforge/forge/";if(loader=="neoforge")return "https://maven.neoforged.net/releases/net/neoforged/neoforge/";fail("Unsupported loader");return {};}
void save(const QString &path,const QByteArray &data){QDir().mkpath(QFileInfo(path).path());QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())fail("Cannot save "+path);}
QString hashFile(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))fail("Cannot read "+path);QCryptographicHash hash(QCryptographicHash::Sha1);if(!hash.addData(&f))fail("Cannot hash "+path);return QString::fromLatin1(hash.result().toHex());}
void copy(const QString &source,const QString &dest){QFile in(source);QDir().mkpath(QFileInfo(dest).path());QSaveFile out(dest);if(!in.open(QIODevice::ReadOnly)||!out.open(QIODevice::WriteOnly))fail("Cannot copy installer output");while(!in.atEnd()){auto b=in.read(1024*1024);if(b.isEmpty()&&in.error()!=QFile::NoError)fail("Cannot read installer output");if(out.write(b)!=b.size())fail("Cannot write installer output");}if(!out.commit())fail("Cannot commit installer output");}
QJsonObject json(const QByteArray &bytes){QJsonParseError e;auto d=QJsonDocument::fromJson(bytes,&e);if(e.error!=QJsonParseError::NoError||!d.isObject())fail(QCoreApplication::translate("M2CL", "加载器元数据无效"));return d.object();}
QString neoGame(const QString &v){auto p=v.section('-',0,0).split('.');if(p.size()<3)return {};if(p[0].toInt()<20)return {};if(p[0].toInt()>=26){if(p.size()!=4)return {};return p[0]+"."+p[1]+(p[2]=="0"?QString():"."+p[2]);}return "1."+p[0]+(p[1]=="0"?QString():"."+p[1]);}
}
QString forgeMavenPath(const QString &coordinate){
 const auto m=QRegularExpression("^([A-Za-z0-9_.-]+):([A-Za-z0-9_.-]+):([A-Za-z0-9_.+-]+)(?::([A-Za-z0-9_.-]+))?(?:@([A-Za-z0-9]+))?$").match(coordinate);
 if(!m.hasMatch())fail(QCoreApplication::translate("M2CL", "无效的 Maven 坐标"));auto group=m.captured(1);group.replace('.','/');const auto ext=m.captured(5).isEmpty()?QString("jar"):m.captured(5);const auto path=group+"/"+m.captured(2)+"/"+m.captured(3)+"/"+m.captured(2)+"-"+m.captured(3)+(m.captured(4).isEmpty()?QString():"-"+m.captured(4))+"."+ext;if(!safeRelativePath(path))fail(QCoreApplication::translate("M2CL", "不安全的 Maven 路径"));return path;
}
QJsonArray forgeVersionsFromXml(const QByteArray &bytes,const QString &loader,const QString &game){
 repository(loader);QXmlStreamReader xml(bytes);QStringList versions;
 while(!xml.atEnd()){xml.readNext();if(xml.isStartElement()&&xml.name()==QStringLiteral("version")){
  auto v=xml.readElementText();if(loader=="forge"){if(v.startsWith(game+"-")&&QVersionNumber::fromString(game)>=QVersionNumber(1,13))versions.append(v.mid(game.size()+1));}
  else if(neoGame(v)==game)versions.append(v);
 }}if(xml.hasError())fail(QCoreApplication::translate("M2CL", "加载器版本列表格式无效"));versions.removeDuplicates();
 std::sort(versions.begin(),versions.end(),[](const QString &a,const QString &b){return QVersionNumber::fromString(a)>QVersionNumber::fromString(b);});QJsonArray out;
 for(const auto &v:versions)out.append(QJsonObject{{"loader",QJsonObject{{"version",v},{"stable",!v.contains("beta")&&!v.contains("alpha")}}}});return out;
}
VersionList fetchForgeVersions(const QString &loader,const QString &game,const CancelFlag &flag,const QString &source){try{return {forgeVersionsFromXml(fetchInstallData(QUrl(repository(loader)+"maven-metadata.xml"),flag,{},nullptr,source),loader,game),{}};}catch(const std::exception &e){return {{},QString::fromUtf8(e.what())};}}
QUrl forgeInstallerUrl(const QString &loader,const QString &game,const QString &version){
 if(!QRegularExpression("^[0-9]+(?:\\.[0-9]+)+(?:-[A-Za-z0-9.]+)?$").match(version).hasMatch()||!QRegularExpression("^[0-9]+(?:\\.[0-9]+)+$").match(game).hasMatch())fail(QCoreApplication::translate("M2CL", "加载器版本无效"));
 if(loader=="neoforge"&&neoGame(version)!=game)fail(QCoreApplication::translate("M2CL", "NeoForge 构建与游戏版本不匹配"));
 const auto coordinate=loader=="forge"?game+"-"+version:version;return QUrl(repository(loader)+coordinate+"/"+loader+"-"+coordinate+"-installer.jar");
}
void verifyForgeOutputs(const QJsonObject &metadata,const QString &root){
 const auto outputs=metadata.value("_m2clLoaderFiles").toArray();if(outputs.isEmpty())fail(QCoreApplication::translate("M2CL", "此加载器缺少安装记录，请重新安装加载器"));
 for(const auto &v:outputs){auto o=v.toObject();auto path=checkedPath(root,o.value("path").toString());if(!QFileInfo::exists(path)||QFileInfo(path).size()!=qint64(o.value("size").toDouble())||hashFile(path)!=o.value("sha1").toString())fail(QCoreApplication::translate("M2CL", "加载器文件缺失或损坏，请重新安装加载器：%1").arg(o.value("path").toString()));}
}
QJsonObject installForgeClient(const InstallRequest &req,const QJsonObject &base,const QString &java,const CancelFlag &flag,const InstallProgress &progress,QNetworkAccessManager *network){
 auto check=[&]{if(flag->load())fail(QCoreApplication::translate("M2CL", "加载器安装已取消"));};auto report=[&](const QString &s){check();if(progress)progress(s,-1);};
 const QString game=base.value("id").toString();const auto url=forgeInstallerUrl(req.loader,game,req.loaderVersion);
 report(QCoreApplication::translate("M2CL", "下载 %1 官方安装器…").arg(req.loader));const auto sha=QString::fromLatin1(fetchInstallData(QUrl(url.toString()+".sha1"),flag,{},network,req.downloadSource,progress)).trimmed().section(' ',0,0).toLower();
 if(!QRegularExpression("^[a-f0-9]{40}$").match(sha).hasMatch())fail(QCoreApplication::translate("M2CL", "安装器校验值无效"));
 const auto bytes=fetchInstallData(url,flag,sha,network,req.downloadSource,progress);
 const auto workRoot=checkedPath(req.root,".m2cl-loader-work");QDir().mkpath(workRoot);QTemporaryDir work(workRoot+"/install-XXXXXX");if(!work.isValid())fail(QCoreApplication::translate("M2CL", "无法创建加载器暂存目录"));
 const auto stage=work.path();const auto installer=stage+"/installer.jar";save(installer,bytes);const auto profile=json(jarEntry(installer,"install_profile.json"));
 if(profile.value("spec").toInt()!=1||profile.value("minecraft").toString()!=game)fail(QCoreApplication::translate("M2CL", "此安装器格式或游戏版本暂不支持"));
 const auto child=json(jarEntry(installer,"version.json"));const auto versionId=child.value("id").toString();
 if(!safeRelativePath(versionId)||versionId.contains('/')||child.value("inheritsFrom").toString()!=game)fail(QCoreApplication::translate("M2CL", "加载器启动配置不匹配"));
 downloadVerifiedGameFiles({gameFile(base.value("downloads").toObject().value("client").toObject(),"versions/"+game+"/"+game+".jar")},stage,flag,progress,network,req.downloadSource);
 // Resolve official processor placeholders and Maven coordinates inside the staging root.
 QMap<QString,QString> vars{{"SIDE","client"},{"ROOT",stage},{"LIBRARY_DIR",stage+"/libraries"},{"INSTALLER",installer},{"MINECRAFT_JAR",stage+"/versions/"+game+"/"+game+".jar"},{"MINECRAFT_VERSION",game}};
 const auto data=profile.value("data").toObject();for(auto it=data.begin();it!=data.end();++it)vars.insert(it.key(),it.value().toObject().value("client").toString());
 std::function<QString(QString,int)> resolve=[&](QString value,int depth)->QString{
  if(depth>24)fail(QCoreApplication::translate("M2CL", "安装描述存在循环引用"));
  if(value.startsWith('\'')&&value.endsWith('\''))return value.mid(1,value.size()-2);
  const QRegularExpression variable("\\{([A-Z0-9_]+)\\}");auto match=variable.match(value);
  while(match.hasMatch()){const auto key=match.captured(1);if(!vars.contains(key))fail(QCoreApplication::translate("M2CL", "未知安装变量：%1").arg(key));value.replace(match.capturedStart(),match.capturedLength(),resolve(vars.value(key),depth+1));match=variable.match(value);}
  if(value.startsWith('[')&&value.endsWith(']'))return checkedPath(stage,"libraries/"+forgeMavenPath(value.mid(1,value.size()-2)));
  if(value.startsWith("/data/")){const auto dest=checkedPath(stage,value.mid(1));if(!QFileInfo::exists(dest))save(dest,jarEntry(installer,value.mid(1),64*1024*1024));return dest;}
  if(value.contains('{')||value.contains('}'))fail(QCoreApplication::translate("M2CL", "无法解析安装参数"));return value;
 };
 QMap<QString,QJsonObject> artifacts;auto libs=profile.value("libraries").toArray();for(const auto &v:child.value("libraries").toArray())libs.append(v);
 QList<GameFile> downloads;
 for(const auto &v:libs){const auto lib=v.toObject();const auto a=lib.value("downloads").toObject().value("artifact").toObject();if(a.isEmpty())fail(QCoreApplication::translate("M2CL", "不支持的加载器库格式"));const auto relative="libraries/"+a.value("path").toString();checkedPath(stage,relative);
  if(artifacts.contains(relative)){if(artifacts.value(relative).value("sha1")!=a.value("sha1"))fail(QCoreApplication::translate("M2CL", "安装库声明冲突"));continue;}artifacts.insert(relative,a);
  if(a.value("url").toString().isEmpty()){
   const auto bytes=jarEntry(installer,"maven/"+a.value("path").toString(),64*1024*1024);
   if(bytes.isEmpty()||bytes.size()!=qint64(a.value("size").toDouble())||QCryptographicHash::hash(bytes,QCryptographicHash::Sha1).toHex()!=a.value("sha1").toString().toLatin1())fail(QCoreApplication::translate("M2CL", "内置安装库校验失败"));save(checkedPath(stage,relative),bytes);
  }else downloads.append(gameFile(a,relative));
 }
 // Reuse our verified download cache. Processors never own networking or launcher profiles.
 report(QCoreApplication::translate("M2CL", "下载 %1 安装与运行依赖…").arg(req.loader));downloadVerifiedGameFiles(downloads,req.root,flag,progress,network,req.downloadSource);
 for(const auto &f:downloads){check();copy(checkedPath(req.root,f.path),checkedPath(stage,f.path));}
 const auto processors=profile.value("processors").toArray();int step=0,total=0;
 auto clientSide=[](const QJsonObject &p){const auto sides=p.value("sides").toArray();return sides.isEmpty()||sides.contains("client");};
 for(const auto &v:processors)if(clientSide(v.toObject()))++total;
 for(const auto &v:processors){auto processor=v.toObject();if(!clientSide(processor))continue;check();++step;
  report(QCoreApplication::translate("M2CL", "处理 %1 客户端补丁 · %2 / %3").arg(req.loader).arg(step).arg(total));QStringList args;for(const auto &a:processor.value("args").toArray())args.append(resolve(a.toString(),0));
  const auto outputs=processor.value("outputs").toObject();auto verifyOutputs=[&](bool required){bool valid=true;for(auto it=outputs.begin();it!=outputs.end();++it){const auto path=resolve(it.key(),0);const auto relative=QDir(stage).relativeFilePath(path);checkedPath(stage,relative);const auto hash=resolve(it.value().toString(),0);if(!QRegularExpression("^[a-fA-F0-9]{40}$").match(hash).hasMatch())fail(QCoreApplication::translate("M2CL", "补丁输出校验值无效"));if(!QFileInfo::exists(path)||hashFile(path)!=hash.toLower())valid=false;}if(required&&!valid)fail(QCoreApplication::translate("M2CL", "补丁输出校验失败"));return valid;};
  if(!outputs.isEmpty()&&verifyOutputs(false))continue;
  const int task=args.indexOf("--task");
  if(task>=0&&args.value(task+1)=="DOWNLOAD_MOJMAPS"){
   const int output=args.indexOf("--output");if(output<0||args.value(output+1).isEmpty())fail(QCoreApplication::translate("M2CL", "映射下载缺少输出路径"));
   const auto relative=QDir(stage).relativeFilePath(args[output+1]);const auto descriptor=base.value("downloads").toObject().value("client_mappings").toObject();
   downloadVerifiedGameFiles({gameFile(descriptor,relative)},stage,flag,progress,network,req.downloadSource);verifyOutputs(true);continue;
  }
  const auto jar=checkedPath(stage,"libraries/"+forgeMavenPath(processor.value("jar").toString()));
  if(!artifacts.contains(QDir(stage).relativeFilePath(jar)))fail(QCoreApplication::translate("M2CL", "补丁工具未在已校验依赖中声明"));
  auto manifest=QString::fromUtf8(jarEntry(jar,"META-INF/MANIFEST.MF"));manifest.replace("\r\n ","");manifest.replace("\n ","");const auto main=QRegularExpression("(?:^|\\n)Main-Class: ([A-Za-z_$][A-Za-z0-9_$.]*)\\r?(?:\\n|$)").match(manifest);
  if(!main.hasMatch())fail(QCoreApplication::translate("M2CL", "补丁工具缺少有效主类"));QStringList cp{jar};
  for(const auto &c:processor.value("classpath").toArray()){const auto relative="libraries/"+forgeMavenPath(c.toString());if(!artifacts.contains(relative))fail(QCoreApplication::translate("M2CL", "补丁工具依赖未声明"));cp.append(checkedPath(stage,relative));}cp.removeDuplicates();
  QProcess process;process.setWorkingDirectory(stage);process.setProcessChannelMode(QProcess::MergedChannels);
  QStringList command{"-Djava.awt.headless=true","-cp",cp.join(QDir::listSeparator()),main.captured(1)};command+=args;process.start(java,command);
  if(!process.waitForStarted(10000))fail(QCoreApplication::translate("M2CL", "无法启动补丁工具：%1").arg(process.errorString()));QElapsedTimer elapsed;elapsed.start();QByteArray tail;
  while(process.state()!=QProcess::NotRunning){process.waitForFinished(100);tail=(tail+process.readAll()).right(8192);if(flag->load()||elapsed.elapsed()>10*60*1000){process.kill();process.waitForFinished(10000);fail(flag->load()?QCoreApplication::translate("M2CL", "加载器安装已取消"):QCoreApplication::translate("M2CL", "客户端补丁处理超时"));}}
  tail=(tail+process.readAll()).right(8192);if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)fail(QCoreApplication::translate("M2CL", "补丁步骤 %1 / %2 失败：\n%3").arg(step).arg(total).arg(QString::fromUtf8(tail)));verifyOutputs(true);
 }
 check();auto actual=child;
 for(const auto &key:QStringList{"PATCHED","MC_EXTRA"})if(vars.contains(key)&&!QFileInfo::exists(resolve(vars.value(key),0)))fail(QCoreApplication::translate("M2CL", "缺少生成的客户端文件：%1").arg(key));
 QJsonArray manifest;QDirIterator it(stage+"/libraries",QDir::Files|QDir::NoSymLinks,QDirIterator::Subdirectories);
 while(it.hasNext()){check();const auto source=it.next();const auto relative=QDir(stage).relativeFilePath(source);const auto dest=checkedPath(req.root,relative);const auto hash=hashFile(source);manifest.append(QJsonObject{{"path",relative},{"sha1",hash},{"size",double(QFileInfo(source).size())}});if(!QFileInfo::exists(dest)||hashFile(dest)!=hash)copy(source,dest);}
 if(manifest.isEmpty())fail(QCoreApplication::translate("M2CL", "加载器未生成运行库"));actual.insert("_m2clLoaderFiles",manifest);actual.insert("_m2clLoader",req.loader);actual.insert("_m2clLoaderVersion",req.loaderVersion);actual.insert("_m2clInstallerSha1",sha);return actual;
}
}
