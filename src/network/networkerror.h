// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QString>
class QNetworkReply;
namespace mc {
// Empty on success. HTTP status and transport errors remain distinct.
QString networkReplyError(const QNetworkReply *reply, bool timedOut=false);
QString networkErrorSummary(const QString &details);
}
