// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QList>
#include <QString>
#include <QUrl>
#include <QStringList>

namespace mc {
// Only known provider endpoints are rewritten. Unmapped services keep their original URL.
QList<QUrl> downloadCandidates(const QUrl &official, const QString &source);
QString downloadSourceLabel(const QUrl &url);
// Resolve auto once at task start. The resolved values are internal, never saved as preferences.
QString resolveDownloadSource(const QString &source,const QByteArray &timeZoneId={});
QStringList downloadSourceOrder(const QString &source);
QString automaticDownloadSourceLabel(const QByteArray &timeZoneId={});
}
