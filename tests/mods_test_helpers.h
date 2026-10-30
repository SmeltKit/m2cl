// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/mods.h"
#include "network/modinstaller.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QtEndian>
#include <cstring>
#include <zlib.h>

namespace {
QByteArray zip(const QMap<QByteArray,QByteArray> &entries) {
    QByteArray bytes,central;
    auto number=[](QByteArray &b,quint32 value,int size){char data[4];qToLittleEndian(value,data);b.append(data,size);};
    for(auto it=entries.begin();it!=entries.end();++it){const auto name=it.key(),body=it.value();const quint32 offset=bytes.size(),crc=crc32(0,reinterpret_cast<const Bytef *>(body.constData()),body.size());
        number(bytes,0x04034b50,4);number(bytes,20,2);number(bytes,0,2);number(bytes,0,2);number(bytes,0,2);number(bytes,0,2);number(bytes,crc,4);number(bytes,body.size(),4);number(bytes,body.size(),4);number(bytes,name.size(),2);number(bytes,0,2);bytes+=name;bytes+=body;
        number(central,0x02014b50,4);number(central,20,2);number(central,20,2);number(central,0,2);number(central,0,2);number(central,0,2);number(central,0,2);number(central,crc,4);number(central,body.size(),4);number(central,body.size(),4);number(central,name.size(),2);number(central,0,2);number(central,0,2);number(central,0,2);number(central,0,2);number(central,0,4);number(central,offset,4);central+=name;
    }
    const auto offset=bytes.size();bytes+=central;number(bytes,0x06054b50,4);number(bytes,0,2);number(bytes,0,2);number(bytes,entries.size(),2);number(bytes,entries.size(),2);number(bytes,central.size(),4);number(bytes,offset,4);number(bytes,0,2);return bytes;
}
QByteArray jar(QString id,QString version="1.0.0",QJsonObject depends={},QJsonObject breaks={}) {
    depends.insert("minecraft","~1.20.1");return zip({{"fabric.mod.json",QJsonDocument(QJsonObject{{"id",id},{"name",id},{"version",version},{"depends",depends},{"breaks",breaks}}).toJson()}});
}
bool save(QString path,QByteArray bytes){QDir().mkpath(QFileInfo(path).path());QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();}
mc::Instance target(QString path){QDir().mkpath(path);path=QFileInfo(path).canonicalFilePath();mc::Instance i;i.id="fabric";i.path=path;i.gameDirectory=path;i.version="1.20.1";i.loader="fabric";i.loaderVersion="0.16.10";return i;}
class Reply:public QNetworkReply {
    QByteArray body; qint64 offset=0;bool available=false;
public:
    Reply(const QNetworkRequest&r,QByteArray b,QObject *p):QNetworkReply(p),body(b){setRequest(r);setUrl(r.url());open(QIODevice::ReadOnly|QIODevice::Unbuffered);QTimer::singleShot(0,this,[this]{if(isFinished())return;available=true;emit readyRead();if(!isFinished()){setFinished(true);emit finished();}});}
    void abort() override {if(!isFinished()){setError(OperationCanceledError,"Cancelled");setFinished(true);emit finished();}}
    qint64 bytesAvailable() const override{return (available?body.size()-offset:0)+QNetworkReply::bytesAvailable();}
    qint64 readData(char *out,qint64 max) override {if(!available||offset>=body.size())return -1;auto n=qMin(max,body.size()-offset);std::memcpy(out,body.constData()+offset,n);offset+=n;return n;}
};
class Network:public QNetworkAccessManager {
public:
    QHash<QString,QByteArray> bodies;QStringList requested;
    QNetworkReply *createRequest(Operation,const QNetworkRequest&r,QIODevice *) override {requested.append(r.url().path());return new Reply(r,bodies.value(r.url().path(),r.url().path()=="/v2/version_files"?QByteArray("{}"):QByteArray()),this);}
    mc::ResourceFile add(QString project,QString version,QByteArray body,QList<mc::ResourceDependency> deps={}){
        mc::ResourceFile f;f.projectId=project;f.versionId=version;f.versionName=project+" "+version;f.versionNumber=version;f.releaseType="release";f.fileName=project+"-"+version+".jar";f.url=QUrl("https://cdn.modrinth.com/"+f.fileName);f.size=body.size();f.sha512=QString::fromLatin1(QCryptographicHash::hash(body,QCryptographicHash::Sha512).toHex());f.dependencies=deps;f.gameVersions={"1.20.1"};f.loaders={"fabric"};bodies.insert(f.url.path(),body);QJsonArray dependencies;for(auto d:deps)dependencies.append(QJsonObject{{"project_id",d.projectId},{"version_id",d.versionId},{"dependency_type",d.type}});
        auto o=QJsonObject{{"id",version},{"project_id",project},{"name",f.versionName},{"version_number",version},{"version_type","release"},{"game_versions",QJsonArray{"1.20.1"}},{"loaders",QJsonArray{"fabric"}},{"dependencies",dependencies},{"files",QJsonArray{QJsonObject{{"filename",f.fileName},{"primary",true},{"url",f.url.toString()},{"size",f.size},{"hashes",QJsonObject{{"sha512",f.sha512}}}}}}};bodies.insert("/v2/version/"+version,QJsonDocument(o).toJson());bodies.insert("/v2/project/"+project+"/version",QJsonDocument(QJsonArray{o}).toJson());return f;
    }
};
mc::CancelFlag flag(){return std::make_shared<std::atomic_bool>(false);}
}
