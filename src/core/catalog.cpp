// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "catalog.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QMap>
#include <algorithm>

namespace mc {
QMap<QString,QString> gameLoaderComponents(const QJsonObject &json) {
    QMap<QString,QString> found;const auto game=json.value("_baseId").toString(json.value("inheritsFrom").toString(json.value("id").toString()));
    for(const auto &entry:json.value("libraries").toArray()){
        const auto name=entry.toObject().value("name").toString();QString loader;
        if(name.startsWith("net.fabricmc:fabric-loader:"))loader="fabric";
        else if(name.startsWith("org.quiltmc:quilt-loader:"))loader="quilt";
        else if(name.startsWith("net.neoforged:neoforge:")||name.startsWith("net.neoforged:forge:"))loader="neoforge";
        else if(name.startsWith("net.minecraftforge:forge:")||name.startsWith("net.minecraftforge:fmlloader:"))loader="forge";
        if(!loader.isEmpty()){auto version=name.section(':',2,2);if((loader=="forge"||loader=="neoforge")&&version.startsWith(game+"-"))version.remove(0,game.size()+1);found.insert(loader,version);}
    }
    const auto args=json.value("arguments").toObject().value("game").toArray();
    for(int n=0;n+1<args.size();++n){if(args[n]=="--fml.neoForgeVersion")found.insert("neoforge",args[n+1].toString());if(args[n]=="--fml.forgeVersion")found.insert("forge",args[n+1].toString());}
    return found;
}

QString Instance::loaderName() const {
    if(!loaderProblem.isEmpty())return QCoreApplication::translate("M2CL", "加载器冲突");
    if (loader == "vanilla") return QCoreApplication::translate("M2CL", "原版");
    if (loader == "neoforge") return QStringLiteral("NeoForge");
    QString text = loader;
    if (!text.isEmpty()) text[0] = text.at(0).toUpper();
    return text;
}

int requiredJavaMajor(const QString &version) {
    const auto match = QRegularExpression(QStringLiteral("^1\\.(\\d+)(?:\\.(\\d+))?$")).match(version);
    if (!match.hasMatch()) return 0;
    const int minor = match.captured(1).toInt();
    const int patch = match.captured(2).toInt();
    if (minor > 20 || (minor == 20 && patch >= 5)) return 21;
    if (minor >= 18) return 17;
    if (minor == 17) return 16;
    return 8;
}

static QJsonObject manifest(const QString &root, const QString &id, QSet<QString> chain, QString *error) {
    if (id.isEmpty() || id == "." || id == ".." || id.contains('/') || id.contains('\\') || id.contains(':')) {
        *error = QCoreApplication::translate("M2CL", "无效的继承版本名");
        return {};
    }
    if (chain.contains(id) || chain.size() >= 16) {
        *error = QCoreApplication::translate("M2CL", "版本继承存在循环或层级过深");
        return {};
    }
    chain.insert(id);
    QFile file(QDir(root).filePath(QStringLiteral("versions/%1/%1.json").arg(id)));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024) {
        *error = QCoreApplication::translate("M2CL", "无法读取版本文件 %1").arg(id);
        return {};
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QCoreApplication::translate("M2CL", "版本文件格式无效 %1").arg(id);
        return {};
    }
    auto own = document.object();
    const auto parentId = own.value("inheritsFrom").toString();
    if (parentId.isEmpty()) {
        if(!own.contains("_baseId"))own.insert("_baseId", own.value("id").toString(id));
        return own;
    }
    auto parent = manifest(root, parentId, chain, error);
    if (!error->isEmpty()) return {};
    auto libraries = parent.value("libraries").toArray();
    for (const auto &entry : own.value("libraries").toArray()) libraries.append(entry);
    for (auto it = own.constBegin(); it != own.constEnd(); ++it) parent.insert(it.key(), it.value());
    parent.insert("libraries", libraries);
    return parent;
}

CatalogResult scanInstances(const QStringList &roots, const QJsonObject &isolationOverrides) {
    CatalogResult result;
    QSet<QString> seen;
    for (const auto &rawRoot : roots) {
        const QString root = QFileInfo(rawRoot).canonicalFilePath();
        if (root.isEmpty()) {
            result.warnings.append(QCoreApplication::translate("M2CL", "目录不存在：%1").arg(rawRoot));
            continue;
        }
        const QDir versions(QDir(root).filePath("versions"));
        for (const auto &folder : versions.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name)) {
            const QString path = folder.canonicalFilePath();
            if (seen.contains(path)) continue;
            seen.insert(path);
            QString error;
            const auto json = manifest(root, folder.fileName(), {}, &error);
            if (!error.isEmpty()) {
                result.warnings.append(folder.fileName() + QStringLiteral("：") + error);
                continue;
            }
            Instance instance;
            instance.id = folder.fileName();
            instance.path = path;
            instance.root = root;
            instance.version = json.value("_baseId").toString(instance.id);
            instance.loader = "vanilla";
            const auto of=json.value("_m2clOptifine").toObject();instance.optifineFile=of.value("modFile").toString();instance.optifineVersion=of.value("version").toString();instance.optifineSha512=of.value("sha512").toString();instance.optifineForgeVersion=of.value("forge").toString();
            const auto primaryLoaders=gameLoaderComponents(json);
            if(primaryLoaders.size()==1){instance.loader=primaryLoaders.firstKey();instance.loaderVersion=primaryLoaders.first();}
            else if(primaryLoaders.size()>1){instance.loader="conflict";instance.loaderProblem=QCoreApplication::translate("M2CL", "同时检测到多个主加载器：%1。请为不同加载器使用独立实例。").arg(primaryLoaders.keys().join(" / "));result.warnings.append(instance.id+QStringLiteral("：")+instance.loaderProblem);}
            const auto isoDir = QDir(path);
            instance.isolated = isoDir.exists("mods") || isoDir.exists("saves") || isoDir.exists("options.txt");
            if (!instance.isolated && !QDir(root).exists("mods") && instance.supportsMods()) instance.isolated = true;
            if (isolationOverrides.value(path).isBool()) instance.isolated = isolationOverrides.value(path).toBool();
            instance.gameDirectory = instance.isolated ? path : root;
            instance.javaMajor = json.value("javaVersion").toObject().value("majorVersion").toInt();
            if (!instance.javaMajor) instance.javaMajor = json.value("java_version").toInt();
            if (!instance.javaMajor) instance.javaMajor = requiredJavaMajor(instance.version);
            result.instances.append(instance);
        }
    }
    std::sort(result.instances.begin(), result.instances.end(), [](const Instance &a, const Instance &b) {
        return QString::localeAwareCompare(a.id, b.id) < 0;
    });
    return result;
}
}
