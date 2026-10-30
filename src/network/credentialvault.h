// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QString>
namespace mc {
struct CredentialResult { QString value,error; };
class CredentialVault {
public:
    virtual ~CredentialVault()=default;
    virtual QString name() const=0;
    virtual bool available() const=0;
    virtual CredentialResult read(const QString &key)=0;
    virtual QString write(const QString &key,const QString &secret)=0;
    virtual QString remove(const QString &key)=0;
};
class SystemCredentialVault final:public CredentialVault {
public:
    explicit SystemCredentialVault(QString service):m_service(std::move(service)){}
    QString name() const override;
    bool available() const override;
    CredentialResult read(const QString &key) override;
    QString write(const QString &key,const QString &secret) override;
    QString remove(const QString &key) override;
private:
    QString m_service;
    CredentialResult operation(const QString &key,const QString &secret,int kind);
};
}
