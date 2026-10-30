// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.
#include "skins.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
static void initializeSkinResources(){Q_INIT_RESOURCE(skins);}
static void skinResources(){static const bool initialized=(initializeSkinResources(),true);Q_UNUSED(initialized);}
namespace mc {
namespace {
QImage mirrorLimb(const QImage &image){
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
    return image.flipped(Qt::Horizontal);
#else
    return image.mirrored(true,false);
#endif
}
}
QString skinSource(const QJsonObject &s){return s.value("source").toString("default");}
bool slimSkin(const QJsonObject &s){return s.value("model").toString()=="slim";}
QString skinSettingsProblem(const QJsonObject &s){
    if(!QStringList{"default","steve","alex","local"}.contains(skinSource(s))||!QStringList{"classic","slim"}.contains(s.value("model").toString("classic")))return QCoreApplication::translate("M2CL","皮肤或手臂模型设置无效");
    if(skinSource(s)=="local"&&!QRegularExpression("^[a-f0-9]{64}$").match(s.value("sha256").toString()).hasMatch())return QCoreApplication::translate("M2CL","本地皮肤记录无效");
    return {};
}
QByteArray validateSkinPng(const QByteArray &bytes,bool slim,QString *error){
    if(error)error->clear();QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer,"png");const auto size=reader.size();
    if(bytes.size()>1024*1024||size.width()!=64||(size.height()!=32&&size.height()!=64)){
        if(error)*error=QCoreApplication::translate("M2CL","请选择不超过 1 MiB 的 64×64 或 64×32 PNG 皮肤");return {};
    }
    if(slim&&size.height()==32){if(error)*error=QCoreApplication::translate("M2CL","64×32 旧版皮肤只支持经典手臂，请选择经典模型");return {};}
    const auto image=reader.read();if(image.isNull()){if(error)*error=QCoreApplication::translate("M2CL","PNG 皮肤无法解码");return {};}
    QByteArray normalized;QBuffer output(&normalized);output.open(QIODevice::WriteOnly);image.save(&output,"PNG");return normalized;
}
QImage expandedSkin(const QImage &original){
    if(original.size()!=QSize(64,32))return original;
    QImage image(64,64,QImage::Format_ARGB32);image.fill(Qt::transparent);QPainter p(&image);p.drawImage(0,0,original);
    // The old format reuses mirrored right limbs for the left limbs.
    for(int offset:{0,40}){
        const int dst=offset?32:16;const int src=offset?40:0;
        p.drawImage(QRect(dst+4,48,4,4),mirrorLimb(original.copy(src+4,16,4,4)));
        p.drawImage(QRect(dst+8,48,4,4),mirrorLimb(original.copy(src+8,16,4,4)));
        p.drawImage(QRect(dst,52,4,12),mirrorLimb(original.copy(src+8,20,4,12)));
        p.drawImage(QRect(dst+4,52,4,12),mirrorLimb(original.copy(src+4,20,4,12)));
        p.drawImage(QRect(dst+8,52,4,12),mirrorLimb(original.copy(src,20,4,12)));
        p.drawImage(QRect(dst+12,52,4,12),mirrorLimb(original.copy(src+12,20,4,12)));
    }return image;
}
QJsonObject SkinStore::importPng(const QString &path,bool slim,QString *error)const{
    QFile input(path);if(!input.open(QIODevice::ReadOnly)||input.size()>1024*1024){if(error)*error=QCoreApplication::translate("M2CL","无法读取皮肤文件，或文件超过 1 MiB");return {};}
    const auto bytes=validateSkinPng(input.readAll(),slim,error);if(bytes.isEmpty())return {};
    const auto hash=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    const auto directory=QDir(m_directory).filePath("skins");QSaveFile file(directory+"/"+hash+".png");
    if(!QDir().mkpath(directory)||!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){
        if(error)*error=QCoreApplication::translate("M2CL","无法保存启动器中的皮肤副本");return {};
    }return {{"source","local"},{"model",slim?"slim":"classic"},{"sha256",hash}};
}
QByteArray SkinStore::texture(const QJsonObject &s,QString *error)const{
    if(error)*error=skinSettingsProblem(s);if(!skinSettingsProblem(s).isEmpty())return {};
    const auto source=skinSource(s);if(source=="default")return {};
    skinResources();const auto hash=s.value("sha256").toString();
    const auto path=source=="local"?QDir(m_directory).filePath("skins/"+hash+".png"):
        QString(":/skins/%1-%2.png").arg(source,slimSkin(s)?"slim":"wide");
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>1024*1024){if(error)*error=QCoreApplication::translate("M2CL","皮肤文件不存在，请重新导入或恢复默认皮肤");return {};}
    const auto bytes=file.readAll();if(source=="local"&&QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())!=hash){if(error)*error=QCoreApplication::translate("M2CL","皮肤文件校验失败，请重新导入");return {};}
    return validateSkinPng(bytes,slimSkin(s),error);
}
}
