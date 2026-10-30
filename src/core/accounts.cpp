// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "accounts.h"
#include "gameplan.h"
#include "skins.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

namespace mc {
bool validPlayerName(const QString &name){return QRegularExpression("^[A-Za-z0-9_]{1,16}$").match(name).hasMatch();}
Account offlineAccount(const QString &name){Account a;a.name=name;a.uuid=offlineUuid(name);a.id="offline:"+a.uuid;return a;}
QString accountProblem(const Account &a){
    if(!validPlayerName(a.name)||!QRegularExpression("^[0-9a-f]{32}$").match(a.uuid).hasMatch())return QCoreApplication::translate("M2CL", "角色名称或 UUID 无效");
    if(a.id!=(a.microsoft?"microsoft:":"offline:")+a.uuid)return QCoreApplication::translate("M2CL", "账号标识无效");
    if(!a.microsoft&&a.uuid!=offlineUuid(a.name))return QCoreApplication::translate("M2CL", "离线角色 UUID 与名称不匹配");
    if(!a.microsoft&&!skinSettingsProblem(a.offlineSkin).isEmpty())return skinSettingsProblem(a.offlineSkin);
    if(a.microsoft&&!QRegularExpression("^[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}$").match(a.clientId).hasMatch())return QCoreApplication::translate("M2CL", "微软应用 ID 无效");
    return {};
}
QJsonObject accountJson(const Account &a){
    return {{"id",a.id},{"type",a.microsoft?"microsoft":"offline"},{"name",a.name},{"uuid",a.uuid},{"clientId",a.microsoft?a.clientId:QString()},
            {"remembered",a.microsoft&&a.remembered},{"needsLogin",a.needsLogin},{"skinUrl",a.skinUrl.toString()},{"offlineSkin",a.microsoft?QJsonObject():a.offlineSkin}};
}
AccountList AccountStore::load() const {
    AccountList result;QFile f(QDir(m_directory).filePath("accounts.json"));if(!f.exists())return result;
    if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024){result.error=QCoreApplication::translate("M2CL", "无法读取账号列表");return result;}
    QJsonParseError parse;const auto doc=QJsonDocument::fromJson(f.readAll(),&parse);const auto root=doc.object();
    if(parse.error!=QJsonParseError::NoError||!doc.isObject()||root.value("schemaVersion").toInt()!=1||!root.value("accounts").isArray()){result.error=QCoreApplication::translate("M2CL", "账号列表格式无效，原文件未被覆盖");return result;}
    QSet<QString> seen;
    for(const auto &entry:root.value("accounts").toArray()){
        const auto o=entry.toObject();Account a;a.id=o.value("id").toString();a.name=o.value("name").toString();a.uuid=o.value("uuid").toString();a.microsoft=o.value("type")=="microsoft";a.clientId=o.value("clientId").toString();a.remembered=o.value("remembered").toBool();a.needsLogin=o.value("needsLogin").toBool();a.skinUrl=QUrl(o.value("skinUrl").toString());
        a.offlineSkin=o.value("offlineSkin").toObject();
        if(!QStringList{"microsoft","offline"}.contains(o.value("type").toString())||!accountProblem(a).isEmpty()||seen.contains(a.id)){result.error=QCoreApplication::translate("M2CL", "账号列表包含无效或重复记录，原文件未被覆盖");result.accounts.clear();return result;}seen.insert(a.id);result.accounts.append(a);
    }
    result.selected=root.value("selected").toString();if(!result.selected.isEmpty()&&!seen.contains(result.selected)){result.error=QCoreApplication::translate("M2CL", "选中的账号不存在，请在账号管理中重新选择");result.selected.clear();}
    return result;
}
bool AccountStore::save(const QList<Account> &accounts,const QString &selected,QString *error) const {
    if(error)error->clear();QJsonArray array;QSet<QString> seen;
    for(const auto &a:accounts){const auto problem=accountProblem(a);if(!problem.isEmpty()||seen.contains(a.id)){if(error)*error=problem.isEmpty()?QCoreApplication::translate("M2CL", "账号重复"):problem;return false;}seen.insert(a.id);array.append(accountJson(a));}
    if(!selected.isEmpty()&&!seen.contains(selected)){if(error)*error=QCoreApplication::translate("M2CL", "请选择有效账号");return false;}
    if(!QDir().mkpath(m_directory)){if(error)*error=QCoreApplication::translate("M2CL", "无法创建账号配置目录");return false;}
    QLockFile lock(QDir(m_directory).filePath("accounts.lock"));if(!lock.tryLock(0)){if(error)*error=QCoreApplication::translate("M2CL", "其他启动器窗口正在修改账号，请稍后重试");return false;}
    QSaveFile f(QDir(m_directory).filePath("accounts.json"));const auto data=QJsonDocument(QJsonObject{{"schemaVersion",1},{"selected",selected},{"accounts",array}}).toJson();
    if(!f.open(QIODevice::WriteOnly)||!f.setPermissions(QFile::ReadOwner|QFile::WriteOwner)||f.write(data)!=data.size()||!f.commit()){if(error)*error=QCoreApplication::translate("M2CL", "无法保存账号列表：")+f.errorString();return false;}return true;
}
QString AccountStore::credentialNamespace() const {auto path=QFileInfo(m_directory).canonicalFilePath();if(path.isEmpty())path=QDir(m_directory).absolutePath();return "app.m2cl.accounts."+QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(),QCryptographicHash::Sha256).toHex());}
SecretRedactor::SecretRedactor(const QStringList &secrets){for(const auto &s:secrets)if(!s.isEmpty()){m_secrets.append(s.toUtf8());m_longest=qMax(m_longest,int(s.toUtf8().size()));}}
QByteArray SecretRedactor::append(const QByteArray &bytes,bool finish){
    m_pending+=bytes;QByteArray out;
    while(!m_pending.isEmpty()){
        const int safe=finish?int(m_pending.size()):qMax(0,int(m_pending.size())-m_longest+1);if(!safe)break;
        int first=-1,length=0;for(const auto &s:m_secrets){const int at=m_pending.indexOf(s);if(at>=0&&(first<0||at<first||(at==first&&s.size()>length))){first=at;length=s.size();}}
        if(first>=0&&first<safe){out+=m_pending.left(first);out+="[REDACTED]";m_pending.remove(0,first+length);}
        else {out+=m_pending.left(safe);m_pending.remove(0,safe);}
    }return out;
}
}
