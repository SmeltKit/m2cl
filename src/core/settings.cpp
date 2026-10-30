// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "settings.h"
#include "i18n.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace mc {
SettingsStore::SettingsStore(QString directory) : m_directory(std::move(directory)) {}

static QStringList strings(const QJsonValue &value) {
    QStringList result;
    for (const auto &entry : value.toArray()) {
        if (entry.isString() && !entry.toString().trimmed().isEmpty())
            result.append(entry.toString());
    }
    result.removeDuplicates();
    return result;
}

Settings SettingsStore::load(QString *error) const {
    Settings result;
    QFile file(QDir(m_directory).filePath(QStringLiteral("settings.json")));
    if (!file.exists()) return result;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return result;
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QCoreApplication::translate("M2CL", "设置文件无法读取：%1").arg(parse.errorString());
        return result;
    }
    const auto root = document.object();
    result.gameRoots = strings(root.value(QStringLiteral("gameRoots")));
    result.javaFolders = strings(root.value(QStringLiteral("javaFolders")));
    result.selectedInstance = root.value(QStringLiteral("selectedInstance")).toString();
    result.modInstallTarget = root.value("modInstallTarget").toString();
    result.legacyInstanceJava = root.value("legacyInstanceJava").toObject();
    const auto oldChoices = root.value("instanceJava").toObject();
    for (auto it = oldChoices.begin(); it != oldChoices.end(); ++it) if (!result.legacyInstanceJava.contains(it.key())) result.legacyInstanceJava.insert(it.key(), it.value());
    result.javaCatalog = root.value("javaCatalog").toArray();
    result.microsoftClientId=root.value("microsoftClientId").toString();
    result.offlinePlayer = root.value("offlinePlayer").toString("Player");
    result.memoryMiB = qBound(512, root.value("memoryMiB").toInt(2048), 32768);
    const auto appearance = root.value(QStringLiteral("appearance")).toString();
    const auto language = root.value("language").toString();
    if (validLanguage(language)) result.language = language;
    if (QStringList{"system", "light", "dark"}.contains(appearance)) result.appearance = appearance;
    const auto theme = root.value(QStringLiteral("theme")).toString();
    if (QStringList{"forest", "blue", "sand", "violet"}.contains(theme)) result.theme = theme;
    const auto source = root.value("downloadSource").toString();
    if (QStringList{"auto", "official", "bmclapi"}.contains(source)) result.downloadSource = source;
    result.exitAfterLaunch = root.value(QStringLiteral("exitAfterLaunch")).toBool(true);
    result.isolationOverrides = root.value(QStringLiteral("isolationOverrides")).toObject();
    result.favoriteInstances = strings(root.value(QStringLiteral("favoriteInstances")));
    result.instanceNames = root.value(QStringLiteral("instanceNames")).toObject();
    result.coverOverrides = root.value(QStringLiteral("coverOverrides")).toObject();
    return result;
}

bool SettingsStore::save(const Settings &settings, QString *error) const {
    if (!QDir().mkpath(m_directory)) {
        if (error) *error = QCoreApplication::translate("M2CL", "无法创建设置目录");
        return false;
    }
    QSaveFile file(QDir(m_directory).filePath(QStringLiteral("settings.json")));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonObject root{
        {"schemaVersion", 3}, {"gameRoots", QJsonArray::fromStringList(settings.gameRoots)},
        {"selectedInstance", settings.selectedInstance}, {"modInstallTarget",settings.modInstallTarget},
        {"javaCatalog", settings.javaCatalog},
        {"microsoftClientId",settings.microsoftClientId}, {"offlinePlayer", settings.offlinePlayer}, {"memoryMiB", settings.memoryMiB},
        {"javaFolders", QJsonArray::fromStringList(settings.javaFolders)},
        {"downloadSource", settings.downloadSource}, {"appearance", settings.appearance}, {"language", settings.language}, {"theme", settings.theme},
        {"exitAfterLaunch", settings.exitAfterLaunch}, {"isolationOverrides", settings.isolationOverrides},
        {"favoriteInstances", QJsonArray::fromStringList(settings.favoriteInstances)},
        {"instanceNames", settings.instanceNames}, {"coverOverrides", settings.coverOverrides}
    };
    if (!settings.legacyInstanceJava.isEmpty()) root.insert("legacyInstanceJava", settings.legacyInstanceJava);
    const auto bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QString instanceConfigPath(const QString &instancePath) {
    return instancePath.isEmpty() ? QString() : QDir(instancePath).filePath(".m2cl-instance.json");
}

static QJsonObject readInstanceConfig(const QString &instancePath, QString *error) {
    if (error) error->clear();
    const auto path = instanceConfigPath(instancePath);
    if (path.isEmpty()) { if (error) *error = QCoreApplication::translate("M2CL", "实例路径为空"); return {}; }
    QFile file(path);
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
        if (error) *error = QCoreApplication::translate("M2CL", "无法读取实例配置：%1").arg(path);
        return {};
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QCoreApplication::translate("M2CL", "实例配置格式无效：%1").arg(path);
        return {};
    }
    return document.object();
}

QJsonObject loadInstanceJavaChoice(const QString &instancePath, QString *error) {
    QString problem;
    const auto config = readInstanceConfig(instancePath, &problem);
    const auto value = config.value("java");
    auto choice = value.toObject();
    const auto mode = choice.value("mode").toString("auto");
    if (problem.isEmpty() && ((!value.isUndefined() && !value.isObject()) || (mode != "auto" && mode != "fixed"))) problem = QCoreApplication::translate("M2CL", "实例 Java 设置无效：%1").arg(instanceConfigPath(instancePath));
    if (error) *error = problem;
    if (!problem.isEmpty() || mode != "fixed") return {{"mode", "auto"}};
    return choice;
}

static bool saveRuntimeSettings(const QString &instancePath,const QJsonObject &choice,const LaunchOptions *launch,QString *error) {
    if (error) error->clear();
    const auto mode = choice.value("mode").toString();
    if (instancePath.isEmpty() || !QFileInfo(instancePath).isDir() || (mode != "auto" && mode != "fixed") || (mode == "fixed" && choice.value("path").toString().isEmpty())) {
        if (error) *error = QCoreApplication::translate("M2CL", "实例目录或 Java 选择无效");
        return false;
    }
    QString problem;
    auto config = readInstanceConfig(instancePath, &problem);
    if (!problem.isEmpty()) { if (error) *error = problem; return false; }
    config.insert("schemaVersion", 1);
    config.insert("java", mode == "auto" ? QJsonObject{{"mode", "auto"}} : choice);
    if(launch){
        const auto validation=validateLaunchOptions(*launch);if(!validation.isEmpty()){if(error)*error=validation;return false;}
        config.insert("launch",QJsonObject{{"autoMemory",launch->autoMemory},{"maxMemoryMiB",launch->maxMemoryMiB},{"minMemoryMiB",launch->minMemoryMiB},{"customResolution",launch->customResolution},{"width",launch->width},{"height",launch->height},{"fullscreen",launch->fullscreen},{"jvmArguments",launch->jvmArguments},{"gameArguments",launch->gameArguments}});
    }
    QSaveFile file(instanceConfigPath(instancePath));
    const auto bytes = QJsonDocument(config).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = QCoreApplication::translate("M2CL", "无法保存实例配置：%1").arg(file.errorString());
        return false;
    }
    return true;
}
bool saveInstanceJavaChoice(const QString &instancePath,const QJsonObject &choice,QString *error){return saveRuntimeSettings(instancePath,choice,nullptr,error);}
bool saveInstanceRuntimeSettings(const QString &instancePath,const QJsonObject &choice,const LaunchOptions &launch,QString *error){return saveRuntimeSettings(instancePath,choice,&launch,error);}

LaunchOptions loadInstanceLaunchOptions(const QString &instancePath,int defaultMemoryMiB,QString *error){
    LaunchOptions result;result.maxMemoryMiB=qBound(512,defaultMemoryMiB,32768);
    QString problem;const auto config=readInstanceConfig(instancePath,&problem);const auto value=config.value("launch");
    if(problem.isEmpty()&&!value.isUndefined()&&!value.isObject())problem=QCoreApplication::translate("M2CL", "实例启动设置格式无效");
    const auto object=value.toObject();
    auto number=[&](const QString &key,int fallback){const auto field=object.value(key);if(field.isUndefined())return fallback;if(!field.isDouble()||field.toDouble()!=field.toInt(-1))problem=QCoreApplication::translate("M2CL", "实例启动设置中的 %1 必须是整数").arg(key);return field.toInt(fallback);};
    auto boolean=[&](const QString &key){const auto field=object.value(key);if(!field.isUndefined()&&!field.isBool())problem=QCoreApplication::translate("M2CL", "实例启动设置中的 %1 必须是开关值").arg(key);return field.toBool();};
    auto text=[&](const QString &key){const auto field=object.value(key);if(!field.isUndefined()&&!field.isString())problem=QCoreApplication::translate("M2CL", "实例启动设置中的 %1 必须是文本").arg(key);return field.toString();};
    // Existing launch objects predate auto mode and represent explicit manual settings.
    result.autoMemory=value.isUndefined()?true:object.contains("autoMemory")?boolean("autoMemory"):false;
    result.maxMemoryMiB=number("maxMemoryMiB",result.maxMemoryMiB);result.minMemoryMiB=number("minMemoryMiB",0);result.width=number("width",854);result.height=number("height",480);
    result.customResolution=boolean("customResolution");result.fullscreen=boolean("fullscreen");result.jvmArguments=text("jvmArguments");result.gameArguments=text("gameArguments");
    if(problem.isEmpty())problem=validateLaunchOptions(result);if(error)*error=problem;return result;
}

QStringList migrateInstanceJavaChoices(Settings &settings, const QStringList &instancePaths) {
    QStringList errors;
    for (const auto &path : instancePaths) {
        if (!settings.legacyInstanceJava.contains(path)) continue;
        const auto choice = settings.legacyInstanceJava.value(path).toObject();
        const auto mode = choice.value("mode").toString();
        QString problem;
        const auto config = readInstanceConfig(path, &problem);
        if (!problem.isEmpty()) { errors.append(problem); continue; }
        if (config.contains("java") || mode == "inherit" || choice.isEmpty()) {
            settings.legacyInstanceJava.remove(path);
            continue;
        }
        QString error;
        if (saveInstanceJavaChoice(path, choice, &error)) settings.legacyInstanceJava.remove(path);
        else errors.append(error);
    }
    return errors;
}

QString defaultGameRoot() {
#ifdef Q_OS_WIN
    return QDir(qEnvironmentVariable("APPDATA", QDir::homePath())).filePath(".minecraft");
#elif defined(Q_OS_MACOS)
    return QDir::home().filePath("Library/Application Support/minecraft");
#else
    return QDir::home().filePath(".minecraft");
#endif
}
}
