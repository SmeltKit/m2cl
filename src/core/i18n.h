// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QLocale>
#include <QStringList>
class QCoreApplication;

namespace mc {
bool validLanguage(const QString &language);
QString resolvedLanguage(const QString &setting, const QLocale &system = QLocale::system());
QString currentLanguage();
// Install catalogs before constructing the UI. Changing settings takes effect on restart.
bool installLanguage(QCoreApplication &application, const QString &setting);
// Transitional display-only categorization for legacy text diagnostics. Never use for authorization.
bool diagnosticContains(const QString &text, const char *sourceKeyword);
}
