// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

// Opt-in network integration probe; uses auto-removed fixtures and never changes a user's game.
#include "network/modinstaller.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QTemporaryDir>
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTemporaryDir temp;if(!temp.isValid())return 2;const auto base=QFileInfo(temp.path()).canonicalFilePath();
    mc::Instance a;a.id="mod-install-probe";a.path=base+"/A";a.gameDirectory=a.path;a.version="1.20.1";a.loader="fabric";a.loaderVersion="0.16.10";QDir().mkpath(a.path);auto b=a;b.path=base+"/B";b.gameDirectory=b.path;QDir().mkpath(b.path);
    const auto cancel=std::make_shared<std::atomic_bool>(false);QNetworkAccessManager net;
    try{
        const auto project=argc>1?app.arguments().at(1):QString("comforts");const auto bytes=mc::fetchInstallData(mc::ModrinthProvider().versionsUrl(project,a.version,a.loader),cancel,{},&net);const auto files=mc::ModrinthProvider::files(QJsonDocument::fromJson(bytes).array());mc::ResourceFile root;for(auto f:files)if(f.releaseType=="release"&&mc::ModrinthProvider::compatible(f,a.version,a.loader)){root=f;break;}if(root.versionId.isEmpty()){qCritical()<<"No stable compatible file";return 1;}
        const auto plan=mc::planModInstall(a,root,cancel,&net);if(!plan.error.isEmpty()){qCritical().noquote()<<plan.error;return 1;}qInfo()<<"Plan for A:"<<plan.files.size()<<"files";for(auto f:plan.files)qInfo().noquote()<<f.fileName<<f.projectId<<f.versionId;
        const auto result=mc::applyModPlan(plan,cancel,{},&net);if(!result.error.isEmpty()){qCritical().noquote()<<result.error;return 1;}const auto mods=mc::localMods(a);if(mods.size()!=plan.files.size()||!mc::localMods(b).isEmpty()){qCritical()<<"Target isolation or inventory mismatch";return 1;}for(auto m:mods)qInfo().noquote()<<"Verified local metadata:"<<m.id<<m.version<<m.ids.size()<<"Mod IDs";
        const auto repeat=mc::planModInstall(a,root,cancel,&net);if(!repeat.error.isEmpty()||!repeat.files.isEmpty()){qCritical()<<"Idempotence failed:"<<repeat.error;return 1;}
        auto manual=a;manual.id="manual-local-probe";manual.path=base+"/manual";manual.gameDirectory=manual.path;QDir().mkpath(manual.path+"/mods");for(const auto &mod:mods)if(!QFile::copy(a.gameDirectory+"/mods/"+mod.fileName,manual.gameDirectory+"/mods/user-renamed-"+mod.fileName)){qCritical()<<"Cannot prepare manual-copy fixture";return 1;}
        const auto reuse=mc::planModInstall(manual,root,cancel,&net);if(!reuse.error.isEmpty()||!reuse.files.isEmpty()||reuse.reusedFiles.size()!=mods.size()){qCritical()<<"Manual-copy recognition failed:"<<reuse.error<<reuse.files.size()<<reuse.reusedFiles.size();return 1;}
        const auto adopted=mc::applyModPlan(reuse,cancel,{},&net);if(!adopted.error.isEmpty()||!adopted.installed.isEmpty()){qCritical()<<"Manual-copy reuse failed:"<<adopted.error;return 1;}qInfo()<<"PASS: actual dependencies verified; target B untouched; repeat and manually renamed JARs require no downloads";return 0;
    }catch(const std::exception&e){qCritical()<<e.what();return 1;}
}
