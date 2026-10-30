// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "networkerror.h"
#include "javaprovider.h"
#include "core/javaarchive.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <stdexcept>
#include <algorithm>
#include <QVersionNumber>

namespace mc {
namespace {
[[noreturn]] void fail(const QString &s) { throw std::runtime_error(s.toUtf8().constData()); }
void check(const JavaCancel &flag) { if (flag->load()) fail(QCoreApplication::translate("M2CL", "Java 操作已取消")); }
QJsonDocument json(const QByteArray &bytes) {
    QJsonParseError error; const auto doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError) fail(QCoreApplication::translate("M2CL", "Java 发行源返回的数据无效")); return doc;
}
QString dotted(const QJsonArray &array) { QStringList parts; for (const auto &v : array) parts.append(QString::number(v.toInt())); return parts.join('.'); }
QStringList packageArchitectures(const QList<JavaPackage> &packages) {
    QStringList result;
    for (const auto &package : packages) if (!package.arch.isEmpty() && !result.contains(package.arch)) result.append(package.arch);
    return result;
}
QString mojangKey(const GamePlatform &p) {
    if (p.os == "osx") return p.arch == "aarch64" ? "mac-os-arm64" : p.arch == "x86_64" ? "mac-os" : QString();
    if (p.os == "windows") return p.arch == "aarch64" ? "windows-arm64" : p.arch == "x86" ? "windows-x86" : p.arch == "x86_64" ? "windows-x64" : QString();
    return p.arch == "x86_64" ? "linux" : p.arch == "x86" ? "linux-i386" : QString();
}
void save(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).path()); QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) fail(QCoreApplication::translate("M2CL", "无法保存 Java 安装信息"));
}
QString findJava(const QString &root) {
#ifdef Q_OS_WIN
    const QString name = "java.exe";
#else
    const QString name = "java";
#endif
    QDirIterator it(root, {name}, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    QString found;
    while (it.hasNext()) { it.next(); if (it.fileInfo().dir().dirName() != "bin") continue; if (!found.isEmpty()) fail(QCoreApplication::translate("M2CL", "压缩包中包含多个 Java，无法确定运行时")); found = it.filePath(); }
    return found;
}
QString downloadArchive(const JavaPackage &p, const QString &destination, const JavaCancel &flag, const InstallProgress &progress, QNetworkAccessManager &network) {
    if (p.url.scheme() != "https" || !p.url.userInfo().isEmpty() || !QRegularExpression("^[a-fA-F0-9]{64}$").match(p.sha256).hasMatch()) fail(QCoreApplication::translate("M2CL", "Java 下载地址或 SHA-256 无效"));
    if (p.size < 0 || p.size > 512LL * 1024 * 1024) fail(QCoreApplication::translate("M2CL", "Java 压缩包大小无效"));
    QFile existing(destination);
    auto sizeWarning = [&](qint64 bytes) { return p.size > 0 && bytes != p.size ? QCoreApplication::translate("M2CL", "发行源大小标注有差异；传输及 SHA-256 校验通过（%1 字节）").arg(bytes) : QString(); };
    if (existing.open(QIODevice::ReadOnly) && existing.size() <= 512LL * 1024 * 1024) {
        QCryptographicHash hash(QCryptographicHash::Sha256); while (!existing.atEnd()) { check(flag); hash.addData(existing.read(1024 * 1024)); }
        if (hash.result().toHex() == p.sha256.toLatin1().toLower()) return sizeWarning(existing.size());
    }
    existing.close();
    QString error;
    for (int attempt = 0; attempt < 3; ++attempt) {
        check(flag); QSaveFile file(destination); if (!file.open(QIODevice::WriteOnly)) fail(file.errorString());
        QCryptographicHash hash(QCryptographicHash::Sha256); QEventLoop loop; QTimer timer; qint64 received = 0;
        QNetworkRequest req(p.url); req.setRawHeader("User-Agent", "m2cl/0.1.0"); req.setRawHeader("Accept-Encoding", "identity"); req.setTransferTimeout(30000);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        auto reply = network.get(req); error.clear();
        auto consume = [&] {
            const auto problem=networkReplyError(reply);if(!problem.isEmpty()){error=problem;reply->abort();return;}
            const auto bytes = reply->readAll(); received += bytes.size();
            if (received > 512LL * 1024 * 1024) error = QCoreApplication::translate("M2CL", "Java 文件超过预期大小");
            else if (file.write(bytes) != bytes.size()) error = file.errorString();
            else hash.addData(bytes);
            if (!error.isEmpty()) reply->abort();
        };
        QObject::connect(reply, &QNetworkReply::readyRead, &loop, consume);
        QObject::connect(reply, &QNetworkReply::downloadProgress, &loop, [&](qint64 got, qint64 total) {
            if (progress) progress(QCoreApplication::translate("M2CL", "下载 Java · %1 / %2 MiB").arg(got / 1048576., 0, 'f', 1).arg(total > 0 ? total / 1048576. : p.size / 1048576., 0, 'f', 1), total > 0 ? int(got * 1000 / total) : -1);
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, [&] { if (flag->load()) reply->abort(); });
        timer.start(100); loop.exec(); if (error.isEmpty()) consume();
        if (error.isEmpty())error=networkReplyError(reply,reply->error()==QNetworkReply::OperationCanceledError&&!flag->load());
        const auto transportLength = reply->header(QNetworkRequest::ContentLengthHeader);
        reply->deleteLater(); check(flag);
        if (error.isEmpty() && transportLength.isValid() && transportLength.toLongLong() != received) error = QCoreApplication::translate("M2CL", "Java 文件传输大小校验失败");
        if (error.isEmpty() && hash.result().toHex() != p.sha256.toLatin1().toLower()) error = QCoreApplication::translate("M2CL", "Java SHA-256 校验失败");
        if (error.isEmpty() && file.commit()) return sizeWarning(received);
        if (error.isEmpty()) error = file.errorString(); file.cancelWriting();
    }
    fail(error);
}
bool correctVersion(const JavaRuntime &j, const JavaPackage &p) {
    if (j.major != p.major || j.architecture != p.arch) return false;
    if (p.provider == "mojang") return p.version.isEmpty() || j.version == p.version;
    QString actual = j.version;
    if (actual.startsWith("1.8.0_")) actual = "8.0." + actual.mid(6);
    if (!p.version.isEmpty() && actual != p.version) {
        if (p.provider != "microsoft" || actual != p.version.section('.', 0, 2)) return false;
    }
    if (p.provider == "microsoft") return j.vendor.contains("Microsoft", Qt::CaseInsensitive);
    const auto build = QRegularExpression("(?:\\+|(?:^|-)b)0*(\\d+)").match(j.runtimeVersion);
    if (p.build > 0 && (!build.hasMatch() || build.captured(1).toInt() != p.build)) return false;
    if(p.provider=="oracle-openjdk")return j.vendor.contains("Oracle",Qt::CaseInsensitive)&&j.vmName.contains("OpenJDK",Qt::CaseInsensitive);
    return p.provider == "zulu" ? j.vendor.contains("Azul", Qt::CaseInsensitive) : j.vendor.contains("Adoptium", Qt::CaseInsensitive) || j.vendor.contains("Temurin", Qt::CaseInsensitive);
}
}
QList<JavaPackage> parseTemurinPackages(const QJsonArray &releases, int major, const GamePlatform &platform) {
    QList<JavaPackage> result;
    const auto os = platform.os == "osx" ? "mac" : platform.os;
    const QRegularExpression pattern(QStringLiteral("^OpenJDK%1U-(jre|jdk)_([^_]+)_%2_hotspot_.*\\.(zip|tar\\.gz)$").arg(major).arg(QRegularExpression::escape(os)));
    for (const auto &value : releases) {
        const auto release = value.toObject(); if (release.value("draft").toBool() || release.value("prerelease").toBool()) continue;
        const auto assets = release.value("assets").toArray();
        for (const auto &v : assets) {
            const auto a = v.toObject(); const auto name = a.value("name").toString();
            const auto match = pattern.match(name);
            if (!match.hasMatch() || !(platform.os == "windows" ? name.endsWith(".zip") : name.endsWith(".tar.gz"))) continue;
            const auto architecture = match.captured(2) == "x86-32" ? QString("x86") : normalizeArchitecture(match.captured(2));
            if (platform.arch != "*" && architecture != platform.arch) continue;
            JavaPackage p; p.provider = "temurin"; p.major = major; p.os = platform.os; p.arch = architecture; p.packageType = match.captured(1).toUpper();
            p.id = release.value("tag_name").toString() + "-" + QString::number(a.value("id").toDouble(), 'f', 0);
            p.sourcePage=QUrl(release.value("html_url").toString()); p.label = release.value("tag_name").toString(); p.archiveName = name; p.url = QUrl(a.value("browser_download_url").toString()); p.size = a.value("size").toDouble();
            const auto tag = release.value("tag_name").toString();
            auto version = QRegularExpression("^jdk-(\\d+(?:\\.\\d+)*)(?:\\+(\\d+))?").match(tag);
            if (version.hasMatch()) { p.version = version.captured(1); p.build = version.captured(2).toInt(); }
            else { version = QRegularExpression("^jdk8u(\\d+)-b(\\d+)").match(tag); if (version.hasMatch()) { p.version = "8.0." + version.captured(1); p.build = version.captured(2).toInt(); } }
            const auto digest = a.value("digest").toString(); if (digest.startsWith("sha256:")) p.sha256 = digest.mid(7);
            for (const auto &other : assets) if (other.toObject().value("name").toString() == name + ".sha256.txt") p.checksumUrl = QUrl(other.toObject().value("browser_download_url").toString());
            if (!p.sha256.isEmpty() || !p.checksumUrl.isEmpty()) result.append(p);
        }
    }
    return result;
}
QList<JavaPackage> parseZuluPackages(const QJsonArray &packages, int major, const GamePlatform &platform) {
    QList<JavaPackage> result;
    for (const auto &v : packages) {
        const auto o = v.toObject(); if (o.value("java_version").toArray().first().toInt() != major) continue;
        auto architecture = normalizeArchitecture(o.value("arch").toString()); const auto bits = o.value("hw_bitness").toInt();
        if (architecture == "arm" && bits == 64) architecture = "aarch64";
        if (architecture == "x86" && bits == 64) architecture = "x86_64";
        if (platform.arch == "*" ? architecture.isEmpty() : !architecture.isEmpty() && architecture != platform.arch) continue;
        const auto os = o.value("os").toString();
        if (!os.isEmpty() && (os == "macos" ? QString("osx") : os.startsWith("linux") ? QString("linux") : os) != platform.os) continue;
        if (platform.os == "linux" && o.value("lib_c_type") == "musl") continue;
        JavaPackage p; p.provider = "zulu"; p.id = o.value("package_uuid").toString(); p.major = major; p.os = platform.os; p.arch = architecture.isEmpty() ? platform.arch : architecture;
        p.version = dotted(o.value("java_version").toArray()); p.build = o.value("openjdk_build_number").toInt(); p.distributionVersion = dotted(o.value("distro_version").toArray());
        p.label = p.version + "+" + QString::number(p.build) + " · Zulu " + p.distributionVersion;
        p.url = QUrl(o.value("download_url").toString()); p.archiveName = o.value("name").toString(); p.sha256 = o.value("sha256_hash").toString(); p.size = o.value("size").toDouble();
        if (!p.id.isEmpty() && p.url.scheme() == "https") result.append(p);
    }
    return result;
}
QList<JavaPackage> parseMicrosoftPackages(const QString &html, int major, const GamePlatform &platform) {
    QList<JavaPackage> result; QSet<QString> seen;
    const auto os = platform.os == "osx" ? "macos" : platform.os;
    const auto arch = platform.arch == "*" ? QString("[a-z0-9_]+") : platform.arch == "x86_64" ? QString("x64") : QRegularExpression::escape(platform.arch);
    const auto suffix = platform.os == "windows" ? "zip" : "tar\\.gz";
    const auto pattern = QStringLiteral("https://aka\\.ms/download-jdk/(microsoft-jdk-([0-9]+(?:\\.[0-9]+)*)-%1-%2\\.%3)").arg(QRegularExpression::escape(os), arch, suffix);
    auto matches = QRegularExpression(pattern).globalMatch(html);
    while (matches.hasNext()) {
        const auto match = matches.next(); const auto version = match.captured(2);
        if (version.section('.', 0, 0).toInt() != major || seen.contains(match.captured(1))) continue;
        // Match archive links only, not the corresponding checksum / signature links.
        const auto next = html.mid(match.capturedEnd(), 1); if (next == ".") continue;
        seen.insert(match.captured(1)); JavaPackage p; p.provider = "microsoft"; p.major = major; p.version = version; p.label = version; p.packageType = "JDK";
        p.os = platform.os; p.arch = normalizeArchitecture(match.captured(1).section('-', -1).section('.', 0, 0)); p.id = match.captured(1); p.archiveName = match.captured(1); p.url = QUrl(match.captured()); p.sourcePage=QUrl("https://learn.microsoft.com/en-us/java/openjdk/download");p.checksumUrl = QUrl(match.captured() + ".sha256sum.txt"); result.append(p);
    }
    return result;
}
QList<JavaPackage> parseOracleOpenJdkPackages(const QString &raw, int major, const GamePlatform &platform, const QUrl &sourcePage) {
    QList<JavaPackage> result;
    if (sourcePage.scheme() != "https" || sourcePage.host() != "jdk.java.net" || !sourcePage.userInfo().isEmpty()) return result;
    auto html = raw; html.replace("&#160;", " ").replace("&nbsp;", " ").replace("&#43;", "+");
    if (!html.contains(QRegularExpression("https?://openjdk\\.(?:org|java\\.net)/legal/gplv2(?:\\+|%2[Bb])ce\\.html"))) return result;
    QMap<QString, int> builds; auto headings = QRegularExpression("build\\s+(\\d+(?:\\.\\d+)*)\\+(\\d+)").globalMatch(html);
    while (headings.hasNext()) { auto h = headings.next(); builds.insert(h.captured(1), h.captured(2).toInt()); }
    auto rows = QRegularExpression("<tr(?:\\s[^>]*)?>(.*?)</tr>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption).globalMatch(html); QSet<QString> seen;
    while (rows.hasNext()) {
        auto row = rows.next().captured(1); auto anchors = QRegularExpression("href\\s*=\\s*[\"']([^\"']+)[\"']", QRegularExpression::CaseInsensitiveOption).globalMatch(row); QList<QUrl> links;
        while (anchors.hasNext()) links.append(QUrl(anchors.next().captured(1)));
        for (const auto &url : links) {
            if (url.scheme() != "https" || url.host() != "download.java.net" || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() || (url.port(-1) != -1 && url.port() != 443) || !url.path().startsWith("/java/GA/", Qt::CaseInsensitive)) continue;
            const auto name = QFileInfo(url.path()).fileName(); const auto m = QRegularExpression("^openjdk-(\\d+(?:\\.\\d+)*)_(windows|linux|macos|osx)-([a-z0-9_-]+)_bin\\.(zip|tar\\.gz)$").match(name);
            if (!m.hasMatch() || m.captured(1).section('.', 0, 0).toInt() != major) continue;
            auto os = m.captured(2); if (os == "macos") os = "osx"; auto arch = normalizeArchitecture(m.captured(3));
            if (os != platform.os || (platform.arch != "*" && arch != platform.arch) || (os == "windows" ? m.captured(4) != "zip" : m.captured(4) != "tar.gz")) continue;
            QUrl checksum; for (const auto &candidate : links) if (candidate.toString() == url.toString() + ".sha256" || candidate.toString() == url.toString().section("_bin", 0, 0) + "_bin.sha256") checksum = candidate;
            if (checksum.isEmpty() || seen.contains(url.toString())) continue;
            JavaPackage p; p.provider = "oracle-openjdk"; p.major = major; p.os = os; p.arch = arch; p.packageType = "JDK"; p.version = m.captured(1); p.build = builds.value(p.version);
            const auto pathBuild = QRegularExpression("/(\\d+)/GPL/").match(url.path()); if (pathBuild.hasMatch()) { const auto n = pathBuild.captured(1).toInt(); if (p.build && p.build != n) continue; p.build = n; }
            if (p.build <= 0) continue;
            p.id = url.toString(); p.url = url; p.checksumUrl = checksum; p.archiveName = name; p.sourcePage = sourcePage; p.archived = sourcePage.path().startsWith("/archive");
            p.label = p.version + "+" + QString::number(p.build) + (p.archived ? QCoreApplication::translate("M2CL", " · 官方归档") : QString());
            auto size = QRegularExpression("</a>\\s*(\\d+(?:\\.\\d+)?)\\s*(MiB|MB|M)?\\s*<span", QRegularExpression::CaseInsensitiveOption).match(row);
            if (size.hasMatch()) p.size = qint64(size.captured(1).toDouble() * (size.captured(2).isEmpty() ? 1 : size.captured(2) == "MiB" ? 1048576 : 1000000));
            seen.insert(url.toString()); result.append(p);
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const JavaPackage &a, const JavaPackage &b) { const auto cmp = QVersionNumber::compare(QVersionNumber::fromString(a.version), QVersionNumber::fromString(b.version)); return cmp == 0 ? a.build > b.build : cmp > 0; });
    return result;
}
JavaLicenseInfo javaPackageLicense(const JavaPackage &p) {
    JavaLicenseInfo info; info.name = QCoreApplication::translate("M2CL", "未提供许可信息"); info.buildUrl = p.sourcePage;
    if (p.provider == "oracle-openjdk") { info.name = "GPLv2 + Classpath Exception"; info.licenseUrl = QUrl("https://openjdk.org/legal/gplv2+ce.html"); info.website = QUrl("https://jdk.java.net/"); info.scope = QCoreApplication::translate("M2CL", "官方 GPL 发布页覆盖所选构建；第三方声明保留在发行包内。"); }
    else if (p.provider == "temurin") { info.name = "GPLv2 + Classpath Exception"; info.licenseUrl = QUrl("https://adoptium.net/docs/faq/"); info.website = QUrl("https://adoptium.net/"); info.scope = QCoreApplication::translate("M2CL", "Temurin 通用许可说明；此构建的完整声明以发行包内文件为准。"); }
    else if (p.provider == "zulu") { info.name = "GPLv2 + Classpath Exception"; info.licenseUrl = QUrl("https://docs.azul.com/core/tpl"); info.website = QUrl("https://www.azul.com/downloads/"); info.buildUrl = QUrl("https://api.azul.com/metadata/v1/zulu/packages/" + QString::fromLatin1(QUrl::toPercentEncoding(p.id))); info.scope = QCoreApplication::translate("M2CL", "Zulu 通用许可及声明索引；未确认该构建的专用在线文档，请以包内声明为准。"); }
    else if (p.provider == "microsoft") { info.name = "GPLv2 + Classpath Exception"; info.licenseUrl = QUrl("https://learn.microsoft.com/en-us/java/openjdk/faq"); info.website = QUrl("https://learn.microsoft.com/en-us/java/openjdk/download"); info.scope = QCoreApplication::translate("M2CL", "Microsoft OpenJDK 通用许可说明；完整第三方声明以所选发行包为准。"); }
    return info;
}
JavaPackageList fetchJavaPackages(const QString &provider, int major, const GamePlatform &platform, const JavaCancel &cancelled, int page, QNetworkAccessManager *network, bool discoverArchitectures) {
    JavaPackageList result;
    try {
        check(cancelled); if (major < 6 || major > 99 || page < 0 || page > 100) fail(QCoreApplication::translate("M2CL", "Java 查询条件无效"));
        if (provider == "all") {
            QStringList failures;
            result.architectureListComplete = true;
            for (const auto &source : {QString("temurin"), QString("zulu"), QString("microsoft"),QString("oracle-openjdk")}) {
                check(cancelled);
                const auto list = fetchJavaPackages(source, major, platform, cancelled, page, network, discoverArchitectures);
                result.packages.append(list.packages); result.hasMore |= list.hasMore;
                for (const auto &arch : list.availableArchitectures) if (!result.availableArchitectures.contains(arch)) result.availableArchitectures.append(arch);
                result.architectureListComplete &= list.architectureListComplete;
                if (!list.error.isEmpty()) failures.append(source);
            }
            if (!failures.isEmpty()) result.error = QCoreApplication::translate("M2CL", "找到 %1 个构建；%2 未返回匹配构建，可单独选择发行版查看详情").arg(result.packages.size()).arg(failures.join(" / "));
        } else if (provider == "oracle-openjdk") {
            auto allPlatforms = platform; allPlatforms.arch = "*";
            const QUrl current(QString("https://jdk.java.net/%1/").arg(major)), archive("https://jdk.java.net/archive/");QList<JavaPackage> packages;
            try { packages = parseOracleOpenJdkPackages(QString::fromUtf8(fetchInstallData(current,cancelled,{},network)),major,allPlatforms,current); } catch(const std::exception &) { check(cancelled); }
            auto older=parseOracleOpenJdkPackages(QString::fromUtf8(fetchInstallData(archive,cancelled,{},network)),major,allPlatforms,archive);QSet<QString> seen;for(auto p:packages)seen.insert(p.id);for(auto p:older)if(!seen.contains(p.id)){packages.append(p);seen.insert(p.id);}
            result.availableArchitectures = packageArchitectures(packages); result.architectureListComplete = true;
            packages.erase(std::remove_if(packages.begin(), packages.end(), [&platform](const JavaPackage &p) { return platform.arch != "*" && p.arch != platform.arch; }), packages.end());
            std::stable_sort(packages.begin(),packages.end(),[](const JavaPackage&a,const JavaPackage&b){const auto cmp=QVersionNumber::compare(QVersionNumber::fromString(a.version),QVersionNumber::fromString(b.version));return cmp==0?a.build>b.build:cmp>0;});result.packages=packages.mid(page*20,20);result.hasMore=packages.size()>(page+1)*20;
        } else if (provider == "microsoft") {
            const auto current = QString::fromUtf8(fetchInstallData(QUrl("https://learn.microsoft.com/en-us/java/openjdk/download"), cancelled, {}, network));
            const auto archive = QString::fromUtf8(fetchInstallData(QUrl("https://learn.microsoft.com/en-us/java/openjdk/older-releases"), cancelled, {}, network));
            auto allPlatforms = platform; allPlatforms.arch = "*";
            auto packages = parseMicrosoftPackages(current + "\n" + archive, major, allPlatforms);for(auto &p:packages)if(!current.contains(p.url.toString()))p.sourcePage=QUrl("https://learn.microsoft.com/en-us/java/openjdk/older-releases");
            result.availableArchitectures = packageArchitectures(packages); result.architectureListComplete = true;
            packages.erase(std::remove_if(packages.begin(), packages.end(), [&platform](const JavaPackage &p) { return platform.arch != "*" && p.arch != platform.arch; }), packages.end());
            result.packages = packages.mid(page * 20, 20); result.hasMore = packages.size() > (page + 1) * 20;
        } else if (provider == "temurin") {
            const auto url = QString("https://api.github.com/repos/adoptium/temurin%1-binaries/releases?per_page=20&page=%2").arg(major).arg(page + 1);
            const auto releases = json(fetchInstallData(QUrl(url), cancelled, {}, network)).array();
            auto allPlatforms = platform; allPlatforms.arch = "*";
            result.availableArchitectures = packageArchitectures(parseTemurinPackages(releases, major, allPlatforms)); result.architectureListComplete = page == 0 && releases.size() < 20;
            result.packages = parseTemurinPackages(releases, major, platform); result.hasMore = releases.size() == 20;
            if (page == 0) {
                // GitHub's release-list order is creation order; the latest stable endpoint is authoritative.
                try {
                    const auto latest = json(fetchInstallData(QUrl(QString("https://api.github.com/repos/adoptium/temurin%1-binaries/releases/latest").arg(major)), cancelled, {}, network)).object();
                    const auto current = parseTemurinPackages({latest}, major, platform);
                    for (const auto &arch : packageArchitectures(parseTemurinPackages({latest}, major, allPlatforms))) if (!result.availableArchitectures.contains(arch)) result.availableArchitectures.append(arch);
                    for (auto it = current.crbegin(); it != current.crend(); ++it) { for (int i = result.packages.size() - 1; i >= 0; --i) if (result.packages.at(i).id == it->id) result.packages.removeAt(i); result.packages.prepend(*it); }
                } catch (const std::exception &) { check(cancelled); }
            }
        } else if (provider == "zulu") {
            QUrl url("https://api.azul.com/metadata/v1/zulu/packages/"); QUrlQuery query;
            query.addQueryItem("java_version", QString::number(major)); query.addQueryItem("os", platform.os == "osx" ? "macos" : platform.os == "linux" ? "linux-glibc" : platform.os);
            if (platform.arch != "*") query.addQueryItem("arch", platform.arch == "x86_64" ? "x64" : platform.arch);
            query.addQueryItem("include_fields", "size,sha256_hash,arch,hw_bitness,os,java_package_type,lib_c_type");
            for (const auto &pair : QList<QPair<QString, QString>>{{"java_package_type", "jre"}, {"javafx_bundled", "false"}, {"archive_type", platform.os == "windows" ? "zip" : "tar.gz"}, {"release_status", "ga"}, {"availability_types", "CA"}, {"page_size", "30"}, {"latest", "false"}, {"page", QString::number(page + 1)}}) query.addQueryItem(pair.first, pair.second);
            url.setQuery(query); const auto packages = json(fetchInstallData(url, cancelled, {}, network)).array();
            result.packages = parseZuluPackages(packages, major, platform); result.hasMore = packages.size() == 30;
            result.availableArchitectures = packageArchitectures(result.packages);
            if (discoverArchitectures && page == 0) {
                // Discover from actual historical builds, including discontinued architectures.
                auto catalogQuery = query; catalogQuery.removeAllQueryItems("arch"); catalogQuery.removeAllQueryItems("page_size"); catalogQuery.addQueryItem("page_size", "1000");
                auto catalogUrl = url; catalogUrl.setQuery(catalogQuery);
                try {
                    const auto catalog = json(fetchInstallData(catalogUrl, cancelled, {}, network)).array(); auto allPlatforms = platform; allPlatforms.arch = "*";
                    result.availableArchitectures = packageArchitectures(parseZuluPackages(catalog, major, allPlatforms)); result.architectureListComplete = catalog.size() < 1000;
                } catch (const std::exception &) { check(cancelled); }
            }
        } else if (provider == "mojang") {
            const auto key = mojangKey(platform);
            if (!key.isEmpty()) {
                const auto all = json(fetchInstallData(QUrl("https://piston-meta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json"), cancelled, {}, network)).object().value(key).toObject();
                QSet<QString> seen;
                for (auto it = all.begin(); it != all.end(); ++it) for (const auto &v : it.value().toArray()) {
                    const auto o = v.toObject(); const auto name = o.value("version").toObject().value("name").toString(); auto normal = name; if (normal.startsWith("1.")) normal.remove(0, 2);
                    if (normal.section('.', 0, 0).toInt() != major) continue;
                    const auto manifest = o.value("manifest").toObject(); const auto hash = manifest.value("sha1").toString(); if (seen.contains(hash)) continue; seen.insert(hash);
                    JavaPackage p; p.provider = provider; p.major = major; p.os = platform.os; p.arch = platform.arch; p.id = hash; p.version = name; p.manifest = manifest;
                    p.label = name + QCoreApplication::translate("M2CL", " · 构建标识 ") + hash.left(12); result.packages.append(p);
                }
            }
        } else fail(QCoreApplication::translate("M2CL", "未知 Java 发行版"));
        if (result.packages.isEmpty()) result.error = result.hasMore ? QCoreApplication::translate("M2CL", "本页无匹配构建，可查看下一页或更换平台条件") : QCoreApplication::translate("M2CL", "此发行版没有提供符合当前主版本与架构的构建");
    } catch (const std::exception &e) { result.error = QString::fromUtf8(e.what()); }
    return result;
}
JavaInstallResult installJavaPackage(JavaPackage p, const QString &rawRoot, const JavaCancel &cancelled, const InstallProgress &progress, QNetworkAccessManager *manager) {
    JavaInstallResult result;
    try {
        check(cancelled); QNetworkAccessManager owned; auto &network = manager ? *manager : owned;
        if (!QDir().mkpath(rawRoot)) fail(QCoreApplication::translate("M2CL", "无法创建 Java 管理目录"));
        const auto root = QFileInfo(rawRoot).canonicalFilePath();
        if (p.provider != "mojang" && p.provider != "temurin" && p.provider != "zulu" && p.provider != "microsoft" && p.provider != "oracle-openjdk") fail(QCoreApplication::translate("M2CL", "未知 Java 发行源"));
        if(p.provider=="oracle-openjdk"&&(p.url.host()!="download.java.net"||p.checksumUrl.host()!="download.java.net"||!p.url.path().startsWith("/java/GA/",Qt::CaseInsensitive)||(p.checksumUrl.toString()!=p.url.toString()+".sha256"&&p.checksumUrl.toString()!=p.url.toString().section("_bin",0,0)+"_bin.sha256")))fail(QCoreApplication::translate("M2CL", "OpenJDK 官方构建地址或校验地址无效"));
        const auto identity = p.provider + "\n" + p.id + "\n" + p.os + "\n" + p.arch;
        const auto key = p.provider + "-" + QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
        const auto destination = checkedPath(root, key); QLockFile lock(destination + ".lock"); lock.setStaleLockTime(0);
        if (!lock.tryLock(0)) fail(QCoreApplication::translate("M2CL", "此 Java 构建正在由另一个安装任务处理"));
        if (QFileInfo::exists(destination)) {
            if (!QFileInfo::exists(checkedPath(destination, ".linjian-package.json"))) fail(QCoreApplication::translate("M2CL", "此构建目录不完整，请先移出后重试"));
            result.runtime = probeJava(findJava(destination), cancelled);
            if (!correctVersion(result.runtime, p)) fail(QCoreApplication::translate("M2CL", "已安装构建无法运行或版本不匹配")); return result;
        }
        QTemporaryDir stage(QDir(root).filePath(".linjian-java-stage-XXXXXX")); if (!stage.isValid()) fail(QCoreApplication::translate("M2CL", "无法创建 Java 暂存目录"));
        const auto data = stage.path() + "/runtime"; QDir().mkpath(data);
        if (p.provider == "mojang") {
            if (progress) progress(QCoreApplication::translate("M2CL", "读取官方 Java 构建清单…"), -1);
            const auto manifest = json(fetchInstallData(QUrl(p.manifest.value("url").toString()), cancelled, p.manifest.value("sha1").toString(), &network)).object().value("files").toObject();
            QList<GameFile> files; QMap<QString, QString> links; quint64 expanded = 0;
            for (auto it = manifest.begin(); it != manifest.end(); ++it) {
                const auto entry = it.value().toObject(); checkedPath(data, it.key());
                if (entry.value("type") == "file") { auto f = gameFile(entry.value("downloads").toObject().value("raw").toObject(), it.key()); f.executable = entry.value("executable").toBool(); expanded += f.size; if (expanded > 2ULL * 1024 * 1024 * 1024) fail(QCoreApplication::translate("M2CL", "Java 构建文件超过大小限制")); files.append(f); }
                else if (entry.value("type") == "directory") QDir().mkpath(checkedPath(data, it.key()));
                else if (entry.value("type") == "link") links.insert(it.key(), QDir::cleanPath(QFileInfo(it.key()).path() + "/" + entry.value("target").toString()));
            }
            // Reuse a persistent per-build cache so cancellation does not redownload verified runtime files.
            const auto cache = checkedPath(root, ".linjian-java-cache/" + key); downloadVerifiedGameFiles(files, cache, cancelled, progress, &network);
            for (const auto &f : files) { check(cancelled); const auto target = checkedPath(data, f.path); QDir().mkpath(QFileInfo(target).path()); if (!QFile::copy(checkedPath(cache, f.path), target)) fail(QCoreApplication::translate("M2CL", "无法准备 Java 文件")); }
            for (int pass = 0; !links.isEmpty() && pass < 64; ++pass) {
                const auto pending = links;
                for (auto it = pending.begin(); it != pending.end(); ++it) { check(cancelled); const auto source = checkedPath(data, it.value()), target = checkedPath(data, it.key()); if (!QFileInfo(source).isFile() || links.contains(it.value())) continue; expanded += QFileInfo(source).size(); if (expanded > 2ULL * 1024 * 1024 * 1024) fail(QCoreApplication::translate("M2CL", "Java 构建链接展开超过限制")); QDir().mkpath(QFileInfo(target).path()); if (!QFile::copy(source, target)) fail(QCoreApplication::translate("M2CL", "无法准备 Java 链接")); links.remove(it.key()); }
            }
            if (!links.isEmpty()) fail(QCoreApplication::translate("M2CL", "Java 构建链接无效"));
        } else {
            if (p.provider == "zulu") {
                const auto detail = json(fetchInstallData(QUrl("https://api.azul.com/metadata/v1/zulu/packages/" + p.id), cancelled, {}, &network)).object();
                const auto parsed = parseZuluPackages({detail}, p.major, {p.os, p.arch, {}});
                if (parsed.size() != 1 || parsed.first().url != p.url || parsed.first().archiveName != p.archiveName) fail(QCoreApplication::translate("M2CL", "Zulu 构建信息发生变化，请刷新列表")); p = parsed.first();
            } else if (p.sha256.isEmpty()) {
                const auto checksum = QString::fromUtf8(fetchInstallData(p.checksumUrl, cancelled, {}, &network)).trimmed().section(QRegularExpression("\\s+"), 0, 0);
                p.sha256 = checksum;
            }
            const auto suffix = p.archiveName.endsWith(".zip") ? ".zip" : p.archiveName.endsWith(".tar.gz") ? ".tar.gz" : QString();
            if (suffix.isEmpty()) fail(QCoreApplication::translate("M2CL", "不支持此 Java 包格式"));
            const auto archive = checkedPath(root, ".linjian-java-cache/" + key + suffix); QDir().mkpath(QFileInfo(archive).path());
            result.warning = downloadArchive(p, archive, cancelled, progress, network); if (progress) progress(QCoreApplication::translate("M2CL", "校验通过，正在解压 Java…"), -1);
            extractJavaArchive(archive, data, cancelled);
        }
        check(cancelled); if (progress) progress(QCoreApplication::translate("M2CL", "正在验证 Java 的实际版本、构建与架构…"), -1);
        auto runtime = probeJava(findJava(data), cancelled); check(cancelled);
        if (!correctVersion(runtime, p)) fail(QCoreApplication::translate("M2CL", "Java 未通过运行验证，或实际版本 / 构建 / 发行版 / 架构与选择不符"));
        const auto license=javaPackageLicense(p);
        save(checkedPath(data, ".linjian-package.json"), QJsonDocument(QJsonObject{{"provider", p.provider}, {"id", p.id}, {"label", p.label}, {"sha256", p.sha256}, {"warning", result.warning},{"license",license.name},{"licenseScope",license.scope},{"licenseUrl",license.licenseUrl.toString()},{"sourcePage",license.buildUrl.toString()},{"downloadUrl",p.url.toString()}}).toJson());
        const auto relative = QDir(data).relativeFilePath(runtime.executable);
        if (!QDir().rename(data, destination)) fail(QCoreApplication::translate("M2CL", "无法提交 Java 安装目录"));
        runtime.executable = checkedPath(destination, relative); result.runtime = runtime;
        if (progress) progress(QCoreApplication::translate("M2CL", "Java 安装完成 · %1").arg(javaLabel(runtime)), 1000);
    } catch (const std::exception &e) { result.error = QString::fromUtf8(e.what()); }
    return result;
}
}
