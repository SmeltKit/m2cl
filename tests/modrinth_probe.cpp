// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

// Opt-in live API check. Never run by CTest; all downloads go to an auto-removed temporary directory.
#include "network/resourceclient.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); QTemporaryDir directory;
    if (!directory.isValid()) return 2;
    mc::ResourceClient client;
    QObject::connect(&client, &mc::ResourceClient::failed, &app, [&app](const QString &message) { qCritical().noquote() << message; app.exit(1); });
    QObject::connect(&client, &mc::ResourceClient::projectsReady, &app, [&](const QList<mc::ResourceProject> &projects) {
        if (projects.isEmpty()) { qCritical() << "No search results"; app.exit(1); return; }
        qInfo().noquote() << "Search OK:" << projects.size() << "results; selected" << projects.first().name;
        client.versions(projects.first().id, "1.20.1", "fabric");
    });
    QObject::connect(&client, &mc::ResourceClient::versionsReady, &app, [&](const QList<mc::ResourceFile> &files) {
        if (files.isEmpty()) { qCritical() << "No compatible versions"; app.exit(1); return; }
        qInfo().noquote() << "Versions OK:" << files.size() << "files; selected" << files.first().fileName;
        if(!mc::ModrinthProvider::compatible(files.first(),"1.20.1","fabric")){qCritical()<<"Unexpected compatibility";app.exit(1);return;}
        if(app.arguments().contains("--metadata-only")){app.exit(0);return;}
        client.download(files.first(), directory.path());
    });
    QObject::connect(&client, &mc::ResourceClient::downloadFinished, &app, [&app] { qInfo() << "Download, size and hash verification OK"; app.exit(0); });
    QTimer::singleShot(90000, &app, [&] { qCritical() << "Live probe timed out"; client.cancelDownload(); app.exit(1); });
    QObject::connect(&client, &mc::ResourceClient::tagsFailed, &app, [&app](const QString &error){ qCritical().noquote()<<error;app.exit(1); });
    QObject::connect(&client, &mc::ResourceClient::tagsReady, &app, [&](const mc::ResourceTags &tags){
        bool category=false,loader=false;for(const auto &t:tags.categories)if(t.id=="optimization")category=true;for(const auto &t:tags.loaders)if(t.id=="fabric")loader=true;
        if(!category || !loader || !tags.gameVersions.contains("1.20.1")){qCritical()<<"Tags missing expected filters";app.exit(1);return;}
        qInfo()<<"Live tags OK:"<<tags.categories.size()<<"categories,"<<tags.loaders.size()<<"loaders,"<<tags.gameVersions.size()<<"game versions";
        client.search("sodium", "1.20.1", "fabric", "optimization", "downloads");
    });
    client.tags(); return app.exec();
}
