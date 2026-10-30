// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include <QUrl>
#include "gameplan.h"
#include "catalog.h"
#include "java.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSysInfo>
#include <QtEndian>
#include <stdexcept>
#include <zlib.h>

namespace mc {
static void fail(const QString &message) { throw std::runtime_error(message.toUtf8().constData()); }
GamePlatform GamePlatform::current() {
    GamePlatform p;
#ifdef Q_OS_WIN
    p.os="windows";
#elif defined(Q_OS_MACOS)
    p.os="osx";
#else
    p.os="linux";
#endif
    p.arch=normalizeArchitecture(QSysInfo::currentCpuArchitecture());p.version=QSysInfo::kernelVersion();return p;
}
bool safeRelativePath(const QString &path) {
    if(path.isEmpty()||path.startsWith('/')||path.contains('\\')||path.contains(':')||path.contains(QChar::Null))return false;
    for(const auto &part:path.split('/')) {
        if(part.isEmpty()||part=="."||part==".."||part.endsWith('.')||part.endsWith(' '))return false;
        if(QRegularExpression("[\\x00-\\x1f<>\"|?*]").match(part).hasMatch())return false;
        if(QRegularExpression("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)",QRegularExpression::CaseInsensitiveOption).match(part).hasMatch())return false;
    }
    return true;
}
QString checkedPath(const QString &root,const QString &relative) {
    if(!safeRelativePath(relative))fail(QCoreApplication::translate("M2CL", "不安全的文件路径：%1").arg(relative));
    QString path=QDir(root).absolutePath();
    // Refuse symlinks in every existing component, including ancestors of the root.
    for(QString parent=path;!parent.isEmpty();parent=QFileInfo(parent).path()) {
        if(QFileInfo(parent).isSymLink())fail(QCoreApplication::translate("M2CL", "目录不能经过符号链接：%1").arg(parent));
        if(parent==QFileInfo(parent).path())break;
    }
    for(const auto &part:relative.split('/')) { path=QDir(path).filePath(part);if(QFileInfo(path).isSymLink())fail(QCoreApplication::translate("M2CL", "文件不能经过符号链接：%1").arg(path)); }
    return path;
}
bool rulesAllow(const QJsonArray &rules,const GamePlatform &p) {
    bool allowed=rules.isEmpty();
    for(const auto &value:rules) {
        const auto rule=value.toObject(),os=rule.value("os").toObject();bool matches=true;
        if(os.contains("name"))matches&=os.value("name").toString()==p.os;
        if(os.contains("arch"))matches&=normalizeArchitecture(os.value("arch").toString())==p.arch;
        if(os.contains("version")) { QRegularExpression re(os.value("version").toString());matches&=re.isValid()&&re.match(p.version).hasMatch(); }
        const auto features=rule.value("features").toObject();
        for(auto it=features.begin();it!=features.end();++it)matches&=!it.value().toBool();
        if(matches)allowed=rule.value("action").toString()=="allow";
    }
    return allowed;
}
GameFile gameFile(const QJsonObject &d,const QString &path) {
    GameFile f;f.path=path;f.sha1=d.value("sha1").toString().toLower();f.size=static_cast<qint64>(d.value("size").toDouble(-1));f.url=QUrl(d.value("url").toString());
    if(!safeRelativePath(path)||!QRegularExpression("^[a-f0-9]{40}$").match(f.sha1).hasMatch()||f.size<0||f.size>2LL*1024*1024*1024||f.url.scheme()!="https"||f.url.host().isEmpty()||!f.url.userInfo().isEmpty())fail(QCoreApplication::translate("M2CL", "下载元数据无效：%1").arg(path));
    return f;
}
GamePlan gamePlan(const QJsonObject &m,const QString &id,const GamePlatform &p) {
    if(!safeRelativePath(id)||id.contains('/')||m.contains("inheritsFrom"))fail(QCoreApplication::translate("M2CL", "当前启动流程仅支持独立原版元数据"));
    GamePlan plan;plan.javaMajor=m.value("javaVersion").toObject().value("majorVersion").toInt(requiredJavaMajor(m.value("id").toString()));
    if(plan.javaMajor<=0)fail(QCoreApplication::translate("M2CL", "无法识别此版本的 Java 要求"));
    const auto index=m.value("assetIndex").toObject();plan.assetIndex=index.value("id").toString();
    if(!safeRelativePath(plan.assetIndex)||plan.assetIndex.contains('/'))fail(QCoreApplication::translate("M2CL", "资源索引名称无效"));
    plan.files.append(gameFile(index,"assets/indexes/"+plan.assetIndex+".json"));
    bool lwjgl=false,matchingLwjglNative=false;
    for(const auto &entry:m.value("libraries").toArray()) {
        const auto lib=entry.toObject();if(!rulesAllow(lib.value("rules").toArray(),p))continue;
        const auto name=lib.value("name").toString();
        if(name.startsWith("org.lwjgl:" )||name.startsWith("org.lwjgl.lwjgl:"))lwjgl=true;
        const auto parts=name.split(':');
        // Modern manifests list multiple architecture-specific native jars under the same OS rule.
        if(parts.size()>3&&parts.at(3).startsWith("natives-")) {
            const auto classifier=parts.at(3);QString arch="x86_64";
            if(classifier.contains("arm64")||classifier.contains("aarch64"))arch="aarch64";
            else if(classifier.endsWith("-x86")||classifier.endsWith("-32"))arch="x86";
            if(arch!=p.arch)continue;
            if(name.startsWith("org.lwjgl:"))matchingLwjglNative=true;
        }
        const auto downloads=lib.value("downloads").toObject();const auto artifact=downloads.value("artifact").toObject();
        if(!artifact.isEmpty()) { const auto path="libraries/"+artifact.value("path").toString();plan.files.append(gameFile(artifact,path));plan.classpath.append(path); }
        const auto nativeMap=lib.value("natives").toObject();
        if(nativeMap.contains(p.os)) {
            if(p.arch=="aarch64")fail(QCoreApplication::translate("M2CL", "此旧版原生库没有 ARM64 构建，请选择兼容的 x64 Java 或较新的游戏版本"));
            auto classifier=nativeMap.value(p.os).toString();classifier.replace("${arch}",p.arch=="x86"?"32":"64");
            const auto native=downloads.value("classifiers").toObject().value(classifier).toObject();
            const auto path="libraries/"+native.value("path").toString();plan.files.append(gameFile(native,path));plan.natives.append(path);matchingLwjglNative=true;
        }
    }
    if(lwjgl&&!matchingLwjglNative)fail(QCoreApplication::translate("M2CL", "此版本未提供当前系统 / Java 架构的原生库"));
    const auto client="versions/"+id+"/"+id+".jar";plan.files.append(gameFile(m.value("downloads").toObject().value("client").toObject(),client));plan.classpath.append(client);
    const auto logging=m.value("logging").toObject().value("client").toObject();
    if(!logging.isEmpty()) {
        const auto file=logging.value("file").toObject();const auto path="assets/log_configs/"+file.value("id").toString();
        plan.files.append(gameFile(file,path));plan.loggingArgument=logging.value("argument").toString();plan.loggingArgument.replace("${path}","${root}/"+path);
    }
    return plan;
}
QString offlineUuid(const QString &name) {
    auto hash=QCryptographicHash::hash(("OfflinePlayer:"+name).toUtf8(),QCryptographicHash::Md5);
    hash[6]=char((static_cast<unsigned char>(hash[6])&0x0f)|0x30);hash[8]=char((static_cast<unsigned char>(hash[8])&0x3f)|0x80);
    return QString::fromLatin1(hash.toHex());
}
QString defaultInstanceName(const QString &version,const QString &loader,const QStringList &existing) {
    QString label=loader;
    const auto key=loader.toLower();
    if(key.isEmpty()||key=="vanilla")label=QCoreApplication::translate("M2CL", "原版");
    else if(key=="forge")label="Forge";
    else if(key=="neoforge")label="NeoForge";
    else if(key=="fabric")label="Fabric";
    else if(key=="quilt")label="Quilt";
    const auto base=version+" "+label;
    QString name=base;for(int n=2;existing.contains(name);++n)name=base+QCoreApplication::translate("M2CL", "（%1）").arg(n);return name;
}
LaunchCommand gameCommand(const QJsonObject &m,const GamePlan &plan,const QString &root,const QString &id,
    const QString &directory,const QString &java,const GameIdentity &identity,int memory,const GamePlatform &platform,const LaunchOptions &options) {
    const auto player=identity.name;
    if(identity.microsoft&&(!QRegularExpression("^[0-9a-f]{32}$").match(identity.uuid).hasMatch()||identity.accessToken.isEmpty()||identity.accessToken.contains(QRegularExpression("[\\x00-\\x20\\x7f]"))||identity.expires<=QDateTime::currentDateTimeUtc().addSecs(30)))fail(QCoreApplication::translate("M2CL", "微软登录已失效或角色信息无效，请重新验证登录"));
    if(!QRegularExpression("^[A-Za-z0-9_]{1,16}$").match(player).hasMatch())fail(QCoreApplication::translate("M2CL", "离线玩家名需要 1–16 位英文字母、数字或下划线"));
    if(memory<512||memory>32768||(platform.arch=="x86"&&memory>1024))fail(QCoreApplication::translate("M2CL", "内存设置无效，32 位 Java 最大分配 1024 MiB"));
    auto effective=options;effective.autoMemory=false;effective.maxMemoryMiB=memory;const auto problem=validateLaunchOptions(effective);if(!problem.isEmpty())fail(problem);
    QStringList cp;for(const auto &path:plan.classpath)cp.append(checkedPath(root,path));
    const auto natives=checkedPath(root,"versions/"+id+"/natives");
    QMap<QString,QString> values{{"auth_player_name",player},{"auth_uuid",identity.microsoft?identity.uuid:offlineUuid(player)},{"auth_access_token",identity.microsoft?identity.accessToken:QString("0")},{"auth_session",identity.microsoft?"token:"+identity.accessToken+":"+identity.uuid:QString("0")},
        {"clientid",identity.microsoft?identity.clientId:QString()},{"auth_xuid",identity.microsoft?identity.xuid:QString()},{"user_type",identity.microsoft?"msa":"legacy"},{"user_properties","{}"},{"profile_name",player},
        {"version_name",id},{"version_type",m.value("type").toString("release")},
        {"game_directory",directory},{"assets_root",QDir(root).filePath("assets")},{"assets_index_name",plan.assetIndex},
        {"game_assets",QDir(root).filePath("assets/virtual/"+plan.assetIndex)},{"natives_directory",natives},
        {"library_directory",QDir(root).filePath("libraries")},{"classpath_separator",platform.os=="windows"?";":":"},
        {"classpath",cp.join(platform.os=="windows"?';':':')},{"launcher_name","m2cl"},{"launcher_version","0.1.0"},{"root",root},
        {"resolution_width",QString::number(options.width)},{"resolution_height",QString::number(options.height)}};
    auto expand=[&](QString text){auto matches=QRegularExpression("\\$\\{([^}]+)\\}").globalMatch(text);while(matches.hasNext()){const auto match=matches.next();if(!values.contains(match.captured(1)))fail(QCoreApplication::translate("M2CL", "启动参数中包含不支持的变量：%1").arg(match.captured(0)));}for(auto it=values.begin();it!=values.end();++it)text.replace("${"+it.key()+"}",it.value());
        if(text.contains("${"))fail(QCoreApplication::translate("M2CL", "启动参数变量格式无效"));return text;};
    auto args=[&](const QJsonArray &array){QStringList result;for(const auto &v:array){if(v.isString())result.append(expand(v.toString()));else {
        const auto o=v.toObject();if(!rulesAllow(o.value("rules").toArray(),platform))continue;
        const auto value=o.value("value");if(value.isString())result.append(expand(value.toString()));else for(const auto &part:value.toArray())result.append(expand(part.toString()));
    }}return result;};
    LaunchCommand result;result.online=identity.microsoft;if(result.online)result.redactionSecrets=QStringList{identity.accessToken};result.executable=java;result.directory=directory;result.arguments<<QString("-Xmx%1M").arg(memory)<<"-Dfile.encoding=UTF-8";
    if(options.minMemoryMiB>0)result.arguments<<QString("-Xms%1M").arg(options.minMemoryMiB);
    const auto arguments=m.value("arguments").toObject();
    if(arguments.contains("jvm"))result.arguments+=args(arguments.value("jvm").toArray());
    else {if(platform.os=="osx")result.arguments<<"-XstartOnFirstThread";result.arguments<<"-Djava.library.path="+natives<<"-cp"<<cp.join(platform.os=="windows"?';':':');}
    if(!plan.loggingArgument.isEmpty()) {
        auto argument=expand(plan.loggingArgument);
        const QString prefix=QStringLiteral("-Dlog4j.configurationFile=");
        // Windows drive letters otherwise look like URL schemes to Log4j.
        if(argument.startsWith(prefix))argument=prefix+QUrl::fromLocalFile(argument.mid(prefix.size())).toString(QUrl::FullyEncoded);
        result.arguments<<argument;
    }
    result.arguments+=splitLaunchArguments(options.jvmArguments);
    const auto mainClass=m.value("mainClass").toString();
    if(!QRegularExpression("^[A-Za-z_$][A-Za-z0-9_$.]*$").match(mainClass).hasMatch())fail(QCoreApplication::translate("M2CL", "游戏主类无效"));
    result.arguments<<mainClass;
    QStringList game;
    if(arguments.contains("game"))game=args(arguments.value("game").toArray());
    else {QString error;game=splitLaunchArguments(m.value("minecraftArguments").toString(),&error);if(!error.isEmpty())fail(error);for(auto &arg:game)arg=expand(arg);}
    auto removeOption=[&](const QString &key,bool takesValue){for(int i=0;i<game.size();){if(game.at(i)==key){game.removeAt(i);if(takesValue&&i<game.size())game.removeAt(i);}else if(game.at(i).startsWith(key+"="))game.removeAt(i);else ++i;}};
    if(options.customResolution){removeOption("--width",true);removeOption("--height",true);game<<"--width"<<QString::number(options.width)<<"--height"<<QString::number(options.height);}
    if(options.fullscreen){removeOption("--fullscreen",false);game<<"--fullscreen";}
    result.arguments+=game;result.arguments+=splitLaunchArguments(options.gameArguments);
    return result;
}

LaunchCommand offlineCommand(const QJsonObject &metadata,const GamePlan &plan,const QString &root,const QString &id,
    const QString &directory,const QString &java,const QString &player,int memory,const GamePlatform &platform,const LaunchOptions &options){
    GameIdentity identity;identity.name=player;return gameCommand(metadata,plan,root,id,directory,java,identity,memory,platform,options);
}

// Read only regular ZIP entries; reject ZIP64, links, traversal and excessive expansion.
void extractNatives(const QString &archive,const QString &directory) {
    QFile file(archive);if(!file.open(QIODevice::ReadOnly)||file.size()>128*1024*1024)fail(QCoreApplication::translate("M2CL", "无法读取原生库压缩包"));
    const auto bytes=file.readAll();const auto size=bytes.size();
    auto u16=[&](int off)->quint16 {if(off<0||off+2>size)fail("Truncated ZIP");return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData()+off));};
    auto u32=[&](int off)->quint32 {if(off<0||off+4>size)fail("Truncated ZIP");return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData()+off));};
    int end=-1;for(int i=size-22;i>=qMax(0,size-65557);--i)if(u32(i)==0x06054b50&&i+22+u16(i+20)==size){end=i;break;}
    if(end<0||u16(end+4)||u16(end+6)||u16(end+10)==65535)fail("Unsupported ZIP");
    quint32 cursor=u32(end+16);quint64 total=0;
    for(int n=0;n<u16(end+10);++n) {
        if(cursor>quint32(size)||u32(cursor)!=0x02014b50)fail("Invalid ZIP directory");
        const auto flags=u16(cursor+8),method=u16(cursor+10);const auto crc=u32(cursor+16),compressed=u32(cursor+20),uncompressed=u32(cursor+24),offset=u32(cursor+42);
        const auto length=u16(cursor+28);const auto name=QString::fromUtf8(bytes.mid(cursor+46,length));
        const auto mode=(u32(cursor+38)>>16)&0170000;
        if(mode==0120000||flags&1||compressed>128*1024*1024||uncompressed>128*1024*1024)fail("Unsafe ZIP entry");
        cursor+=46+length+u16(cursor+30)+u16(cursor+32);
        if(cursor>quint32(size))fail("Truncated ZIP directory");
        const auto cleanName=name.endsWith('/')?name.left(name.size()-1):name;
        if(!safeRelativePath(cleanName))fail("Unsafe ZIP path");
        if(name.startsWith("META-INF/")||name.endsWith('/'))continue;
        total+=uncompressed;if(total>256*1024*1024)fail("ZIP expansion limit");
        if(offset>quint32(size)||u32(offset)!=0x04034b50)fail("Invalid ZIP header");
        const quint64 start=quint64(offset)+30+u16(offset+26)+u16(offset+28);
        if(start+compressed>quint64(size))fail("Truncated ZIP data");
        QByteArray output;
        if(method==0)output=bytes.mid(start,compressed);
        else if(method==8) {
            output.resize(uncompressed);z_stream stream{};stream.next_in=reinterpret_cast<Bytef *>(const_cast<char *>(bytes.constData()+start));stream.avail_in=compressed;
            stream.next_out=reinterpret_cast<Bytef *>(output.data());stream.avail_out=uncompressed;
            if(inflateInit2(&stream,-MAX_WBITS)!=Z_OK)fail("ZIP inflate init failed");const int result=inflate(&stream,Z_FINISH);inflateEnd(&stream);
            if(result!=Z_STREAM_END||stream.total_out!=uncompressed)fail("Corrupt ZIP payload");
        }else fail("Unsupported ZIP compression");
        if(output.size()!=uncompressed||crc32(0,reinterpret_cast<const Bytef *>(output.constData()),output.size())!=crc)fail("ZIP CRC mismatch");
        const auto target=checkedPath(directory,name);QDir().mkpath(QFileInfo(target).path());QSaveFile out(target);
        if(!out.open(QIODevice::WriteOnly)||out.write(output)!=output.size()||!out.commit())fail(QCoreApplication::translate("M2CL", "无法解压原生库：%1").arg(name));
    }
}
}
