// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "core/i18n.h"
#include "networkerror.h"
#include <QMetaEnum>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStringList>

namespace mc {
namespace {
QString httpSummary(int status) {
    QString reason;
    switch(status){
    case 400:reason=QCoreApplication::translate("M2CL", "请求无效");break;
    case 401:reason=QCoreApplication::translate("M2CL", "需要身份验证");break;
    case 403:reason=QCoreApplication::translate("M2CL", "访问被拒绝");break;
    case 404:reason=QCoreApplication::translate("M2CL", "资源不存在");break;
    case 408:reason=QCoreApplication::translate("M2CL", "服务器等待请求超时");break;
    case 429:reason=QCoreApplication::translate("M2CL", "请求过于频繁，请稍后重试");break;
    case 500:reason=QCoreApplication::translate("M2CL", "服务器内部错误");break;
    case 502:reason=QCoreApplication::translate("M2CL", "网关错误");break;
    case 503:reason=QCoreApplication::translate("M2CL", "服务暂时不可用");break;
    case 504:reason=QCoreApplication::translate("M2CL", "网关超时");break;
    default:reason=QCoreApplication::translate("M2CL", "服务器返回错误");break;
    }
    return QStringLiteral("HTTP %1 · %2").arg(status).arg(reason);
}
}
QString networkReplyError(const QNetworkReply *reply,bool timedOut) {
    const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto code=reply->error();QString problem;
    if(status>=400)problem=httpSummary(status);
    else if(timedOut||reply->property("networkTimedOut").toBool()||code==QNetworkReply::TimeoutError)problem=QCoreApplication::translate("M2CL", "连接超时（Timeout，未收到完整响应）");
    else if(code!=QNetworkReply::NoError){
        const auto name=QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(code);
        problem=QCoreApplication::translate("M2CL", "网络错误 %1（%2） · %3").arg(int(code)).arg(QString::fromLatin1(name?name:"UnknownError"),reply->errorString().left(512));
    }
    if(problem.isEmpty())return {};
    const auto url=reply->url().adjusted(QUrl::RemoveUserInfo|QUrl::RemoveQuery|QUrl::RemoveFragment);
    return problem+QCoreApplication::translate("M2CL", "\n请求：")+url.toDisplayString();
}
QString networkErrorSummary(const QString &details) {
    QStringList parts;
    static const QRegularExpression http(QStringLiteral("\\bHTTP\\s*([45][0-9]{2})\\b"),QRegularExpression::CaseInsensitiveOption);
    auto matches=http.globalMatch(details);
    while(matches.hasNext()){const auto part=httpSummary(matches.next().captured(1).toInt());if(!parts.contains(part))parts.append(part);if(parts.size()==3)break;}
    if(diagnosticContains(details, QT_TRANSLATE_NOOP("M2CL", "连接超时"))||details.contains("Timeout",Qt::CaseInsensitive)||details.contains("timed out",Qt::CaseInsensitive))parts.append(QCoreApplication::translate("M2CL", "连接超时（Timeout）"));
    static const QRegularExpression transport(QStringLiteral("(?:网络错误|網路錯誤|Network error)\\s+([0-9]+)\\s*[（(]([^）)]+)[）)]"),QRegularExpression::CaseInsensitiveOption);
    const auto code=transport.match(details);if(code.hasMatch())parts.append(code.captured(0));
    parts.removeDuplicates();return parts.join(QCoreApplication::translate("M2CL", "；"));
}
}
