// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "credentialvault.h"
#include "microsoftauth.h"
#include <QFutureWatcher>
#include <QHash>
#include <memory>
namespace mc {
class AccountManager final:public QObject {
    Q_OBJECT
public:
    AccountManager(const QString &directory,const QString &legacyPlayer,QObject *parent=nullptr,
                   std::shared_ptr<CredentialVault> vault={},QNetworkAccessManager *network=nullptr,int timeoutMs=15000);
    ~AccountManager() override;
    QList<Account> accounts() const{return m_accounts;}
    Account current() const;
    QString state(const Account &account) const;
    QString loadError() const{return m_loadError;}
    QString vaultName() const{return m_vault->name();}
    bool canRemember() const{return m_vault->available();}
    bool busy() const{return m_busy;}
    bool canCancel() const{return !m_jobPending;}
    bool hasPendingSession() const{return !m_pending.account.id.isEmpty();}
    QString addOffline(const QString &name);
    QString select(const QString &id);
    void remove(const QString &id);
    void login(const QString &clientId,bool remember,const QString &expectedId={});
    void ensureSession(const QString &id);
    bool cancel();
    void continueWithoutRemembering();
signals:
    void changed();
    void busyChanged();
    void progress(const QString &message);
    void browserRequested(const QUrl &url);
    void sessionReady(const mc::AuthSession &session);
    void failed(const mc::AuthFailure &failure);
private:
    bool commit(const QList<Account> &accounts,const QString &selected,QString *error);
    void commitSession(bool remembered);
    void error(const AuthFailure &failure);
    Account find(const QString &id) const;
    void setBusy(bool busy);
    AccountStore m_store;
    QList<Account> m_accounts;
    QString m_selected,m_loadError,m_expectedId,m_jobId;
    QHash<QString,AuthSession> m_sessions;
    std::shared_ptr<CredentialVault> m_vault;
    MicrosoftAuth m_auth;
    QFutureWatcher<CredentialResult> m_job;
    enum class Job { Read,Write,Remove,Forget } m_jobKind=Job::Read;
    bool m_busy=false,m_remember=false,m_jobPending=false;
    AuthSession m_pending;
};
}
