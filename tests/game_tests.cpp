// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "core/gameplan.h"
#include "core/gamelog.h"
#include "core/settings.h"
#include "network/gameinstaller.h"
#include "network/forgeinstaller.h"
#include "network/optifine.h"
#include "network/downloadsource.h"
#include "core/mods.h"
#include "core/catalog.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QProcess>
#include <QTest>
#include <QTimer>
#include <QtEndian>
#include <cstring>
#include <zlib.h>

namespace {
QJsonObject descriptor(const QByteArray &bytes, const QString &url) {
    return {{"sha1", QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex())}, {"size", bytes.size()}, {"url", url}};
}
class Reply : public QNetworkReply {
    QByteArray body; qint64 position = 0; bool available = false;
public:
    Reply(const QNetworkRequest &req, QByteArray bytes, QObject *parent) : QNetworkReply(parent), body(std::move(bytes)) {
        setRequest(req); setUrl(req.url()); open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(0, this, [this] { if (isFinished()) return; available = true; emit readyRead(); if (!isFinished()) { setFinished(true); emit finished(); } });
    }
    void abort() override { if (!isFinished()) { setError(OperationCanceledError, "Cancelled"); setFinished(true); emit finished(); } }
    qint64 bytesAvailable() const override { return (available ? body.size() - position : 0) + QNetworkReply::bytesAvailable(); }
    qint64 readData(char *data, qint64 maximum) override {
        if (!available || position >= body.size()) return -1;
        const auto n = qMin(maximum, body.size() - position); std::memcpy(data, body.constData() + position, n); position += n; return n;
    }
};
class Network : public QNetworkAccessManager {
public:
    QMap<QString, QByteArray> responses;
    QMap<QString, int> counts;
    QString corrupt;
    bool alwaysCorrupt = false;
    QNetworkReply *createRequest(Operation, const QNetworkRequest &req, QIODevice *) override {
        const auto key = req.url().toString(); const int n = ++counts[key];
        const auto bytes = key == corrupt && (alwaysCorrupt || n == 1) ? QByteArray("bad") : responses.value(key);
        return new Reply(req, bytes, this);
    }
};
// Minimal ZIP builder used to exercise extraction of untrusted entries.
QByteArray zip(const QByteArray &name, const QByteArray &body, quint32 attributes = 0100644u << 16) {
    QByteArray bytes;
    auto u16 = [&](quint16 n) { char b[2]; qToLittleEndian(n, b); bytes.append(b, 2); };
    auto u32 = [&](quint32 n) { char b[4]; qToLittleEndian(n, b); bytes.append(b, 4); };
    const auto crc = crc32(0, reinterpret_cast<const Bytef *>(body.constData()), body.size());
    u32(0x04034b50); u16(20); u16(0); u16(0); u16(0); u16(0); u32(crc); u32(body.size()); u32(body.size()); u16(name.size()); u16(0); bytes += name; bytes += body;
    const auto offset = bytes.size();
    u32(0x02014b50); u16(0x0314); u16(20); u16(0); u16(0); u16(0); u16(0); u32(crc); u32(body.size()); u32(body.size()); u16(name.size()); u16(0); u16(0); u16(0); u16(0); u32(attributes); u32(0); bytes += name;
    const auto length = bytes.size() - offset;
    u32(0x06054b50); u16(0); u16(0); u16(1); u16(1); u32(length); u32(offset); u16(0); return bytes;
}
bool save(const QString &path, const QByteArray &bytes) { QDir().mkpath(QFileInfo(path).path()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
}
class GameTests : public QObject {
    Q_OBJECT
private slots:
    void sourcesAreBoundedAndNeverRewriteUnknownServices() {
        const QUrl url("https://libraries.minecraft.net/org/example.jar"),mirror("https://bmclapi2.bangbang93.com/libraries/org/example.jar");
        QCOMPARE(mc::downloadCandidates(url,mc::resolveDownloadSource("auto","Europe/London")),(QList<QUrl>{url,mirror}));
        QCOMPARE(mc::downloadCandidates(url,mc::resolveDownloadSource("auto","Asia/Shanghai")),(QList<QUrl>{mirror,url}));
        QCOMPARE(mc::downloadCandidates(url,mc::resolveDownloadSource("auto","asia/shanghai")),(QList<QUrl>{mirror,url}));
        QCOMPARE(mc::downloadSourceOrder(mc::resolveDownloadSource("auto","Asia/Tokyo")),(QStringList{"official","bmclapi"}));
        QCOMPARE(mc::downloadSourceOrder(mc::resolveDownloadSource("bmclapi","UTC")),(QStringList{"bmclapi"}));
        QCOMPARE(mc::downloadCandidates(url,"official"),(QList<QUrl>{url}));
        QCOMPARE(mc::downloadCandidates(url,"bmclapi"),(QList<QUrl>{mirror}));
        for(const QUrl u:{QUrl("https://api.modrinth.com/v2/search"),QUrl("https://maven.neoforged.net/releases/x"),QUrl("https://meta.quiltmc.org/v3/versions/loader")})QCOMPARE(mc::downloadCandidates(u,"auto"),(QList<QUrl>{u}));
        QCOMPARE(mc::downloadCandidates(QUrl("https://resources.download.minecraft.net/aa/hash"),"bmclapi").first().path(),QString("/assets/aa/hash"));
    }
    void metadataAndFilesSwitchOnceAndPreserveChecksumValidation() {
        Network net;const QUrl url("https://libraries.minecraft.net/test.jar");const auto candidates=mc::downloadCandidates(url,"auto");const auto mirror=mc::downloadCandidates(url,"bmclapi").first();
        const QByteArray good="verified file";auto flag=std::make_shared<std::atomic_bool>(false);const auto sha=descriptor(good,url.toString()).value("sha1").toString();
        net.responses={{candidates.first().toString(),"corrupt"},{candidates.last().toString(),good}};
        QStringList messages;auto progress=[&](const QString &s,int){messages.append(s);};
        QCOMPARE(mc::fetchInstallData(url,flag,sha,&net,"auto",progress),good);
        QCOMPARE(net.counts.value(url.toString()),1);QCOMPARE(net.counts.value(mirror.toString()),1);QVERIFY(messages.join(" ").contains("BMCLAPI"));
        net.responses[url.toString()]="corrupt";net.responses[mirror.toString()]="also corrupt";
        QTemporaryDir dir;QVERIFY(save(dir.filePath("test.jar"),"original"));
        QVERIFY_EXCEPTION_THROWN(mc::downloadVerifiedGameFiles({mc::gameFile(descriptor(good,url.toString()),"test.jar")},QFileInfo(dir.path()).canonicalFilePath(),flag,progress,&net,"auto"),std::exception);
        QCOMPARE(net.counts.value(url.toString()),2);QCOMPARE(net.counts.value(mirror.toString()),2);
        QFile original(dir.filePath("test.jar"));QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("original"));original.close();
        net.responses[mirror.toString()]=good;
        mc::downloadVerifiedGameFiles({mc::gameFile(descriptor(good,url.toString()),"test.jar")},QFileInfo(dir.path()).canonicalFilePath(),flag,progress,&net,"bmclapi");
        QCOMPARE(net.counts.value(url.toString()),2);QCOMPARE(net.counts.value(mirror.toString()),3);
        flag->store(true);QVERIFY_EXCEPTION_THROWN(mc::fetchInstallData(url,flag,sha,&net,"auto"),std::exception);QCOMPARE(net.counts.value(url.toString()),2);
    }
    void mirrorOptifineListKeepsExactGameForgeAndChannels() {
        const QByteArray json=R"([{"mcversion":"1.20.1","type":"HD_U","patch":"I6","filename":"OptiFine_1.20.1_HD_U_I6.jar","forge":"Forge 47.2.18"},{"mcversion":"1.20.1","type":"HD_U_I6","patch":"pre1","filename":"preview_OptiFine_1.20.1_HD_U_I6_pre1.jar"},{"mcversion":"1.20.2","type":"HD_U","patch":"I6"}])";
        const auto builds=mc::parseBmclOptifineBuilds(json,"1.20.1");QCOMPARE(builds.size(),2);QVERIFY(builds[0].toObject().value("preview").toBool());QCOMPARE(builds[1].toObject().value("forge").toString(),QString("47.2.18"));
    }
    void optifineOfficialBuildsAreSeparatedAndVersionSpecific() {
        const QByteArray html=R"HTML(<table>
          <tr><td><a href="adloadx?f=OptiFine_1.20.1_HD_U_I6.jar">Mirror</a></td><td class='colForge'>Forge 47.2.18</td></tr>
          <tr><td><a href="adloadx?f=preview_OptiFine_1.20.1_HD_U_I6_pre1.jar">Mirror</a></td><td class='colForge'>Forge N/A</td></tr>
          <tr><td><a href="adloadx?f=OptiFine_1.20_HD_U_I5.jar">Mirror</a></td><td class='colForge'>Forge 46.0.14</td></tr>
          <tr><td><a href="adloadx?f=OptiFine_1.20.1_HD_U_I6.jar">duplicate</a></td></tr>
        </table>)HTML";
        const auto builds=mc::parseOptifineBuilds(html,"1.20.1");QCOMPARE(builds.size(),2);
        const auto release=builds[0].toObject(),preview=builds[1].toObject();
        QCOMPARE(release.value("version").toString(),QString("HD_U_I6"));QVERIFY(!release.value("preview").toBool());QVERIFY(preview.value("preview").toBool());
        QVERIFY(mc::optifineCompatibility(release,"1.20.1","vanilla",{}).isEmpty());
        QVERIFY(mc::optifineCompatibility(release,"1.20.1","forge","47.2.18").isEmpty());
        QVERIFY(!mc::optifineCompatibility(release,"1.20.1","forge","47.3.0").isEmpty());
        QVERIFY(!mc::optifineCompatibility(preview,"1.20.1","forge","47.2.18").isEmpty());
        for(const auto &loader:{"fabric","quilt","neoforge"})QVERIFY(!mc::optifineCompatibility(release,"1.20.1",loader,"1").isEmpty());
        QVERIFY(!mc::optifineCompatibility(release,"1.20.2","vanilla",{}).isEmpty());
    }
    void optifineRemovalRollsBackUntilMetadataIsCommitted() {
        QTemporaryDir dir;const auto file=dir.path()+"/mods/OptiFine.jar";const QByteArray bytes="original local component";QVERIFY(save(file,bytes));
        const QJsonObject base{{"id","1.20.1"},{"mainClass","net.minecraft.client.main.Main"}};
        const QJsonObject installed{{"_m2clOptifine",QJsonObject{{"base",base},{"modFile","OptiFine.jar"},{"sha256",QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())}}}};
        mc::InstallRequest req;req.directory=QFileInfo(dir.path()).canonicalFilePath();req.optifineAction="remove";auto flag=std::make_shared<std::atomic_bool>(false);
        {mc::OptifineChange change;auto metadata=installed;change.prepare(req,metadata,{},flag,{});QCOMPARE(metadata,base);change.commitFiles();QVERIFY(!QFile::exists(file));}
        QVERIFY(QFile::exists(file));QFile restored(file);QVERIFY(restored.open(QIODevice::ReadOnly));QCOMPARE(restored.readAll(),bytes);restored.close();
        {mc::OptifineChange change;auto metadata=installed;change.prepare(req,metadata,{},flag,{});change.commitFiles();change.finish();}
        QVERIFY(!QFile::exists(file));QCOMPARE(QDir(dir.path()+"/mods").entryList(QDir::Files).size(),0);
    }
    void optifineChangesRespectTheSharedModLockAndCancellation() {
        QTemporaryDir dir;const auto root=QFileInfo(dir.path()).canonicalFilePath();QVERIFY(save(root+"/mods/OptiFine.jar","unchanged"));
        mc::InstallRequest req;req.directory=root;req.optifineAction="remove";
        QJsonObject metadata{{"_m2clOptifine",QJsonObject{{"base",QJsonObject{{"id","1.20.1"}}}}}};
        QLockFile lock(root+"/mods/.m2cl-install.lock");QVERIFY(lock.tryLock());
        mc::OptifineChange change;auto flag=std::make_shared<std::atomic_bool>(false);
        QVERIFY_THROWS_EXCEPTION(std::exception,change.prepare(req,metadata,{},flag,{}));QVERIFY(QFile::exists(root+"/mods/OptiFine.jar"));
        lock.unlock();flag->store(true);QVERIFY_THROWS_EXCEPTION(std::exception,change.prepare(req,metadata,{},flag,{}));
    }
    void optifineDoesNotOverwriteExternallyChangedFiles() {
        QTemporaryDir dir;QVERIFY(save(dir.path()+"/mods/OptiFine.jar","changed"));mc::InstallRequest req;req.directory=QFileInfo(dir.path()).canonicalFilePath();req.optifineAction="remove";
        QJsonObject metadata{{"_m2clOptifine",QJsonObject{{"base",QJsonObject{{"id","1.20.1"}}},{"modFile","OptiFine.jar"},{"sha256","wrong"}}}};
        mc::OptifineChange change;QVERIFY_EXCEPTION_THROWN(change.prepare(req,metadata,{},std::make_shared<std::atomic_bool>(false),{}),std::runtime_error);
        QVERIFY(QFile::exists(dir.path()+"/mods/OptiFine.jar"));
    }
    void optifineComponentParticipatesInLocalConflictChecks() {
        mc::Instance instance;instance.version="1.20.1";instance.loader="forge";instance.loaderVersion="47.2.18";instance.optifineVersion="HD_U_I6";instance.optifineForgeVersion="47.2.18";instance.optifineSha512="verified";
        mc::LocalMod of;of.known=true;of.id="optifine";of.name="OptiFine";of.ids={"optifine"};of.sha512="verified";
        QVERIFY(mc::modEnvironmentProblem(of,instance).isEmpty());of.sha512="changed";QVERIFY(!mc::modEnvironmentProblem(of,instance).isEmpty());
        mc::LocalMod iris;iris.known=true;iris.id="iris";iris.ids={"iris"};iris.name="Iris";iris.loader="forge";
        QVERIFY(!mc::checkModSet(instance,{iris}).errors.isEmpty());
    }
    void loggingConfigurationIsAnEncodedFileUrl() {
        QTemporaryDir dir;mc::GamePlan plan;plan.classpath={"client.jar"};
        plan.loggingArgument="-Dlog4j.configurationFile=C:/Game Folder/log.xml";
        QJsonObject metadata{{"mainClass","example.Main"}};
        const auto root=QFileInfo(dir.path()).canonicalFilePath();
        auto command=mc::offlineCommand(metadata,plan,root,"id",root,"java","Tester",2048,{"windows","x86_64",""});
        QVERIFY(command.arguments.contains("-Dlog4j.configurationFile=file:///C:/Game%20Folder/log.xml"));
    }
    void forgeMetadataIsFilteredByExactGame() {
        const QByteArray xml="<metadata><versioning><versions><version>1.20.1-47.3.0</version><version>1.20.1-47.2.0</version><version>1.20.2-48.0.0</version></versions></versioning></metadata>";
        const auto list=mc::forgeVersionsFromXml(xml,"forge","1.20.1");
        QCOMPARE(list.size(),2);QCOMPARE(list.first().toObject().value("loader").toObject().value("version").toString(),QString("47.3.0"));
        QCOMPARE(mc::forgeVersionsFromXml("<metadata><version>21.1.172</version><version>21.0.12-beta</version></metadata>","neoforge","1.21.1").size(),1);
        QVERIFY_EXCEPTION_THROWN(mc::forgeInstallerUrl("neoforge","1.20.1","21.1.172"),std::exception);
        QCOMPARE(mc::forgeVersionsFromXml("<metadata><version>26.3.0.99</version><version>26.1.2.124</version></metadata>","neoforge","26.3").size(),1);
        QCOMPARE(mc::forgeMavenPath("net.minecraft:client:1.20.1:slim@jar"),QString("net/minecraft/client/1.20.1/client-1.20.1-slim.jar"));
        QVERIFY_EXCEPTION_THROWN(mc::forgeMavenPath("net.minecraft:../escape:1"),std::exception);
    }
    void modernLoaderDetectionUsesLaunchArguments() {
        QJsonObject neo{{"arguments",QJsonObject{{"game",QJsonArray{"--fml.neoForgeVersion","21.1.172"}}}},{"libraries",QJsonArray{QJsonObject{{"name","net.neoforged:srgutils:1.0"}}}}};
        QCOMPARE(mc::gameLoaderComponents(neo), (QMap<QString,QString>{{"neoforge","21.1.172"}}));
        QJsonObject forge{{"inheritsFrom","1.20.1"},{"libraries",QJsonArray{QJsonObject{{"name","net.minecraftforge:fmlloader:1.20.1-47.3.0"}}}}};
        QCOMPARE(mc::gameLoaderComponents(forge).value("forge"),QString("47.3.0"));
    }
    void forgeOutputTamperingIsBlocked() {
        QTemporaryDir dir;const QByteArray bytes="patched client";QVERIFY(save(dir.path()+"/libraries/patched.jar",bytes));
        auto item=descriptor(bytes,"");item.insert("path","libraries/patched.jar");QJsonObject metadata{{"_m2clLoaderFiles",QJsonArray{item}}};
        mc::verifyForgeOutputs(metadata,QFileInfo(dir.path()).canonicalFilePath());QVERIFY(save(dir.path()+"/libraries/patched.jar","broken"));
        QVERIFY_EXCEPTION_THROWN(mc::verifyForgeOutputs(metadata,QFileInfo(dir.path()).canonicalFilePath()),std::exception);
    }
    void forgeIgnoreListUsesActualClientJarName() {
        QTemporaryDir dir;mc::GamePlan plan;plan.classpath={"classes"};
        QJsonObject metadata{{"id","upstream-forge-id"},{"mainClass","Main"},{"arguments",QJsonObject{{"jvm",QJsonArray{"-DignoreList=${version_name}.jar","-cp","${classpath}"}}}}};
        const auto command=mc::offlineCommand(metadata,plan,QFileInfo(dir.path()).canonicalFilePath(),"my-instance",QFileInfo(dir.path()).canonicalFilePath(),"java","Tester",1024,mc::GamePlatform::current());
        QVERIFY(command.arguments.contains("-DignoreList=my-instance.jar"));
    }
    void quiltProfileIsOneLoaderWithFabricLibraries() {
        QJsonObject profile{{"libraries",QJsonArray{
            QJsonObject{{"name","org.quiltmc:quilt-loader:0.29.2"}},
            QJsonObject{{"name","net.fabricmc:intermediary:1.20.1"}},
            QJsonObject{{"name","net.fabricmc:sponge-mixin:0.16.3+mixin.0.8.7"}}}}};
        QCOMPARE(mc::gameLoaderComponents(profile),(QMap<QString,QString>{{"quilt","0.29.2"}}));
        auto libs=profile.value("libraries").toArray();libs.append(QJsonObject{{"name","net.fabricmc:fabric-loader:0.16.10"}});profile.insert("libraries",libs);
        QCOMPARE(mc::gameLoaderComponents(profile).size(),2);
    }
    void fabricProfileInheritsVanillaAndReplacesLibraries(){
        QJsonObject base{{"id","1.20.1"},{"mainClass","Vanilla"},{"downloads",QJsonObject{{"client",QJsonObject{{"url","client"}}}}},{"libraries",QJsonArray{QJsonObject{{"name","a:b:1"}},QJsonObject{{"name","c:d:1"}}}},{"arguments",QJsonObject{{"game",QJsonArray{"--demo"}}}}};
        QJsonObject child{{"id","fabric-loader-1.20.1"},{"inheritsFrom","1.20.1"},{"mainClass","KnotClient"},{"libraries",QJsonArray{QJsonObject{{"name","a:b:2"}}}},{"arguments",QJsonObject{{"jvm",QJsonArray{"-Dfabric=true"}}}}};
        auto merged=mc::mergeGameMetadata(base,child);QCOMPARE(merged.value("_baseId").toString(),QString("1.20.1"));QCOMPARE(merged.value("mainClass").toString(),QString("KnotClient"));QVERIFY(merged.contains("downloads"));QVERIFY(!merged.contains("inheritsFrom"));QCOMPARE(merged.value("libraries").toArray().size(),2);QCOMPARE(merged.value("arguments").toObject().value("game").toArray().size(),1);
    }

    void realJavaReceivesSavedInstanceArguments(){
        const auto home=qEnvironmentVariable("M2CL_JAVA_TEST_HOME");if(home.isEmpty())QSKIP("Opt-in real Java check; set M2CL_JAVA_TEST_HOME to an installed JDK");
        QTemporaryDir temporary;const auto root=QFileInfo(temporary.path()).canonicalFilePath();QDir().mkpath(root+"/classes");
        const QByteArray source=R"JAVA(public class LaunchArgumentsProbe {
 public static void main(String[] args) {
  System.out.println("heap="+Runtime.getRuntime().maxMemory()/1024/1024);
  System.out.println("initial="+Runtime.getRuntime().totalMemory()/1024/1024);
  System.out.println("property="+System.getProperty("m2cl.probe"));
  System.out.println("args="+String.join("|",args));
 }
})JAVA";
        QVERIFY(save(root+"/LaunchArgumentsProbe.java",source));QProcess compiler;compiler.start(home+"/bin/javac",{"-encoding","UTF-8","-d",root+"/classes",root+"/LaunchArgumentsProbe.java"});QVERIFY(compiler.waitForFinished(30000));QVERIFY2(compiler.exitCode()==0,compiler.readAllStandardError().constData());
        mc::LaunchOptions options;options.autoMemory=false;options.maxMemoryMiB=768;options.minMemoryMiB=128;options.jvmArguments="-XX:+UseG1GC -Dm2cl.probe=\"value with spaces\"";options.gameArguments="--server localhost --port 25565";options.customResolution=true;options.width=1280;options.height=720;options.fullscreen=true;
        QString error;QVERIFY2(mc::saveInstanceRuntimeSettings(root,{{"mode","auto"}},options,&error),qPrintable(error));const auto loaded=mc::loadInstanceLaunchOptions(root,2048,&error);QVERIFY(error.isEmpty());
        mc::GamePlan plan;plan.classpath={"classes"};const QJsonObject metadata{{"id","test"},{"mainClass","LaunchArgumentsProbe"},{"arguments",QJsonObject{{"jvm",QJsonArray{"-cp","${classpath}"}},{"game",QJsonArray{"--username","${auth_player_name}"}}}}};
        const auto command=mc::offlineCommand(metadata,plan,root,"id",root,home+"/bin/java","Tester",loaded.maxMemoryMiB,mc::GamePlatform::current(),loaded);
        QProcess java;java.setWorkingDirectory(command.directory);java.start(command.executable,command.arguments);QVERIFY(java.waitForFinished(30000));const auto output=java.readAllStandardOutput();QVERIFY2(java.exitCode()==0,java.readAllStandardError().constData());
        QVERIFY2(output.contains("heap=768"),output.constData());QVERIFY2(output.contains("initial=130")||output.contains("initial=128"),output.constData());QVERIFY(output.contains("property=value with spaces"));QVERIFY(output.contains("--width|1280|--height|720|--fullscreen|--server|localhost|--port|25565"));qInfo().noquote()<<output;
        const auto recommendation=mc::recommendMemory(mc::systemMemory(),mc::memoryContext("1.20.1",root,"vanilla",mc::GamePlatform::current().arch));
        QVERIFY2(recommendation.valid(),qPrintable(recommendation.error));options.autoMemory=true;options.minMemoryMiB=0;QVERIFY(mc::saveInstanceRuntimeSettings(root,{{"mode","auto"}},options,&error));
        auto automatic=mc::loadInstanceLaunchOptions(root);QVERIFY(automatic.autoMemory);const auto automaticCommand=mc::offlineCommand(metadata,plan,root,"id",root,home+"/bin/java","Tester",recommendation.maximumMiB,mc::GamePlatform::current(),automatic);
        java.start(automaticCommand.executable,automaticCommand.arguments);QVERIFY(java.waitForFinished(30000));const auto actual=java.readAllStandardOutput();QVERIFY2(java.exitCode()==0,java.readAllStandardError().constData());QVERIFY2(actual.contains("heap="+QByteArray::number(recommendation.maximumMiB)),actual.constData());qInfo().noquote()<<"Automatic heap verified by actual Java:"<<recommendation.maximumMiB<<"MiB";
    }
    void argumentsRespectQuotesAndKeepJvmBeforeTheMainClass(){
        QString error;const auto split=mc::splitLaunchArguments("-Dlabel=\"hello world\" --add-opens java.base/java.lang=ALL-UNNAMED\n-Dpath=C:\\Games\\Minecraft 'single quoted'",&error);
        QVERIFY(error.isEmpty());QCOMPARE(split,QStringList({"-Dlabel=hello world","--add-opens","java.base/java.lang=ALL-UNNAMED","-Dpath=C:\\Games\\Minecraft","single quoted"}));
        mc::splitLaunchArguments("-Dlabel=\"unfinished",&error);QVERIFY(!error.isEmpty());
        QTemporaryDir dir;const auto root=QFileInfo(dir.path()).canonicalFilePath();mc::GamePlan plan;plan.classpath={"classes"};
        const QJsonObject metadata{{"id","1.20.1"},{"mainClass","example.Main"},{"arguments",QJsonObject{{"jvm",QJsonArray{"-cp","${classpath}"}},{"game",QJsonArray{"--username","${auth_player_name}","--width","854","--height","480"}}}}};
        mc::LaunchOptions options;options.maxMemoryMiB=4096;options.minMemoryMiB=512;options.customResolution=true;options.width=1280;options.height=720;options.fullscreen=true;options.jvmArguments="-Dlabel=\"hello world\" -XX:+UseG1GC";options.gameArguments="--server \"my server\" --port 25565";
        const auto command=mc::offlineCommand(metadata,plan,root,"id",root,"java","Player",4096,{"linux","x86_64",""},options);
        const auto args=command.arguments;const int main=args.indexOf("example.Main");QVERIFY(args.indexOf("-Dlabel=hello world")<main);QVERIFY(args.indexOf("-XX:+UseG1GC")<main);QVERIFY(args.indexOf("--server")>main);
        QCOMPARE(args.count("-Xmx4096M"),1);QCOMPARE(args.count("-Xms512M"),1);QCOMPARE(args.count("--width"),1);QCOMPARE(args.at(args.indexOf("--width")+1),QString("1280"));QCOMPARE(args.at(args.indexOf("--height")+1),QString("720"));QVERIFY(args.contains("--fullscreen"));QVERIFY(args.contains("my server"));
        options.jvmArguments="-Xmx8G";QVERIFY(!mc::validateLaunchOptions(options).isEmpty());options.jvmArguments="-XX:+UseG1GC";options.gameArguments="--gameDir elsewhere";QVERIFY(!mc::validateLaunchOptions(options).isEmpty());
        options.gameArguments.clear();bool rejected=false;try{mc::offlineCommand(metadata,plan,root,"id",root,"java","Player",2048,{"windows","x86",""},options);}catch(const std::runtime_error &){rejected=true;}QVERIFY(rejected);
    }
    void consoleXmlAndSplitUtf8AreReadable() {
        mc::GameLogDecoder decoder;
        const auto bytes = QStringLiteral("普通日志\n  <log4j:Event timestamp='1' thread='Render thread' level='ERROR'><log4j:Message><![CDATA[启动失败]]></log4j:Message><log4j:Throwable><![CDATA[stack trace]]></log4j:Throwable></log4j:Event>\n").toUtf8();
        QString text; for (const auto byte : bytes) text += decoder.append(QByteArray(1, byte));
        QVERIFY(text.contains(QStringLiteral("普通日志"))); QVERIFY(text.contains(QStringLiteral("启动失败"))); QVERIFY(text.contains("Render thread/ERROR")); QVERIFY(text.contains("stack trace")); QVERIFY(!text.contains("log4j:")); QVERIFY(!text.contains(QChar::ReplacementCharacter));
    }
    void namingAndOfflineIdentity() {
        QCOMPARE(mc::defaultInstanceName("1.20.1", "vanilla", {"1.20.1 原版", "1.20.1 原版（2）"}), QStringLiteral("1.20.1 原版（3）"));
        QCOMPARE(mc::defaultInstanceName("1.20.1", "forge", {}), QString("1.20.1 Forge"));
        QCOMPARE(mc::offlineUuid("Notch"), QString("b50ad385829d3141a2167e7d7539ba7f"));
    }
    void platformRules() {
        const mc::GamePlatform p{"osx", "aarch64", "24.0"};
        QVERIFY(mc::rulesAllow({}, p));
        QVERIFY(!mc::rulesAllow({QJsonObject{{"action", "allow"}, {"features", QJsonObject{{"is_demo_user", true}}}}}, p));
        QVERIFY(mc::rulesAllow({QJsonObject{{"action", "allow"}, {"os", QJsonObject{{"name", "osx"}, {"arch", "arm64"}}}}}, p));
    }
    void nativeArchitectureFiltering() {
        QJsonArray libraries;
        for (const auto &arch : {QString(""), QString("-arm64")}) {
            auto file = descriptor("native", "https://test/native" + arch); file.insert("path", "lwjgl/native" + arch + ".jar");
            libraries.append(QJsonObject{{"name", "org.lwjgl:lwjgl:3.3.1:natives-macos" + arch}, {"downloads", QJsonObject{{"artifact", file}}}});
        }
        const QJsonObject metadata{{"id", "1.20.1"}, {"libraries", libraries}, {"assetIndex", QJsonObject{{"id", "5"}, {"sha1", QString(40, '0')}, {"size", 0}, {"url", "https://test/index"}}}, {"downloads", QJsonObject{{"client", descriptor("client", "https://test/client")}}}};
        const auto plan = mc::gamePlan(metadata, "instance", {"osx", "aarch64", "24"});
        QVERIFY(plan.classpath.contains("libraries/lwjgl/native-arm64.jar")); QVERIFY(!plan.classpath.contains("libraries/lwjgl/native.jar"));
    }
    void nativeZipRejectsTraversalSymlinksAndCorruption() {
        QTemporaryDir dir; const auto root = QFileInfo(dir.path()).canonicalFilePath(); const auto archive = root + "/native.jar";
        QVERIFY(save(archive, zip("libnative.dylib", "native"))); mc::extractNatives(archive, root + "/out"); QVERIFY(QFileInfo::exists(root + "/out/libnative.dylib"));
        QVERIFY(save(archive, zip("../escape", "bad"))); { bool threw = false; try { mc::extractNatives(archive, root + "/out"); } catch (const std::runtime_error &) { threw = true; } QVERIFY(threw); }
        QVERIFY(save(archive, zip("link", "target", 0120777u << 16))); { bool threw = false; try { mc::extractNatives(archive, root + "/out"); } catch (const std::runtime_error &) { threw = true; } QVERIFY(threw); }
        auto corrupt = zip("native", "payload"); corrupt[36] = '!'; QVERIFY(save(archive, corrupt)); { bool threw = false; try { mc::extractNatives(archive, root + "/out"); } catch (const std::runtime_error &) { threw = true; } QVERIFY(threw); }
    }
    void unsafePathsAndPlayers() {
        for (const auto &p : {"../x", "/etc/passwd", "a/../../x", "C:x", "a\\b", "CON.txt"}) QVERIFY(!mc::safeRelativePath(p));
        bool threw = false; try { mc::offlineCommand({}, {}, "/", "id", "/", "java", "bad name", 2048, {}); } catch (const std::runtime_error &) { threw = true; } QVERIFY(threw);
    }
    void interruptedRuntimeIsCompletedBeforeReuse() {
#ifdef Q_OS_WIN
        QSKIP("Fixture Java is a POSIX script");
#endif
        QTemporaryDir dir; const auto root = QFileInfo(dir.path()).canonicalFilePath();
        const auto platform = mc::GamePlatform::current();
        const auto key = platform.os == "osx" ? (platform.arch == "aarch64" ? "mac-os-arm64" : "mac-os") : "linux";
        const QByteArray java = ("#!/bin/sh\nprintf 'java.version = 99.0.1\\nos.arch = " + platform.arch + "\\njava.vendor = Test\\n'\n").toUtf8();
        QJsonObject files{
            {"bin/java", QJsonObject{{"type", "file"}, {"executable", true}, {"downloads", QJsonObject{{"raw", descriptor(java, "https://test/java")}}}}},
            {"lib/needed", QJsonObject{{"type", "file"}, {"downloads", QJsonObject{{"raw", descriptor("needed", "https://test/needed")}}}}},
            {"legal", QJsonObject{{"type", "directory"}}},
            {"legal/copy", QJsonObject{{"type", "link"}, {"target", "../lib/needed"}}}
        };
        const auto runtime = QJsonDocument(QJsonObject{{"files", files}}).toJson();
        const auto all = QJsonDocument(QJsonObject{{key, QJsonObject{{"unit-test", QJsonArray{QJsonObject{{"manifest", descriptor(runtime, "https://test/runtime")}}}}}}}).toJson();
        const QByteArray index = "{\"objects\":{}}"; auto asset = descriptor(index, "https://test/index"); asset.insert("id", "5");
        QJsonObject meta{{"id", "1.20.1"}, {"mainClass", "Main"}, {"assetIndex", asset}, {"javaVersion", QJsonObject{{"majorVersion", 99}, {"component", "unit-test"}}},
            {"downloads", QJsonObject{{"client", descriptor("client", "https://test/client")}}}};
        const auto bytes = QJsonDocument(meta).toJson(); Network network;
        network.responses = {{"https://test/meta", bytes}, {"https://test/index", index}, {"https://test/client", "client"}, {"https://test/java", java}, {"https://test/needed", "needed"}, {"https://test/runtime", runtime},
            {"https://piston-meta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json", all}};
        mc::InstallRequest request; request.root = root; request.id = "runtime-test"; request.version = "1.20.1"; request.directory = root + "/versions/runtime-test"; request.player = "Tester";
        request.launchOptions.autoMemory=false;
        request.versionEntry = descriptor(bytes, "https://test/meta"); const auto flag = std::make_shared<std::atomic_bool>(false);
        network.corrupt = "https://test/needed"; network.alwaysCorrupt = true;
        auto result = mc::prepareGame(request, flag, {}, &network); QVERIFY(!result.error.isEmpty());
        const auto runtimeRoot = root + "/runtime/linjian/unit-test/" + key;
        QVERIFY(QFileInfo::exists(runtimeRoot + "/bin/java")); QVERIFY(!QFileInfo::exists(runtimeRoot + "/.linjian-complete"));
        network.alwaysCorrupt = false; network.corrupt.clear(); request.versionEntry = {};
        result = mc::prepareGame(request, flag, {}, &network); QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(network.counts.value("https://test/runtime"), 2); QVERIFY(QFileInfo::exists(runtimeRoot + "/.linjian-complete"));
        QFile copy(runtimeRoot + "/legal/copy"); QVERIFY(copy.open(QIODevice::ReadOnly)); QCOMPARE(copy.readAll(), QByteArray("needed")); QVERIFY(!QFileInfo(copy).isSymLink());
        const auto before = network.counts; result = mc::prepareGame(request, flag, {}, &network); QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(network.counts, before);
    }
    void installRetriesVerifiesAndReusesFiles() {
#ifdef Q_OS_WIN
        QSKIP("Fixture Java is a POSIX script");
#endif
        QTemporaryDir dir; const auto root = QFileInfo(dir.path()).canonicalFilePath();
        const auto java = root + "/bin/java";
        const auto platform = mc::GamePlatform::current();
        QVERIFY(save(java, ("#!/bin/sh\nprintf 'java.version = 17.0.1\\nos.arch = " + platform.arch + "\\njava.vendor = Test\\n'\n").toUtf8()));
        QVERIFY(QFile::setPermissions(java, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        Network network; const QByteArray index = "{\"objects\":{}}";
        auto asset = descriptor(index, "https://test/index"); asset.insert("id", "5");
        QJsonObject meta{{"id", "1.20.1"}, {"mainClass", "net.minecraft.client.main.Main"}, {"assetIndex", asset}, {"javaVersion", QJsonObject{{"majorVersion", 17}}},
            {"downloads", QJsonObject{{"client", descriptor("client", "https://test/client")}}},
            {"arguments", QJsonObject{{"jvm", QJsonArray{"-cp", "${classpath}"}}, {"game", QJsonArray{"--username", "${auth_player_name}", "--uuid", "${auth_uuid}", "--gameDir", "${game_directory}"}}}}};
        const auto bytes = QJsonDocument(meta).toJson();
        network.responses = {{"https://test/meta", bytes}, {"https://test/index", index}, {"https://test/client", "client"}};
        network.corrupt = "https://test/client";
        mc::InstallRequest request; request.root = root; request.id = "test"; request.version = "1.20.1"; request.directory = root + "/versions/test"; request.player = "Tester"; request.java = java;
        request.versionEntry = descriptor(bytes, "https://test/meta");
        bool sampledAfterDownloads=false;qint64 availableMiB=10240;request.memorySampler=[&]{sampledAfterDownloads=network.counts.value("https://test/client")>=2;mc::MemorySnapshot s;s.totalMiB=16384;s.availableMiB=availableMiB;s.source="test";return s;};
        const auto flag = std::make_shared<std::atomic_bool>(false);
        auto result = mc::prepareGame(request, flag, {}, &network); QVERIFY(!result.error.isEmpty());
        QCOMPARE(network.counts.value("https://test/client"),1);
        result = mc::prepareGame(request, flag, {}, &network); QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(sampledAfterDownloads);QVERIFY(result.command.arguments.contains("-Xmx3072M"));
        request.launchOptions.autoMemory=false;request.memoryMiB=1536;result=mc::prepareGame(request,flag,{},&network);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(result.command.arguments.contains("-Xmx1536M"));request.launchOptions.autoMemory=true;
        result=mc::prepareGame(request,flag,[&](const QString &message,int){if(message==QStringLiteral("正在准备原生库…"))availableMiB=2816;},&network);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(result.command.arguments.contains("-Xmx768M"));availableMiB=10240;
        request.launchOptions.minMemoryMiB=4096;result=mc::prepareGame(request,flag,{},&network);QVERIFY(result.error.contains(QStringLiteral("初始内存")));request.launchOptions.minMemoryMiB=0;
        QVERIFY(save(java,"#!/bin/sh\nprintf 'java.version = 17.0.1\\nos.arch = x86\\njava.vendor = Test\\n'\n"));result=mc::prepareGame(request,flag,{},&network);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(result.command.arguments.contains("-Xmx768M"));
        QVERIFY(save(java,("#!/bin/sh\nprintf 'java.version = 17.0.1\\nos.arch = "+platform.arch+"\\njava.vendor = Test\\n'\n").toUtf8()));
        QCOMPARE(network.counts.value("https://test/client"), 2); QVERIFY(result.command.arguments.contains("Tester")); QVERIFY(result.command.arguments.contains(mc::offlineUuid("Tester")));
        request.versionEntry = {}; const auto pinnedRequests = network.counts;
        request.javaChoice = {{"mode", "fixed"}, {"runtimeVersion", "17.0.99+99"}};
        result = mc::prepareGame(request, flag, {}, &network); QVERIFY(result.error.contains(QStringLiteral("构建"))); QCOMPARE(network.counts, pinnedRequests);
        request.javaChoice = {}; request.java = root + "/missing/bin/java";
        result = mc::prepareGame(request, flag, {}, &network); QVERIFY(!result.error.isEmpty()); QCOMPARE(network.counts, pinnedRequests);
        request.java = java;
        const auto before = network.counts; request.versionEntry = {};
        result = mc::prepareGame(request, flag, {}, &network); QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(network.counts, before);
        // Explicit pins may select a higher major, and lower majors require the persisted force flag.
        for(int major:{21,8}){QVERIFY(save(java,("#!/bin/sh\nprintf 'java.version = "+QString::number(major)+".0.1\\nos.arch = "+platform.arch+"\\njava.vendor = Test\\n'\n").toUtf8()));request.javaChoice={{"mode","fixed"}};result=mc::prepareGame(request,flag,{},&network);if(major==21){QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QCOMPARE(result.javaRuntime.major,21);}else{QVERIFY(result.error.contains(QStringLiteral("低于")));request.javaChoice.insert("allowBelowRecommended",true);result=mc::prepareGame(request,flag,{},&network);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QCOMPARE(result.javaRuntime.major,8);request.javaChoice.insert("runtimeVersion","17.0.99+99");result=mc::prepareGame(request,flag,{},&network);QVERIFY(result.error.contains(QStringLiteral("构建")));}}
        QVERIFY(save(java,("#!/bin/sh\nprintf 'java.version = 17.0.1\\nos.arch = "+platform.arch+"\\njava.vendor = Test\\n'\n").toUtf8()));request.javaChoice={};
        // A corrupt transfer must preserve the previous destination, then recover on retry.
        QVERIFY(save(root + "/versions/test/test.jar", "previous")); network.alwaysCorrupt = true;
        result = mc::prepareGame(request, flag, {}, &network); QVERIFY(!result.error.isEmpty());
        QFile original(root + "/versions/test/test.jar"); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), QByteArray("previous")); original.close();
        network.alwaysCorrupt = false; network.corrupt.clear();
        result = mc::prepareGame(request, flag, {}, &network); QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(save(root + "/versions/test/test.jar", "previous"));
        result = mc::prepareGame(request, flag, [flag](const QString &message, int) { if (message.startsWith(QStringLiteral("下载游戏与依赖 ·"))) flag->store(true); }, &network);
        QVERIFY(result.error.contains(QStringLiteral("取消")));
        flag->store(false); result = mc::prepareGame(request, flag, {}, &network); QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        flag->store(true); const auto requests = network.counts;
        result = mc::prepareGame(request, flag, {}, &network); QVERIFY(result.error.contains(QStringLiteral("取消"))); QCOMPARE(network.counts, requests);
    }
};
QTEST_GUILESS_MAIN(GameTests)
#include "game_tests.moc"
