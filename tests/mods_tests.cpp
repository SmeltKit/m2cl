// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#ifndef Q_MOC_RUN
#include "mods_test_helpers.h"
#include "core/packs.h"
#include "core/compatibility.h"
#include <QUrlQuery>
#endif
class ModsTests : public QObject {
    Q_OBJECT
private slots:
    void managedOptifineCannotBeDisabledRemovedOrReplacedAsAMod(){
        QTemporaryDir dir;auto i=target(dir.filePath("instance"));i.loader="forge";i.loaderVersion="47.2.18";i.optifineVersion="HD_U_I6";i.optifineFile="OptiFine.jar";
        QVERIFY(save(i.gameDirectory+"/mods/OptiFine.jar",jar("optifine")));QVERIFY(save(i.gameDirectory+"/mods/other.jar",jar("other")));
        QVERIFY(mc::isVersionComponentFile(i,"OptiFine.jar"));QVERIFY(mc::isVersionComponentFile(i,"OptiFine.jar.disabled"));QVERIFY(!mc::isVersionComponentFile(i,"other.jar"));
        QVERIFY(mc::setModsEnabled(i,{"other.jar","OptiFine.jar"},false).contains(QStringLiteral("版本组件")));
        QVERIFY(mc::removeLocalMods(i,{"other.jar","OptiFine.jar"}).contains(QStringLiteral("版本组件")));
        QVERIFY(QFile::exists(i.gameDirectory+"/mods/other.jar"));QVERIFY(QFile::exists(i.gameDirectory+"/mods/OptiFine.jar"));
        Network net;auto iris=net.add("YL57xq9U","iris-version",jar("iris"));const auto plan=mc::planModInstall(i,iris,flag(),&net);
        QVERIFY(plan.error.contains(QStringLiteral("管理版本组件")));QVERIFY(net.requested.isEmpty());
        mc::ModPlan altered;altered.target=i;altered.disableCandidates.insert("OptiFine.jar","conflict");altered.disableFiles={"OptiFine.jar"};altered.expectedFiles={{"OptiFine.jar","hash"}};
        auto applied=mc::applyModPlan(altered,flag(),{},&net);QVERIFY(applied.error.contains(QStringLiteral("版本组件")));QVERIFY(QFile::exists(i.gameDirectory+"/mods/OptiFine.jar"));QVERIFY(net.requested.isEmpty());
        // Vanilla OptiFine has no JAR in mods, and still must block a renderer swap.
        i.optifineFile.clear();i.loader="fabric";const auto noLocal=mc::planModInstall(i,iris,flag(),&net);QVERIFY(noLocal.error.contains(QStringLiteral("管理版本组件")));QVERIFY(net.requested.isEmpty());
    }
    void failedCompatibilityUpdateHasPersistentCooldown(){
        QTemporaryDir dir;auto i=target(dir.path());
        const auto now=QDateTime::currentDateTimeUtc();
        QVERIFY(mc::compatibilityUpdateDue(i,now));
        mc::recordCompatibilityAttempt(i,now);
        QVERIFY(!mc::compatibilityUpdateDue(i,now.addSecs(60)));
        QVERIFY(!mc::compatibilityUpdateDue(i,now.addSecs(86399)));
        QVERIFY(mc::compatibilityUpdateDue(i,now.addSecs(86400)));
        // Disk state alone also suppresses attempts, as it would after restart.
        auto second=target(dir.filePath("second"));
        QVERIFY(save(second.gameDirectory+"/.m2cl-quilt-compatibility.json.attempt",now.toString(Qt::ISODate).toUtf8()));
        QVERIFY(!mc::compatibilityUpdateDue(second,now.addSecs(60)));
        mc::recordCompatibilityAttempt(i,now.addSecs(120));
        QVERIFY(!mc::compatibilityUpdateDue(i,now.addSecs(86400)));
        QVERIFY(!mc::compatibilityList(i).rules.isEmpty());
    }
    void officialCompatibilityCacheIsValidatedAndAdvisory(){
        QTemporaryDir dir;auto i=target(dir.path());i.loader="quilt";
        mc::LocalMod mod;mod.id="vulkanmod";mod.ids={"vulkanmod"};mod.enabled=true;
        QVERIFY(!mc::knownCompatibilityWarnings(i,mod).isEmpty());
        mod.enabled=false;QVERIFY(mc::knownCompatibilityWarnings(i,mod).isEmpty());mod.enabled=true;
        i.loader="fabric";QVERIFY(mc::knownCompatibilityWarnings(i,mod).isEmpty());i.loader="quilt";
        QString error;QVERIFY(!mc::saveCompatibilityList(i,"<html>error</html>",&error));QVERIFY(!mc::compatibilityList(i).cached);
        const QByteArray rules=R"([{"ids":["sample"],"name":"Sample","type":"GAME"}])";
        QVERIFY(mc::saveCompatibilityList(i,rules,&error));QVERIFY(mc::compatibilityList(i).cached);
        QVERIFY(mc::knownCompatibilityWarnings(i,mod).isEmpty());mod.ids={"sample"};QVERIFY(!mc::knownCompatibilityWarnings(i,mod).isEmpty());
        QVERIFY(!mc::saveCompatibilityList(i,"[]",&error));QVERIFY(mc::compatibilityList(i).cached);QCOMPARE(mc::compatibilityList(i).rules.size(),1);
    }
    void quiltAllowsFabricMetadataButNotTheReverse(){
        mc::ResourceFile file;file.gameVersions={"1.20.1"};file.loaders={"fabric"};
        QVERIFY(mc::ModrinthProvider::compatible(file,"1.20.1","quilt"));
        QVERIFY(!mc::ModrinthProvider::compatible(file,"1.20.2","quilt"));
        file.loaders={"quilt"};QVERIFY(!mc::ModrinthProvider::compatible(file,"1.20.1","fabric"));
        const auto query=QUrlQuery(mc::ModrinthProvider().versionsUrl("sample","1.20.1","quilt"));
        QCOMPARE(QJsonDocument::fromJson(query.queryItemValue("loaders").toUtf8()).array(),QJsonArray({"quilt","fabric"}));
        QTemporaryDir dir;auto i=target(dir.path());i.loader="quilt";
        const auto path=dir.filePath("fabric.jar");QVERIFY(save(path,jar("sample","1.0")));auto mod=mc::inspectMod(path);
        QVERIFY2(mc::modEnvironmentProblem(mod,i).isEmpty(),qPrintable(mc::modEnvironmentProblem(mod,i)));
        mod.loader="quilt";mod.metadata.clear();i.loader="fabric";QVERIFY(!mc::modEnvironmentProblem(mod,i).isEmpty());
    }

    void shaderAndModsCommitTogetherAndRestoreOnMetadataFailure(){
        QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));
        const auto old=net.add("support","v1",jar("support","1.0"));
        QVERIFY(mc::applyModPlan(mc::planModInstall(i,old,flag(),&net),flag(),{},&net).error.isEmpty());
        const auto before=mc::modManifest(i.gameDirectory);
        const auto next=net.add("support","v2",jar("support","2.0"));
        auto plan=mc::planModInstall(i,next,flag(),&net);QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));
        mc::AttachedPack pack;pack.source=dir.filePath("shader.zip");
        QVERIFY(save(pack.source,zip({{"shaders/main.fsh","void main(){}"}})));
        pack.file.fileName="shader.zip";pack.file.sha512=mc::inspectPack(pack.source,pack.kind).sha512;pack.project.name="Shader";
        // A file occupying the metadata directory forces failure after mods and
        // the shader have been written, exercising restoration of both folders.
        QVERIFY(save(i.gameDirectory+"/.m2cl-pack-metadata","occupied"));
        auto result=mc::applyModPlan(plan,flag(),{},&net,&pack);QVERIFY(!result.error.isEmpty());
        QVERIFY(!QFile::exists(i.gameDirectory+"/shaderpacks/shader.zip"));
        QVERIFY(QFile::exists(i.gameDirectory+"/mods/"+old.fileName));
        QVERIFY(!QFile::exists(i.gameDirectory+"/mods/"+next.fileName));
        QCOMPARE(mc::modManifest(i.gameDirectory),before);
        QVERIFY(QFile::remove(i.gameDirectory+"/.m2cl-pack-metadata"));
        result=mc::applyModPlan(plan,flag(),{},&net,&pack);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));
        QVERIFY(QFile::exists(i.gameDirectory+"/shaderpacks/shader.zip"));
        QVERIFY(QFile::exists(i.gameDirectory+"/mods/"+next.fileName));
        QCOMPARE(mc::localPacks(i,pack.kind).first().name,QString("Shader"));
    }
    void existingShaderIsNotRemovedWhenCombinedInstallFails(){
        QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));
        const auto mod=net.add("support","v1",jar("support"));auto plan=mc::planModInstall(i,mod,flag(),&net);
        mc::AttachedPack pack;pack.source=dir.filePath("shader.zip");QVERIFY(save(pack.source,zip({{"shaders/main.fsh","shader"}})));
        pack.file.fileName="shader.zip";pack.file.sha512=mc::inspectPack(pack.source,pack.kind).sha512;
        QVERIFY(mc::importPack(i,pack.kind,pack.source).isEmpty());
        QVERIFY(save(i.gameDirectory+"/.m2cl-pack-metadata","occupied"));
        QVERIFY(!mc::applyModPlan(plan,flag(),{},&net,&pack).error.isEmpty());
        QVERIFY(QFile::exists(i.gameDirectory+"/shaderpacks/shader.zip"));QVERIFY(mc::localMods(i).isEmpty());
    }
    void manuallyAddedRenderConflictsAreCheckedAtLaunch(){
        QTemporaryDir dir;auto i=target(dir.filePath("game"));
        QVERIFY(save(i.gameDirectory+"/mods/iris.jar",jar("iris")));
        QVERIFY(save(i.gameDirectory+"/mods/optifabric.jar",jar("optifabric")));
        QVERIFY(mc::checkModSet(i,mc::localMods(i),false).errors.join('\n').contains("OptiFabric"));
    }
    void identicalEmbeddedComponentsAreSharedButRootDuplicatesAreNot(){
        QTemporaryDir dir;auto i=target(dir.filePath("game"));auto child=jar("shared","1.0");auto parent=[&](QString id,QByteArray dependency){return zip({{"fabric.mod.json",QJsonDocument(QJsonObject{{"id",id},{"version","1.0"},{"jars",QJsonArray{QJsonObject{{"file","nested.jar"}}}}}).toJson()},{"nested.jar",dependency}});};
        auto a=dir.filePath("a.jar"),b=dir.filePath("b.jar");QVERIFY(save(a,parent("a",child)));QVERIFY(save(b,parent("b",child)));auto first=mc::inspectMod(a),second=mc::inspectMod(b);QVERIFY(mc::duplicateModProblem(first,second).isEmpty());QVERIFY(mc::checkModSet(i,{first,second},true).errors.isEmpty());
        QVERIFY(save(b,parent("b",jar("shared","2.0"))));QVERIFY(!mc::duplicateModProblem(first,mc::inspectMod(b)).isEmpty());
        QVERIFY(save(a,jar("same")));QVERIFY(save(b,jar("same")));QVERIFY(!mc::duplicateModProblem(mc::inspectMod(a),mc::inspectMod(b)).isEmpty());
    }

    void irisConflictRequiresExplicitDisableAndKeepsOptifineFile(){
        QTemporaryDir dir;auto i=target(dir.filePath("game"));const auto file=i.gameDirectory+"/mods/renamed.jar";
        QVERIFY(save(file,zip({{"optifine/OptiFineClassTransformer.class",QByteArray::fromHex("cafebabe")},{"optifine/Installer.class",QByteArray::fromHex("cafebabe")}})));
        QCOMPARE(mc::inspectMod(file).id,QString("optifine"));Network net;auto iris=net.add("YL57xq9U","iris-version",jar("iris"));auto plan=mc::planModInstall(i,iris,flag(),&net);QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));QVERIFY(plan.disableCandidates.contains("renamed.jar"));
        auto result=mc::applyModPlan(plan,flag(),{},&net);QVERIFY(!result.error.isEmpty());QVERIFY(QFile::exists(file));
        plan.disableFiles={"renamed.jar"};result=mc::applyModPlan(plan,flag(),{},&net);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(!QFile::exists(file));QVERIFY(QFile::exists(file+".disabled"));
    }

    void resourcePackagesValidateReuseAndBackup(){
        QTemporaryDir dir;auto i=target(dir.filePath("game"));const auto source=dir.filePath("shader.zip");
        QVERIFY(save(source,zip({{"shaders/main.fsh","void main() {}"}})));
        QVERIFY(mc::inspectPack(source,mc::PackKind::Shader).problem.isEmpty());
        QVERIFY(!mc::inspectPack(source,mc::PackKind::Resource).problem.isEmpty());
        QVERIFY(mc::importPack(i,mc::PackKind::Shader,source).isEmpty());
        QVERIFY(mc::importPack(i,mc::PackKind::Shader,source).isEmpty());
        QCOMPARE(mc::localPacks(i,mc::PackKind::Shader).size(),1);
        mc::ResourceProject project;project.name="Official name";project.description="Description";
        mc::ResourceFile resource;resource.fileName="shader.zip";resource.versionNumber="1.2";resource.sha512="wrong";
        QVERIFY(!mc::savePackMetadata(i,mc::PackKind::Shader,project,resource).isEmpty());
        resource.sha512=mc::inspectPack(source,mc::PackKind::Shader).sha512;
        QVERIFY(mc::savePackMetadata(i,mc::PackKind::Shader,project,resource).isEmpty());
        QCOMPARE(mc::localPacks(i,mc::PackKind::Shader).first().name,QString("Official name"));
        QCOMPARE(mc::localPacks(i,mc::PackKind::Shader).first().version,QString("1.2"));
        QVERIFY(save(source,zip({{"shaders/main.fsh","changed"}})));
        QVERIFY(!mc::importPack(i,mc::PackKind::Shader,source).isEmpty());
        QVERIFY(mc::removePack(i,mc::PackKind::Shader,"shader.zip").isEmpty());
        QVERIFY(mc::localPacks(i,mc::PackKind::Shader).isEmpty());
        QVERIFY(!QDir(i.gameDirectory+"/.m2cl-resource-backups").exists());
        QVERIFY(save(source,zip({{"../escape","bad"},{"shaders/main.fsh","shader"}})));
        QVERIFY(!mc::importPack(i,mc::PackKind::Shader,source).isEmpty());
        QVERIFY(save(source,zip({{"pack.mcmeta",R"({"pack":{"pack_format":15,"description":"Sample"}})"}})));
        QVERIFY(mc::importPack(i,mc::PackKind::Resource,source).isEmpty());
        QCOMPARE(mc::localPacks(i,mc::PackKind::Resource).first().format,15);
    }

    void loaderComponentsNeverBecomeOrdinaryModDependencies(){
        QTemporaryDir dir;auto instance=target(dir.path());instance.loaderVersion="0.16.10";
        const QJsonObject dependencies{{"minecraft","1.20.1"},{"fabricloader",">=0.16"},{"forge","*"},{"neoforge","*"},{"quilt_loader","*"},{"fabric-api","*"}};
        const auto path=dir.filePath("test.jar");QVERIFY(save(path,jar("example","1.0",dependencies)));
        auto mod=mc::inspectMod(path);QCOMPARE(mod.requiredIds,QStringList{"fabric-api"});
        QVERIFY(!mc::modEnvironmentProblem(mod,instance).isEmpty());
        QVERIFY(save(path,jar("example","1.0",{{"fabricloader",">=0.17"}})));mod=mc::inspectMod(path);
        QVERIFY(mod.requiredIds.isEmpty());QVERIFY(mc::modDependenciesProblem(mod,{}).isEmpty());
        const auto problem=mc::modEnvironmentProblem(mod,instance);QVERIFY(problem.contains("0.17"));QVERIFY(problem.contains(QStringLiteral("实例设置")));
        instance.loaderVersion="0.17.0";QVERIFY(mc::modEnvironmentProblem(mod,instance).isEmpty());
    }
    void localBatchOperationsValidateTheFinalSetAndKeepBackups(){
        QTemporaryDir dir;auto i=target(dir.filePath("game"));const auto mods=i.gameDirectory+"/mods/";
        QVERIFY(save(mods+"dep.jar",jar("dep")));QVERIFY(save(mods+"app.jar",jar("app","1.0.0",{{"dep","*"}})));QVERIFY(save(mods+"extra.jar",jar("extra")));
        QVERIFY(!mc::setModsEnabled(i,{"dep.jar","extra.jar"},false).isEmpty());QVERIFY(QFile::exists(mods+"dep.jar"));QVERIFY(QFile::exists(mods+"extra.jar"));
        QVERIFY(mc::setModsEnabled(i,{"dep.jar","app.jar"},false).isEmpty());QVERIFY(QFile::exists(mods+"dep.jar.disabled"));QVERIFY(QFile::exists(mods+"app.jar.disabled"));
        QVERIFY(mc::setModsEnabled(i,{"app.jar.disabled","dep.jar.disabled"},true).isEmpty());QVERIFY(QFile::exists(mods+"app.jar"));
        QVERIFY(QDir().mkpath(mods+"extra.jar.disabled"));QVERIFY(!mc::setModsEnabled(i,{"app.jar","extra.jar"},false).isEmpty());QVERIFY(QFile::exists(mods+"app.jar"));QVERIFY(QFile::exists(mods+"extra.jar"));QVERIFY(QDir().rmdir(mods+"extra.jar.disabled"));
        QVERIFY(!mc::removeLocalMods(i,{"dep.jar","extra.jar"}).isEmpty());QVERIFY(QFile::exists(mods+"dep.jar"));QVERIFY(QFile::exists(mods+"extra.jar"));
        const auto original=mc::fileSha512(mods+"dep.jar");QVERIFY(mc::removeLocalMods(i,{"dep.jar","app.jar"}).isEmpty());QCOMPARE(mc::localMods(i).size(),1);QVERIFY(QFile::exists(mods+"extra.jar"));
        QVERIFY(!QDir(i.gameDirectory+"/.m2cl-mod-backups").exists());QVERIFY(QDir(i.gameDirectory).entryList({".m2cl-removal-*"},QDir::Dirs|QDir::Hidden).isEmpty());
    }
    void localIconsFollowTheSelectedLoaderAndNeverReadOutsideTheJar(){
        QTemporaryDir dir;const auto path=dir.filePath("mixed.jar");
        auto fabric=QJsonObject{{"id","test"},{"name","Local name"},{"version","1.2"},{"icon",QJsonObject{{"16","small.png"},{"64","fabric.png"},{"128","large.png"}}}};
        const QByteArray toml="[[mods]]\nmodId=\"test\"\nversion=\"2.0\"\ndisplayName=\"Forge name\"\nlogoFile=\"forge.png\"\n";
        QVERIFY(save(path,zip({{"fabric.mod.json",QJsonDocument(fabric).toJson()},{"META-INF/mods.toml",toml},{"fabric.png","fabric icon bytes"},{"forge.png","forge icon bytes"}})));
        const auto f=mc::inspectMod(path,"fabric"),g=mc::inspectMod(path,"forge");
        QCOMPARE(mc::localModIcon(path,f),QByteArray("fabric icon bytes"));QCOMPARE(mc::localModIcon(path,g),QByteArray("forge icon bytes"));
        for(const auto &invalid:QStringList{"../private.png","/private.png","https://example.com/icon.png","a\\b.png"}){auto unsafe=f;unsafe.iconPath=invalid;QVERIFY(mc::localModIcon(path,unsafe).isEmpty());}
        auto missing=f;missing.iconPath="missing.png";QVERIFY(mc::localModIcon(path,missing).isEmpty());QCOMPARE(f.name,QString("Local name"));QCOMPARE(f.version,QString("1.2"));
        QJsonObject quilt{{"quilt_loader",QJsonObject{{"id","test"},{"version","3.0"},{"metadata",QJsonObject{{"icon","quilt.png"}}}}}};
        QVERIFY(save(path,zip({{"quilt.mod.json",QJsonDocument(quilt).toJson()},{"quilt.png","quilt icon bytes"}})));QCOMPARE(mc::localModIcon(path,mc::inspectMod(path,"quilt")),QByteArray("quilt icon bytes"));
    }
    void manualDependencySelectionDownloadsOnlyCheckedFilesAndKeepsHardChecks(){
        QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));auto dependency=net.add("dep","d1",jar("dep"));auto root=net.add("root","r1",jar("root","1.0.0",{{"dep",">=1"}}),{{"dep","","required"}});const auto plan=mc::planModInstall(i,root,flag(),&net);QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));auto selected=mc::selectModDependencies(plan,{});QCOMPARE(selected.files.size(),1);QCOMPARE(selected.skippedProjects,QStringList{"dep"});net.requested.clear();auto result=mc::applyModPlan(selected,flag(),{},&net);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(!net.requested.contains(dependency.url.path()));QCOMPARE(mc::localMods(i).size(),1);QVERIFY(!result.warnings.isEmpty());QVERIFY(!mc::modDependenciesProblem(mc::localMods(i).first(),mc::localMods(i)).isEmpty());
        auto wrong=net.add("wrong","w1",jar("wrong","1.0.0",{},{{"root","*"}}),{{"dep","","required"}});auto blocked=mc::selectModDependencies(mc::planModInstall(i,wrong,flag(),&net),{});auto conflict=mc::applyModPlan(blocked,flag(),{},&net);QVERIFY(!conflict.error.isEmpty());QVERIFY(!QFile::exists(i.gameDirectory+"/mods/"+wrong.fileName));
        auto duplicate=net.add("duplicate","v1",jar("root","2.0.0"),{{"dep","","required"}});auto duplicateResult=mc::applyModPlan(mc::selectModDependencies(mc::planModInstall(i,duplicate,flag(),&net),{}),flag(),{},&net);QVERIFY(!duplicateResult.error.isEmpty());
    }
    void skippingDependencyUpgradePreservesExistingCopy(){
        QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));auto old=net.add("dep","d1",jar("dep","1.0.0"));QVERIFY(mc::applyModPlan(mc::planModInstall(i,old,flag(),&net),flag(),{},&net).error.isEmpty());auto next=net.add("dep","d2",jar("dep","2.0.0"));auto root=net.add("root","r1",jar("root","1.0.0",{{"dep",">=1"}}),{{"dep","d2","required"}});auto plan=mc::planModInstall(i,root,flag(),&net);QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));QVERIFY(plan.replacements.contains(old.fileName));auto selected=mc::selectModDependencies(plan,{});QVERIFY(selected.replacements.isEmpty());net.requested.clear();auto result=mc::applyModPlan(selected,flag(),{},&net);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(QFile::exists(i.gameDirectory+"/mods/"+old.fileName));QVERIFY(!QFile::exists(i.gameDirectory+"/mods/"+next.fileName));QVERIFY(!net.requested.contains(next.url.path()));
    }
    void redownloadReplacesRenamedExistingCopyAndKeepsBackup(){
        QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));auto root=net.add("root","r1",jar("root"));auto first=mc::planModInstall(i,root,flag(),&net);QVERIFY(mc::applyModPlan(first,flag(),{},&net).error.isEmpty());QVERIFY(QFile::rename(i.gameDirectory+"/mods/"+root.fileName,i.gameDirectory+"/mods/user-renamed.jar"));auto version=QJsonDocument::fromJson(net.bodies.value("/v2/version/r1")).object();net.bodies.insert("/v2/version_files",QJsonDocument(QJsonObject{{root.sha512,version}}).toJson());auto observed=mc::planModInstall(i,root,flag(),&net);QVERIFY2(observed.error.isEmpty(),qPrintable(observed.error));QVERIFY(observed.files.isEmpty());auto selected=mc::selectModDependencies(observed,{}, {"root"});QCOMPARE(selected.files.size(),1);QCOMPARE(selected.replacements,QStringList{"user-renamed.jar"});auto good=net.bodies.value(root.url.path());net.bodies.insert(root.url.path(),"bad");auto failed=mc::applyModPlan(selected,flag(),{},&net);QVERIFY(!failed.error.isEmpty());QVERIFY(QFile::exists(i.gameDirectory+"/mods/user-renamed.jar"));QCOMPARE(mc::localMods(i).size(),1);net.bodies.insert(root.url.path(),good);auto result=mc::applyModPlan(selected,flag(),{},&net);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QCOMPARE(mc::localMods(i).size(),1);QVERIFY(QFile::exists(i.gameDirectory+"/mods/"+root.fileName));QVERIFY(!QFile::exists(i.gameDirectory+"/mods/user-renamed.jar"));QDir backups(i.gameDirectory+"/.m2cl-mod-backups");auto folders=backups.entryList(QDir::Dirs|QDir::NoDotAndDotDot);QCOMPARE(folders.size(),1);QVERIFY(QFile::exists(backups.filePath(folders.first()+"/user-renamed.jar")));
        const auto again=mc::planModInstall(i,root,flag(),&net);auto sameName=mc::selectModDependencies(again,{}, {"root"});QVERIFY2(mc::applyModPlan(sameName,flag(),{},&net).error.isEmpty(),"Same filename must also be replaceable");QCOMPARE(mc::localMods(i).size(),1);
    }
    void forgeJarInFabricInstanceIsRejectedRegardlessOfNameOrWebsiteTag(){
        const auto bytes=zip({{"META-INF/mods.toml",R"TOML(modLoader="javafml"
[[mods]]
modId="example"
version="1.0.0"
displayName="Example Forge mod"
)TOML"}});
        QTemporaryDir dir;auto i=target(dir.filePath("fabric-instance"));auto source=dir.filePath("source/fabric-looking-name.jar");QVERIFY(save(source,bytes));const auto local=mc::inspectMod(source);QVERIFY(local.known);QCOMPARE(local.loader,QString("forge"));QVERIFY(!mc::importLocalMods(i,{source}).isEmpty());QVERIFY(mc::localMods(i).isEmpty());
        QVERIFY(save(i.gameDirectory+"/mods/manually-renamed.jar",bytes));const auto found=mc::localMods(i);QCOMPARE(found.size(),1);QVERIFY(mc::modEnvironmentProblem(found.first(),i).contains("forge",Qt::CaseInsensitive));Network net;auto good=net.add("good","g1",jar("good"));net.requested.clear();auto plan=mc::planModInstall(i,good,flag(),&net);QVERIFY(!plan.error.isEmpty());QVERIFY(net.requested.isEmpty());
        // Even incorrect website loader tags cannot bypass the staged JAR check.
        auto clean=target(dir.filePath("clean-fabric"));auto mislabeled=net.add("bad","b1",bytes);QVERIFY(mislabeled.loaders.contains("fabric"));auto downloadPlan=mc::planModInstall(clean,mislabeled,flag(),&net);QVERIFY2(downloadPlan.error.isEmpty(),qPrintable(downloadPlan.error));auto result=mc::applyModPlan(downloadPlan,flag(),{},&net);QVERIFY(!result.error.isEmpty());QVERIFY(mc::localMods(clean).isEmpty());QVERIFY(mc::modManifest(clean.gameDirectory).isEmpty());
    }
    void universalJarKeepsTargetSpecificMetadataAndDependencies();
    void universalJarRejectsBrokenTargetVariantAndKeepsOtherVariants();
    void nestedDependenciesKeepTheirOwnVersionConstraints();
    void disabledDependencyCanBeEnabledWithoutRedownloading();
    void disabledActivationFailureLeavesOriginalFileAndInventory();
    void selectionPrunesOrphansButRetainsSharedDependencies();
    void optionalDependenciesAreOffByDefaultAndIncludeTheirRequiredChain();
    void optionalIncompatibilityBlocksOnlyTheSelectedCombination();
    void identifiedSameFilenameUpgradeIsBackedUpAndUnidentifiedCollisionIsBlocked();
    void localMetadataAndRanges();
    void forgeAndQuiltMetadata();
    void nestedFabricIdsAreReadWithoutRunningJars();
    void localImportPreflightsWholeBatchAndToggleChecksDependents();
    void requiredGraphCyclesAndOptionalDependencies();
    void exactDependencyConflictAndMissingStableFail();
    void declaredIncompatibleStopsPlan();
    void installIsBoundToOneTargetAndRepeatSkipsDownloads();
    void failedOrCancelledDownloadLeavesNoPartialMods();
    void upgradeRetainsBackupAndChangedFilesBlockCommit();
    void localDuplicateAndDeclaredBreaksAreNotOverwritten();
    void upgradingDependencyCannotBreakExistingMod();
    void manualRenamedDependencyIsRecognizedAndReusedRecursively();
    void reverseConflictsBlockLocalImportAndEnable();
    void softConflictsAndLoaderRequirementsAreDistinct();
    void addedLocalFileInvalidatesPlanBeforeDownloading();
};
#ifndef Q_MOC_RUN
void ModsTests::universalJarKeepsTargetSpecificMetadataAndDependencies(){
    QTemporaryDir dir;
    auto fabric=QJsonDocument(QJsonObject{{"id","universal"},{"name","Universal"},{"version","1.0.0"},{"depends",QJsonObject{{"fabric_dep",">=1"},{"minecraft","1.20.1"}}}}).toJson();
    const QByteArray forge=R"TOML(modLoader="javafml"
[[mods]]
modId="universal"
version="2.0.0"
displayName="Universal Forge"
[[dependencies.universal]]
modId="forge_dep"
mandatory=true
versionRange="[2,3)"
)TOML";
    const auto bytes=zip({{"fabric.mod.json",fabric},{"META-INF/mods.toml",forge}});const auto path=dir.filePath("universal.jar");QVERIFY(save(path,bytes));
    const auto generic=mc::inspectMod(path);QCOMPARE(generic.metadata.size(),2);const auto forFabric=mc::selectModMetadata(generic,"fabric"),forForge=mc::selectModMetadata(generic,"forge");QCOMPARE(forFabric.version,QString("1.0.0"));QCOMPARE(forForge.version,QString("2.0.0"));QVERIFY(forFabric.requiredIds.contains("fabric_dep"));QVERIFY(!forFabric.requiredIds.contains("forge_dep"));QVERIFY(forForge.requiredIds.contains("forge_dep"));QVERIFY(!forForge.requiredIds.contains("fabric_dep"));
    auto fabricTarget=target(dir.filePath("fabric")),forgeTarget=target(dir.filePath("forge"));forgeTarget.loader="forge";forgeTarget.loaderVersion="47.2.0";mc::LocalMod fabricDep;fabricDep.known=true;fabricDep.loader="fabric";fabricDep.id="fabric_dep";fabricDep.ids={"fabric_dep"};fabricDep.version="1.1";fabricDep.fileName="fabric_dep.jar";auto forgeDep=fabricDep;forgeDep.loader="forge";forgeDep.id="forge_dep";forgeDep.ids={"forge_dep"};forgeDep.version="2.1";forgeDep.fileName="forge_dep.jar";
    QVERIFY(mc::checkModSet(fabricTarget,{generic,fabricDep},true).errors.isEmpty());QVERIFY(mc::checkModSet(forgeTarget,{generic,forgeDep},true).errors.isEmpty());QVERIFY(!mc::checkModSet(forgeTarget,{generic},true).errors.isEmpty());
    Network net;auto file=net.add("universal","v1",bytes);file.loaders={"fabric","forge"};net.bodies.insert("/v2/project/universal/version","[]");auto plan=mc::planModInstall(forgeTarget,file,flag(),&net);QVERIFY(plan.error.isEmpty());auto missing=mc::applyModPlan(plan,flag(),{},&net);QVERIFY(!missing.error.isEmpty());QVERIFY(mc::localMods(forgeTarget).isEmpty());
    const QByteArray noDepsForge=R"TOML([[mods]]
modId="universal"
version="2.0.0"
)TOML";QVERIFY(save(path,zip({{"fabric.mod.json",fabric},{"META-INF/mods.toml",noDepsForge}})));QVERIFY(mc::importLocalMods(forgeTarget,{path}).isEmpty());QCOMPARE(mc::localMods(forgeTarget).first().loader,QString("forge"));QCOMPARE(mc::localMods(forgeTarget).first().version,QString("2.0.0"));
}
void ModsTests::universalJarRejectsBrokenTargetVariantAndKeepsOtherVariants(){
    QTemporaryDir dir;const QByteArray forge="[[mods]]\nmodId=\"dual\"\nversion=\"1.0\"\n";const QByteArray neo="[[mods]]\nmodId=\"dual\"\nversion=\"2.0\"\n";const auto path=dir.filePath("dual.jar");QVERIFY(save(path,zip({{"fabric.mod.json","{broken"},{"META-INF/mods.toml",forge},{"META-INF/neoforge.mods.toml",neo}})));
    const auto mod=mc::inspectMod(path);QCOMPARE(mod.metadata.size(),2);QVERIFY(mod.metadataProblems.contains("fabric"));QCOMPARE(mc::inspectMod(path,"forge").version,QString("1.0"));QCOMPARE(mc::inspectMod(path,"neoforge").version,QString("2.0"));auto i=target(dir.filePath("fabric"));QVERIFY(!mc::inspectMod(path,"fabric").known);QVERIFY(!mc::modEnvironmentProblem(mod,i).isEmpty());QVERIFY(!mc::importLocalMods(i,{path},true).isEmpty());
    const auto quilt=QJsonDocument(QJsonObject{{"quilt_loader",QJsonObject{{"id","quilt_id"},{"version","3.0"}}}}).toJson();const auto fabric=QJsonDocument(QJsonObject{{"id","fabric_id"},{"version","4.0"}}).toJson();QVERIFY(save(path,zip({{"fabric.mod.json",fabric},{"quilt.mod.json",quilt}})));QCOMPARE(mc::inspectMod(path,"quilt").id,QString("quilt_id"));QCOMPARE(mc::inspectMod(path,"fabric").id,QString("fabric_id"));
}
void ModsTests::nestedDependenciesKeepTheirOwnVersionConstraints(){
    QTemporaryDir dir;const auto path=dir.filePath("nested.jar");const auto root=QJsonDocument(QJsonObject{{"id","root"},{"version","1"},{"jars",QJsonArray{QJsonObject{{"file","nested.jar"}}}}}).toJson();QVERIFY(save(path,zip({{"fabric.mod.json",root},{"nested.jar",jar("child","1",{{"dep",">=2"}})}})));auto i=target(dir.filePath("game"));auto mod=mc::inspectMod(path,"fabric");QVERIFY(mod.known);QVERIFY(mod.ids.contains("child"));mc::LocalMod dep;dep.known=true;dep.loader="fabric";dep.ids={"dep"};dep.version="1";dep.name="Dependency";dep.fileName="dep.jar";QVERIFY(!mc::checkModSet(i,{mod,dep},true).errors.isEmpty());dep.version="2";QVERIFY(mc::checkModSet(i,{mod,dep},true).errors.isEmpty());
}
void ModsTests::disabledDependencyCanBeEnabledWithoutRedownloading(){
    QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));const auto dep=net.add("dep","d1",jar("dep"));const auto root=net.add("root","r1",jar("root","1",{{"dep",">=1"}}),{{"dep","","required"}});QVERIFY(save(i.gameDirectory+"/mods/my-dep.jar.disabled",net.bodies.value(dep.url.path())));const auto version=QJsonDocument::fromJson(net.bodies.value("/v2/version/d1")).object();net.bodies.insert("/v2/version_files",QJsonDocument(QJsonObject{{dep.sha512,version}}).toJson());
    const auto plan=mc::planModInstall(i,root,flag(),&net);QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));QCOMPARE(plan.disabledFiles.value("dep"),QString("my-dep.jar.disabled"));QVERIFY(plan.reusedProjects.isEmpty());auto selected=mc::selectModDependencies(plan,{"dep"},{},{"dep"});QCOMPARE(selected.files.size(),1);QCOMPARE(selected.enableFiles.size(),1);net.requested.clear();auto result=mc::applyModPlan(selected,flag(),{},&net);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(!net.requested.contains(dep.url.path()));QCOMPARE(result.enabled,QStringList{"my-dep.jar"});QVERIFY(QFile::exists(i.gameDirectory+"/mods/my-dep.jar"));QVERIFY(!QFile::exists(i.gameDirectory+"/mods/my-dep.jar.disabled"));QCOMPARE(mc::localMods(i).size(),2);QCOMPARE(mc::modManifest(i.gameDirectory).value("files").toArray().size(),2);
}
void ModsTests::disabledActivationFailureLeavesOriginalFileAndInventory(){
    QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));const auto dep=net.add("dep","d1",jar("dep"));QVERIFY(mc::applyModPlan(mc::planModInstall(i,dep,flag(),&net),flag(),{},&net).error.isEmpty());QVERIFY(mc::setModEnabled(i,dep.fileName,false).isEmpty());const auto root=net.add("root","r1",jar("root","1",{{"dep",">=2"}}),{{"dep","d1","required"}});auto plan=mc::planModInstall(i,root,flag(),&net);QVERIFY(plan.error.isEmpty());const auto inventory=mc::modManifest(i.gameDirectory);auto selected=mc::selectModDependencies(plan,{"dep"},{},{"dep"});auto result=mc::applyModPlan(selected,flag(),{},&net);QVERIFY(!result.error.isEmpty());QVERIFY(QFile::exists(i.gameDirectory+"/mods/"+dep.fileName+".disabled"));QVERIFY(!QFile::exists(i.gameDirectory+"/mods/"+dep.fileName));QCOMPARE(mc::modManifest(i.gameDirectory),inventory);
    auto same=mc::planModInstall(i,dep,flag(),&net);QVERIFY(same.error.isEmpty());auto enableRoot=mc::selectModDependencies(same,{},{},{"dep"});QVERIFY(enableRoot.files.isEmpty());QVERIFY(mc::applyModPlan(enableRoot,flag(),{},&net).error.isEmpty());QCOMPARE(mc::modManifest(i.gameDirectory).value("files").toArray().size(),1);
}
void ModsTests::selectionPrunesOrphansButRetainsSharedDependencies(){
    QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));net.add("leaf","l1",jar("leaf"));net.add("exclusive","e1",jar("exclusive"));net.add("left","a1",jar("left"),{{"leaf","","required"},{"exclusive","","required"}});net.add("right","b1",jar("right"),{{"leaf","","required"}});auto root=net.add("root","r1",jar("root"),{{"left","","required"},{"right","","required"}});auto plan=mc::planModInstall(i,root,flag(),&net);QVERIFY(plan.error.isEmpty());auto selected=mc::selectModDependencies(plan,{"right","leaf","exclusive"});QVERIFY(selected.activeProjects.contains("leaf"));QVERIFY(!selected.activeProjects.contains("exclusive"));QVERIFY(!selected.demandedProjects.contains("exclusive"));QVERIFY(selected.demandedProjects.contains("left"));net.requested.clear();QVERIFY(mc::applyModPlan(selected,flag(),{},&net).error.isEmpty());QVERIFY(!net.requested.contains("/left-a1.jar"));QVERIFY(!net.requested.contains("/exclusive-e1.jar"));QCOMPARE(mc::localMods(i).size(),3);
}
void ModsTests::optionalDependenciesAreOffByDefaultAndIncludeTheirRequiredChain(){
    QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));auto leaf=net.add("leaf","l1",jar("leaf"));auto extra=net.add("extra","e1",jar("extra","1",{{"leaf",">=1"}}),{{"leaf","","required"}});auto root=net.add("root","r1",jar("root"),{{"extra","","optional"}});auto plan=mc::planModInstall(i,root,flag(),&net);QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));QCOMPARE(plan.files.size(),1);QCOMPARE(plan.optionalPlans.size(),1);auto defaults=mc::selectModDependencies(plan,{"leaf"});QCOMPARE(defaults.files.size(),1);QVERIFY(defaults.skippedProjects.isEmpty());auto installedOptional=plan;installedOptional.optionalPlans.first().files.removeFirst();installedOptional.optionalPlans.first().files.clear();installedOptional.optionalPlans.first().files.append(leaf);installedOptional.optionalPlans.first().reusedProjects={"extra"};installedOptional.optionalPlans.first().reusedFiles.insert("extra","existing-extra.jar");auto preserve=mc::selectModDependencies(installedOptional,{"leaf"});QVERIFY(preserve.activeProjects.contains("extra"));QVERIFY(preserve.activeProjects.contains("leaf"));auto selected=mc::selectModDependencies(plan,{"extra","leaf"});QCOMPARE(selected.files.size(),3);QVERIFY(selected.skippedProjects.isEmpty());net.requested.clear();QVERIFY(mc::applyModPlan(selected,flag(),{},&net).error.isEmpty());QVERIFY(net.requested.contains(extra.url.path()));QVERIFY(net.requested.contains(leaf.url.path()));
    auto withoutLeaf=mc::selectModDependencies(plan,{"extra"});QCOMPARE(withoutLeaf.skippedProjects,QStringList{"leaf"});QVERIFY(withoutLeaf.activeProjects.contains("extra"));QVERIFY(!withoutLeaf.activeProjects.contains("leaf"));
}
void ModsTests::optionalIncompatibilityBlocksOnlyTheSelectedCombination(){
    QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));net.add("extra","e1",jar("extra"),{{"root","","incompatible"}});auto root=net.add("root","r1",jar("root"),{{"extra","","optional"}});auto plan=mc::planModInstall(i,root,flag(),&net);QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));QCOMPARE(plan.optionalPlans.size(),1);QVERIFY(mc::selectModDependencies(plan,{}).error.isEmpty());auto selected=mc::selectModDependencies(plan,{"extra"});QVERIFY(!selected.error.isEmpty());net.requested.clear();QVERIFY(!mc::applyModPlan(selected,flag(),{},&net).error.isEmpty());QVERIFY(net.requested.isEmpty());QVERIFY(mc::localMods(i).isEmpty());
}
void ModsTests::identifiedSameFilenameUpgradeIsBackedUpAndUnidentifiedCollisionIsBlocked(){
    QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));auto old=net.add("root","v1",jar("root","1.0"));QVERIFY(mc::applyModPlan(mc::planModInstall(i,old,flag(),&net),flag(),{},&net).error.isEmpty());auto next=net.add("root","v2",jar("root","2.0"));next.fileName=old.fileName;auto plan=mc::planModInstall(i,next,flag(),&net);QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));QVERIFY(plan.replacements.contains(old.fileName));QCOMPARE(plan.replacementVersions.value(old.fileName),QString("1.0"));QVERIFY(mc::applyModPlan(plan,flag(),{},&net).error.isEmpty());QCOMPARE(mc::localMods(i).size(),1);QCOMPARE(mc::localMods(i).first().version,QString("2.0"));
    auto foreign=target(dir.filePath("foreign"));QVERIFY(save(foreign.gameDirectory+"/mods/"+next.fileName,jar("unrelated")));QVERIFY(!mc::planModInstall(foreign,next,flag(),&net).error.isEmpty());QCOMPARE(mc::localMods(foreign).first().id,QString("unrelated"));
}
void ModsTests::localMetadataAndRanges(){
        QTemporaryDir dir;QVERIFY(save(dir.filePath("a.jar"),jar("test")));auto m=mc::inspectMod(dir.filePath("a.jar"));QVERIFY(m.known);QVERIFY(m.validArchive);QCOMPARE(m.id,QString("test"));QVERIFY(mc::modEnvironmentProblem(m,target(dir.path())).isEmpty());auto other=target(dir.path());other.loader="forge";QVERIFY(!mc::modEnvironmentProblem(m,other).isEmpty());other.loader="fabric";other.version="1.21.1";QVERIFY(!mc::modEnvironmentProblem(m,other).isEmpty());QCOMPARE(mc::modVersionMatches("[1.20,1.21)","1.20.1"),1);QCOMPARE(mc::modVersionMatches("~1.20.1","1.21.1"),0);QCOMPARE(mc::modVersionMatches("1.20.x","1.20.6"),1);QCOMPARE(mc::modVersionMatches("not a version","1.20.1"),-1);QCOMPARE(mc::modVersionMatches(">=1.20 <1.20.2-","1.20.1"),1);QCOMPARE(mc::modVersionMatches("<1.20.2-","1.20.2-pre1"),0);QCOMPARE(mc::modVersionMatches(">=1.2-","1.2-beta"),1);QCOMPARE(mc::modVersionMatches("1.2","1.2.0+build"),1);QCOMPARE(mc::modVersionMatches("~1.20.1","1.21-alpha"),0);QCOMPARE(mc::modVersionMatches("^0.2","0.3.0"),1);
        QVERIFY(save(dir.filePath("broken.jar"),"not a zip"));auto bad=mc::inspectMod(dir.filePath("broken.jar"));QVERIFY(!bad.known);QVERIFY(!bad.validArchive);
}
void ModsTests::forgeAndQuiltMetadata(){
        QTemporaryDir dir;const QByteArray toml=R"TOML(modLoader="javafml"
[[mods]]
modId="example"
version="1.0.0"
displayName="Example"
[[dependencies.example]]
modId="minecraft"
mandatory=true
versionRange="[1.20,1.21)"
[[dependencies.example]]
modId="optional"
mandatory = false
versionRange="*"
)TOML";
        QVERIFY(save(dir.filePath("forge.jar"),zip({{"META-INF/mods.toml",toml}})));auto mod=mc::inspectMod(dir.filePath("forge.jar"));QVERIFY(mod.known);QCOMPARE(mod.loader,QString("forge"));QVERIFY(!mod.requiredIds.contains("optional"));auto i=target(dir.path());i.loader="forge";QVERIFY2(mc::modEnvironmentProblem(mod,i).isEmpty(),qPrintable(mc::modEnvironmentProblem(mod,i)));i.version="1.21.1";QVERIFY(!mc::modEnvironmentProblem(mod,i).isEmpty());
        const auto quilt=QJsonObject{{"quilt_loader",QJsonObject{{"id","quilt-test"},{"version","1.0"},{"depends",QJsonArray{QJsonObject{{"id","minecraft"},{"versions",">=1.21"}}}}}}};
        QVERIFY(save(dir.filePath("quilt.jar"),zip({{"quilt.mod.json",QJsonDocument(quilt).toJson()}})));auto q=mc::inspectMod(dir.filePath("quilt.jar"));QCOMPARE(q.loader,QString("quilt"));i.loader="quilt";i.version="1.20.1";QVERIFY(!mc::modEnvironmentProblem(q,i).isEmpty());
}
void ModsTests::nestedFabricIdsAreReadWithoutRunningJars(){
        QTemporaryDir dir;auto meta=QJsonObject{{"id","api"},{"version","1.0.0"},{"jars",QJsonArray{QJsonObject{{"file","nested.jar"}}}}};QVERIFY(save(dir.filePath("api.jar"),zip({{"fabric.mod.json",QJsonDocument(meta).toJson()},{"nested.jar",jar("module","2.0.0")}})));auto api=mc::inspectMod(dir.filePath("api.jar"));QVERIFY(api.known);QVERIFY(api.ids.contains("module"));QCOMPARE(api.providedVersions.value("module"),QString("2.0.0"));
}
void ModsTests::localImportPreflightsWholeBatchAndToggleChecksDependents(){
        QTemporaryDir dir;auto i=target(dir.filePath("game"));auto source=dir.filePath("source");QVERIFY(save(source+"/a.jar",jar("a","1.0.0",{{"dep",">=1.0"}})));QVERIFY(!mc::importLocalMods(i,{source+"/a.jar"}).isEmpty());QVERIFY(!QFile::exists(i.gameDirectory+"/mods/a.jar"));QVERIFY(save(source+"/dep.jar",jar("dep")));QVERIFY(mc::importLocalMods(i,{source+"/a.jar",source+"/dep.jar"}).isEmpty());QCOMPARE(mc::localMods(i).size(),2);QVERIFY(!mc::setModEnabled(i,"dep.jar",false).isEmpty());QVERIFY(mc::setModEnabled(i,"a.jar",false).isEmpty());QVERIFY(mc::setModEnabled(i,"dep.jar",false).isEmpty());QVERIFY(!mc::setModEnabled(i,"a.jar.disabled",true).isEmpty());QVERIFY(mc::setModEnabled(i,"dep.jar.disabled",true).isEmpty());QVERIFY(mc::setModEnabled(i,"a.jar.disabled",true).isEmpty());QVERIFY(save(source+"/duplicate.jar",jar("dep")));QVERIFY(!mc::importLocalMods(i,{source+"/duplicate.jar"}).isEmpty());QCOMPARE(mc::localMods(i).size(),2);QVERIFY(!mc::removeLocalMod(i,"dep.jar").isEmpty());QVERIFY(mc::removeLocalMod(i,"a.jar").isEmpty());QVERIFY(mc::removeLocalMod(i,"dep.jar").isEmpty());QVERIFY(mc::localMods(i).isEmpty());
}
void ModsTests::requiredGraphCyclesAndOptionalDependencies(){
        QTemporaryDir dir;Network net;auto root=net.add("root","r1",jar("root"),{{"dep","","required"},{"optional","","optional"}});net.add("dep","d1",jar("dep"),{{"root","r1","required"}});auto p=mc::planModInstall(target(dir.filePath("game")),root,flag(),&net);QVERIFY2(p.error.isEmpty(),qPrintable(p.error));QCOMPARE(p.files.size(),2);QVERIFY(!p.optionalErrors.isEmpty());
}
void ModsTests::exactDependencyConflictAndMissingStableFail(){
        QTemporaryDir dir;Network net;net.add("dep","d1",jar("dep"));net.add("dep","d2",jar("dep","2.0.0"));net.add("other","o1",jar("other"),{{"dep","d2","required"}});auto root=net.add("root","r1",jar("root"),{{"dep","d1","required"},{"other","o1","required"}});QVERIFY(!mc::planModInstall(target(dir.filePath("game")),root,flag(),&net).error.isEmpty());auto missing=net.add("missing-root","m1",jar("m"),{{"missing","","required"}});net.bodies.insert("/v2/project/missing/version","[]");QVERIFY(!mc::planModInstall(target(dir.filePath("game")),missing,flag(),&net).error.isEmpty());
}
void ModsTests::declaredIncompatibleStopsPlan(){
        QTemporaryDir dir;Network net;net.add("dep","d1",jar("dep"));auto root=net.add("root","r1",jar("root"),{{"dep","d1","required"},{"dep","","incompatible"}});QVERIFY(!mc::planModInstall(target(dir.filePath("game")),root,flag(),&net).error.isEmpty());
}
void ModsTests::installIsBoundToOneTargetAndRepeatSkipsDownloads(){
        QTemporaryDir dir;Network net;net.add("dep","d1",jar("dep"));auto root=net.add("root","r1",jar("root","1.0.0",{{"dep",">=1"}}),{{"dep","","required"}});auto a=target(dir.filePath("A")),b=target(dir.filePath("B"));auto p=mc::planModInstall(a,root,flag(),&net);QVERIFY2(p.error.isEmpty(),qPrintable(p.error));auto r=mc::applyModPlan(p,flag(),{},&net);QVERIFY2(r.error.isEmpty(),qPrintable(r.error));QCOMPARE(r.installed.size(),2);QCOMPARE(mc::localMods(a).size(),2);QCOMPARE(mc::localMods(b).size(),0);QCOMPARE(mc::modManifest(a.gameDirectory).value("files").toArray().size(),2);auto again=mc::planModInstall(a,root,flag(),&net);QVERIFY2(again.error.isEmpty(),qPrintable(again.error));QVERIFY(again.files.isEmpty());
}
void ModsTests::failedOrCancelledDownloadLeavesNoPartialMods(){
        QTemporaryDir dir;Network net;auto dep=net.add("a-dep","d1",jar("dep"));auto root=net.add("z-root","r1",jar("root"),{{"a-dep","","required"}});auto i=target(dir.filePath("game"));auto p=mc::planModInstall(i,root,flag(),&net);net.bodies.insert(root.url.path(),"bad");auto r=mc::applyModPlan(p,flag(),{},&net);QVERIFY(!r.error.isEmpty());QVERIFY(mc::localMods(i).isEmpty());QVERIFY(mc::modManifest(i.gameDirectory).isEmpty());net.bodies.insert(root.url.path(),jar("root"));auto cancelled=flag();auto c=mc::applyModPlan(p,cancelled,[cancelled](const QString&,int){cancelled->store(true);},&net);QVERIFY(!c.error.isEmpty());QVERIFY(mc::localMods(i).isEmpty());
}
void ModsTests::upgradeRetainsBackupAndChangedFilesBlockCommit(){
        QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));auto v1=net.add("root","v1",jar("root"));auto p1=mc::planModInstall(i,v1,flag(),&net);QVERIFY(mc::applyModPlan(p1,flag(),{},&net).error.isEmpty());auto v2=net.add("root","v2",jar("root","2.0.0"));auto p2=mc::planModInstall(i,v2,flag(),&net);QCOMPARE(p2.replacements,QStringList{v1.fileName});auto result=mc::applyModPlan(p2,flag(),{},&net);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(!QFile::exists(i.gameDirectory+"/mods/"+v1.fileName));QDir backups(i.gameDirectory+"/.m2cl-mod-backups");auto dirs=backups.entryList(QDir::Dirs|QDir::NoDotAndDotDot);QCOMPARE(dirs.size(),1);QVERIFY(QFile::exists(backups.filePath(dirs.first()+"/"+v1.fileName)));
        auto v3=net.add("root","v3",jar("root","3.0.0"));auto p3=mc::planModInstall(i,v3,flag(),&net);QVERIFY(p3.error.isEmpty());QVERIFY(save(i.gameDirectory+"/mods/"+v2.fileName,"changed"));QVERIFY(!mc::applyModPlan(p3,flag(),{},&net).error.isEmpty());QVERIFY(!QFile::exists(i.gameDirectory+"/mods/"+v3.fileName));
}
void ModsTests::localDuplicateAndDeclaredBreaksAreNotOverwritten(){
        QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));QVERIFY(save(i.gameDirectory+"/mods/local.jar",jar("duplicate")));auto duplicate=net.add("project","p1",jar("duplicate","2.0.0"));auto p=mc::planModInstall(i,duplicate,flag(),&net);QVERIFY(p.error.isEmpty());QVERIFY(!mc::applyModPlan(p,flag(),{},&net).error.isEmpty());QVERIFY(!QFile::exists(i.gameDirectory+"/mods/"+duplicate.fileName));auto broken=net.add("breaks","b1",jar("breaks","1.0.0",{},{{"duplicate","*"}}));auto bp=mc::planModInstall(i,broken,flag(),&net);QVERIFY(!mc::applyModPlan(bp,flag(),{},&net).error.isEmpty());QCOMPARE(mc::localMods(i).size(),1);
}
void ModsTests::upgradingDependencyCannotBreakExistingMod(){
    QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));auto dep1=net.add("dependency","d1",jar("dependency","1.0.0"));auto root=net.add("root","r1",jar("root","1.0.0",{{"dependency","<2"}}),{{"dependency","d1","required"}});auto first=mc::planModInstall(i,root,flag(),&net);QVERIFY(mc::applyModPlan(first,flag(),{},&net).error.isEmpty());auto dep2=net.add("dependency","d2",jar("dependency","2.0.0"));auto upgrade=mc::planModInstall(i,dep2,flag(),&net);QVERIFY(upgrade.error.isEmpty());auto result=mc::applyModPlan(upgrade,flag(),{},&net);QVERIFY(!result.error.isEmpty());QVERIFY(QFile::exists(i.gameDirectory+"/mods/"+dep1.fileName));QVERIFY(!QFile::exists(i.gameDirectory+"/mods/"+dep2.fileName));
}
void ModsTests::manualRenamedDependencyIsRecognizedAndReusedRecursively(){
    QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));
    auto grand=net.add("grand","g1",jar("grand"));net.add("mid","m1",jar("mid","1.0.0",{{"grand",">=1"}}),{{"grand","g1","required"}});
    auto root=net.add("root","r1",jar("root","1.0.0",{{"mid",">=1"}}),{{"mid","","required"},{"grand","g1","required"},{"optional","","optional"}});
    QVERIFY(save(i.gameDirectory+"/mods/我手动改过名字.jar",jar("grand")));
    const auto version=QJsonDocument::fromJson(net.bodies.value("/v2/version/g1")).object();net.bodies.insert("/v2/version_files",QJsonDocument(QJsonObject{{grand.sha512,version}}).toJson());
    const auto plan=mc::planModInstall(i,root,flag(),&net);QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));QCOMPARE(plan.resolved.size(),3);QCOMPARE(plan.files.size(),2);QCOMPARE(plan.reusedProjects,QStringList{"grand"});QCOMPARE(plan.reusedFiles.value("grand"),QString::fromUtf8("我手动改过名字.jar"));QVERIFY(plan.replacements.isEmpty());QCOMPARE(plan.edges.size(),4);
    net.requested.clear();const auto result=mc::applyModPlan(plan,flag(),{},&net);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(!net.requested.contains(grand.url.path()));QVERIFY(QFile::exists(i.gameDirectory+"/mods/我手动改过名字.jar"));QVERIFY(!QFile::exists(i.gameDirectory+"/mods/"+grand.fileName));
    const auto again=mc::planModInstall(i,root,flag(),&net);QVERIFY2(again.error.isEmpty(),qPrintable(again.error));QVERIFY(again.files.isEmpty());QCOMPARE(again.resolved.size(),3);QCOMPARE(again.reusedProjects.size(),3);
}
void ModsTests::reverseConflictsBlockLocalImportAndEnable(){
    QTemporaryDir dir;auto i=target(dir.filePath("game"));QVERIFY(save(i.gameDirectory+"/mods/a.jar",jar("a","1.0.0",{},{{"b",QJsonArray{"*"}}})));QVERIFY(save(dir.filePath("b.jar"),jar("b")));
    QVERIFY(!mc::importLocalMods(i,{dir.filePath("b.jar")}).isEmpty());QVERIFY(!QFile::exists(i.gameDirectory+"/mods/b.jar"));
    QVERIFY(save(i.gameDirectory+"/mods/b.jar.disabled",jar("b")));QVERIFY(!mc::setModEnabled(i,"b.jar.disabled",true).isEmpty());QVERIFY(QFile::exists(i.gameDirectory+"/mods/b.jar.disabled"));QVERIFY(mc::setModEnabled(i,"a.jar",false).isEmpty());QVERIFY(mc::setModEnabled(i,"b.jar.disabled",true).isEmpty());
}
void ModsTests::softConflictsAndLoaderRequirementsAreDistinct(){
    QTemporaryDir dir;auto i=target(dir.path());const auto metadata=QJsonObject{{"id","a"},{"version","1.0.0"},{"depends",QJsonObject{{"minecraft","1.20.1"},{"fabricloader",">=0.16"}}},{"conflicts",QJsonObject{{"b",QJsonArray{"<2"}}}}};
    QVERIFY(save(dir.filePath("mods/a.jar"),zip({{"fabric.mod.json",QJsonDocument(metadata).toJson()}})));QVERIFY(save(dir.filePath("mods/b.jar"),jar("b")));auto mods=mc::localMods(i);QCOMPARE(mods.size(),2);auto check=mc::checkModSet(i,mods);QVERIFY(check.errors.isEmpty());QCOMPARE(check.warnings.size(),1);
    i.loaderVersion="0.15.0";QVERIFY(!mc::checkModSet(i,mods).errors.isEmpty());i.loader="neoforge";QVERIFY(!mc::checkModSet(i,mods).errors.isEmpty());i.loader="fabric+neoforge";QVERIFY(!i.supportsMods());
}
void ModsTests::addedLocalFileInvalidatesPlanBeforeDownloading(){
    QTemporaryDir dir;Network net;auto i=target(dir.filePath("game"));auto root=net.add("root","r1",jar("root"));auto plan=mc::planModInstall(i,root,flag(),&net);QVERIFY(plan.error.isEmpty());QVERIFY(save(i.gameDirectory+"/mods/added.jar",jar("added")));net.requested.clear();auto result=mc::applyModPlan(plan,flag(),{},&net);QVERIFY(!result.error.isEmpty());QVERIFY(net.requested.isEmpty());QVERIFY(!QFile::exists(i.gameDirectory+"/mods/"+root.fileName));
}
#endif
QTEST_GUILESS_MAIN(ModsTests)
#include "mods_tests.moc"
