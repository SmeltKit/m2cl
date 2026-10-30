// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "packs.h"
#include "mods.h"
#include "gameplan.h"
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QLockFile>
#include <QUuid>
#include <QtEndian>
#include <stdexcept>
namespace mc {
namespace {
void fail(const QString &s){throw std::runtime_error(s.toUtf8().constData());}
QStringList entries(const QString &path){
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>1024LL*1024*1024)fail(QCoreApplication::translate("M2CL", "资源包无法读取或超过 1 GiB"));
    f.seek(qMax(qint64(0),f.size()-65557));auto tail=f.readAll();int e=-1;for(int i=tail.size()-22;i>=0;--i)if(tail.mid(i,4)==QByteArray::fromHex("504b0506")){e=i;break;}
    if(e<0)fail(QCoreApplication::translate("M2CL", "不是有效 ZIP 文件"));auto u16=[](const char *p){return qFromLittleEndian<quint16>(p);};auto u32=[](const char *p){return qFromLittleEndian<quint32>(p);};
    const auto n=u16(tail.constData()+e+10);if(n==65535||u16(tail.constData()+e+4)||u16(tail.constData()+e+6)||n>20000)fail(QCoreApplication::translate("M2CL", "不支持此 ZIP 格式或文件过多"));f.seek(u32(tail.constData()+e+16));QStringList result;qint64 total=0;
    for(int i=0;i<n;++i){auto h=f.read(46);if(h.size()!=46||h.left(4)!=QByteArray::fromHex("504b0102"))fail(QCoreApplication::translate("M2CL", "ZIP 目录损坏"));const auto len=u16(h.constData()+28);auto name=QString::fromUtf8(f.read(len));f.seek(f.pos()+u16(h.constData()+30)+u16(h.constData()+32));
        auto clean=name;if(clean.endsWith('/'))clean.chop(1);if(!safeRelativePath(clean)||((u32(h.constData()+38)>>16)&0170000)==0120000||u16(h.constData()+8)&1)fail(QCoreApplication::translate("M2CL", "资源包含不安全路径、链接或加密文件"));
        total+=u32(h.constData()+24);if(total>2LL*1024*1024*1024)fail(QCoreApplication::translate("M2CL", "资源包解压体积过大"));result.append(name);
    }return result;
}
QStringList directoryEntries(const QString &path){QStringList out;QDirIterator it(path,QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden,QDirIterator::Subdirectories);qint64 total=0;while(it.hasNext()){it.next();auto info=it.fileInfo();if(info.isSymLink())fail(QCoreApplication::translate("M2CL", "资源目录不能包含符号链接"));auto relative=QDir(path).relativeFilePath(info.filePath());if(!safeRelativePath(relative))fail(QCoreApplication::translate("M2CL", "资源路径无效"));if(info.isFile()){out.append(relative);total+=info.size();}if(out.size()>20000||total>2LL*1024*1024*1024)fail(QCoreApplication::translate("M2CL", "资源目录过大"));}return out;}
}
QString packFolder(PackKind kind){return kind==PackKind::Shader?"shaderpacks":"resourcepacks";}
LocalPack inspectPack(const QString &path,PackKind kind){
    LocalPack p;p.fileName=QFileInfo(path).fileName();p.name=QFileInfo(path).completeBaseName();p.directory=QFileInfo(path).isDir();
    try{if(QFileInfo(path).isSymLink())fail(QCoreApplication::translate("M2CL", "不能导入符号链接"));const auto files=p.directory?directoryEntries(path):entries(path);
        auto read=[&](const QString &name){if(!files.contains(name))return QByteArray();if(!p.directory)return jarEntry(path,name);QFile f(QDir(path).filePath(name));if(!f.open(QIODevice::ReadOnly)||f.size()>2*1024*1024)fail(QCoreApplication::translate("M2CL", "资源元数据过大"));return f.readAll();};
        if(kind==PackKind::Shader){bool shader=false;for(const auto &name:files)if(name.startsWith("shaders/")&&(name.endsWith(".vsh")||name.endsWith(".fsh")||name.endsWith(".glsl")||name.endsWith(".csh")))shader=true;if(!shader)fail(QCoreApplication::translate("M2CL", "未找到 shaders/ 下的着色器；请选择光影包本体"));}
        else{auto doc=QJsonDocument::fromJson(read("pack.mcmeta"));if(!doc.isObject()||!doc.object().value("pack").isObject())fail(QCoreApplication::translate("M2CL", "缺少有效 pack.mcmeta，请选择资源包本体"));const auto pack=doc.object().value("pack").toObject();p.format=pack.value("pack_format").toInt();auto description=pack.value("description");p.description=description.isString()?description.toString():QString::fromUtf8(QJsonDocument(description.toObject()).toJson(QJsonDocument::Compact));}
        p.icon=read("pack.png");if(!p.directory)p.sha512=QString::fromLatin1(fileSha512(path));
    }catch(const std::exception &e){p.problem=QString::fromUtf8(e.what());}return p;
}
QString savePackMetadata(const Instance &i,PackKind kind,const ResourceProject &project,const ResourceFile &file,const QByteArray &icon){
 try{
 const auto pack=inspectPack(checkedPath(i.gameDirectory,packFolder(kind)+"/"+file.fileName),kind);
 if(!pack.problem.isEmpty()||pack.sha512.isEmpty()||pack.sha512!=file.sha512)return QCoreApplication::translate("M2CL", "资源身份校验失败");
 auto folder=checkedPath(i.gameDirectory,".m2cl-pack-metadata");QDir().mkpath(folder);QSaveFile out(folder+"/"+pack.sha512+".json");
 QJsonObject data{{"name",project.name},{"description",project.description},{"version",file.versionNumber},{"icon",QString::fromLatin1(icon.size()<=2*1024*1024?icon.toBase64():QByteArray())}};
 if(!out.open(QIODevice::WriteOnly)||out.write(QJsonDocument(data).toJson())<0||!out.commit())return QCoreApplication::translate("M2CL", "无法保存资源信息");return {};
 }catch(const std::exception &e){return QString::fromUtf8(e.what());}
}
QList<LocalPack> localPacks(const Instance &i,PackKind kind){
 QList<LocalPack> out;const auto folder=checkedPath(i.gameDirectory,packFolder(kind));
 for(const auto &file:QDir(folder).entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)){
 if(!file.isDir()&&file.suffix().compare("zip",Qt::CaseInsensitive)!=0)continue;
 auto pack=inspectPack(file.filePath(),kind);
 if(!pack.sha512.isEmpty()){QFile meta(checkedPath(i.gameDirectory,".m2cl-pack-metadata/"+pack.sha512+".json"));
 if(meta.open(QIODevice::ReadOnly)&&meta.size()<3*1024*1024){auto data=QJsonDocument::fromJson(meta.readAll()).object();if(!data["name"].toString().isEmpty())pack.name=data["name"].toString();if(!data["description"].toString().isEmpty())pack.description=data["description"].toString();pack.version=data["version"].toString();auto icon=QByteArray::fromBase64(data["icon"].toString().toLatin1());if(!icon.isEmpty())pack.icon=icon;}}
 out.append(pack);
 }return out;
}
QString importPack(const Instance &i,PackKind kind,const QString &source){
    try{auto p=inspectPack(source,kind);if(!p.problem.isEmpty())fail(p.problem);if(!safeRelativePath(p.fileName)||p.fileName.contains('/'))fail(QCoreApplication::translate("M2CL", "资源名称无效"));const auto folder=checkedPath(i.gameDirectory,packFolder(kind));if(!QDir().mkpath(folder))fail(QCoreApplication::translate("M2CL", "无法创建资源目录"));QLockFile lock(folder+"/.m2cl-pack.lock");if(!lock.tryLock())fail(QCoreApplication::translate("M2CL", "资源目录正在使用"));const auto dest=checkedPath(folder,p.fileName);
        if(QFileInfo::exists(dest)){if(!p.directory&&fileSha512(dest)==p.sha512.toLatin1())return {};fail(QCoreApplication::translate("M2CL", "已有同名但内容不同的资源，请先移除旧副本（会保留备份）"));}
        QTemporaryDir stage(folder+"/.m2cl-import-XXXXXX");if(!stage.isValid())fail(QCoreApplication::translate("M2CL", "无法创建暂存目录"));const auto temp=stage.path()+"/"+p.fileName;
        if(p.directory){QDir().mkpath(temp);for(const auto &name:directoryEntries(source)){const auto to=checkedPath(temp,name);QDir().mkpath(QFileInfo(to).path());if(!QFile::copy(checkedPath(source,name),to))fail(QCoreApplication::translate("M2CL", "复制资源失败"));}}
        else if(!QFile::copy(source,temp)||QString::fromLatin1(fileSha512(temp))!=p.sha512)fail(QCoreApplication::translate("M2CL", "复制过程中资源发生变化"));
        if(!inspectPack(temp,kind).problem.isEmpty())fail(QCoreApplication::translate("M2CL", "暂存资源校验失败"));if(!QDir().rename(temp,dest))fail(QCoreApplication::translate("M2CL", "无法完成资源导入"));
    }catch(const std::exception &e){return QString::fromUtf8(e.what());}return {};
}
QString removePack(const Instance &i,PackKind kind,const QString &name){try{if(name.contains('/'))fail("Invalid filename");const auto folder=checkedPath(i.gameDirectory,packFolder(kind));QLockFile lock(folder+"/.m2cl-pack.lock");if(!lock.tryLock())fail(QCoreApplication::translate("M2CL", "资源目录正在使用"));const auto source=checkedPath(folder,name);if(!QFile::moveToTrash(source))fail(QCoreApplication::translate("M2CL", "无法移到系统废纸篓／回收站，文件未删除"));}catch(const std::exception &e){return QString::fromUtf8(e.what());}return {};}
ShaderSupport shaderSupport(const Instance &i){ShaderSupport result;bool iris=false,optifine=!i.optifineVersion.isEmpty(),sodium=false,optifabric=false;if(optifine)result.components.append("OptiFine "+i.optifineVersion);const auto mods=localMods(i);for(const auto &m:mods)if(m.enabled){for(const auto &id:m.ids){if(id=="optifabric")optifabric=true;if(id=="iris"){iris=true;result.components.append("Iris "+m.version);}if(id=="sodium"){sodium=true;result.components.append("Sodium "+m.version);}if(id=="optifine"&&!optifine){optifine=true;result.components.append("OptiFine "+m.version);}}
        if(m.known&&!modDependenciesProblem(m,mods).isEmpty())result.conflicts.append(m.name+" · "+modDependenciesProblem(m,mods));
        if(m.known&&!modEnvironmentProblem(m,i).isEmpty())result.conflicts.append(m.name+" · "+modEnvironmentProblem(m,i));
        if(!m.known&&m.fileName.contains("optifine",Qt::CaseInsensitive))result.conflicts.append(QCoreApplication::translate("M2CL", "检测到疑似 OptiFine 文件，需先在模组页核对并停用：")+m.fileName);
    }
    if(iris&&optifabric)result.conflicts.append(QCoreApplication::translate("M2CL", "Iris 与 OptiFabric 不能同时启用"));if(iris&&optifine)result.conflicts.append(QCoreApplication::translate("M2CL", "Iris 与 OptiFine 不能同时启用"));if(sodium&&optifine)result.conflicts.append(QCoreApplication::translate("M2CL", "Sodium 与 OptiFine 不能同时启用"));result.available=(iris||optifine)&&result.conflicts.isEmpty();result.summary=!result.conflicts.isEmpty()?QCoreApplication::translate("M2CL", "光影支持需要处理"):result.available?result.components.join(" · "):QCoreApplication::translate("M2CL", "尚未配置光影支持");return result;}
}
