// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/accounts.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTcpServer>
#include <QTimer>
#include <functional>
class QNetworkReply;
namespace mc {
struct AuthFailure { QString title,details,code;bool reloginRequired=false; };
class MicrosoftAuth final:public QObject {
    Q_OBJECT
public:
    explicit MicrosoftAuth(QObject *parent=nullptr,QNetworkAccessManager *network=nullptr,int timeoutMs=15000);
    ~MicrosoftAuth() override;
    void login(const QString &clientId);
    void refresh(const QString &clientId,const QString &refreshToken);
    void cancel();
    bool busy() const{return m_busy;}
    static bool validClientId(const QString &clientId);
signals:
    void browserRequested(const QUrl &url);
    void progress(const QString &stage);
    void succeeded(const mc::AuthSession &session);
    void failed(const mc::AuthFailure &failure);
private:
    void reset(const QString &clientId);
    void receiveBrowser();
    void exchange(const QByteArray &body);
    void xbox(const QString &accessToken);
    void xsts(const QString &userToken);
    void minecraft(const QString &token);
    void entitlements();
    void profile();
    void fail(const QString &title,const QString &details,const QString &code={},bool relogin=false);
    void send(const QUrl &url,const QByteArray &body,const QString &bearer,const QString &stage,
              const std::function<void(const QJsonObject &)> &ready,bool form=false);
    QNetworkAccessManager m_ownedNetwork;
    QNetworkAccessManager *m_network;
    QTcpServer m_callback;
    QTimer m_browserDeadline;
    QPointer<QNetworkReply> m_reply;
    quint64 m_generation=0;
    int m_timeout;
    bool m_busy=false;
    QString m_clientId,m_state,m_verifier,m_redirect,m_refreshToken,m_uhs,m_xuid,m_minecraftToken;
    QDateTime m_expires;
};
}
Q_DECLARE_METATYPE(mc::AuthFailure)
