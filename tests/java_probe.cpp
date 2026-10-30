// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

// Opt-in live provider / runtime check. Not registered with CTest.
#include "network/javaprovider.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); const auto args = app.arguments();
    if (args.size() < 3) { qCritical() << "Usage: java_probe ROOT PROVIDER [MAJOR=17] [--install] [--game-root PATH]"; return 2; }
    const auto root = args.at(1), provider = args.at(2); const int major = args.size() > 3 ? args.at(3).toInt() : 17;
    const auto flag = std::make_shared<std::atomic_bool>(false); const auto list = mc::fetchJavaPackages(provider, major, mc::GamePlatform::current(), flag);
    if (list.packages.isEmpty()) { qCritical().noquote() << list.error; return 1; }
    qInfo() << "Available builds:" << list.packages.size(); for (int i = 0; i < qMin(5, int(list.packages.size())); ++i) qInfo().noquote() << list.packages.at(i).label;
    if (!args.contains("--install")) return 0;
    int last = -1; QString stage;
    const auto installed = mc::installJavaPackage(list.packages.first(), root, flag, [&](const QString &message, int value) {
        const auto current = message.section(QStringLiteral(" · "), 0, 0); if (current != stage || value / 100 != last) { qInfo().noquote() << message; last = value / 100; stage = current; }
    });
    if (!installed.error.isEmpty()) { qCritical().noquote() << installed.error; return 1; }
    qInfo().noquote() << "Verified installed runtime:" << mc::javaLabel(installed.runtime) << installed.runtime.executable;
    QDir().mkpath(root); QFile record(QDir(root).filePath("probe-runtime.json"));
    if (record.open(QIODevice::WriteOnly)) record.write(QJsonDocument(mc::javaCatalogJson({installed.runtime})).toJson());
    const int gameOption = args.indexOf("--game-root"); if (gameOption < 0 || gameOption + 1 >= args.size()) return 0;
    mc::InstallRequest request; request.root = QFileInfo(args.at(gameOption + 1)).canonicalFilePath(); request.id = "1.20.1-offline-probe"; request.version = "1.20.1"; request.directory = QDir(request.root).filePath("versions/" + request.id);
    request.player = "JavaTester"; request.java = installed.runtime.executable; request.javaChoice = mc::javaPin(installed.runtime);
    const auto prepared = mc::prepareGame(request, flag, {}); if (!prepared.error.isEmpty()) { qCritical().noquote() << prepared.error; return 1; }
    const auto logPath = QDir(request.directory).filePath("java-manager-probe.log"); QProcess game; game.setWorkingDirectory(prepared.command.directory); game.setProcessChannelMode(QProcess::MergedChannels); game.setStandardOutputFile(logPath);
    game.start(prepared.command.executable, prepared.command.arguments); if (!game.waitForStarted(10000)) return 1;
    QElapsedTimer elapsed; elapsed.start(); bool ready = false;
    while (elapsed.elapsed() < 90000 && game.state() != QProcess::NotRunning) { game.waitForFinished(500); QFile log(logPath); if (log.open(QIODevice::ReadOnly)) { const auto bytes = log.readAll(); if (bytes.contains("Created:") && bytes.contains("atlas")) { ready = true; QThread::sleep(3); break; } } }
    const bool alive = game.state() != QProcess::NotRunning;
    if (alive) { game.terminate(); if (!game.waitForFinished(5000)) { game.kill(); game.waitForFinished(5000); } }
    qInfo().noquote() << "Game log:" << logPath; if (!ready || !alive) return 1;
    qInfo() << "Minecraft initialized successfully with the selected distribution and build"; return 0;
}
