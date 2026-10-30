// Copyright (C) 2026 ppm203; GPL-3.0-only with ADDITIONAL_TERMS.md.
#include "offlineskin.h"
#include "core/skins.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QRegularExpression>
#include <QVersionNumber>
#include <stdexcept>
namespace mc {
namespace {
void fail(const QString &error){throw std::runtime_error(error.toUtf8().constData());}
QByteArray read(const QString &path){QFile file(path);return file.open(QIODevice::ReadOnly)&&file.size()<=4*1024*1024?file.readAll():QByteArray();}
QString digest(const QByteArray &bytes){return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
void save(const QString &path,const QByteArray &bytes){
    QSaveFile file(path);if(!QDir().mkpath(QFileInfo(path).path())||!file.open(QIODevice::WriteOnly)||!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)||file.write(bytes)!=bytes.size()||!file.commit())fail(QCoreApplication::translate("M2CL","无法保存本地皮肤运行文件"));
}
QString property(QString value){return value.replace("\\","\\\\").replace("\n","\\n").replace("\r","\\r").replace("=","\\=").replace(":","\\:");}
}
QStringList prepareOfflineSkin(const InstallRequest &request,const JavaRuntime &java,const CancelFlag &cancelled,const InstallProgress &progress,QNetworkAccessManager *network){
    if(request.identity.microsoft||!request.launch||skinSource(request.offlineSkin)=="default")return {};
    if(java.major<8)fail(QCoreApplication::translate("M2CL","自定义离线皮肤需要 Java 8 或更新版本，请切换 Java 或恢复默认皮肤"));
    const auto game=QVersionNumber::fromString(request.version);
    const auto weekly=QRegularExpression("^(\\d{2})w\\d{2}").match(request.version);
    if((!game.isNull()&&game<QVersionNumber(1,7,2))||QRegularExpression("^(?:a|b|c|rd-|inf-)").match(request.version).hasMatch()||(weekly.hasMatch()&&weekly.captured(1).toInt()<14))fail(QCoreApplication::translate("M2CL","此旧版本暂不支持自定义离线皮肤，请为该角色恢复默认皮肤"));
    if(slimSkin(request.offlineSkin)&&((!game.isNull()&&game<QVersionNumber(1,8))||(weekly.hasMatch()&&weekly.captured(1).toInt()<=14)))fail(QCoreApplication::translate("M2CL","Minecraft 1.8 之前不支持纤细手臂，请改用经典模型"));
    QString error;const auto png=SkinStore(request.launcherDirectory).texture(request.offlineSkin,&error);if(png.isEmpty())fail(error);
    if(progress)progress(QCoreApplication::translate("M2CL","正在准备离线角色皮肤"),-1);
    const auto base=QDir(request.launcherDirectory).filePath("skin-runtime");
    const auto textureHash=digest(png),texturePath=base+"/"+textureHash+".png";
    if(digest(read(texturePath))!=textureHash)save(texturePath,png);
    // Pinned, unchanged upstream agent. A valid cached copy works without a network request.
    const QString hash="9c7f4343e6c82034958ffb48c14a2cb0c85928be7283103ce17da00c6d5a7b10";
    const auto injector=base+"/authlib-injector-1.2.8.jar";
    if(digest(read(injector))!=hash){
        auto bytes=fetchInstallData(QUrl("https://authlib-injector.yushi.moe/artifact/56/authlib-injector-1.2.8.jar"),cancelled,{},network,request.downloadSource,progress);
        if(digest(bytes)!=hash)fail(QCoreApplication::translate("M2CL","皮肤支持组件 SHA-256 校验失败"));save(injector,bytes);
    }
    const auto helper=read(":/skins/m2cl-skin-agent.jar");if(helper.isEmpty())fail(QCoreApplication::translate("M2CL","此构建缺少本地皮肤支持组件"));
    const auto helperPath=base+"/m2cl-skin-"+digest(helper).left(16)+".jar";if(digest(read(helperPath))!=digest(helper))save(helperPath,helper);
    const auto player=request.identity.name.isEmpty()?request.player:request.identity.name;
    QString settings="name="+player+"\nuuid="+offlineUuid(player)+"\nmodel="+(slimSkin(request.offlineSkin)?"slim":"classic")+"\nsha256="+textureHash+"\ntexture="+property(texturePath)+"\ninjector="+property(injector)+"\n";
    const auto configuration=base+"/"+digest(settings.toUtf8())+".properties";save(configuration,settings.toUtf8());
    return {"-javaagent:"+helperPath+"="+configuration};
}
}
