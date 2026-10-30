// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "compatibility.h"
#include "mods.h"
#include "resources.h"
#include "gameplan.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QLockFile>
#include <QUuid>
#include <QVersionNumber>
#include <QSet>
#include <QtEndian>
#include <zlib.h>
#include <stdexcept>
#include <limits>
namespace mc {
namespace {
// Runtime components are supplied by the instance, never by another mods/ JAR.
const QMap<QString,QString> loaderComponentIds{{"fabricloader","fabric"},{"quilt_loader","quilt"},{"forge","forge"},{"neoforge","neoforge"}};
bool isRuntimeComponent(const QString &id){return id=="minecraft"||id=="java"||loaderComponentIds.contains(id);}
void fail(const QString &s){throw std::runtime_error(s.toUtf8().constData());}
QByteArray part(QFile &file,qint64 pos,int n){if(pos<0||n<0||pos+n>file.size()||!file.seek(pos))fail("Invalid JAR bounds");auto b=file.read(n);if(b.size()!=n)fail("Truncated JAR");return b;}
quint16 u16(const QByteArray &b,int n){return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(b.constData()+n));}
quint32 u32(const QByteArray &b,int n){return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(b.constData()+n));}
// Fabric's SemVer superset permits empty prereleases and zero-fills absent components.
struct ModVersion {QList<quint64> numbers;QStringList pre;bool prerelease=false,valid=false;};
ModVersion parseVersion(const QString &text){
    ModVersion out;const auto m=QRegularExpression("^(\\d+(?:\\.\\d+)*)(?:-([0-9A-Za-z.-]*))?(?:\\+[^\\s]+)?$").match(text);if(!m.hasMatch())return out;
    for(auto n:m.captured(1).split('.')){bool ok;auto value=n.toULongLong(&ok);if(!ok)return out;out.numbers.append(value);}out.prerelease=m.capturedStart(2)>=0;if(out.prerelease&&!m.captured(2).isEmpty())out.pre=m.captured(2).split('.');out.valid=true;return out;
}
int compareVersion(const ModVersion &a,const ModVersion &b){
    for(int i=0;i<qMax(a.numbers.size(),b.numbers.size());++i){auto x=a.numbers.value(i),y=b.numbers.value(i);if(x!=y)return x<y?-1:1;}
    if(a.prerelease!=b.prerelease)return a.prerelease?-1:1;
    for(int i=0;i<qMax(a.pre.size(),b.pre.size());++i){if(i>=a.pre.size())return -1;if(i>=b.pre.size())return 1;auto x=a.pre.at(i),y=b.pre.at(i);if(x==y)continue;bool xn,yn;auto nx=x.toULongLong(&xn),ny=y.toULongLong(&yn);if(xn&&yn){if(nx!=ny)return nx<ny?-1:1;continue;}if(xn!=yn)return xn?-1:1;return x<y?-1:1;}return 0;
}
int matches(QString range,const QString &version){
    range=range.trimmed();if(range=="*"||range.isEmpty())return 1;
    if(range.contains("||")){bool unknown=false;for(auto r:range.split("||")){auto value=matches(r,version);if(value==1)return 1;unknown|=value<0;}return unknown?-1:0;}
    auto current=parseVersion(version);
    const auto interval=QRegularExpression("^([\\[\\(])([^,]*),([^\\]\\)]*)([\\]\\)])$").match(range);
    if(interval.hasMatch()){if(!current.valid)return -1;for(int bound:{2,3}){auto text=interval.captured(bound).trimmed();if(text.isEmpty())continue;auto endpoint=parseVersion(text);if(!endpoint.valid)return -1;auto cmp=compareVersion(current,endpoint);if(bound==2?(cmp<0||(cmp==0&&interval.captured(1)=="(")):(cmp>0||(cmp==0&&interval.captured(4)==")")))return 0;}return 1;}
    if(range.startsWith('[')&&range.endsWith(']')&&!range.contains(','))range=range.mid(1,range.size()-2);
    for(auto token:range.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts)){
        const auto t=QRegularExpression("^(>=|<=|>|<|=|\\^|~)?(.+)$").match(token);if(!t.hasMatch())return -1;auto op=t.captured(1),operand=t.captured(2);
        auto wildcard=QRegularExpression("^(\\d+(?:\\.\\d+)*)(?:\\.[xX*])+$").match(operand);
        if(wildcard.hasMatch()){if(!current.valid||(!op.isEmpty()&&op!="="))return -1;auto prefix=parseVersion(wildcard.captured(1));if(!prefix.valid)return -1;for(int i=0;i<prefix.numbers.size();++i)if(prefix.numbers.at(i)!=current.numbers.value(i))return 0;continue;}
        auto want=parseVersion(operand);if(!current.valid||!want.valid){if(range.contains(QRegularExpression("\\s")))return -1;if(op.isEmpty()||op=="="){if(version!=operand)return 0;continue;}return -1;}
        const auto cmp=compareVersion(current,want);
        if(op=="^"||op=="~"){auto upper=want;const int index=op=="^"?0:1;while(upper.numbers.size()<=index)upper.numbers.append(0);if(upper.numbers[index]==std::numeric_limits<quint64>::max())return -1;++upper.numbers[index];for(int i=index+1;i<upper.numbers.size();++i)upper.numbers[i]=0;upper.prerelease=true;upper.pre.clear();if(cmp<0||compareVersion(current,upper)>=0)return 0;}
        else if(op==">="?cmp<0:op=="<="?cmp>0:op==">"?cmp<=0:op=="<"?cmp>=0:cmp!=0)return 0;
    }return 1;
}
int matchesRequirement(const QJsonValue &range,const QString &version){
    if(range.isString())return matches(range.toString(),version);
    if(!range.isArray())return -1;bool unknown=false;for(auto item:range.toArray()){const int result=matchesRequirement(item,version);if(result==1)return 1;unknown|=result<0;}return unknown?-1:0;
}

}
int modVersionMatches(const QString &range,const QString &version){return matches(range,version);}
QByteArray fileSha512(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))fail(QCoreApplication::translate("M2CL", "无法读取 %1").arg(path));QCryptographicHash h(QCryptographicHash::Sha512);if(!h.addData(&f))fail("Cannot hash mod");return h.result().toHex();}
QByteArray jarEntry(const QString &archive,const QString &entry,int maxBytes){
    if(maxBytes<=0||maxBytes>128*1024*1024)fail("Invalid archive entry limit");
    QFile f(archive);if(!f.open(QIODevice::ReadOnly)||f.size()>512LL*1024*1024||f.size()<22)fail(QCoreApplication::translate("M2CL", "JAR 文件无效或过大"));
    qint64 offset=qMax<qint64>(0,f.size()-65557);auto tail=part(f,offset,int(f.size()-offset));int end=-1;for(int i=tail.size()-22;i>=0;--i)if(u32(tail,i)==0x06054b50 && i+22+u16(tail,i+20)==tail.size()){end=i;break;}
    if(end<0||u16(tail,end+4)||u16(tail,end+6)||u16(tail,end+10)==65535)fail("Unsupported JAR ZIP directory");
    qint64 cursor=u32(tail,end+16);for(int n=0;n<u16(tail,end+10);++n){auto header=part(f,cursor,46);if(u32(header,0)!=0x02014b50)fail("Invalid JAR directory");int len=u16(header,28);auto name=QString::fromUtf8(part(f,cursor+46,len));cursor+=46+len+u16(header,30)+u16(header,32);if(name!=entry)continue;
        const auto packed=u32(header,20),size=u32(header,24);if(size>quint32(maxBytes)||packed>quint32(maxBytes)*2||u16(header,8)&1)fail("Unsafe JAR metadata");auto local=part(f,u32(header,42),30);if(u32(local,0)!=0x04034b50)fail("Invalid JAR header");auto data=part(f,qint64(u32(header,42))+30+u16(local,26)+u16(local,28),int(packed));QByteArray out;
        auto method=u16(header,10);if(method==0)out=data;else if(method==8){out.resize(int(size));z_stream z{};z.next_in=reinterpret_cast<Bytef *>(data.data());z.avail_in=packed;z.next_out=reinterpret_cast<Bytef *>(out.data());z.avail_out=size;if(inflateInit2(&z,-MAX_WBITS)!=Z_OK)fail("JAR decompression failed");int result=inflate(&z,Z_FINISH);auto written=z.total_out;inflateEnd(&z);if(result!=Z_STREAM_END||written!=size)fail("Corrupt JAR metadata");}else fail("Unsupported JAR compression");
        if(out.size()!=int(size)||crc32(0,reinterpret_cast<const Bytef *>(out.constData()),uInt(out.size()))!=u32(header,16))fail("JAR metadata CRC failed");return out;
    }return {};
}
static LocalMod inspectModImpl(const QString &path,const QString &preferredLoader,int depth,int &budget);
static ModMetadata parseModMetadata(const QString &path,const QString &loader,const QByteArray &bytes,int depth,int &budget){
    ModMetadata result;result.loader=loader;
    try{
        if(loader=="fabric"||loader=="quilt"){
            QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);if(error.error!=QJsonParseError::NoError||!doc.isObject())fail("Invalid mod JSON");
            auto object=doc.object();const bool quilt=loader=="quilt";if(quilt)object=object.value("quilt_loader").toObject();
            result.id=object.value("id").toString();result.version=object.value("version").toString();result.name=quilt?object.value("metadata").toObject().value("name").toString(result.id):object.value("name").toString(result.id);
            auto icon=quilt?object.value("metadata").toObject().value("icon"):object.value("icon");
            if(icon.isString())result.iconPath=icon.toString();
            else if(icon.isObject()){
                int best=0;const auto sizes=icon.toObject();
                for(auto it=sizes.constBegin();it!=sizes.constEnd();++it){const int size=it.key().toInt();if(size<=0||!it.value().isString())continue;if(!best||(size>=64&&(best<64||size<best))||(size<64&&best<64&&size>best)){best=size;result.iconPath=it.value().toString();}}
            }
            result.requirements=object.value("depends").toObject();result.breaks=object.value("breaks").toObject();result.conflicts=object.value("conflicts").toObject();
            if(quilt)for(const auto &value:object.value("depends").toArray()){const auto dep=value.toObject();const auto id=dep.value("id").toString();if(!id.isEmpty())result.requirements.insert(id,dep.value("versions"));}
            for(auto it=result.requirements.begin();it!=result.requirements.end();++it)if(!isRuntimeComponent(it.key()))result.requiredIds.append(it.key());
            for(const auto &value:object.value("provides").toArray())if(value.isString())result.ids.append(value.toString());
            if(result.id.isEmpty()||result.version.isEmpty())fail("Missing mod ID or version");result.ids.append(result.id);for(const auto &id:result.ids)result.providedVersions.insert(id,result.version);
            // A nested component belongs to this loader's metadata, never to another variant.
            for(const auto &value:doc.object().value("jars").toArray()){
                const auto name=value.toObject().value("file").toString();if(name.isEmpty())fail("Missing nested JAR name");const auto nestedBytes=jarEntry(path,name);if(nestedBytes.isEmpty())fail("Missing nested JAR");
                QTemporaryFile nested;if(!nested.open()||nested.write(nestedBytes)!=nestedBytes.size()||!nested.flush())fail("Cannot read nested mod");const auto child=inspectModImpl(nested.fileName(),loader,depth+1,budget);if(!child.known||child.loader!=loader)fail("Unrecognized nested mod metadata");result.components.append(static_cast<const ModMetadata &>(child));for(const auto &id:child.requiredIds)if(!result.requiredIds.contains(id))result.requiredIds.append(id);
                for(const auto &id:child.ids)result.embeddedHashes.insert(id,child.embeddedHashes.value(id,child.sha512));
                for(const auto &id:child.ids)if(!result.ids.contains(id))result.ids.append(id);for(auto it=child.providedVersions.begin();it!=child.providedVersions.end();++it)result.providedVersions.insert(it.key(),it.value());
            }
        }else{
            const auto text=QString::fromUtf8(bytes);auto blocks=text.split(QRegularExpression("\\[\\[mods\\]\\]"));
            for(int i=1;i<blocks.size();++i){const auto block=blocks.at(i).section("[[",0,0);auto field=[&](const QString &key){return QRegularExpression("(?m)^\\s*"+key+"\\s*=\\s*\"([^\"]*)\"").match(block).captured(1);};const auto id=field("modId");if(id.isEmpty())continue;result.ids.append(id);result.providedVersions.insert(id,field("version"));if(result.id.isEmpty()){result.id=id;result.name=field("displayName");result.version=field("version");result.iconPath=field("logoFile");}}
            auto deps=QRegularExpression("(?s)\\[\\[dependencies\\.([^]]+)\\]\\](.*?)(?=\\[\\[|$)").globalMatch(text);
            while(deps.hasNext()){const auto match=deps.next();const auto block=match.captured(2);const auto id=QRegularExpression("modId\\s*=\\s*\"([^\"]+)\"").match(block).captured(1);const auto range=QRegularExpression("versionRange\\s*=\\s*\"([^\"]+)\"").match(block).captured(1);if(id.isEmpty())continue;
                // Keep every constraint from multi-mod TOML sections independently.
                ModMetadata constraint;constraint.known=true;constraint.loader=loader;constraint.name=match.captured(1);
                if(QRegularExpression("type\\s*=\\s*\"discouraged\"").match(block).hasMatch())constraint.conflicts.insert(id,range);
                else if(QRegularExpression("mandatory\\s*=\\s*false|type\\s*=\\s*\"optional\"").match(block).hasMatch())continue;
                else if(QRegularExpression("type\\s*=\\s*\"incompatible\"").match(block).hasMatch())constraint.breaks.insert(id,range);
                else{constraint.requirements.insert(id,range);if(!isRuntimeComponent(id))constraint.requiredIds.append(id);}
                result.components.append(constraint);
                // Preserve the common top-level fields for existing callers and diagnostics.
                if(!constraint.requirements.isEmpty()&&!result.requirements.contains(id))result.requirements.insert(id,range);
                for(const auto &required:constraint.requiredIds)if(!result.requiredIds.contains(required))result.requiredIds.append(required);
                if(constraint.breaks.contains(id))result.breaks.insert(id,range);if(constraint.conflicts.contains(id))result.conflicts.insert(id,range);
            }
            if(result.id.isEmpty()||result.version.isEmpty())fail("Missing mod ID or version");if(result.name.isEmpty())result.name=result.id;
        }
        result.known=true;
    }catch(const std::exception &error){result.known=false;result.problem=QString::fromUtf8(error.what());}
    return result;
}
LocalMod selectModMetadata(const LocalMod &mod,const QString &loader){
    auto selected=mod;if(loader.isEmpty())return selected;
    if(mod.metadata.contains(loader))static_cast<ModMetadata &>(selected)=mod.metadata.value(loader);
    else if(mod.metadataProblems.contains(loader)){static_cast<ModMetadata &>(selected)=ModMetadata{};selected.loader=loader;selected.name=mod.fileName;selected.problem=QCoreApplication::translate("M2CL", "%1 元数据无法确认：%2").arg(resourceTagLabel(loader),mod.metadataProblems.value(loader));}
    return selected;
}
static LocalMod inspectModImpl(const QString &path,const QString &preferredLoader,int depth,int &budget){
    LocalMod result;result.fileName=QFileInfo(path).fileName();result.enabled=!result.fileName.endsWith(".disabled");result.name=result.fileName;
    try{
        if(depth>4||--budget<0)fail("Nested mod metadata exceeds limit");result.sha512=QString::fromLatin1(fileSha512(path));
        const QList<QPair<QString,QString>> entries{{"fabric","fabric.mod.json"},{"quilt","quilt.mod.json"},{"forge","META-INF/mods.toml"},{"neoforge","META-INF/neoforge.mods.toml"}};
        for(const auto &entry:entries){const auto bytes=jarEntry(path,entry.second);result.validArchive=true;if(bytes.isEmpty())continue;auto loader=entry.first;
            // Early NeoForge versions used mods.toml; rely on an explicit dependency ID.
            if(loader=="forge"&&QRegularExpression("modId\\s*=\\s*\"neoforge\"").match(QString::fromUtf8(bytes)).hasMatch())loader="neoforge";
            auto metadata=parseModMetadata(path,loader,bytes,depth,budget);if(metadata.known){result.metadata.insert(loader,metadata);result.metadataProblems.remove(loader);}else if(!result.metadata.contains(loader))result.metadataProblems.insert(loader,metadata.problem);
        }
        for(const auto &loader:QStringList{"fabric","quilt","neoforge","forge"})if(result.metadata.contains(loader)){static_cast<ModMetadata &>(result)=result.metadata.value(loader);break;}
        // Identify the OptiFine component from class signatures, never from its filename.
        if(!result.known&&jarEntry(path,"optifine/OptiFineClassTransformer.class").startsWith(QByteArray::fromHex("cafebabe"))&&jarEntry(path,"optifine/Installer.class").startsWith(QByteArray::fromHex("cafebabe"))){result.known=true;result.id="optifine";result.ids=QStringList{"optifine"};result.name="OptiFine";result.loader="optifine";result.version="unknown";}
        if(!result.known)result.problem=result.metadataProblems.isEmpty()?QCoreApplication::translate("M2CL", "无法完整识别元数据，兼容性需确认"):QCoreApplication::translate("M2CL", "模组元数据无效：%1").arg(result.metadataProblems.values().join(QCoreApplication::translate("M2CL", "；")));
        return selectModMetadata(result,preferredLoader);
    }catch(const std::exception &error){result.known=false;result.problem=QString::fromUtf8(error.what());return result;}
}
LocalMod inspectMod(const QString &path,const QString &preferredLoader){int budget=128;return inspectModImpl(path,preferredLoader,0,budget);}
QByteArray localModIcon(const QString &archive,const ModMetadata &metadata){
    const auto path=metadata.iconPath;
    if(path.isEmpty()||path.size()>1024||path.startsWith('/')||path.contains('\\')||path.contains(':')||path.split('/').contains(".."))return {};
    try{return jarEntry(archive,path);}catch(const std::exception &){return {};}
}
QString modEnvironmentProblem(const LocalMod &input,const Instance &instance){
    const auto mod=selectModMetadata(input,instance.loader);
    if(!instance.loaderProblem.isEmpty())return instance.loaderProblem;
    if(mod.id=="optifine"&&instance.loader=="forge"&&!instance.optifineVersion.isEmpty()&&mod.sha512==instance.optifineSha512&&instance.loaderVersion==instance.optifineForgeVersion)return {};
    if(mod.id=="optifine")return QCoreApplication::translate("M2CL", "已检测到 OptiFine；尚未核对该构建与当前 Minecraft / 加载器的兼容性。Forge 需匹配官方要求的构建，Fabric 还需匹配 OptiFabric，不能直接作为通用模组启用。");
    if(!instance.supportsMods())return QCoreApplication::translate("M2CL", "此实例尚未安装模组加载器");if(!mod.known)return mod.problem;
    if(mod.loader!=instance.loader && !(instance.loader=="quilt"&&mod.loader=="fabric"))return QCoreApplication::translate("M2CL", "加载器不匹配：此文件用于 %1，当前实例为 %2").arg(resourceTagLabel(mod.loader),instance.loaderName());
    for(auto it=loaderComponentIds.begin();it!=loaderComponentIds.end();++it)if(mod.requirements.contains(it.key())){
        if(instance.loader=="quilt"&&it.value()=="fabric")continue; // Quilt supplies the Fabric API; its version is not the Quilt version.
        if(it.value()!=instance.loader)return QCoreApplication::translate("M2CL", "%1 要求 %2，目标为 %3").arg(mod.name,resourceTagLabel(it.value()),instance.loaderName());
        const auto range=mod.requirements.value(it.key());if(range.isString()&&(range.toString().isEmpty()||range=="*"))continue;
        if(instance.loaderVersion.isEmpty())return QCoreApplication::translate("M2CL", "无法确认 %1 的加载器版本要求，请重新扫描实例").arg(mod.name);
        if(matchesRequirement(range,instance.loaderVersion)!=1){
            QStringList requested;if(range.isArray())for(const auto &value:range.toArray())requested.append(value.toString());else requested.append(range.toString());
            return QCoreApplication::translate("M2CL", "%1 要求 %2 %3（当前 %4）。请在实例设置中调整加载器版本；加载器属于游戏组件，不能作为前置模组安装。").arg(mod.name,instance.loaderName(),requested.join(QCoreApplication::translate("M2CL", " 或 ")),instance.loaderVersion);
        }
    }
    for(const auto &component:mod.components){LocalMod nested;static_cast<ModMetadata &>(nested)=component;const auto problem=modEnvironmentProblem(nested,instance);if(!problem.isEmpty())return component.name+" · "+problem;}
    auto requirement=mod.requirements.value("minecraft");if(!requirement.isUndefined()){int result=0;if(requirement.isArray()){bool unknown=false;for(auto value:requirement.toArray()){int m=matches(value.toString(),instance.version);if(m==1){result=1;break;}unknown|=m<0;}if(!result&&unknown)result=-1;}else result=matches(requirement.toString(),instance.version);if(result==0)return QCoreApplication::translate("M2CL", "文件要求 Minecraft %1，当前实例为 %2").arg(requirement.isArray()?QString::fromUtf8(QJsonDocument(requirement.toArray()).toJson(QJsonDocument::Compact)):requirement.toString(),instance.version);if(result<0)return QCoreApplication::translate("M2CL", "无法确认此 Minecraft 版本范围：%1").arg(requirement.toVariant().toString());}return {};
}
QString modDependenciesProblem(const LocalMod &mod,const QList<LocalMod> &all,bool allowMissing){
    for(const auto &component:mod.components){LocalMod nested;static_cast<ModMetadata &>(nested)=component;const auto problem=modDependenciesProblem(nested,all,allowMissing);if(!problem.isEmpty())return problem;}
    for(const auto &id:mod.requiredIds){const LocalMod *provider=nullptr;for(const auto &other:all)if(other.enabled&&other.ids.contains(id)){provider=&other;break;}if(!provider&&allowMissing)continue;if(!provider)return QCoreApplication::translate("M2CL", "%1 缺少启用的必需依赖：%2").arg(mod.name,id);
        auto range=mod.requirements.value(id);if(range.isUndefined())continue;if(matchesRequirement(range,provider->providedVersions.value(id,provider->version))!=1)return QCoreApplication::translate("M2CL", "%1 所需的 %2 版本不匹配或无法确认").arg(mod.name,id);
    }
    for(const auto &other:all)if(other.enabled)for(const auto &id:other.ids){auto range=mod.breaks.value(id);if(range.isUndefined())continue;if(matchesRequirement(range,other.providedVersions.value(id,other.version))!=0)return QCoreApplication::translate("M2CL", "%1 声明与 %2 冲突或范围无法确认").arg(mod.name,other.name);}
    return {};
}
QString duplicateModProblem(const LocalMod &a,const LocalMod &b){
    for(const auto &id:a.ids)if(b.ids.contains(id)){
        const bool nested=a.embeddedHashes.contains(id)||b.embeddedHashes.contains(id);
        const auto first=a.embeddedHashes.value(id,a.sha512),second=b.embeddedHashes.value(id,b.sha512);
        if(nested&&!first.isEmpty()&&first==second)continue;
        return QCoreApplication::translate("M2CL", "重复 Mod ID %1：%2 / %3").arg(id,a.fileName,b.fileName);
    }
    return {};
}
ModCheck checkModSet(const Instance &instance,const QList<LocalMod> &inputs,bool requireKnown,bool allowMissing){
    QList<LocalMod> mods;for(const auto &input:inputs)mods.append(selectModMetadata(input,instance.loader));
    ModCheck result;QHash<QString,QString> owners;
    QSet<QString> activeIds;for(const auto &m:mods)if(m.enabled)for(const auto &id:m.ids)activeIds.insert(id);
    if(!instance.optifineVersion.isEmpty())activeIds.insert("optifine");
    if(activeIds.contains("optifine")&&(activeIds.contains("iris")||activeIds.contains("sodium")||activeIds.contains("oculus")))result.errors.append(QCoreApplication::translate("M2CL", "OptiFine 与 Iris / Sodium / Oculus 不能同时启用，请先停用冲突组件"));
    if(activeIds.contains("optifabric")&&activeIds.contains("iris"))result.errors.append(QCoreApplication::translate("M2CL", "OptiFabric 与 Iris 不能同时启用，请选择一种光影支持方案"));
    for(const auto &mod:mods)if(mod.enabled){
        if(!mod.known){const auto message=QCoreApplication::translate("M2CL", "无法完整核对 %1：%2").arg(mod.fileName,mod.problem);(requireKnown?result.errors:result.warnings).append(message);continue;}
        const auto environment=modEnvironmentProblem(mod,instance);if(!environment.isEmpty())result.errors.append(mod.name+" · "+environment);
        for(const auto &other:mods)if(other.enabled&&other.fileName!=mod.fileName){const auto duplicate=duplicateModProblem(mod,other);if(!duplicate.isEmpty())result.errors.append(duplicate);}
        const auto dependency=modDependenciesProblem(mod,mods,allowMissing);if(!dependency.isEmpty())result.errors.append(dependency);
        if(allowMissing)for(const auto &id:mod.requiredIds){bool found=false;for(const auto &other:mods)if(other.enabled&&other.ids.contains(id))found=true;if(!found)result.warnings.append(QCoreApplication::translate("M2CL", "%1 缺少必需前置 %2（本次选择手动处理前置）").arg(mod.name,id));}
        result.warnings.append(modCompatibilityWarnings(mod,mods));
        result.warnings.append(knownCompatibilityWarnings(instance,mod));
        if(instance.loader=="quilt"&&mod.requirements.contains("fabricloader"))result.warnings.append(QCoreApplication::translate("M2CL", "%1：Fabric Loader API 版本要求由 Quilt 启动时核对，未用 Quilt 版本号代替校验。").arg(mod.name));
    }
    result.errors.removeDuplicates();result.warnings.removeDuplicates();return result;
}
QStringList modCompatibilityWarnings(const LocalMod &mod,const QList<LocalMod> &mods){
    QStringList result;if(!mod.enabled)return result;for(const auto &component:mod.components){LocalMod nested;static_cast<ModMetadata &>(nested)=component;result.append(modCompatibilityWarnings(nested,mods));}for(const auto &other:mods)if(other.enabled)for(const auto &id:other.ids)if(mod.conflicts.contains(id)&&matchesRequirement(mod.conflicts.value(id),other.providedVersions.value(id,other.version))!=0)result.append(QCoreApplication::translate("M2CL", "作者兼容性警告：%1 与 %2（此声明不等于必定冲突）").arg(mod.name,other.name));result.removeDuplicates();return result;
}
QJsonObject modManifest(const QString &gameDirectory){QFile f(checkedPath(gameDirectory,"mods/.m2cl-mods.json"));if(!f.exists())return {};if(!f.open(QIODevice::ReadOnly)||f.size()>4*1024*1024)fail("Cannot read mod manifest");auto d=QJsonDocument::fromJson(f.readAll());if(!d.isObject())fail("Invalid mod manifest");return d.object();}
QList<LocalMod> localMods(const Instance &instance){QList<LocalMod> out;QDir dir(checkedPath(instance.gameDirectory,"mods"));auto entries=modManifest(instance.gameDirectory).value("files").toArray();for(const auto &name:dir.entryList({"*.jar","*.jar.disabled"},QDir::Files,QDir::Name)){auto path=checkedPath(instance.gameDirectory,"mods/"+name);auto mod=inspectMod(path,instance.loader);for(auto entry:entries){auto o=entry.toObject();if((o.value("fileName")==name||o.value("fileName").toString()+".disabled"==name)&&o.value("sha512")==mod.sha512){mod.projectId=o.value("projectId").toString();mod.versionId=o.value("versionId").toString();break;}}out.append(mod);}return out;}
bool isVersionComponentFile(const Instance &instance,const QString &fileName){
    return !instance.optifineVersion.isEmpty()&&!instance.optifineFile.isEmpty()&&(fileName==instance.optifineFile||fileName==instance.optifineFile+".disabled");
}
QString importLocalMods(const Instance &instance,const QStringList &paths,bool allowUnknown){
    try{if(!instance.supportsMods())fail(QCoreApplication::translate("M2CL", "先为此实例安装加载器后再添加模组"));if(!QDir().mkpath(checkedPath(instance.gameDirectory,"mods")))fail("Cannot create mods directory");QLockFile lock(checkedPath(instance.gameDirectory,"mods/.m2cl-install.lock"));if(!lock.tryLock())fail(QCoreApplication::translate("M2CL", "该实例已有模组操作正在进行"));auto existing=localMods(instance);QList<LocalMod> incoming;QSet<QString> ids,names;for(const auto &m:existing)if(m.enabled)for(auto id:m.ids)ids.insert(id);
        for(const auto &path:paths){if(!safeModFileName(QFileInfo(path).fileName()))fail("Only JAR files can be imported");auto mod=inspectMod(path,instance.loader);if(!mod.validArchive)fail(mod.problem);auto problem=modEnvironmentProblem(mod,instance);if(!problem.isEmpty() && (!allowUnknown || mod.known || mod.metadataProblems.contains(instance.loader)))fail(problem+" · "+mod.fileName);if(names.contains(mod.fileName))fail("Duplicate filename");names.insert(mod.fileName);for(const auto &other:incoming){const auto duplicate=duplicateModProblem(mod,other);if(!duplicate.isEmpty())fail(duplicate);}auto dest=checkedPath(instance.gameDirectory,"mods/"+mod.fileName);if(QFileInfo::exists(dest))fail(QCoreApplication::translate("M2CL", "文件已存在：%1").arg(mod.fileName));incoming.append(mod);}
        auto combined=existing;combined.append(incoming);const auto problems=checkModSet(instance,combined,!allowUnknown);if(!problems.errors.isEmpty())fail(problems.errors.join('\n'));
        auto root=checkedPath(instance.gameDirectory,"mods");if(!QDir().mkpath(root))fail("Cannot create mods directory");QStringList committed;
        try{for(int i=0;i<paths.size();++i){auto dest=checkedPath(instance.gameDirectory,"mods/"+incoming.at(i).fileName);QSaveFile out(dest);QCryptographicHash copiedHash(QCryptographicHash::Sha512);QFile in(paths.at(i));if(!in.open(QIODevice::ReadOnly)||!out.open(QIODevice::WriteOnly))fail("Cannot import mod");while(!in.atEnd()){auto data=in.read(1024*1024);if(data.isEmpty() && in.error()!=QFile::NoError)fail("Cannot read mod");copiedHash.addData(data);if(out.write(data)!=data.size())fail("Cannot write mod");}if(QString::fromLatin1(copiedHash.result().toHex())!=incoming.at(i).sha512)fail("Source mod changed during import");if(!out.commit())fail("Cannot commit mod");committed.append(dest);}}catch(...){for(auto p:committed)QFile::remove(p);throw;}return {};
    }catch(const std::exception &e){return QString::fromUtf8(e.what());}
}
static QString dependentProblem(const Instance &instance,const LocalMod &chosen,const QList<LocalMod> &all){
    for(auto m:all)if(m.enabled&&m.fileName!=chosen.fileName)for(auto id:chosen.ids)if(m.requiredIds.contains(id))return QCoreApplication::translate("M2CL", "%1 仍依赖此模组，请先禁用依赖者").arg(m.name);
    for(auto value:modManifest(instance.gameDirectory).value("files").toArray()){auto entry=value.toObject();if(entry.value("fileName")==chosen.fileName)continue;bool active=false;for(auto m:all)if(m.fileName==entry.value("fileName").toString()&&m.enabled)active=true;if(!active)continue;for(auto raw:entry.value("dependencies").toArray()){auto d=raw.toObject();if(d.value("type")!="required")continue;const auto project=d.value("projectId").toString(),version=d.value("versionId").toString();if((!project.isEmpty()&&project==chosen.projectId)||(!version.isEmpty()&&version==chosen.versionId))return QCoreApplication::translate("M2CL", "%1 仍依赖此项目，请先禁用依赖者").arg(entry.value("name").toString());}}
    return {};
}
namespace {
enum class LocalModOperation { Enable,Disable,Remove };
QString mutateLocalMods(const Instance &instance,QStringList files,LocalModOperation operation){
    try{
        files.removeDuplicates();if(files.isEmpty())return {};
        for(const auto &file:files)if(isVersionComponentFile(instance,file))fail(QCoreApplication::translate("M2CL", "OptiFine 属于版本组件，请在“管理版本组件”中修改或移除。"));
        QLockFile lock(checkedPath(instance.gameDirectory,"mods/.m2cl-install.lock"));if(!lock.tryLock())fail(QCoreApplication::translate("M2CL", "该实例已有模组操作正在进行"));
        const auto all=localMods(instance);auto proposed=all;QList<LocalMod> chosen;
        for(const auto &file:files){bool found=false;for(auto &mod:proposed)if(mod.fileName==file){chosen.append(mod);mod.enabled=operation==LocalModOperation::Enable;found=true;break;}if(!found)fail(QCoreApplication::translate("M2CL", "模组文件已变化，请刷新列表：%1").arg(file));}
        if(operation==LocalModOperation::Enable){const auto problems=checkModSet(instance,proposed,true);if(!problems.errors.isEmpty())fail(problems.errors.join('\n'));}
        else for(const auto &mod:chosen){const auto problem=dependentProblem(instance,mod,proposed);if(!problem.isEmpty())fail(problem);}
        const bool removing=operation==LocalModOperation::Remove;
        QJsonObject inventory;if(removing)inventory=modManifest(instance.gameDirectory);const auto originalInventory=inventory;
        const auto backupDirectory=removing?checkedPath(instance.gameDirectory,".m2cl-removal-"+QUuid::createUuid().toString(QUuid::WithoutBraces)):QString();
        QList<QPair<QString,QString>> moves,committed;
        for(const auto &mod:chosen){
            const bool enabled=operation==LocalModOperation::Enable;if(!removing&&mod.enabled==enabled)continue;
            const auto source=checkedPath(instance.gameDirectory,"mods/"+mod.fileName);
            const auto target=removing?backupDirectory+"/"+mod.fileName:checkedPath(instance.gameDirectory,"mods/"+(enabled?mod.fileName.left(mod.fileName.size()-9):mod.fileName+".disabled"));
            if(QFileInfo::exists(target))fail(QCoreApplication::translate("M2CL", "目标文件已存在，未修改本批模组：%1").arg(QFileInfo(target).fileName()));moves.append({source,target});
        }
        if(moves.isEmpty())return {};
        if(removing&&!QDir().mkpath(backupDirectory))fail("Cannot create mod backup directory");
        try{
            for(const auto &move:moves){if(!QFile::rename(move.first,move.second))fail(QCoreApplication::translate("M2CL", "无法修改模组文件：%1").arg(QFileInfo(move.first).fileName()));committed.append(move);}
            if(removing){
                QJsonArray entries;for(auto entry:inventory.value("files").toArray()){const auto name=entry.toObject().value("fileName").toString();if(!files.contains(name)&&!files.contains(name+".disabled"))entries.append(entry);}
                inventory.insert("files",entries);inventory.insert("schemaVersion",1);const auto bytes=QJsonDocument(inventory).toJson();QSaveFile manifest(checkedPath(instance.gameDirectory,"mods/.m2cl-mods.json"));if(!manifest.open(QIODevice::WriteOnly)||manifest.write(bytes)!=bytes.size()||!manifest.commit())fail("Cannot update mod inventory");
                if(!QFile::moveToTrash(backupDirectory)){QSaveFile restore(checkedPath(instance.gameDirectory,"mods/.m2cl-mods.json"));const auto original=QJsonDocument(originalInventory).toJson();const bool restored=restore.open(QIODevice::WriteOnly)&&restore.write(original)==original.size()&&restore.commit();fail(restored?QCoreApplication::translate("M2CL", "无法移到系统废纸篓／回收站"):QCoreApplication::translate("M2CL", "无法移到废纸篓且清单恢复失败，请刷新核对"));}
            }
        }catch(const std::exception &error){
            QStringList failed;for(auto it=committed.crbegin();it!=committed.crend();++it)if(!QFile::rename(it->second,it->first))failed.append(it->second);
            if(removing)QDir().rmdir(backupDirectory);
            fail(QString::fromUtf8(error.what())+(failed.isEmpty()?QCoreApplication::translate("M2CL", "；本批变更已回退"):QCoreApplication::translate("M2CL", "；部分文件无法回退，请检查：%1").arg(failed.join(QCoreApplication::translate("M2CL", "、")))));
        }
        return {};
    }catch(const std::exception &error){return QString::fromUtf8(error.what());}
}
}
QString removeLocalMods(const Instance &instance,const QStringList &files){return mutateLocalMods(instance,files,LocalModOperation::Remove);}
QString setModsEnabled(const Instance &instance,const QStringList &files,bool enabled){return mutateLocalMods(instance,files,enabled?LocalModOperation::Enable:LocalModOperation::Disable);}
QString removeLocalMod(const Instance &instance,const QString &file){return removeLocalMods(instance,{file});}
QString setModEnabled(const Instance &instance,const QString &file,bool enabled){return setModsEnabled(instance,{file},enabled);}
}
