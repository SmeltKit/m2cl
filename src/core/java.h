// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QList>
#include <QJsonArray>
#include <QStringList>
#include <atomic>
#include <functional>
#include <memory>

namespace mc {
struct JavaRuntime {
    QString executable, version, architecture, vendor;
    int major = 0;
    QString runtimeVersion, vmName, vendorVersion;
};
struct JavaScanOptions { bool deep = false; int maxDepth = 5, directoryBudget = 12000; };
struct JavaScanProgress { QString path; int directories = 0, candidates = 0, found = 0, skipped = 0; };
struct JavaScanResult { QList<JavaRuntime> runtimes; JavaScanProgress progress; bool cancelled = false, limited = false; };
struct JavaLegalPaths { QString home, legalDirectory; QStringList licenses; };
JavaLegalPaths javaLegalPaths(const QString &executable);
using JavaCancel = std::shared_ptr<std::atomic_bool>;
JavaRuntime parseJavaOutput(const QString &output);
JavaRuntime probeJava(const QString &executable, const JavaCancel &cancelled);
QString javaLabel(const JavaRuntime &java);
QJsonObject javaPin(const JavaRuntime &java);
QString normalizeArchitecture(const QString &architecture);
QStringList defaultJavaRoots(const QStringList &gameRoots);
QStringList fullJavaScanRoots();
JavaScanResult scanJavaRuntimes(const QStringList &roots, const JavaCancel &cancelled,
    const JavaScanOptions &options = {}, const std::function<void(const JavaScanProgress &)> &progress = {});
QList<JavaRuntime> discoverJava(const QStringList &roots, const JavaCancel &cancelled);
QJsonArray javaCatalogJson(const QList<JavaRuntime> &runtimes);
QList<JavaRuntime> javaCatalogFromJson(const QJsonArray &catalog);
}
