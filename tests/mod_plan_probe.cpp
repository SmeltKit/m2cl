// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

// Opt-in: real metadata and native plan UI, no mod downloads or installation.
#include "ui/modplanview.h"
#include "ui/theme.h"
#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <QTimer>
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setFont(mc::applicationFont());mc::applyTheme(mc::makeTheme("forest",!app.arguments().contains("--light")));
    const auto args=app.arguments();if(args.size()<3){qCritical()<<"Usage: mod_plan_probe PROJECT OUTPUT.png";return 2;}QTemporaryDir temp;if(!temp.isValid())return 2;
    mc::Instance instance;instance.id=QStringLiteral("隔离验证实例");instance.path=QFileInfo(temp.path()).canonicalFilePath();instance.gameDirectory=instance.path;instance.loader="fabric";instance.loaderVersion="0.16.10";instance.version="1.20.1";
    const auto cancel=std::make_shared<std::atomic_bool>(false);QNetworkAccessManager net;
    try{
        const auto bytes=mc::fetchInstallData(mc::ModrinthProvider().versionsUrl(args.at(1),instance.version,instance.loader),cancel,{},&net);const auto files=mc::ModrinthProvider::files(QJsonDocument::fromJson(bytes).array());mc::ResourceFile root;
        for(const auto &file:files)if(file.releaseType=="release"&&mc::ModrinthProvider::compatible(file,instance.version,instance.loader)){root=file;break;}if(root.versionId.isEmpty()){qCritical()<<"No stable compatible root version";return 1;}
        auto plan=mc::planModInstall(instance,root,cancel,&net);if(!plan.error.isEmpty()){qCritical().noquote()<<plan.error;return 1;}
        qInfo()<<"Resolved metadata:"<<plan.resolved.size()<<"projects,"<<plan.edges.size()<<"relations; no JARs downloaded";
        QJsonArray nodes,edges;for(const auto &file:plan.resolved)nodes.append(QJsonObject{{"project",file.projectId},{"version",file.versionId},{"file",file.fileName}});for(const auto &edge:plan.edges)edges.append(QJsonObject{{"from",edge.from},{"to",edge.to},{"type",edge.type}});
        QDir().mkpath(QFileInfo(args.at(2)).path());QFile record(args.at(2)+".json");if(record.open(QIODevice::WriteOnly))record.write(QJsonDocument(QJsonObject{{"root",plan.rootProjectId},{"nodes",nodes},{"edges",edges}}).toJson());
        // Visual-only reused state; this probe never installs or changes game files.
        if(args.contains("--preview-reuse")&&plan.files.size()>1){for(int i=0;i<plan.files.size();++i)if(plan.files[i].projectId!=plan.rootProjectId){const auto file=plan.files.takeAt(i);plan.reusedProjects.append(file.projectId);plan.reusedFiles.insert(file.projectId,file.fileName);break;}}
        mc::ModPlanView view(plan);view.show();if(args.contains("--uncheck"))for(auto check:view.findChildren<QCheckBox *>("dependencyChoice"))check->setChecked(false);QTimer::singleShot(1800,&app,[&]{if(!view.grab().save(args.at(2))){app.exit(1);return;}view.reject();app.exit(0);});return app.exec();
    }catch(const std::exception &e){qCritical()<<e.what();return 1;}
}
