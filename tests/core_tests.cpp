// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "core/catalog.h"
#include "core/java.h"
#include "core/resources.h"
#include "core/settings.h"
#include "core/memory.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

namespace {
bool writeJson(const QString &path, const QJsonObject &object) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(object).toJson()) > 0;
}
bool writeVersion(const QString &root, const QString &id, const QJsonObject &json) {
    return writeJson(QDir(root).filePath(QStringLiteral("versions/%1/%1.json").arg(id)), json);
}
}
class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void gameVersionChannels() {
        QCOMPARE(mc::gameVersionChannel("1.21.1", "release"), QString("release"));
        QCOMPARE(mc::gameVersionChannel("1.21-pre1", "snapshot"), QString("test"));
        QCOMPARE(mc::gameVersionChannel("24w14potato", "snapshot"), QString("special"));
        QCOMPARE(mc::gameVersionChannel("3D Shareware v1.34", "release"), QString("special"));
        QCOMPARE(mc::gameVersionChannel("b1.7.3", "old_beta"), QString("historical"));
        QCOMPARE(mc::gameVersionChannel("24w14a", "snapshot"), QString("test"));
    }

    void memoryRecommendationsRespectDemandBudgetAndJavaBits(){
        mc::MemoryContext context;context.gameVersion="1.20.1";context.javaArchitecture="aarch64";
        mc::MemorySnapshot memory;memory.totalMiB=8192;memory.availableMiB=5120;memory.source="test";
        auto result=mc::recommendMemory(memory,context);QVERIFY(result.valid());QCOMPARE(result.maximumMiB,3072);QCOMPARE(result.reserveMiB,1024);
        context.gameVersion="1.12.2";QCOMPARE(mc::recommendMemory(memory,context).maximumMiB,2048);
        memory.totalMiB=16384;memory.availableMiB=10240;context.gameVersion="1.20.1";
        for(const auto &entry:QList<QPair<int,int>>{{12,4096},{100,6144},{200,8192}}){context.enabledModFiles=entry.first;QCOMPARE(mc::recommendMemory(memory,context).maximumMiB,entry.second);}
        context.enabledModFiles=0;memory.totalMiB=65536;memory.availableMiB=49152;QCOMPARE(mc::recommendMemory(memory,context).maximumMiB,3072);
        context.javaArchitecture="x86";result=mc::recommendMemory(memory,context);QCOMPARE(result.maximumMiB,768);QVERIFY(!result.warning.isEmpty());
        context.javaArchitecture="amd64";memory.totalMiB=8192;memory.availableMiB=2048;result=mc::recommendMemory(memory,context);QCOMPARE(result.maximumMiB,1024);QVERIFY(!result.warning.isEmpty());
        context.packSuggestedMiB=8192;QVERIFY(!mc::recommendMemory(memory,context).valid());context.packSuggestedMiB=0;
        memory.availableMiB=0;QVERIFY(!mc::recommendMemory(memory,context).valid());memory.availableMiB=memory.totalMiB+1;QVERIFY(!mc::recommendMemory(memory,context).valid());memory.error="unavailable";QCOMPARE(mc::recommendMemory(memory,context).error,QString("unavailable"));
    }
    void automaticDefaultsPreserveLegacyManualChoices(){
        QTemporaryDir dir;QString error;QVERIFY(mc::loadInstanceLaunchOptions(dir.path(),2048,&error).autoMemory);QVERIFY(error.isEmpty());
        QVERIFY(writeJson(mc::instanceConfigPath(dir.path()),{{"java",QJsonObject{{"mode","auto"}}}}));QVERIFY(mc::loadInstanceLaunchOptions(dir.path()).autoMemory);
        QVERIFY(writeJson(mc::instanceConfigPath(dir.path()),{{"launch",QJsonObject{{"maxMemoryMiB",6144},{"minMemoryMiB",512}}}}));auto old=mc::loadInstanceLaunchOptions(dir.path());QVERIFY(!old.autoMemory);QCOMPARE(old.maxMemoryMiB,6144);
        old.autoMemory=true;QVERIFY(mc::saveInstanceRuntimeSettings(dir.path(),{{"mode","auto"}},old,&error));auto restored=mc::loadInstanceLaunchOptions(dir.path());QVERIFY(restored.autoMemory);QCOMPARE(restored.maxMemoryMiB,6144);
        QVERIFY(writeJson(mc::instanceConfigPath(dir.path()),{{"launch",QJsonObject{{"autoMemory","true"}}}}));mc::loadInstanceLaunchOptions(dir.path(),2048,&error);QVERIFY(!error.isEmpty());
    }
    void memoryEstimationCountsExternalEnabledFilesOnly(){
        QTemporaryDir dir;QDir().mkpath(dir.filePath("mods"));for(const auto &name:QStringList{"a.jar","b.jar.disabled","c.txt","d.zip","e.litemod"}){QFile file(dir.filePath("mods/"+name));QVERIFY(file.open(QIODevice::WriteOnly));}
        QCOMPARE(mc::memoryContext("1.20.1",dir.path(),"fabric").enabledModFiles,3);QCOMPARE(mc::memoryContext("1.20.1",dir.path(),"vanilla").enabledModFiles,0);
        const auto actual=mc::systemMemory();if(actual.error.isEmpty()){QVERIFY(actual.totalMiB>0);QVERIFY(actual.availableMiB>=0);QVERIFY(actual.availableMiB<=actual.totalMiB);}
    }
    void instanceRuntimeSettingsAreAtomicAndIndependent(){
        QTemporaryDir dir;const auto a=dir.filePath("A"),b=dir.filePath("B");QDir().mkpath(a);QDir().mkpath(b);
        QVERIFY(writeJson(mc::instanceConfigPath(a),{{"custom","keep"}}));
        mc::LaunchOptions first;first.autoMemory=false;first.maxMemoryMiB=4096;first.minMemoryMiB=512;first.jvmArguments="-Dlabel=\"hello world\"";first.gameArguments="--server localhost";first.customResolution=true;first.width=1280;first.height=720;
        QString error;QVERIFY2(mc::saveInstanceRuntimeSettings(a,{{"mode","auto"}},first,&error),qPrintable(error));
        auto second=first;second.maxMemoryMiB=8192;second.jvmArguments="-XX:+UseG1GC";QVERIFY(mc::saveInstanceRuntimeSettings(b,{{"mode","fixed"},{"path","/example/bin/java"}},second,&error));
        QCOMPARE(mc::loadInstanceLaunchOptions(a,2048,&error).maxMemoryMiB,4096);QVERIFY(error.isEmpty());QCOMPARE(mc::loadInstanceLaunchOptions(b).maxMemoryMiB,8192);
        const auto read=mc::loadInstanceLaunchOptions(a);QCOMPARE(read.jvmArguments,first.jvmArguments);QCOMPARE(read.gameArguments,first.gameArguments);QCOMPARE(read.minMemoryMiB,512);QVERIFY(read.customResolution);QCOMPARE(read.width,1280);QCOMPARE(read.height,720);
        QFile file(mc::instanceConfigPath(a));QVERIFY(file.open(QIODevice::ReadOnly));const auto before=file.readAll();file.close();QCOMPARE(QJsonDocument::fromJson(before).object().value("custom").toString(),QString("keep"));
        auto invalid=first;invalid.minMemoryMiB=8192;QVERIFY(!mc::saveInstanceRuntimeSettings(a,{{"mode","fixed"},{"path","changed"}},invalid,&error));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),before);file.close();QCOMPARE(mc::loadInstanceJavaChoice(a).value("mode").toString(),QString("auto"));
        QVERIFY(mc::saveInstanceJavaChoice(a,{{"mode","auto"}}));QCOMPARE(mc::loadInstanceLaunchOptions(a).maxMemoryMiB,4096);
        QVERIFY(writeJson(mc::instanceConfigPath(b),{{"launch",QJsonObject{{"maxMemoryMiB","8192"}}}}));mc::loadInstanceLaunchOptions(b,2048,&error);QVERIFY(!error.isEmpty());
        QCOMPARE(mc::loadInstanceLaunchOptions(dir.filePath("new"),3072,&error).maxMemoryMiB,3072);QVERIFY(error.isEmpty());
    }
    void settingsRoundTrip() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        mc::SettingsStore store(dir.path() + "/设置 空间");
        mc::Settings input; input.downloadSource="bmclapi"; input.theme = "violet"; input.appearance = "dark";
        input.gameRoots = QStringList{dir.path() + "/我的世界"}; input.selectedInstance = "实例 A";
        input.javaFolders = QStringList{"/jdk"}; input.exitAfterLaunch = false;
        input.offlinePlayer = "TestPlayer"; input.memoryMiB = 4096;
        input.isolationOverrides.insert("实例 A", true);
        input.favoriteInstances=QStringList{"实例 A"};input.instanceNames.insert("实例 A",QStringLiteral("樱谷生存"));input.coverOverrides.insert("实例 A","/cover.png");
        QString error; QVERIFY2(store.save(input, &error), qPrintable(error));
        const auto output = store.load(&error); QVERIFY(error.isEmpty());
        QCOMPARE(output.downloadSource,input.downloadSource); QCOMPARE(output.theme, input.theme); QCOMPARE(output.appearance, input.appearance);
        QCOMPARE(output.gameRoots, input.gameRoots); QCOMPARE(output.javaFolders, input.javaFolders);
        QCOMPARE(output.selectedInstance, input.selectedInstance);
        QCOMPARE(output.offlinePlayer, input.offlinePlayer); QCOMPARE(output.memoryMiB, input.memoryMiB);
        QCOMPARE(output.exitAfterLaunch, false); QCOMPARE(output.isolationOverrides, input.isolationOverrides);
        QCOMPARE(output.favoriteInstances,input.favoriteInstances);QCOMPARE(output.instanceNames,input.instanceNames);QCOMPARE(output.coverOverrides,input.coverOverrides);
    }
    void invalidSettingsRecoverDefaults() {
        QTemporaryDir dir; const auto path = dir.filePath("settings.json");
        QVERIFY(writeJson(path, {{"theme", "invalid"}, {"appearance", "invalid"}, {"gameRoots", QJsonArray{"a", "a", 4, ""}}}));
        mc::SettingsStore store(dir.path()); const auto settings = store.load();
        QCOMPARE(settings.downloadSource,QString("auto")); QCOMPARE(settings.theme, QString("forest")); QCOMPARE(settings.appearance, QString("system"));
        QCOMPARE(settings.gameRoots, QStringList{"a"});
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{broken"); file.close();
        QString error; QCOMPARE(store.load(&error).theme, QString("forest")); QVERIFY(!error.isEmpty());
    }
    void inheritedInstanceAndIsolation() {
        QTemporaryDir dir; const auto root = dir.path() + "/游戏 空间";
        QVERIFY(writeVersion(root, "1.20.1", {{"id", "1.20.1"}, {"javaVersion", QJsonObject{{"majorVersion", 17}}}}));
        QVERIFY(writeVersion(root, "fabric-test", {{"id", "fabric-test"}, {"inheritsFrom", "1.20.1"},
            {"libraries", QJsonArray{QJsonObject{{"name", "net.fabricmc:fabric-loader:0.16.0"}}}}}));
        const auto result = mc::scanInstances({root, root}); QCOMPARE(result.instances.size(), 2); QVERIFY(result.warnings.isEmpty());
        const auto instance = result.instances.at(1);
        QCOMPARE(instance.id, QString("fabric-test")); QCOMPARE(instance.loader, QString("fabric"));
        QCOMPARE(instance.loaderVersion,QString("0.16.0"));
        QCOMPARE(instance.version, QString("1.20.1")); QCOMPARE(instance.javaMajor, 17);
        QVERIFY(instance.isolated); QCOMPARE(instance.gameDirectory, instance.path);
        const auto shared = mc::scanInstances({root}, {{instance.path, false}}).instances.at(1);
        QVERIFY(!shared.isolated); QCOMPARE(shared.gameDirectory, shared.root);
    }
    void invalidInheritanceIsSkipped() {
        QTemporaryDir dir;
        QVERIFY(writeVersion(dir.path(), "cycle-a", {{"inheritsFrom", "cycle-b"}}));
        QVERIFY(writeVersion(dir.path(), "cycle-b", {{"inheritsFrom", "cycle-a"}}));
        QVERIFY(writeVersion(dir.path(), "escape", {{"inheritsFrom", "../../outside"}}));
        QVERIFY(writeVersion(dir.path(), "missing", {{"inheritsFrom", "does-not-exist"}}));
        const auto result = mc::scanInstances({dir.path()});
        QVERIFY(result.instances.isEmpty()); QCOMPARE(result.warnings.size(), 4);
    }
    void conflictingPrimaryLoadersCannotAcceptMods(){
        QTemporaryDir dir;QVERIFY(writeVersion(dir.path(),"mixed",{{"id","mixed"},{"libraries",QJsonArray{QJsonObject{{"name","net.fabricmc:fabric-loader:0.16.10"}},QJsonObject{{"name","net.neoforged:neoforge:21.1.1"}}}}}));
        const auto result=mc::scanInstances({dir.path()});QCOMPARE(result.instances.size(),1);QVERIFY(!result.instances.first().supportsMods());QVERIFY(!result.instances.first().loaderProblem.isEmpty());QVERIFY(!result.warnings.isEmpty());
    }
    void javaVersionRequirements_data() {
        QTest::addColumn<QString>("version"); QTest::addColumn<int>("major");
        QTest::newRow("legacy") << "1.12.2" << 8; QTest::newRow("1.17") << "1.17.1" << 16;
        QTest::newRow("1.18") << "1.18" << 17; QTest::newRow("before-21") << "1.20.4" << 17;
        QTest::newRow("java-21") << "1.20.5" << 21; QTest::newRow("unrecognized") << "snapshot-name" << 0;
    }
    void javaVersionRequirements() { QFETCH(QString, version); QFETCH(int, major); QCOMPARE(mc::requiredJavaMajor(version), major); }
    void javaArchitectureAndVersionParsing() {
        auto java = mc::parseJavaOutput("Property settings:\n    java.version = 1.8.0_412\n    os.arch = x86\n    java.vendor = Example\njava version \"1.8.0_412\"\n");
        QCOMPARE(java.major, 8); QCOMPARE(java.architecture, QString("x86")); QCOMPARE(java.vendor, QString("Example"));
        java = mc::parseJavaOutput("java.version = 21.0.8\nos.arch = aarch64\njava.vendor = OpenJDK\n");
        QCOMPARE(java.major, 21); QCOMPARE(java.architecture, QString("aarch64"));
        java = mc::parseJavaOutput("openjdk version \"17.0.15\" 2025-04-15\n    os.arch = amd64\n");
        QCOMPARE(java.major, 17); QCOMPARE(java.architecture, QString("x86_64"));
        QCOMPARE(mc::parseJavaOutput("not java").major, 0);
    }
    void javaExecutableIsActuallyValidated() {
#ifdef Q_OS_WIN
        QSKIP("The executable fixture is a POSIX shell script");
#else
        QTemporaryDir dir; QDir().mkpath(dir.filePath("jdk/bin"));
        QFile fake(dir.filePath("jdk/bin/java")); QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nprintf '    java.version = 17.0.10\n    java.vendor = Test JDK\n    os.arch = aarch64\n' >&2\n"); fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const auto cancel = std::make_shared<std::atomic_bool>(false);
        const auto found = mc::discoverJava({dir.path(), dir.filePath("jdk")}, cancel);
        QCOMPARE(found.size(), 1); QCOMPARE(found.first().major, 17); QCOMPARE(found.first().architecture, QString("aarch64"));
        cancel->store(true); QVERIFY(mc::discoverJava({dir.path()}, cancel).isEmpty());
#endif
    }
    void modrinthFiltersPreserveUnicode() {
        const mc::ModrinthProvider provider;
        const auto url = provider.searchUrl(QStringLiteral("地图 & 背包"), "1.20.1", "fabric");
        const QUrlQuery query(url); QCOMPARE(query.queryItemValue("query"), QStringLiteral("地图 & 背包"));
        const auto facets = QJsonDocument::fromJson(query.queryItemValue("facets").toUtf8()).array();
        QCOMPARE(facets.size(), 3); QCOMPARE(facets.at(1).toArray().first().toString(), QString("versions:1.20.1"));
        QCOMPARE(facets.at(2).toArray().first().toString(), QString("categories:fabric"));
        const QUrlQuery versions(provider.versionsUrl("A-b12", "1.20.1", "fabric"));
        QCOMPARE(QJsonDocument::fromJson(versions.queryItemValue("loaders").toUtf8()).array().first().toString(), QString("fabric"));
    }
    void modrinthPrimaryFileAndDependencies() {
        const QJsonObject jar{{"filename", "example.jar"}, {"primary", true}, {"url", "https://cdn.modrinth.com/example.jar"}, {"size", 42}, {"hashes", QJsonObject{{"sha512", "aabb"}}}};
        const QJsonObject version{{"id", "version1"}, {"name", "Example 1"}, {"version_number","1.0.2"}, {"files", QJsonArray{QJsonObject{{"filename", "source.jar"}}, jar}},
            {"dependencies", QJsonArray{QJsonObject{{"project_id", "dependency1"}, {"dependency_type", "required"}}, QJsonObject{{"project_id", "optional"}, {"dependency_type", "optional"}}}}};
        const auto files = mc::ModrinthProvider::files(QJsonArray{version}); QCOMPARE(files.size(), 1);
        QCOMPARE(files.first().fileName, QString("example.jar"));QCOMPARE(files.first().versionNumber,QString("1.0.2")); QCOMPARE(files.first().requiredDependencies, QStringList{"dependency1"});
        auto invalidJar = jar; invalidJar.insert("filename", "../bad.jar"); auto invalidVersion = version; invalidVersion.insert("files", QJsonArray{invalidJar});
        QVERIFY(mc::ModrinthProvider::files(QJsonArray{invalidVersion}).isEmpty());
        invalidJar = jar; invalidJar.insert("url", "http://example.com/a.jar"); invalidVersion.insert("files", QJsonArray{invalidJar});
        QVERIFY(mc::ModrinthProvider::files(QJsonArray{invalidVersion}).isEmpty());
    }
    void modrinthDynamicTagsAndIndependentFacets() {
        const mc::ModrinthProvider provider;
        const QUrlQuery query(provider.searchUrl("地图 & 优化", "1.20.1", "neoforge", "optimization", "downloads", 40));
        QCOMPARE(query.queryItemValue("index"), QString("downloads")); QCOMPARE(query.queryItemValue("offset"), QString("40"));
        const auto facets=QJsonDocument::fromJson(query.queryItemValue("facets").toUtf8()).array(); QCOMPARE(facets.size(),4);
        QCOMPARE(facets.at(2).toArray(),QJsonArray{"categories:neoforge"}); QCOMPARE(facets.at(3).toArray(),QJsonArray{"categories:optimization"});
        const auto tags=mc::ModrinthProvider::categories(QJsonArray{QJsonObject{{"name","new-category"},{"project_type","mod"}},QJsonObject{{"name","optimization"},{"project_type","mod"}},QJsonObject{{"name","optimization"},{"project_type","mod"}},QJsonObject{{"name","shadows"},{"project_type","shader"}}});
        QCOMPARE(tags.size(),2); QCOMPARE(tags.first().label,QString("new-category")); QCOMPARE(tags.last().label,QStringLiteral("性能优化"));
        const auto loaders=mc::ModrinthProvider::loaders(QJsonArray{QJsonObject{{"name","future-loader"},{"supported_project_types",QJsonArray{"mod"}}},QJsonObject{{"name","iris"},{"supported_project_types",QJsonArray{"shader"}}}});
        QCOMPARE(loaders.size(),1); QCOMPARE(loaders.first().id,QString("future-loader"));
    }
    void projectSupportDoesNotProveFileCompatibility() {
        mc::ResourceFile forge; forge.gameVersions={"1.20.1"}; forge.loaders={"forge"};
        mc::ResourceFile fabric; fabric.gameVersions={"1.21.1"}; fabric.loaders={"fabric"};
        QVERIFY(mc::ModrinthProvider::compatible(forge,"1.20.1","forge"));
        QVERIFY(!mc::ModrinthProvider::compatible(forge,"1.20.1","neoforge"));
        QVERIFY(!mc::ModrinthProvider::compatible(forge,"1.20.1","fabric"));
        QVERIFY(!mc::ModrinthProvider::compatible(fabric,"1.20.1","fabric"));
        QVERIFY(!mc::ModrinthProvider::compatible(forge,"1.20.1","vanilla"));
        QVERIFY(!mc::ModrinthProvider::compatible({},"1.20.1","forge"));
        const auto projects=mc::ModrinthProvider::projects(QJsonObject{{"hits",QJsonArray{QJsonObject{{"project_id","test"},{"title","Example"},{"versions",QJsonArray{"1.20.1","1.21.1"}},{"categories",QJsonArray{"forge","fabric","optimization"}},{"downloads",3000000000.0}}}}});
        QCOMPARE(projects.size(),1); QCOMPARE(projects.first().gameVersions,QStringList({"1.20.1","1.21.1"})); QCOMPARE(projects.first().downloads,qint64(3000000000LL));
        QCOMPARE(mc::resourceVersionSummary({"1.20.1","1.21.1","26.1"},2),QStringLiteral("1.20.1、1.21.1 等 3 个版本"));
    }
    void versionOverviewGroupsWithoutInventingPatchRanges() {
        const QStringList versions{"b1.7.3","1.20.1","1.20.4","1.21.1","26.1","26.1.2","26.3","24w14a"};
        QCOMPARE(mc::resourceVersionFamily("1.20.4"),QString("1.20"));QCOMPARE(mc::resourceVersionFamily("26.1.2"),QString("26.1"));
        QCOMPARE(mc::resourceVersionFamily("b1.7.3"),QStringLiteral("早期版本"));QCOMPARE(mc::resourceVersionFamily("24w14a"),QStringLiteral("快照 / 预览"));
        const auto overview=mc::resourceVersionOverview(versions,3);QCOMPARE(overview,QStringLiteral("26.3、26.1、1.21 等 6 个系列"));
        QVERIFY(!overview.contains("~"));QVERIFY(!overview.contains("1.20.2"));
    }
    void downloadCountsUseReadableChineseUnits() {
        QCOMPARE(mc::resourceDownloadCount(9999),QString("9999"));QCOMPARE(mc::resourceDownloadCount(10000),QStringLiteral("1.0 万"));
        QCOMPARE(mc::resourceDownloadCount(65363476),QStringLiteral("6536.3 万"));QCOMPARE(mc::resourceDownloadCount(255631722),QStringLiteral("2.56 亿"));
    }
    void unsafeModNamesAreRejected() {
        for (const auto &name : {"../x.jar", "a/b.jar", "a\\b.jar", "C:x.jar", "a\nx.jar", "x.exe", ""}) QVERIFY2(!mc::safeModFileName(name), name);
        QVERIFY(mc::safeModFileName(QStringLiteral("地图 v1.0.jar")));
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
