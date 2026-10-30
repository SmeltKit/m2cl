// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "core/java.h"
#include "core/javaarchive.h"
#include "core/settings.h"
#include "network/javaprovider.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrlQuery>
#include <QtEndian>
#include <cstring>
#include <zlib.h>

namespace {
mc::JavaCancel flag() { return std::make_shared<std::atomic_bool>(false); }
bool save(const QString &path, const QByteArray &bytes) { QDir().mkpath(QFileInfo(path).path()); QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); }
QByteArray script(const QString &version = "17.0.10", const QString &build = "17.0.10+7") {
    return ("#!/bin/sh\nprintf 'java.version = " + version + "\\njava.runtime.version = " + build + "\\njava.vendor = Eclipse Adoptium\\njava.vendor.version = Temurin-" + build + "\\njava.vm.name = OpenJDK 64-Bit Server VM\\nos.arch = " + mc::GamePlatform::current().arch + "\\n'\n").toUtf8();
}
QByteArray zip(const QByteArray &name, const QByteArray &body, quint32 attributes = 0100755u << 16) {
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
QByteArray tarEntry(const QByteArray &name, const QByteArray &body, char type = '0', const QByteArray &target = {}) {
    QByteArray header(512, '\0');
    auto field = [&](int offset, int length, const QByteArray &bytes) { header.replace(offset, qMin(length, int(bytes.size())), bytes.left(length)); };
    auto octal = [&](int offset, int length, quint64 n) { field(offset, length, QByteArray::number(n, 8).rightJustified(length - 1, '0') + '\0'); };
    field(0, 100, name); octal(100, 8, 0755); octal(108, 8, 0); octal(116, 8, 0); octal(124, 12, body.size()); octal(136, 12, 0);
    header.replace(148, 8, QByteArray(8, ' ')); header[156] = type; field(157, 100, target); field(257, 6, "ustar");
    quint64 checksum = 0; for (auto c : header) checksum += static_cast<uchar>(c); octal(148, 7, checksum); header[155] = ' ';
    return header + body + QByteArray((512 - body.size() % 512) % 512, '\0');
}
bool gzip(const QString &path, const QByteArray &bytes) {
    auto file = gzopen(QFile::encodeName(path).constData(), "wb"); if (!file) return false;
    const auto written = gzwrite(file, bytes.constData(), bytes.size()); const auto closed = gzclose(file); return written == bytes.size() && closed == Z_OK;
}
bool rejects(const std::function<void()> &fn) { try { fn(); } catch (const std::exception &) { return true; } return false; }
class Reply : public QNetworkReply {
    QByteArray bytes; qint64 offset = 0; bool available = false;
public:
    Reply(const QNetworkRequest &request, QByteArray data, QObject *parent) : QNetworkReply(parent), bytes(std::move(data)) {
        setRequest(request); setUrl(request.url()); open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(0, this, [this] { if (isFinished()) return; available = true; emit downloadProgress(bytes.size(), bytes.size()); emit readyRead(); if (!isFinished()) { setFinished(true); emit finished(); } });
    }
    void abort() override { if (!isFinished()) { setError(OperationCanceledError, "Cancelled"); setFinished(true); emit finished(); } }
    qint64 bytesAvailable() const override { return (available ? bytes.size() - offset : 0) + QNetworkReply::bytesAvailable(); }
    qint64 readData(char *out, qint64 maximum) override {
        if (!available || offset >= bytes.size()) return -1;
        const auto n = qMin(maximum, bytes.size() - offset); std::memcpy(out, bytes.constData() + offset, n); offset += n; return n;
    }
};
QString openJdkHtml(const QString &url,const QString &version="17.0.2",int build=8) {
    return "<p>JDK builds licensed under <a href='http://openjdk.org/legal/gplv2+ce.html'>GPLv2 + Classpath Exception</a></p><table><tr><th>"+version+" (build "+version+"+"+QString::number(build)+")</th></tr><tr><th>Mac/AArch64</th><td><a href='"+url+"'>tar.gz</a> 174M <span class='sha'>(<a href='"+url+".sha256'>sha256</a>)</span></td></tr></table>";
}
class Network : public QNetworkAccessManager {
public:
    QMap<QString, QByteArray> bodies; QMap<QString, int> requests; int corrupt = 0;
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        const auto key = request.url().toString(); ++requests[key]; auto body = bodies.value(key);
        if (key == "https://test/java.zip" && corrupt-- > 0) body = "broken";
        return new Reply(request, body, this);
    }
};
}
class JavaTests : public QObject {
    Q_OBJECT
private slots:
    void fullVersionAndVendorBuildRoundTrip() {
        const auto j = mc::parseJavaOutput("java.version = 17.0.10\njava.runtime.version = 17.0.10+7\njava.vendor = Eclipse Adoptium\njava.vendor.version = Temurin-17.0.10+7\njava.vm.name = OpenJDK Server VM\nos.arch = aarch64\n");
        QCOMPARE(j.runtimeVersion, QString("17.0.10+7")); QCOMPARE(j.vendorVersion, QString("Temurin-17.0.10+7"));
        QVERIFY(mc::javaLabel(j).contains("Temurin-17.0.10+7"));
        const auto loaded = mc::javaCatalogFromJson(mc::javaCatalogJson({j})); QVERIFY(loaded.isEmpty()); // No executable path is not a valid catalog record.
        auto valid = j; valid.executable = "/test/java"; const auto catalog = mc::javaCatalogFromJson(mc::javaCatalogJson({valid, valid})); QCOMPARE(catalog.size(), 1); QCOMPARE(catalog.first().vendorVersion, j.vendorVersion);
    }
    void instanceJavaConfigsMigrateExplicitChoicesWithoutGlobalFallback() {
        QTemporaryDir dir;
        const auto a = dir.filePath("A"), b = dir.filePath("B"), c = dir.filePath("C");
        for (const auto &path : {a, b, c}) QVERIFY(QDir().mkpath(path));
        const QJsonObject pin{{"mode", "fixed"}, {"path", "/other/java"}, {"major", 17}, {"runtimeVersion", "17.0.10+8"}, {"allowBelowRecommended", true}};
        const QJsonObject old{{"globalJava", QJsonObject{{"mode", "fixed"}, {"path", "/old-global/java"}}}, {"selectedJava", "/legacy/java"},
            {"instanceJava", QJsonObject{{a, QJsonObject{{"mode", "auto"}}}, {b, pin}}}};
        QVERIFY(save(dir.filePath("settings.json"), QJsonDocument(old).toJson()));
        QVERIFY(save(mc::instanceConfigPath(b), QJsonDocument(QJsonObject{{"note", "preserve me"}}).toJson()));
        mc::SettingsStore store(dir.path()); auto settings = store.load();
        QCOMPARE(mc::loadInstanceJavaChoice(c).value("mode").toString(), QString("auto"));
        QVERIFY(mc::migrateInstanceJavaChoices(settings, {a, b, c}).isEmpty());
        QVERIFY(settings.legacyInstanceJava.isEmpty()); QVERIFY(store.save(settings));
        QCOMPARE(mc::loadInstanceJavaChoice(a).value("mode").toString(), QString("auto"));
        QCOMPARE(mc::loadInstanceJavaChoice(b), pin);
        QCOMPARE(mc::loadInstanceJavaChoice(c).value("mode").toString(), QString("auto"));
        QFile config(mc::instanceConfigPath(b)); QVERIFY(config.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(config.readAll()).object().value("note").toString(), QString("preserve me")); config.close();
        QFile central(dir.filePath("settings.json")); QVERIFY(central.open(QIODevice::ReadOnly));
        const auto centralJson = QJsonDocument::fromJson(central.readAll()).object();
        QVERIFY(!centralJson.contains("globalJava")); QVERIFY(!centralJson.contains("selectedJava")); QVERIFY(!centralJson.contains("instanceJava"));
        const auto moved = dir.filePath("moved-B"); QVERIFY(QDir().rename(b, moved));
        QCOMPARE(mc::loadInstanceJavaChoice(moved), pin);
        QCOMPARE(mc::loadInstanceJavaChoice(a).value("mode").toString(), QString("auto"));
        settings.legacyInstanceJava.insert(a, pin);
        QVERIFY(mc::migrateInstanceJavaChoices(settings, {a}).isEmpty());
        QCOMPARE(mc::loadInstanceJavaChoice(a).value("mode").toString(), QString("auto"));
    }
    void invalidInstanceConfigIsNotSilentlyOverwritten() {
        QTemporaryDir dir;
        const auto path = mc::instanceConfigPath(dir.path());
        QVERIFY(save(path, "{broken"));
        QString error; mc::loadInstanceJavaChoice(dir.path(), &error); QVERIFY(!error.isEmpty());
        QVERIFY(!mc::saveInstanceJavaChoice(dir.path(), {{"mode", "auto"}}, &error));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("{broken")); file.close();
        mc::Settings settings; settings.legacyInstanceJava.insert(dir.path(), QJsonObject{{"mode", "fixed"}, {"path", "/keep/java"}});
        QVERIFY(!mc::migrateInstanceJavaChoices(settings, {dir.path()}).isEmpty());
        QVERIFY(settings.legacyInstanceJava.contains(dir.path()));
    }
    void deepScanFindsNestedJavaAndDoesNotFollowDirectoryLinks() {
#ifdef Q_OS_WIN
        QSKIP("POSIX executable fixture");
#endif
        QTemporaryDir dir; auto deep = dir.path(); for (int i = 0; i < 10; ++i) deep += "/nested";
        const auto executable = deep + "/bin/java"; QVERIFY(save(executable, script())); QVERIFY(QFile::setPermissions(executable, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(QFile::link(dir.path(), dir.path() + "/loop"));
        const auto hidden = dir.path() + "/.hidden/jdk/bin/java"; QVERIFY(save(hidden, script())); QVERIFY(QFile::setPermissions(hidden, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const auto tempJava = dir.path() + "/.linjian-java-stage-test/bin/java"; QVERIFY(save(tempJava, script())); QVERIFY(QFile::setPermissions(tempJava, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        mc::JavaScanOptions quick; quick.maxDepth = 2; const auto partial = mc::scanJavaRuntimes({dir.path()}, flag(), quick); QVERIFY(partial.runtimes.size() < 2);
        mc::JavaScanOptions full; full.deep = true; int updates = 0;
        const auto result = mc::scanJavaRuntimes({dir.path(), deep}, flag(), full, [&](const mc::JavaScanProgress &) { ++updates; });
        QCOMPARE(result.runtimes.size(), 2); QVERIFY(updates > 0); QVERIFY(!result.cancelled); QVERIFY(!result.limited);
        const auto stop = flag(); const auto cancelled = mc::scanJavaRuntimes({dir.path()}, stop, full, [&](const mc::JavaScanProgress &p) { if (p.found) stop->store(true); });
        QVERIFY(cancelled.cancelled); QCOMPARE(cancelled.runtimes.size(), 1);
        quick.directoryBudget = 1; QVERIFY(mc::scanJavaRuntimes({dir.path()}, flag(), quick).limited);
    }
    void packageParsingKeepsSpecificBuildAndPlatform() {
        const QJsonObject archive{{"id", 1}, {"name", "OpenJDK17U-jre_aarch64_mac_hotspot_17.0.10_7.tar.gz"}, {"browser_download_url", "https://test/java.tar.gz"}, {"digest", "sha256:" + QString(64, 'a')}, {"size", 123}};
        const auto parsed = mc::parseTemurinPackages({QJsonObject{{"tag_name", "jdk-17.0.10+7"}, {"assets", QJsonArray{archive}}}}, 17, {"osx", "aarch64", {}});
        QCOMPARE(parsed.size(), 1); QCOMPARE(parsed.first().version, QString("17.0.10")); QCOMPARE(parsed.first().build, 7); QVERIFY(parsed.first().id.contains("+7"));
        QVERIFY(mc::parseTemurinPackages({QJsonObject{{"tag_name", "jdk-17.0.10+7"}, {"assets", QJsonArray{archive}}}}, 17, {"osx", "x86_64", {}}).isEmpty());
        const auto zulu = mc::parseZuluPackages({QJsonObject{{"java_version", QJsonArray{17, 0, 10}}, {"openjdk_build_number", 7}, {"distro_version", QJsonArray{17, 48, 15, 0}}, {"package_uuid", "test"}, {"download_url", "https://test/java.tar.gz"}}}, 17, {"osx", "aarch64", {}});
        QCOMPARE(zulu.size(), 1); QVERIFY(zulu.first().label.contains("17.0.10+7")); QVERIFY(zulu.first().label.contains("17.48.15.0"));
    }
    void zuluMetadataCannotMixArchitectures() {
        const QJsonObject entry{{"java_version", QJsonArray{17, 0, 10}}, {"package_uuid", "test"}, {"download_url", "https://test/java.tar.gz"}, {"arch", "x86"}, {"hw_bitness", 64}, {"os", "macos"}, {"size", 123456}};
        QVERIFY(mc::parseZuluPackages({entry}, 17, {"osx", "aarch64", {}}).isEmpty());
        const auto list = mc::parseZuluPackages({entry}, 17, {"osx", "x86_64", {}}); QCOMPARE(list.size(), 1); QCOMPARE(list.first().size, qint64(123456));
    }
    void architectureMetadataKeepsOperatingSystemsAndNonX86BuildsSeparate() {
        QJsonArray assets;
        int id = 0;
        for (const auto &target : QStringList{"aarch64_mac", "aarch64_windows", "riscv64_linux", "arm_linux", "ppc64le_linux", "s390x_linux", "x86-32_windows"}) {
            const auto name = "OpenJDK17U-jdk_" + target + "_hotspot_17.0.10_7" + (target.endsWith("windows") ? ".zip" : ".tar.gz");
            assets.append(QJsonObject{{"id", ++id}, {"name", name}, {"browser_download_url", "https://test/" + name}, {"digest", "sha256:" + QString(64, 'a')}});
        }
        const QJsonArray releases{QJsonObject{{"tag_name", "jdk-17.0.10+7"}, {"assets", assets}}};
        const auto mac = mc::parseTemurinPackages(releases, 17, {"osx", "aarch64", {}});
        const auto windows = mc::parseTemurinPackages(releases, 17, {"windows", "aarch64", {}});
        QCOMPARE(mac.size(), 1); QCOMPARE(windows.size(), 1); QVERIFY(mac.first().url != windows.first().url);
        QCOMPARE(mac.first().packageType, QString("JDK"));
        for (const auto &arch : QStringList{"riscv64", "arm", "ppc64le", "s390x"}) {
            const auto builds = mc::parseTemurinPackages(releases, 17, {"linux", arch, {}});
            QCOMPARE(builds.size(), 1); QCOMPARE(builds.first().arch, arch);
        }
        QVERIFY(mc::parseTemurinPackages(releases, 17, {"linux", "x86", {}}).isEmpty());
        Network network;
        network.bodies["https://api.github.com/repos/adoptium/temurin17-binaries/releases?per_page=20&page=1"] = QJsonDocument(releases).toJson();
        network.bodies["https://api.github.com/repos/adoptium/temurin17-binaries/releases/latest"] = QJsonDocument(releases.first().toObject()).toJson();
        const auto result = mc::fetchJavaPackages("temurin", 17, {"linux", "riscv64", {}}, flag(), 0, &network, true);
        QCOMPARE(result.packages.size(), 1); QVERIFY(result.architectureListComplete);
        QVERIFY(result.availableArchitectures.contains("riscv64")); QVERIFY(result.availableArchitectures.contains("s390x"));
        QVERIFY(!result.availableArchitectures.contains("aarch64"));
        Network zulu;
        mc::fetchJavaPackages("zulu", 17, {"linux", "riscv64", {}}, flag(), 0, &zulu);
        QVERIFY(!zulu.requests.isEmpty());
        for (const auto &url : zulu.requests.keys()) QCOMPARE(QUrlQuery(QUrl(url)).queryItemValue("arch"), QString("riscv64"));
        QCOMPARE(mc::normalizeArchitecture("armv7l"), QString("arm"));
    }
    void microsoftBuildsKeepPlatformAndExcludeChecksumLinks() {
        const QString url = "https://aka.ms/download-jdk/microsoft-jdk-17.0.20.1-macos-aarch64.tar.gz";
        const QString html = "<a href='" + url + "'>archive</a><a href='" + url + ".sha256sum.txt'>checksum</a><a href='" + url + "'>duplicate</a><a href='https://aka.ms/download-jdk/microsoft-jdk-21.0.8-macos-aarch64.tar.gz'>other major</a>";
        const auto packages = mc::parseMicrosoftPackages(html, 17, {"osx", "aarch64", {}});
        QCOMPARE(packages.size(), 1); QCOMPARE(packages.first().provider, QString("microsoft")); QCOMPARE(packages.first().version, QString("17.0.20.1")); QCOMPARE(packages.first().packageType, QString("JDK")); QCOMPARE(packages.first().checksumUrl.toString(), url + ".sha256sum.txt");
        QVERIFY(mc::parseMicrosoftPackages(html, 17, {"osx", "x86_64", {}}).isEmpty());
        Network network; network.bodies["https://learn.microsoft.com/en-us/java/openjdk/download"] = html.toUtf8(); network.bodies["https://learn.microsoft.com/en-us/java/openjdk/older-releases"] = html.toUtf8();
        const auto list = mc::fetchJavaPackages("microsoft", 17, {"osx", "aarch64", {}}, flag(), 0, &network); QCOMPARE(list.packages.size(), 1); QVERIFY2(list.error.isEmpty(), qPrintable(list.error));
    }
    void oracleOpenJdkParserKeepsBuildLicenseAndPlatform() {
        const QString arm="https://download.java.net/java/GA/jdk17.0.2/verified/8/GPL/openjdk-17.0.2_macos-aarch64_bin.tar.gz";auto html=openJdkHtml(arm);auto packages=mc::parseOracleOpenJdkPackages(html,17,{"osx","aarch64",{}});QCOMPARE(packages.size(),1);auto p=packages.first();QCOMPARE(p.provider,QString("oracle-openjdk"));QCOMPARE(p.version,QString("17.0.2"));QCOMPARE(p.build,8);QCOMPARE(p.size,qint64(174000000));QVERIFY(p.archived);QCOMPARE(p.checksumUrl.toString(),arm+".sha256");QCOMPARE(mc::javaPackageLicense(p).name,QString("GPLv2 + Classpath Exception"));QCOMPARE(mc::javaPackageLicense(p).buildUrl,p.sourcePage);
        QVERIFY(mc::parseOracleOpenJdkPackages(html,17,{"osx","x86_64",{}}).isEmpty());QVERIFY(mc::parseOracleOpenJdkPackages(html,21,{"osx","aarch64",{}}).isEmpty());QVERIFY(mc::parseOracleOpenJdkPackages(QString(html).replace("gplv2+ce.html","commercial-license.html"),17,{"osx","aarch64",{}}).isEmpty());QVERIFY(mc::parseOracleOpenJdkPackages(QString(html).replace("/GA/","/EA/"),17,{"osx","aarch64",{}}).isEmpty());QVERIFY(mc::parseOracleOpenJdkPackages(QString(html).replace("download.java.net/","download.java.net.evil.example/"),17,{"osx","aarch64",{}}).isEmpty());QVERIFY(mc::parseOracleOpenJdkPackages(openJdkHtml(arm,"17.0.2",7),17,{"osx","aarch64",{}}).isEmpty());QVERIFY(mc::parseOracleOpenJdkPackages(html,17,{"osx","aarch64",{}},QUrl("https://other.example/archive/")).isEmpty());
        Network net;net.bodies["https://jdk.java.net/17/"]=html.toUtf8();net.bodies["https://jdk.java.net/archive/"]=html.toUtf8();auto result=mc::fetchJavaPackages("oracle-openjdk",17,{"osx","aarch64",{}},flag(),0,&net);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QCOMPARE(result.packages.size(),1);QVERIFY(!result.packages.first().archived);
    }
    void licenseDetailsSeparateBuildEvidenceFromGenericDocs() {
        mc::JavaPackage p;p.provider="temurin";p.sourcePage=QUrl("https://github.com/adoptium/temurin17-binaries/releases/tag/jdk-17.0.10%2B7");auto a=mc::javaPackageLicense(p);QCOMPARE(a.buildUrl,p.sourcePage);QVERIFY(a.scope.contains(QStringLiteral("通用")));p.sourcePage=QUrl("https://github.com/adoptium/temurin17-binaries/releases/tag/jdk-17.0.11%2B9");QVERIFY(mc::javaPackageLicense(p).buildUrl!=a.buildUrl);p.provider="zulu";p.id="one-build";auto z=mc::javaPackageLicense(p);QVERIFY(z.buildUrl.toString().endsWith("one-build"));QVERIFY(!z.licenseUrl.toString().contains("zulu8"));QVERIFY(z.scope.contains(QStringLiteral("未确认")));p.provider="unknown";QVERIFY(mc::javaPackageLicense(p).licenseUrl.isEmpty());
    }
    void oracleOpenJdkInstallVerifiesRuntimeAndRetainsLegalFiles() {
#ifdef Q_OS_WIN
        QSKIP("POSIX executable fixture");
#endif
        QTemporaryDir dir;auto platform=mc::GamePlatform::current();const auto suffix=platform.os=="osx"?QString("macos"):QString("linux");const auto arch=platform.arch=="aarch64"?QString("aarch64"):QString("x64");const QString url="https://download.java.net/java/GA/jdk17.0.2/unit-test/8/GPL/openjdk-17.0.2_"+suffix+"-"+arch+"_bin.tar.gz";auto packages=mc::parseOracleOpenJdkPackages(openJdkHtml(url),17,platform);QCOMPARE(packages.size(),1);auto p=packages.first();auto java=script("17.0.2","17.0.2+8");java.replace("Eclipse Adoptium","Oracle Corporation");const auto archive=dir.filePath("test.tar.gz");QVERIFY(gzip(archive,tarEntry("jdk/bin/java",java)+tarEntry("jdk/legal/java.base/LICENSE","license of this exact fixture build")+QByteArray(1024,'\0')));QFile file(archive);QVERIFY(file.open(QIODevice::ReadOnly));auto bytes=file.readAll();p.size=bytes.size();Network net;net.bodies[url]=bytes;net.bodies[url+".sha256"]=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();auto installed=mc::installJavaPackage(p,dir.filePath("managed"),flag(),{},&net);QVERIFY2(installed.error.isEmpty(),qPrintable(installed.error));QCOMPARE(installed.runtime.major,17);auto legal=mc::javaLegalPaths(installed.runtime.executable);QCOMPARE(legal.licenses.size(),1);QVERIFY(QFileInfo::exists(legal.legalDirectory));QFile actual(legal.licenses.first());QVERIFY(actual.open(QIODevice::ReadOnly));QCOMPARE(actual.readAll(),QByteArray("license of this exact fixture build"));QVERIFY(mc::javaLegalPaths(dir.filePath("missing/bin/java")).licenses.isEmpty());
        QDir root(dir.filePath("managed"));auto directories=root.entryList(QDir::Dirs|QDir::NoDotAndDotDot);QCOMPARE(directories.size(),1);QFile record(root.filePath(directories.first()+"/.linjian-package.json"));QVERIFY(record.open(QIODevice::ReadOnly));auto saved=QJsonDocument::fromJson(record.readAll()).object();QCOMPARE(saved.value("license").toString(),QString("GPLv2 + Classpath Exception"));QCOMPARE(saved.value("downloadUrl").toString(),url);
    }
    void officialManifestBuildsDeduplicateAndFilter() {
        Network network; const auto url = "https://piston-meta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json";
        const QJsonObject entry{{"version", QJsonObject{{"name", "17.0.10"}}}, {"manifest", QJsonObject{{"sha1", QString(40, 'a')}, {"url", "https://test/manifest"}}}};
        network.bodies[url] = QJsonDocument(QJsonObject{{"mac-os-arm64", QJsonObject{{"gamma", QJsonArray{entry}}, {"snapshot", QJsonArray{entry}}}}}).toJson();
        const auto list = mc::fetchJavaPackages("mojang", 17, {"osx", "aarch64", {}}, flag(), 0, &network); QVERIFY2(list.error.isEmpty(), qPrintable(list.error)); QCOMPARE(list.packages.size(), 1); QVERIFY(list.packages.first().label.contains(QString(12, 'a')));
        const auto unavailable = mc::fetchJavaPackages("mojang", 8, {"osx", "aarch64", {}}, flag(), 0, &network); QVERIFY(!unavailable.error.isEmpty()); QVERIFY(unavailable.packages.isEmpty());
    }
    void archiveExtractionPreservesExecutableAndRejectsTraversal() {
        QTemporaryDir dir; const auto root = QFileInfo(dir.path()).canonicalFilePath();
        const auto archive = root + "/java.zip"; QVERIFY(save(archive, zip("jdk/bin/java", "data"))); mc::extractJavaArchive(archive, root + "/out", flag());
        QFile extracted(root + "/out/jdk/bin/java");
        QVERIFY(extracted.open(QIODevice::ReadOnly));
        QCOMPARE(extracted.readAll(), QByteArray("data"));
        extracted.close();
#ifdef Q_OS_UNIX
        QVERIFY(QFileInfo(extracted).isExecutable());
#endif
        QVERIFY(save(archive, zip("../escape", "bad"))); QVERIFY(rejects([&] { mc::extractJavaArchive(archive, root + "/escape-out", flag()); }));
        QVERIFY(save(archive, zip("link", "../../escape", 0120777u << 16))); QVERIFY(rejects([&] { mc::extractJavaArchive(archive, root + "/links", flag()); }));
        auto corrupt = zip("file", "payload"); corrupt[35] = '!'; QVERIFY(save(archive, corrupt)); QVERIFY(rejects([&] { mc::extractJavaArchive(archive, root + "/crc", flag()); }));
    }
    void tarLinksBecomeInternalCopiesAndTruncationFails() {
        QTemporaryDir dir; const auto root = QFileInfo(dir.path()).canonicalFilePath(); const auto archive = root + "/java.tar.gz";
        const auto bytes = tarEntry("jdk/bin/java", "runtime") + tarEntry("jdk/lib/copy", {}, '2', "../bin/java") + QByteArray(1024, '\0');
        QVERIFY(gzip(archive, bytes)); mc::extractJavaArchive(archive, root + "/out", flag()); QFile copy(root + "/out/jdk/lib/copy"); QVERIFY(copy.open(QIODevice::ReadOnly)); QCOMPARE(copy.readAll(), QByteArray("runtime")); QVERIFY(!QFileInfo(copy).isSymLink());
        QVERIFY(gzip(archive, tarEntry("jdk/link", {}, '2', "../../escape") + QByteArray(1024, '\0'))); QVERIFY(rejects([&] { mc::extractJavaArchive(archive, root + "/escape", flag()); }));
        QVERIFY(gzip(archive, bytes.left(700))); QVERIFY(rejects([&] { mc::extractJavaArchive(archive, root + "/truncated", flag()); }));
    }
    void installRetriesCachesAndKeepsBuildsSeparate() {
#ifdef Q_OS_WIN
        QSKIP("POSIX executable fixture");
#endif
        QTemporaryDir dir; const auto root = QFileInfo(dir.path()).canonicalFilePath(); Network network;
        const auto archive = zip("jdk/bin/java", script()); network.bodies["https://test/java.zip"] = archive; network.corrupt = 1;
        mc::JavaPackage p; p.provider = "temurin"; p.id = "build-7"; p.version = "17.0.10"; p.build = 7; p.major = 17; p.os = mc::GamePlatform::current().os; p.arch = mc::GamePlatform::current().arch; p.archiveName = "java.zip"; p.url = QUrl("https://test/java.zip"); p.size = archive.size(); p.sha256 = QString::fromLatin1(QCryptographicHash::hash(archive, QCryptographicHash::Sha256).toHex());
        const auto first = mc::installJavaPackage(p, root, flag(), {}, &network); QVERIFY2(first.error.isEmpty(), qPrintable(first.error)); QCOMPARE(network.requests.value("https://test/java.zip"), 2); QCOMPARE(first.runtime.runtimeVersion, QString("17.0.10+7"));
        const auto before = network.requests; const auto reused = mc::installJavaPackage(p, root, flag(), {}, &network); QVERIFY2(reused.error.isEmpty(), qPrintable(reused.error)); QCOMPARE(network.requests, before);
        const auto secondArchive = zip("jdk/bin/java", script("17.0.10", "17.0.10+8")); network.bodies["https://test/java.zip"] = secondArchive; p.id = "build-8"; p.build = 8; p.size = secondArchive.size(); p.sha256 = QString::fromLatin1(QCryptographicHash::hash(secondArchive, QCryptographicHash::Sha256).toHex());
        const auto second = mc::installJavaPackage(p, root, flag(), {}, &network); QVERIFY2(second.error.isEmpty(), qPrintable(second.error)); QVERIFY(first.runtime.executable != second.runtime.executable); QVERIFY(QFileInfo::exists(first.runtime.executable));
        p.id = "bad-hash"; p.sha256 = QString(64, '0'); const auto bad = mc::installJavaPackage(p, root, flag(), {}, &network); QVERIFY(!bad.error.isEmpty()); QVERIFY(QFileInfo::exists(first.runtime.executable));
        p.id = "cancelled"; p.sha256 = QString::fromLatin1(QCryptographicHash::hash(secondArchive, QCryptographicHash::Sha256).toHex()); const auto stop = flag();
        const auto cancelled = mc::installJavaPackage(p, root, stop, [stop](const QString &, int progress) { if (progress >= 0) stop->store(true); }, &network); QVERIFY(cancelled.error.contains(QStringLiteral("取消"))); QVERIFY(QFileInfo::exists(first.runtime.executable));
        p.id = "size-metadata-difference"; p.size += 23; const auto sizeDifference = mc::installJavaPackage(p, root, flag(), {}, &network); QVERIFY2(sizeDifference.error.isEmpty(), qPrintable(sizeDifference.error)); QVERIFY(!sizeDifference.warning.isEmpty());
        p.id = "wrong-build"; p.build = 9; const auto wrong = mc::installJavaPackage(p, root, flag(), {}, &network); QVERIFY(!wrong.error.isEmpty()); QVERIFY(wrong.runtime.executable.isEmpty());
    }
};
QTEST_GUILESS_MAIN(JavaTests)
#include "java_tests.moc"
