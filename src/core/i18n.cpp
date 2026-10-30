// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "i18n.h"
#include "qttranslationkeys.h"
#include <QCoreApplication>
#include <QTranslator>

static void initializeTranslations() { Q_INIT_RESOURCE(translations); }

namespace mc {
bool validLanguage(const QString &language) {
    return QStringList{"system", "zh_CN", "zh_TW", "en_US"}.contains(language);
}
QString resolvedLanguage(const QString &setting, const QLocale &system) {
    if (validLanguage(setting) && setting != "system") return setting;
    if (system.language() != QLocale::Chinese) return "en_US";
    if (system.script() == QLocale::TraditionalHanScript || system.country() == QLocale::Taiwan
        || system.country() == QLocale::HongKong || system.country() == QLocale::Macau) return "zh_TW";
    return "zh_CN";
}
QString currentLanguage() {
    const auto value=qApp ? qApp->property("m2clLanguage").toString() : QString();
    return value.isEmpty()?QString("zh_CN"):value;
}
bool installLanguage(QCoreApplication &application, const QString &setting) {
    if (!validLanguage(setting)) return false;
    initializeTranslations();
    const auto language = resolvedLanguage(setting);
    auto next = new QTranslator(&application);
    if (!next->load(":/translations/m2cl_" + language + ".qm")) { delete next; return false; }
    // Do not remove the working catalog until the replacement has loaded.
    for (auto translator : application.findChildren<QTranslator *>("m2clTranslator", Qt::FindDirectChildrenOnly)) {
        application.removeTranslator(translator);
        delete translator;
    }
    next->setObjectName("m2clTranslator");
    application.installTranslator(next);
    const auto locale = setting == "system" ? QLocale::system() : QLocale(language);
    QLocale::setDefault(locale);
    application.setProperty("m2clLanguage", language);
    return true;
}
bool diagnosticContains(const QString &text, const char *sourceKeyword) {
    return text.contains(QString::fromUtf8(sourceKeyword), Qt::CaseInsensitive)
        || text.contains(QCoreApplication::translate("M2CL", sourceKeyword), Qt::CaseInsensitive);
}
}
