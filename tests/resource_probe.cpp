// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

// Opt-in network probe; installs only into the explicit isolated instance path.
#include "core/packs.h"
#include "network/modinstaller.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QDebug>
int main(int argc,char **argv){QCoreApplication app(argc,argv);if(argc<2||argc>5)return 2;mc::Instance i;i.path=QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();i.gameDirectory=i.path;i.id="fabric-resource-probe";i.version="1.20.1";i.loader="fabric";i.loaderVersion="0.16.10";if(argc>2)i.version=QString::fromLocal8Bit(argv[2]);if(argc>3)i.loader=QString::fromLocal8Bit(argv[3]);if(argc>4)i.loaderVersion=QString::fromLocal8Bit(argv[4]);auto cancel=std::make_shared<std::atomic_bool>(false);
try{auto bytes=mc::fetchInstallData(mc::ModrinthProvider().versionsUrl("iris",i.version,i.loader),cancel);auto files=mc::ModrinthProvider::files(QJsonDocument::fromJson(bytes).array());mc::ResourceFile selected;for(const auto &file:files)if(file.releaseType=="release"){selected=file;break;}if(selected.projectId.isEmpty())throw std::runtime_error("No Iris release");auto plan=mc::planModInstall(i,selected,cancel);if(!plan.error.isEmpty())throw std::runtime_error(plan.error.toStdString());for(const auto &f:plan.files)qInfo()<<"Installing"<<f.versionName;
bytes=mc::fetchInstallData(mc::ModrinthProvider().versionsUrl("complementary-reimagined",i.version,{}),cancel);files=mc::ModrinthProvider::files(QJsonDocument::fromJson(bytes).array(),true);if(files.isEmpty())throw std::runtime_error("No shader ZIP");selected=files.first();bytes=mc::fetchInstallData(selected.url,cancel,selected.sha1);if(bytes.size()!=selected.size||(!selected.sha512.isEmpty()&&QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha512).toHex())!=selected.sha512))throw std::runtime_error("Hash mismatch");QTemporaryDir temp;QFile f(temp.filePath(selected.fileName));if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size())return 1;f.close();mc::AttachedPack pack;pack.source=f.fileName();pack.file=selected;pack.project.id=selected.projectId;pack.project.name="Complementary Reimagined";auto result=mc::applyModPlan(plan,cancel,{},nullptr,&pack);if(!result.error.isEmpty())throw std::runtime_error(result.error.toStdString());auto support=mc::shaderSupport(i);qInfo()<<support.summary<<support.conflicts;return support.available?0:1;
}catch(const std::exception &e){qCritical()<<e.what();return 1;}}
