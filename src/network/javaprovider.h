// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "gameinstaller.h"
#include "core/java.h"
namespace mc {
struct JavaPackage {
    QString provider, id, label, version, distributionVersion, os, arch, sha256, archiveName;
    QString packageType = "JRE";
    QUrl url, checksumUrl, sourcePage;
    bool archived = false;
    int major = 0, build = 0;
    qint64 size = 0;
    QJsonObject manifest;
};
struct JavaPackageList {
    QList<JavaPackage> packages;
    QString error;
    QStringList availableArchitectures;
    bool hasMore = false, architectureListComplete = false;
};
struct JavaInstallResult { JavaRuntime runtime; QString error, warning; };
struct JavaLicenseInfo { QString name, scope; QUrl licenseUrl, buildUrl, website; };
JavaLicenseInfo javaPackageLicense(const JavaPackage &package);
JavaPackageList fetchJavaPackages(const QString &provider, int major, const GamePlatform &platform,
    const JavaCancel &cancelled, int page = 0, QNetworkAccessManager *network = nullptr, bool discoverArchitectures = false);
QList<JavaPackage> parseTemurinPackages(const QJsonArray &releases, int major, const GamePlatform &platform);
QList<JavaPackage> parseZuluPackages(const QJsonArray &packages, int major, const GamePlatform &platform);
QList<JavaPackage> parseMicrosoftPackages(const QString &html, int major, const GamePlatform &platform);
QList<JavaPackage> parseOracleOpenJdkPackages(const QString &html, int major, const GamePlatform &platform,
    const QUrl &sourcePage = QUrl("https://jdk.java.net/archive/"));
JavaInstallResult installJavaPackage(JavaPackage package, const QString &managedRoot, const JavaCancel &cancelled,
    const InstallProgress &progress, QNetworkAccessManager *network = nullptr);
}
