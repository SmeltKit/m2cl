// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "compatibility.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QSaveFile>
namespace mc {
QString compatibilitySource(){return "https://raw.githubusercontent.com/QuiltMC/quiltmc.org/main/src/data/incompatible-mods.json";}
static QString path(const Instance &i){return QDir(i.gameDirectory).filePath(".m2cl-quilt-compatibility.json");}
// Record attempts independently of successful downloads: failures must not cause
// another request each time the page is opened, including after a restart.
static QHash<QString,QDateTime> attempts;
bool compatibilityUpdateDue(const Instance &i,const QDateTime &now){
 const auto key=path(i);auto last=attempts.value(key);
 QFile state(key+".attempt");
 if(state.open(QIODevice::ReadOnly)&&state.size()<128){
  const auto saved=QDateTime::fromString(QString::fromUtf8(state.readAll()),Qt::ISODate);
  if(saved.isValid()&&(!last.isValid()||saved>last))last=saved;
 }
 if(last.isValid()&&last.secsTo(now)<86400)return false;
 const auto list=compatibilityList(i);
 return !list.cached||list.fetched.secsTo(now)>=86400;
}
void recordCompatibilityAttempt(const Instance &i,const QDateTime &now){
 const auto key=path(i);attempts.insert(key,now);
 QSaveFile state(key+".attempt");
 if(state.open(QIODevice::WriteOnly)){
  const auto bytes=now.toUTC().toString(Qt::ISODate).toUtf8();
  if(state.write(bytes)==bytes.size())state.commit();
 }
}
static bool valid(const QJsonArray &rules){
 if(rules.isEmpty()||rules.size()>2000)return false;
 for(auto v:rules){auto o=v.toObject();if(o.value("name").toString().isEmpty()||o.value("name").toString().size()>256||o.value("ids").toArray().isEmpty()||!QStringList{"GAME","SELF","OTHERS","WORKAROUND"}.contains(o.value("type").toString()))return false;for(auto id:o.value("ids").toArray())if(id.toString().isEmpty()||id.toString().size()>128)return false;}
 return true;
}
CompatibilityList compatibilityList(const Instance &i){
 static const QByteArray builtin=
#include "quilt-compatibility.inc"
 ;
 CompatibilityList result{QJsonDocument::fromJson(builtin).array(),{},false};
 QFile f(path(i));if(f.open(QIODevice::ReadOnly)&&f.size()<512*1024){auto o=QJsonDocument::fromJson(f.readAll()).object();auto rules=o.value("rules").toArray();auto fetched=QDateTime::fromString(o.value("fetched").toString(),Qt::ISODate);if(o.value("source")==compatibilitySource()&&valid(rules)&&fetched.isValid()&&fetched<=QDateTime::currentDateTimeUtc().addSecs(300))result={rules,fetched,true};}return result;
}
bool saveCompatibilityList(const Instance &i,const QByteArray &bytes,QString *error){
 auto d=QJsonDocument::fromJson(bytes);if(bytes.size()>256*1024||!d.isArray()||!valid(d.array())){*error=QCoreApplication::translate("M2CL", "官方列表格式变化，继续使用已有规则");return false;}
 QSaveFile f(path(i));if(!f.open(QIODevice::WriteOnly)){*error=f.errorString();return false;}
 auto payload=QJsonDocument(QJsonObject{{"source",compatibilitySource()},{"fetched",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"rules",d.array()}}).toJson();
 if(f.write(payload)!=payload.size()||!f.commit()){*error=f.errorString();return false;}return true;
}
QStringList knownCompatibilityWarnings(const Instance &i,const LocalMod &m){
 QStringList result;if(i.loader!="quilt"||!m.enabled)return result;
 for(auto value:compatibilityList(i).rules){auto rule=value.toObject();bool match=false;for(auto id:rule.value("ids").toArray())match|=m.ids.contains(id.toString())||m.id==id.toString();if(!match)continue;
 const auto kind=rule.value("type").toString();const auto issue=kind=="GAME"?QCoreApplication::translate("M2CL", "可能导致游戏崩溃"):kind=="WORKAROUND"?QCoreApplication::translate("M2CL", "需要额外配置或绕过方案"):QCoreApplication::translate("M2CL", "存在功能兼容问题");
 auto text=QCoreApplication::translate("M2CL", "Quilt 官方兼容性提醒：%1 — %2。官方未提供精确文件 hash / 完整适用版本范围，不代表当前构建必然受影响。\n%3\n来源：https://quiltmc.org/en/usage/incompatible-mods/").arg(rule.value("name").toString(),issue,rule.value("note").toString());
 const auto tracking=rule.value("tracking").toString();if(tracking.startsWith("https://"))text+=QCoreApplication::translate("M2CL", "\n问题记录：")+tracking;result.append(text);
 }return result;
}
}
