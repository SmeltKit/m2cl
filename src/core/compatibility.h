// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "mods.h"
#include <QDateTime>
#include <QJsonArray>
namespace mc {
struct CompatibilityList { QJsonArray rules; QDateTime fetched; bool cached=false; };
QString compatibilitySource();
bool compatibilityUpdateDue(const Instance &instance, const QDateTime &now = QDateTime::currentDateTimeUtc());
void recordCompatibilityAttempt(const Instance &instance, const QDateTime &now = QDateTime::currentDateTimeUtc());
CompatibilityList compatibilityList(const Instance &instance);
bool saveCompatibilityList(const Instance &instance,const QByteArray &bytes,QString *error);
QStringList knownCompatibilityWarnings(const Instance &instance,const LocalMod &mod);
}
