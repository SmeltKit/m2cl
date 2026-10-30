// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>

namespace mc {
struct Account {
    QString id, name, uuid, clientId;
    bool microsoft=false, remembered=false, needsLogin=false;
    QUrl skinUrl;
    QJsonObject offlineSkin;
};
struct GameIdentity {
    QString name, uuid, accessToken, xuid, clientId;
    QDateTime expires;
    bool microsoft=false;
};
struct AuthSession {
    Account account;
    GameIdentity identity;
    QString refreshToken;
};
struct AccountList { QList<Account> accounts;QString selected;QString error; };
bool validPlayerName(const QString &name);
QString accountProblem(const Account &account);
Account offlineAccount(const QString &name);
QJsonObject accountJson(const Account &account);
class AccountStore {
public:
    explicit AccountStore(QString directory):m_directory(std::move(directory)){}
    AccountList load() const;
    bool save(const QList<Account> &accounts,const QString &selected,QString *error=nullptr) const;
    QString credentialNamespace() const;
private:
    QString m_directory;
};
// Preserve enough trailing input that a token split across output chunks cannot leak.
class SecretRedactor {
public:
    explicit SecretRedactor(const QStringList &secrets={});
    QByteArray append(const QByteArray &bytes,bool finish=false);
private:
    QList<QByteArray> m_secrets;
    QByteArray m_pending;
    int m_longest=1;
};
}
Q_DECLARE_METATYPE(mc::Account)
Q_DECLARE_METATYPE(mc::AuthSession)
