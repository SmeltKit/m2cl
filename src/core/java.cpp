// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "java.h"
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QStorageInfo>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>
#ifdef Q_OS_WIN
#include <QSettings>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mc {
QString normalizeArchitecture(const QString &architecture) {
    const auto text = architecture.trimmed().toLower();
    if (QStringList{"amd64", "x86_64", "x64"}.contains(text)) return "x86_64";
    if (QStringList{"aarch64", "arm64"}.contains(text)) return "aarch64";
    if (QStringList{"x86", "i386", "i486", "i586", "i686"}.contains(text)) return "x86";
    if (QStringList{"arm32", "aarch32", "aarch32hf", "armv7l", "armv7hl", "arm32-vfp-hflt"}.contains(text)) return "arm";
    if (text == "ppc64el") return "ppc64le";
    return text;
}

JavaRuntime parseJavaOutput(const QString &output) {
    JavaRuntime java;
    auto property = [&output](const QString &name) {
        const auto expression = QRegularExpression("(?:^|\\n)\\s*" + QRegularExpression::escape(name) + "\\s*=\\s*([^\\r\\n]+)");
        return expression.match(output).captured(1).trimmed();
    };
    java.version = property("java.version");
    if (java.version.isEmpty()) java.version = QRegularExpression("(?:java|openjdk) version \\\"([^\\\"]+)\\\"").match(output).captured(1);
    QString normalized = java.version;
    if (normalized.startsWith("1.")) normalized.remove(0, 2);
    java.major = QRegularExpression("^(\\d+)").match(normalized).captured(1).toInt();
    java.architecture = normalizeArchitecture(property("os.arch"));
    java.vendor = property("java.vendor");
    java.runtimeVersion = property("java.runtime.version");
    java.vmName = property("java.vm.name");
    java.vendorVersion = property("java.vendor.version");
    return java;
}

QStringList defaultJavaRoots(const QStringList &gameRoots) {
    QStringList roots;
    for (const auto &name : {"JAVA_HOME", "JDK_HOME"}) {
        const auto path = qEnvironmentVariable(name);
        if (!path.isEmpty()) roots.append(path);
    }
    const auto inPath = QStandardPaths::findExecutable("java");
    if (!inPath.isEmpty()) roots.append(inPath);
    for (const auto &gameRoot : gameRoots) roots.append(QDir(gameRoot).filePath("runtime"));
    const auto home = QDir::homePath();
#ifdef Q_OS_WIN
    for (const auto &env : {"ProgramFiles", "ProgramFiles(x86)"}) {
        const auto base = qEnvironmentVariable(env);
        if (base.isEmpty()) continue;
        for (const auto &vendor : {"Java", "Eclipse Adoptium", "Microsoft", "Zulu", "Amazon Corretto"})
            roots.append(QDir(base).filePath(vendor));
    }
    for (const auto &vendor : {"JavaSoft\\Java Runtime Environment", "JavaSoft\\JDK", "Eclipse Adoptium\\JDK"}) {
        for (const auto view : {QSettings::NativeFormat, QSettings::Registry32Format, QSettings::Registry64Format}) {
            QSettings registry(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\") + vendor, view);
            for (const auto &key : registry.allKeys()) {
                if (key.endsWith("JavaHome", Qt::CaseInsensitive) || key.endsWith("Path", Qt::CaseInsensitive))
                    roots.append(registry.value(key).toString());
            }
        }
    }
    const auto appData = qEnvironmentVariable("APPDATA");
    for (const auto &relative : {".minecraft/runtime", ".hmcl/java", "PrismLauncher/java", "ModrinthApp/meta/java_versions"})
        roots.append(QDir(appData).filePath(relative));
#elif defined(Q_OS_MACOS)
    roots << "/Library/Java/JavaVirtualMachines" << home + "/Library/Java/JavaVirtualMachines"
          << "/opt/homebrew/opt/openjdk/libexec/openjdk.jdk/Contents/Home"
          << "/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home"
          << "/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home"
          << "/usr/local/opt/openjdk/libexec/openjdk.jdk/Contents/Home";
#else
    roots << "/usr/lib/jvm" << "/usr/java" << "/opt/java";
#endif
    roots << home + "/.jdks" << home + "/.sdkman/candidates/java";
    roots.removeAll(QString());
    roots.removeDuplicates();
    return roots;
}

QJsonObject javaPin(const JavaRuntime &j) {
    return {{"mode", "fixed"}, {"major",j.major}, {"path", j.executable}, {"runtimeVersion", j.runtimeVersion.isEmpty() ? j.version : j.runtimeVersion}, {"vendor", j.vendor}, {"vendorVersion", j.vendorVersion}, {"arch", j.architecture}};
}

QString javaLabel(const JavaRuntime &j) {
    return QStringLiteral("Java %1 · %2 · %3").arg(j.runtimeVersion.isEmpty() ? j.version : j.runtimeVersion,
        j.vendorVersion.isEmpty() ? (j.vendor.isEmpty() ? QCoreApplication::translate("M2CL", "未知发行版") : j.vendor) : j.vendorVersion, j.architecture);
}

JavaRuntime probeJava(const QString &path, const JavaCancel &cancelled) {
    if (cancelled->load() || !QFileInfo(path).isExecutable()) return {};
    QProcess process; process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(path, {"-XshowSettings:properties", "-version"});
    if (!process.waitForStarted(3000)) return {};
    QElapsedTimer elapsed; elapsed.start(); QByteArray bytes;
    do {
        process.waitForFinished(50); bytes += process.readAll();
        if (cancelled->load() || elapsed.elapsed() > 5000 || bytes.size() > 256 * 1024) {
            process.kill(); process.waitForFinished(1000); return {};
        }
    } while (process.state() != QProcess::NotRunning);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return {};
    auto java = parseJavaOutput(QString::fromUtf8(bytes));
    if (!java.major || java.architecture.isEmpty()) return {};
    java.executable = QFileInfo(path).canonicalFilePath(); return java;
}

QStringList fullJavaScanRoots() {
    QStringList roots;
#ifdef Q_OS_WIN
    for (const auto &drive : QDir::drives()) { const auto path = drive.absoluteFilePath(); if (GetDriveTypeW(reinterpret_cast<const wchar_t *>(path.utf16())) != DRIVE_REMOTE) roots.append(path); }
#else
    roots.append("/");
    for (const auto &volume : QStorageInfo::mountedVolumes()) {
        const auto fs = volume.fileSystemType();
        const auto path = QDir::cleanPath(volume.rootPath());
        bool virtualPath = path.startsWith("/System/Volumes");
        for (const auto &prefix : {QString("/dev"), QString("/proc"), QString("/sys"), QString("/run")}) virtualPath |= path == prefix || path.startsWith(prefix + "/");
        if (volume.isReady() && volume.isValid() && !virtualPath && !QList<QByteArray>{"nfs", "smbfs", "cifs", "proc", "sysfs", "devfs"}.contains(fs)) roots.append(path);
    }
#endif
    roots.removeDuplicates(); return roots;
}

JavaScanResult scanJavaRuntimes(const QStringList &roots, const JavaCancel &cancelled,
    const JavaScanOptions &options, const std::function<void(const JavaScanProgress &)> &callback) {
    JavaScanResult result; QSet<QString> visited, candidates, excludedMounts;
    if (options.deep) for (const auto &volume : QStorageInfo::mountedVolumes()) if (QList<QByteArray>{"nfs", "smbfs", "cifs", "proc", "sysfs", "devfs"}.contains(volume.fileSystemType())) excludedMounts.insert(QDir::cleanPath(volume.rootPath()));
    struct Directory { QString path; int depth; }; QList<Directory> queue;
    for (const auto &root : roots) queue.append({root, 0});
    QElapsedTimer clock; clock.start();
    auto report = [&](bool force = false) { if (callback && (force || clock.elapsed() >= 100)) { callback(result.progress); clock.restart(); } };
#ifdef Q_OS_WIN
    const QString executable = "java.exe";
#else
    const QString executable = "java";
#endif
    auto examine = [&](const QFileInfo &file) {
        if (!file.isFile() || file.fileName() != executable || !file.isExecutable()) return;
        const auto path = file.canonicalFilePath(); if (path.isEmpty() || candidates.contains(path)) return;
        candidates.insert(path); ++result.progress.candidates; result.progress.path = path; report(true);
        const auto java = probeJava(path, cancelled);
        if (java.major) { result.runtimes.append(java); result.progress.found = result.runtimes.size(); report(true); }
    };
    while (!queue.isEmpty() && !cancelled->load()) {
        const auto entry = queue.takeLast(); const QFileInfo info(entry.path);
        const auto canonical = info.canonicalFilePath(); if (canonical.isEmpty() || visited.contains(canonical)) continue;
        if (info.isFile()) { examine(info); continue; }
        if (!options.deep && result.progress.directories >= options.directoryBudget) { result.limited = true; break; }
        visited.insert(canonical); result.progress.path = canonical; ++result.progress.directories;
        if (!info.isReadable()) { ++result.progress.skipped; report(); continue; }
        const QDir dir(canonical);
        for (const auto &suffix : {QString(), QStringLiteral("bin/"), QStringLiteral("Contents/Home/bin/")}) examine(QFileInfo(dir.filePath(suffix + executable)));
        if (!options.deep && entry.depth >= options.maxDepth) continue;
        report();
        QDirIterator iterator(canonical, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
        while (iterator.hasNext() && !cancelled->load()) {
            iterator.next(); const auto child = iterator.fileInfo(); report();
            if (child.isDir() && !child.isSymLink()) {
                if (child.fileName().startsWith(".linjian-java-stage-") || child.fileName() == ".linjian-java-cache") continue;
                const auto path = child.absoluteFilePath();
                if (options.deep && excludedMounts.contains(path)) { ++result.progress.skipped; continue; }
#ifndef Q_OS_WIN
                // Virtual filesystems and macOS's duplicate data-volume namespace.
                if (path == "/dev" || path == "/proc" || path == "/sys" || path == "/run" || path == "/Network" || path == "/System/Volumes") { ++result.progress.skipped; continue; }
#endif
                queue.append({path, entry.depth + 1});
            } else if (options.deep) examine(child);
        }
        report();
    }
    result.cancelled = cancelled->load(); report(true); return result;
}

QList<JavaRuntime> discoverJava(const QStringList &roots, const JavaCancel &cancelled) {
    return scanJavaRuntimes(roots, cancelled).runtimes;
}
QJsonArray javaCatalogJson(const QList<JavaRuntime> &runtimes) {
    QJsonArray array;
    for (const auto &j : runtimes) array.append(QJsonObject{{"path", j.executable}, {"version", j.version}, {"runtimeVersion", j.runtimeVersion}, {"vendor", j.vendor}, {"arch", j.architecture}, {"major", j.major}, {"vm", j.vmName}, {"vendorVersion", j.vendorVersion}});
    return array;
}
QList<JavaRuntime> javaCatalogFromJson(const QJsonArray &catalog) {
    QList<JavaRuntime> runtimes; QSet<QString> seen;
    for (const auto &v : catalog) {
        const auto o = v.toObject(); JavaRuntime j;
        j.executable = o.value("path").toString(); j.version = o.value("version").toString(); j.runtimeVersion = o.value("runtimeVersion").toString();
        j.vendor = o.value("vendor").toString(); j.architecture = o.value("arch").toString(); j.major = o.value("major").toInt(); j.vmName = o.value("vm").toString(); j.vendorVersion = o.value("vendorVersion").toString();
        if (j.executable.isEmpty() || j.major <= 0 || seen.contains(j.executable)) continue;
        seen.insert(j.executable); runtimes.append(j);
    }
    return runtimes;
}
JavaLegalPaths javaLegalPaths(const QString &executable) {
    JavaLegalPaths result;
    const auto canonical = QFileInfo(executable).canonicalFilePath();
    if (canonical.isEmpty()) return result;
    QDir bin(QFileInfo(canonical).path());
    if (bin.dirName() != "bin" || !bin.cdUp()) return result;
    result.home = bin.absolutePath(); QStringList homes{result.home};
    if (bin.dirName() == "jre" && bin.cdUp()) homes.append(bin.absolutePath());
    for (const auto &home : homes) {
        for (const auto &name : QStringList{"LICENSE", "LICENSE.txt", "ASSEMBLY_EXCEPTION", "NOTICE", "NOTICE.txt", "COPYRIGHT", "legal/java.base/LICENSE"}) {
            const auto path = QDir(home).filePath(name);
            if (QFileInfo(path).isFile() && !result.licenses.contains(QFileInfo(path).canonicalFilePath())) result.licenses.append(QFileInfo(path).canonicalFilePath());
        }
        const auto legal = QDir(home).filePath("legal");
        if (result.legalDirectory.isEmpty() && QFileInfo(legal).isDir()) result.legalDirectory = QFileInfo(legal).canonicalFilePath();
    }
    return result;
}

}
