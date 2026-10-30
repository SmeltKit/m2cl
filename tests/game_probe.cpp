// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

// Opt-in end-to-end check; never run by CTest. Uses only the supplied game root.
#include "network/gameinstaller.h"
#include "network/optifine.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QThread>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) { qCritical() << "Usage: game_probe ROOT [VERSION=1.20.1] [--launch] [--cached]"; return 2; }
    const QString root = QString::fromLocal8Bit(argv[1]); QDir().mkpath(root);
    const QString version = argc > 2 ? QString::fromLocal8Bit(argv[2]) : "1.20.1";
    const auto flag = std::make_shared<std::atomic_bool>(false);
    mc::InstallRequest request; request.root = QFileInfo(root).canonicalFilePath(); request.id = version + "-offline-probe";
    request.version = version; request.directory = QDir(request.root).filePath("versions/" + request.id); request.player = "OfflineTester";
    for(const auto &arg:app.arguments())if(arg.startsWith("--source="))request.downloadSource=arg.mid(9);
    for(const auto &arg:app.arguments())if(arg.startsWith("--fabric=")){request.loader="fabric";request.loaderVersion=arg.mid(9);}
    for(const auto &arg:app.arguments())for(const auto &loader:QStringList{"forge","neoforge","quilt"})if(arg.startsWith("--"+loader+"=")){request.loader=loader;request.loaderVersion=arg.mid(loader.size()+3);}
    if(request.loader=="quilt") {
        const auto list=mc::fetchQuiltVersions(version,flag);
        if(!list.error.isEmpty()){qCritical().noquote()<<list.error;return 1;}
        bool found=false;for(const auto &entry:list.versions)found|=entry.toObject().value("loader").toObject().value("version")==request.loaderVersion;
        if(!found){qCritical()<<"Requested Quilt build missing from official list";return 1;}
        qInfo()<<"Quilt builds:"<<list.versions.size()<<"default:"<<list.versions.first().toObject().value("loader").toObject().value("version");
    }
    for(const auto &arg:app.arguments())if(arg.startsWith("--java="))request.java=arg.mid(7);
    for(const auto &arg:app.arguments())if(arg.startsWith("--optifine=")) {
        const auto build=arg.mid(11);request.optifineAction=build=="remove"?"remove":"install";
        if(build!="remove"){const auto list=mc::fetchOptifineBuilds(version,flag,request.downloadSource);if(!list.error.isEmpty()){qCritical().noquote()<<list.error;return 1;}for(const auto &v:list.versions)if(v.toObject().value("version")==build)request.optifineBuild=v.toObject();if(request.optifineBuild.isEmpty()){qCritical()<<"Build missing";return 1;}}
    }
    for(const auto &arg:app.arguments())if(arg.startsWith("--optifine-jar="))request.optifineLocalFile=arg.mid(15);
    if (!app.arguments().contains("--cached")) {
        const auto versions = mc::fetchGameVersions(flag,request.downloadSource);
        if (!versions.error.isEmpty()) { qCritical().noquote() << versions.error; return 1; }
        for (const auto &v : versions.versions) if (v.toObject().value("id") == version) request.versionEntry = v.toObject();
        if (request.versionEntry.isEmpty()) { qCritical() << "Version missing"; return 1; }
    }
    int last = -1; QString stage;
    const auto result = mc::prepareGame(request, flag, [&](const QString &message, int value) {
        const auto current = message.section(QStringLiteral(" · "), 0, 0);
        if (current != stage || value / 50 != last) { qInfo().noquote() << message; stage = current; last = value / 50; }
    });
    if (!result.error.isEmpty()) { qCritical().noquote() << result.error; return 1; }
    qInfo().noquote() << "Installation verified; Java:" << result.java;
    if (!app.arguments().contains("--launch")) return 0;
    const auto logPath = QDir(request.directory).filePath("probe.log");
    QProcess game; game.setWorkingDirectory(result.command.directory); game.setProcessChannelMode(QProcess::MergedChannels);
    game.setStandardOutputFile(logPath); game.start(result.command.executable, result.command.arguments);
    if (!game.waitForStarted(10000)) { qCritical() << game.errorString(); return 1; }
    QElapsedTimer elapsed; elapsed.start(); bool ready = false;
    while (elapsed.elapsed() < 120000 && game.state() != QProcess::NotRunning) {
        game.waitForFinished(500); QFile log(logPath);
        if (log.open(QIODevice::ReadOnly)) {
            const auto text = log.readAll();
            if (text.contains("Created:") && text.contains("atlas")) { ready = true; qInfo() << "Game rendered texture atlases; keeping window open for 8 seconds"; QThread::sleep(8); break; }
        }
    }
    const bool alive = game.state() != QProcess::NotRunning;
    if (alive) { game.terminate(); if (!game.waitForFinished(5000)) { game.kill(); game.waitForFinished(5000); } }
    qInfo().noquote() << "Log:" << logPath;
    if (!ready || !alive) { QFile log(logPath); if (log.open(QIODevice::ReadOnly)) qCritical().noquote() << log.readAll().right(12000); return 1; }
    qInfo() << "Offline Minecraft launch passed"; return 0;
}
