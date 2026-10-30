// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "accountmanager.h"
#include <QJsonDocument>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
namespace mc {
AccountManager::AccountManager(const QString &directory,const QString &legacyPlayer,QObject *parent,std::shared_ptr<CredentialVault> vault,QNetworkAccessManager *network,int timeoutMs)
    :QObject(parent),m_store(directory),m_vault(vault?vault:std::make_shared<SystemCredentialVault>(m_store.credentialNamespace())),m_auth(this,network,timeoutMs){
    auto list=m_store.load();m_accounts=list.accounts;m_selected=list.selected;m_loadError=list.error;
    if(m_accounts.isEmpty()&&m_loadError.isEmpty()){m_accounts.append(offlineAccount(validPlayerName(legacyPlayer)?legacyPlayer:QString("Player")));m_selected=m_accounts.first().id;m_store.save(m_accounts,m_selected,&m_loadError);}
    connect(&m_auth,&MicrosoftAuth::browserRequested,this,&AccountManager::browserRequested);connect(&m_auth,&MicrosoftAuth::progress,this,&AccountManager::progress);
    connect(&m_auth,&MicrosoftAuth::failed,this,&AccountManager::error);
    connect(&m_auth,&MicrosoftAuth::succeeded,this,[this](const AuthSession &session){
        if(!m_expectedId.isEmpty()&&session.account.id!=m_expectedId){error({QCoreApplication::translate("M2CL", "登录的角色与所选账号不同"),QCoreApplication::translate("M2CL", "请使用原账号登录；如需使用其他角色，请选择添加账号。"),"profile_mismatch",false});return;}
        m_pending=session;if(!m_remember){continueWithoutRemembering();return;}if(session.refreshToken.isEmpty()){error({QCoreApplication::translate("M2CL", "微软没有返回可保存的刷新凭据"),QCoreApplication::translate("M2CL", "可以仅在本次会话中使用此账号。"),"credential_store_failed",false});return;}
        emit progress(QCoreApplication::translate("M2CL", "正在安全保存登录凭据"));m_jobKind=Job::Write;m_jobId=session.account.id;const auto value=QString::fromUtf8(QJsonDocument(QJsonObject{{"clientId",session.account.clientId},{"refreshToken",session.refreshToken}}).toJson(QJsonDocument::Compact));const auto vault=m_vault;const auto key=m_jobId;
        m_jobPending=true;m_job.setFuture(QtConcurrent::run([vault,key,value]{return CredentialResult{{},vault->write(key,value)};}));emit busyChanged();
    });
    connect(&m_job,&QFutureWatcher<CredentialResult>::finished,this,[this]{m_jobPending=false;const auto result=m_job.result();
        if(!result.error.isEmpty()){error({m_jobKind==Job::Read?QCoreApplication::translate("M2CL", "需要重新登录"):m_jobKind==Job::Write?QCoreApplication::translate("M2CL", "无法记住登录"):QCoreApplication::translate("M2CL", "未能移除账号凭据"),result.error,m_jobKind==Job::Write?"credential_store_failed":"credential_unavailable",m_jobKind==Job::Read});return;}
        if(m_jobKind==Job::Write){commitSession(true);return;}
        if(m_jobKind==Job::Forget){commitSession(false);return;}
        if(m_jobKind==Job::Remove){auto records=m_accounts;for(int n=records.size()-1;n>=0;--n)if(records.at(n).id==m_jobId)records.removeAt(n);const auto selected=m_selected==m_jobId?(records.isEmpty()?QString():records.first().id):m_selected;QString problem;if(!commit(records,selected,&problem)){error({QCoreApplication::translate("M2CL", "账号列表未能保存"),problem,"account_store_failed",false});return;}m_sessions.remove(m_jobId);setBusy(false);return;}
        const auto a=find(m_jobId);const auto object=QJsonDocument::fromJson(result.value.toUtf8()).object();const auto token=object.value("refreshToken").toString();
        if(a.id.isEmpty()||object.value("clientId").toString()!=a.clientId||token.isEmpty()){error({QCoreApplication::translate("M2CL", "需要重新登录"),QCoreApplication::translate("M2CL", "已保存的凭据与应用配置不匹配，请重新登录。"),"credential_mismatch",true});return;}m_auth.refresh(a.clientId,token);
    });
}
AccountManager::~AccountManager(){m_auth.cancel();m_job.waitForFinished();}
Account AccountManager::find(const QString &id) const{for(const auto &a:m_accounts)if(a.id==id)return a;return {};}
Account AccountManager::current() const{return find(m_selected);}
QString AccountManager::state(const Account &a) const{if(!a.microsoft)return QCoreApplication::translate("M2CL", "离线角色");const auto session=m_sessions.value(a.id);if(session.identity.expires>QDateTime::currentDateTimeUtc().addSecs(90))return a.remembered?QCoreApplication::translate("M2CL", "已登录"):QCoreApplication::translate("M2CL", "已登录 · 当前会话");if(a.needsLogin||!a.remembered)return QCoreApplication::translate("M2CL", "需要重新登录");return QCoreApplication::translate("M2CL", "启动时刷新登录");}
void AccountManager::setBusy(bool value){m_busy=value;emit busyChanged();}
void AccountManager::error(const AuthFailure &failure){if(failure.reloginRequired&&!m_expectedId.isEmpty()){m_sessions.remove(m_expectedId);auto records=m_accounts;for(auto &a:records)if(a.id==m_expectedId)a.needsLogin=true;QString ignored;commit(records,m_selected,&ignored);}setBusy(false);emit failed(failure);}
bool AccountManager::commit(const QList<Account> &accounts,const QString &selected,QString *error){if(!m_loadError.isEmpty()){if(error)*error=m_loadError;return false;}if(!m_store.save(accounts,selected,error))return false;m_accounts=accounts;m_selected=selected;emit changed();return true;}
QString AccountManager::addOffline(const QString &name){if(m_busy)return QCoreApplication::translate("M2CL", "请先完成当前账号操作");if(!validPlayerName(name))return QCoreApplication::translate("M2CL", "玩家名需要 1–16 位英文字母、数字或下划线");const auto a=offlineAccount(name);if(!find(a.id).id.isEmpty())return select(a.id);auto records=m_accounts;records.append(a);QString error;commit(records,a.id,&error);return error;}
QString AccountManager::select(const QString &id){if(m_busy)return QCoreApplication::translate("M2CL", "请先完成当前账号操作");if(find(id).id.isEmpty())return QCoreApplication::translate("M2CL", "请选择有效账号");QString error;commit(m_accounts,id,&error);return error;}
void AccountManager::remove(const QString &id){if(m_busy)return;const auto a=find(id);if(a.id.isEmpty())return;if(!m_loadError.isEmpty()){error({QCoreApplication::translate("M2CL", "账号列表不可修改"),m_loadError,"account_store_failed",false});return;}setBusy(true);m_pending={};m_jobId=id;m_jobKind=Job::Remove;emit progress(QCoreApplication::translate("M2CL", "正在移除账号与已保存凭据"));const auto vault=m_vault;const bool remembered=a.microsoft&&a.remembered;m_jobPending=true;m_job.setFuture(QtConcurrent::run([vault,id,remembered]{return CredentialResult{{},remembered?vault->remove(id):QString()};}));emit busyChanged();}
void AccountManager::login(const QString &clientId,bool remember,const QString &expectedId){if(m_busy)return;m_pending={};m_expectedId=expectedId;m_remember=remember;setBusy(true);m_auth.login(clientId);}
void AccountManager::ensureSession(const QString &id){
    if(m_busy)return;const auto a=find(id);if(a.id.isEmpty()){error({QCoreApplication::translate("M2CL", "请选择账号"),{},"no_account",false});return;}
    if(!a.microsoft){AuthSession session;session.account=a;session.identity={a.name,a.uuid,"0",{},{},{},false};QTimer::singleShot(0,this,[this,session]{emit sessionReady(session);});return;}
    const auto cached=m_sessions.value(id);if(cached.identity.expires>QDateTime::currentDateTimeUtc().addSecs(90)){QTimer::singleShot(0,this,[this,cached]{emit sessionReady(cached);});return;}
    m_pending={};m_expectedId=id;m_remember=a.remembered;setBusy(true);
    if(!cached.refreshToken.isEmpty()){m_auth.refresh(a.clientId,cached.refreshToken);return;}
    if(a.needsLogin||!a.remembered){error({QCoreApplication::translate("M2CL", "此账号需要重新登录"),QCoreApplication::translate("M2CL", "请点击重新登录，在微软官方网页完成验证。"),"login_required",true});return;}
    m_jobId=id;m_jobKind=Job::Read;emit progress(QCoreApplication::translate("M2CL", "正在读取安全保存的登录凭据"));const auto vault=m_vault;m_jobPending=true;m_job.setFuture(QtConcurrent::run([vault,id]{return vault->read(id);}));emit busyChanged();
}
bool AccountManager::cancel(){if(m_jobPending)return false;m_auth.cancel();m_pending={};setBusy(false);return true;}
void AccountManager::commitSession(bool remembered){auto session=m_pending;if(session.account.id.isEmpty())return;session.account.remembered=remembered;session.account.needsLogin=false;auto records=m_accounts;bool found=false;for(auto &a:records)if(a.id==session.account.id){a=session.account;found=true;}if(!found)records.append(session.account);QString problem;if(!commit(records,session.account.id,&problem)){error({QCoreApplication::translate("M2CL", "账号列表未能保存"),problem,"account_store_failed",false});return;}m_sessions.insert(session.account.id,session);m_pending={};setBusy(false);emit sessionReady(session);}
void AccountManager::continueWithoutRemembering(){if(m_pending.account.id.isEmpty()||m_jobPending)return;if(find(m_pending.account.id).remembered){setBusy(true);m_jobKind=Job::Forget;const auto vault=m_vault;const auto id=m_pending.account.id;m_jobPending=true;m_job.setFuture(QtConcurrent::run([vault,id]{return CredentialResult{{},vault->remove(id)};}));emit busyChanged();}else commitSession(false);}
}
