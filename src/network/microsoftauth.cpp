// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "microsoftauth.h"
#include "networkerror.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <memory>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QUuid>
#include <QUrlQuery>

namespace mc {
namespace {
const QString authority="https://login.microsoftonline.com/consumers/oauth2/v2.0/";
const QString scope="XboxLive.signin offline_access";
QString randomText(){QByteArray bytes(32,Qt::Uninitialized);for(int n=0;n<bytes.size();++n)bytes[n]=char(QRandomGenerator::system()->generate()&255);return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));}
bool tokenValid(const QString &value){return !value.isEmpty()&&value.size()<=32768&&!value.contains(QRegularExpression("[\\x00-\\x20\\x7f]"));}
QByteArray formData(const QMap<QString,QString> &fields){QByteArray result;for(auto it=fields.begin();it!=fields.end();++it){if(!result.isEmpty())result+='&';result+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(it.value());}return result;}
QJsonObject claims(const QJsonObject &o){const auto list=o.value("DisplayClaims").toObject().value("xui").toArray();return list.isEmpty()?QJsonObject{}:list.first().toObject();}
QString safeCode(const QString &value){return QRegularExpression("^[A-Za-z0-9_.-]{1,80}$").match(value).hasMatch()?value:QString();}
}
MicrosoftAuth::MicrosoftAuth(QObject *parent,QNetworkAccessManager *network,int timeoutMs):QObject(parent),m_ownedNetwork(this),m_network(network?network:&m_ownedNetwork),m_timeout(qMax(1,timeoutMs)){
    m_callback.setMaxPendingConnections(4);connect(&m_callback,&QTcpServer::newConnection,this,&MicrosoftAuth::receiveBrowser);
    m_browserDeadline.setSingleShot(true);connect(&m_browserDeadline,&QTimer::timeout,this,[this]{fail(QCoreApplication::translate("M2CL", "登录等待超时"),QCoreApplication::translate("M2CL", "浏览器登录已超过 10 分钟，请重新开始。"),"browser_timeout");});
}
MicrosoftAuth::~MicrosoftAuth(){cancel();}
bool MicrosoftAuth::validClientId(const QString &id){return QRegularExpression("^[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}$").match(id).hasMatch();}
void MicrosoftAuth::cancel(){++m_generation;m_busy=false;m_browserDeadline.stop();m_callback.close();if(m_reply){auto reply=m_reply;m_reply.clear();reply->abort();}m_state.clear();m_verifier.clear();m_refreshToken.clear();m_minecraftToken.clear();}
void MicrosoftAuth::reset(const QString &id){cancel();m_clientId=id;m_uhs.clear();m_xuid.clear();m_expires={};m_busy=true;}
void MicrosoftAuth::fail(const QString &title,const QString &details,const QString &code,bool relogin){cancel();emit failed({title,details,code,relogin});}
void MicrosoftAuth::login(const QString &clientId){
    reset(clientId);if(!validClientId(clientId)){fail(QCoreApplication::translate("M2CL", "尚未配置微软登录"),QCoreApplication::translate("M2CL", "此开发构建需要先配置 M²CL 自己的微软应用 ID。"),"client_not_configured");return;}
    if(!m_callback.listen(QHostAddress::LocalHost,0)){fail(QCoreApplication::translate("M2CL", "无法准备浏览器登录"),QCoreApplication::translate("M2CL", "本机登录回调端口无法打开，请稍后重试。"),"callback_unavailable");return;}
    m_state=randomText();m_verifier=randomText();m_redirect=QStringLiteral("http://localhost:%1/").arg(m_callback.serverPort());
    QUrl url(authority+"authorize");QUrlQuery query;query.addQueryItem("client_id",clientId);query.addQueryItem("response_type","code");query.addQueryItem("redirect_uri",m_redirect);query.addQueryItem("scope",scope);query.addQueryItem("response_mode","query");query.addQueryItem("prompt","select_account");query.addQueryItem("state",m_state);query.addQueryItem("code_challenge_method","S256");query.addQueryItem("code_challenge",QString::fromLatin1(QCryptographicHash::hash(m_verifier.toLatin1(),QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)));url.setQuery(query);
    m_browserDeadline.start(10*60*1000);emit progress(QCoreApplication::translate("M2CL", "在微软官方网页完成登录，完成后会自动返回"));emit browserRequested(url);
}
void MicrosoftAuth::receiveBrowser(){
    while(m_callback.hasPendingConnections()){
        auto socket=m_callback.nextPendingConnection();const auto generation=m_generation;auto bytes=std::make_shared<QByteArray>();
        connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);QTimer::singleShot(5000,socket,[socket]{socket->disconnectFromHost();});
        connect(socket,&QTcpSocket::readyRead,this,[this,socket,generation,bytes]{
            *bytes+=socket->readAll();if(bytes->size()>16384){socket->disconnectFromHost();return;}if(!bytes->contains("\r\n\r\n"))return;
            auto answer=[socket](bool ok){const auto body=(ok?QCoreApplication::translate("M2CL", "<!doctype html><meta charset=utf-8><title>M²CL</title><h2>已收到登录结果</h2><p>请返回 M²CL 查看验证结果，可以关闭此页面。</p>"):QCoreApplication::translate("M2CL", "<!doctype html><meta charset=utf-8><title>M²CL</title><p>此回调与当前登录不匹配，请返回 M²CL 重新登录。</p>")).toUtf8();socket->write(QByteArray(ok?"HTTP/1.1 200 OK\r\n":"HTTP/1.1 400 Bad Request\r\n")+"Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nContent-Security-Policy: default-src 'none'\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);socket->disconnectFromHost();};
            const auto line=bytes->left(bytes->indexOf("\r\n")).split(' ');const auto target=line.size()==3?QUrl::fromEncoded(line.at(1)):QUrl();const QUrlQuery query(target);
            const bool valid=generation==m_generation&&m_busy&&!m_state.isEmpty()&&!m_verifier.isEmpty()&&socket->peerAddress().isLoopback()&&line.size()==3&&line.first()=="GET"&&target.isRelative()&&target.path()=="/"&&query.allQueryItemValues("state").size()==1&&query.queryItemValue("state")==m_state;
            if(!valid){answer(false);return;}if(query.hasQueryItem("error")){answer(true);fail(QCoreApplication::translate("M2CL", "微软登录未完成"),QCoreApplication::translate("M2CL", "登录被取消或拒绝，可重新登录。"),safeCode(query.queryItemValue("error")));return;}
            const auto code=query.queryItemValue("code",QUrl::FullyDecoded);if(query.allQueryItemValues("code").size()!=1||!tokenValid(code)){answer(false);return;}
            answer(true);m_callback.close();m_browserDeadline.stop();m_state.clear();exchange(formData({{"client_id",m_clientId},{"grant_type","authorization_code"},{"code",code},{"redirect_uri",m_redirect},{"code_verifier",m_verifier},{"scope",scope}}));m_verifier.clear();
        });
    }
}
void MicrosoftAuth::refresh(const QString &clientId,const QString &refreshToken){reset(clientId);if(!validClientId(clientId)||!tokenValid(refreshToken)){fail(QCoreApplication::translate("M2CL", "需要重新登录"),QCoreApplication::translate("M2CL", "已保存的登录凭据或应用配置不可用。"),"missing_credentials",true);return;}m_refreshToken=refreshToken;exchange(formData({{"client_id",clientId},{"grant_type","refresh_token"},{"refresh_token",refreshToken},{"scope",scope}}));}
void MicrosoftAuth::send(const QUrl &url,const QByteArray &body,const QString &bearer,const QString &stage,const std::function<void(const QJsonObject &)> &ready,bool form){
    emit progress(stage);const auto generation=m_generation;QNetworkRequest request(url);request.setTransferTimeout(0);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setRawHeader("User-Agent","m2cl/0.1.0");request.setRawHeader("Accept","application/json");request.setRawHeader("x-xbl-contract-version","1");if(!bearer.isEmpty())request.setRawHeader("Authorization","Bearer "+bearer.toUtf8());
    QNetworkReply *reply;if(body.isEmpty())reply=m_network->get(request);else{request.setHeader(QNetworkRequest::ContentTypeHeader,form?"application/x-www-form-urlencoded":"application/json");reply=m_network->post(request,body);}m_reply=reply;
    QTimer::singleShot(m_timeout,reply,[reply]{if(!reply->isFinished()){reply->setProperty("networkTimedOut",true);reply->abort();}});
    connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>1024*1024){reply->setProperty("responseTooLarge",true);reply->abort();}});
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,stage,ready,url]{
        reply->deleteLater();if(generation!=m_generation)return;m_reply.clear();const auto data=reply->readAll();QJsonParseError parse;const auto document=QJsonDocument::fromJson(data,&parse);const auto object=document.object();const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString problem=networkReplyError(reply);if(status>=300&&status<400)problem=QCoreApplication::translate("M2CL", "登录服务返回异常跳转（HTTP %1）").arg(status);
        if(reply->property("responseTooLarge").toBool()||data.size()>1024*1024)problem=QCoreApplication::translate("M2CL", "登录服务返回的数据超过大小限制");
        if(!problem.isEmpty()){
            const auto code=safeCode(object.value("error").toString());QString hint;bool relogin=false;
            QString failureCode=code.isEmpty()?QString::number(status):code;
            if(code=="invalid_grant"||code=="interaction_required"||code=="login_required"){hint=QCoreApplication::translate("M2CL", "登录会话已失效，请重新登录。");relogin=true;}
            else if(code=="invalid_client"||code=="unauthorized_client")hint=QCoreApplication::translate("M2CL", "微软应用配置无效或尚未授权，请检查本项目的应用注册。");
            const auto xerr=object.value("XErr").toVariant().toULongLong();if(xerr){hint=QCoreApplication::translate("M2CL", "Xbox 身份验证失败（XErr %1）。").arg(xerr);if(xerr==2148916233ULL)hint+=QCoreApplication::translate("M2CL", "请先在 Xbox 官网建立玩家档案。");else if(xerr==2148916238ULL)hint+=QCoreApplication::translate("M2CL", "此账号需要先完成微软家庭设置。");}
            if(url.host()=="api.minecraftservices.com"&&url.path()=="/launcher/login"&&status==403){
                const auto message=object.value("errorMessage").toString();
                if(message.contains("Invalid app registration",Qt::CaseInsensitive)||message.contains("aka.ms/AppRegInfo",Qt::CaseInsensitive)){
                    failureCode="minecraft_app_registration_rejected";
                    hint=QCoreApplication::translate("M2CL", "Minecraft 服务拒绝了此启动器的应用注册。请由项目维护者申请或核对 Java 版服务 API 的访问许可，反复登录无法解决应用审核问题。");
                }else{
                    failureCode="minecraft_login_forbidden";
                    hint=QCoreApplication::translate("M2CL", "微软与 Xbox 验证已通过，Minecraft 服务拒绝了登录请求。若此启动器的应用刚注册，请先核对 Java 版服务 API 访问申请；账号或服务访问限制也可能返回 HTTP 403，当前响应不足以确定原因。");
                }
            }else if(url.host()=="api.minecraftservices.com"&&status==403)hint=QCoreApplication::translate("M2CL", "Minecraft 服务拒绝读取游戏资格或角色信息，请核对账号状态及服务访问权限。");
            if(url.path()=="/minecraft/profile"&&status==404)hint=QCoreApplication::translate("M2CL", "没有找到 Java 版角色，请先在 Minecraft 官网确认游戏资格并创建角色。");
            QStringList codes;if(!code.isEmpty())codes<<code;for(const auto &v:object.value("error_codes").toArray())if(v.isDouble())codes<<"AADSTS"+QString::number(v.toInt());
            fail(stage+QCoreApplication::translate("M2CL", "失败"),hint+(hint.isEmpty()?QString():"\n")+problem+(codes.isEmpty()?QString():"\n"+codes.join(" · ")),failureCode,relogin);return;
        }
        if(parse.error!=QJsonParseError::NoError||!document.isObject()){fail(stage+QCoreApplication::translate("M2CL", "失败"),QCoreApplication::translate("M2CL", "登录服务返回了无法读取的数据。"),"invalid_response");return;}ready(object);
    });
}
void MicrosoftAuth::exchange(const QByteArray &body){send(QUrl(authority+"token"),body,{},QCoreApplication::translate("M2CL", "验证微软登录"),[this](const QJsonObject &o){const auto access=o.value("access_token").toString();const auto refresh=o.value("refresh_token").toString();if(!tokenValid(access)||(!refresh.isEmpty()&&!tokenValid(refresh))){fail(QCoreApplication::translate("M2CL", "微软登录返回无效凭据"),QCoreApplication::translate("M2CL", "请重新登录。"),"invalid_token",true);return;}if(!refresh.isEmpty())m_refreshToken=refresh;xbox(access);},true);}
void MicrosoftAuth::xbox(const QString &access){const QJsonObject payload{{"Properties",QJsonObject{{"AuthMethod","RPS"},{"SiteName","user.auth.xboxlive.com"},{"RpsTicket","d="+access}}},{"RelyingParty","http://auth.xboxlive.com"},{"TokenType","JWT"}};send(QUrl("https://user.auth.xboxlive.com/user/authenticate"),QJsonDocument(payload).toJson(QJsonDocument::Compact),{},QCoreApplication::translate("M2CL", "验证 Xbox 账号"),[this](const QJsonObject &o){const auto token=o.value("Token").toString();m_uhs=claims(o).value("uhs").toString();if(!tokenValid(token)||!tokenValid(m_uhs)){fail(QCoreApplication::translate("M2CL", "Xbox 登录数据不完整"),QCoreApplication::translate("M2CL", "请重新登录。"),"invalid_xbox_response");return;}xsts(token);});}
void MicrosoftAuth::xsts(const QString &userToken){const QJsonObject payload{{"Properties",QJsonObject{{"SandboxId","RETAIL"},{"UserTokens",QJsonArray{userToken}}}},{"RelyingParty","rp://api.minecraftservices.com/"},{"TokenType","JWT"}};send(QUrl("https://xsts.auth.xboxlive.com/xsts/authorize"),QJsonDocument(payload).toJson(QJsonDocument::Compact),{},QCoreApplication::translate("M2CL", "验证 Xbox 服务权限"),[this](const QJsonObject &o){const auto token=o.value("Token").toString();const auto info=claims(o);if(!tokenValid(token)||info.value("uhs").toString()!=m_uhs){fail(QCoreApplication::translate("M2CL", "Xbox 身份校验失败"),QCoreApplication::translate("M2CL", "服务返回的账号身份不一致，请重新登录。"),"xbox_identity_mismatch",true);return;}m_xuid=info.value("xid").toString();if(!m_xuid.isEmpty()&&!QRegularExpression("^[0-9]{1,25}$").match(m_xuid).hasMatch()){fail(QCoreApplication::translate("M2CL", "Xbox 玩家标识无效"),{},"invalid_xuid");return;}minecraft(token);});}
void MicrosoftAuth::minecraft(const QString &token){send(QUrl("https://api.minecraftservices.com/launcher/login"),QJsonDocument(QJsonObject{{"xtoken","XBL3.0 x="+m_uhs+";"+token},{"platform","PC_LAUNCHER"}}).toJson(QJsonDocument::Compact),{},QCoreApplication::translate("M2CL", "验证 Minecraft 登录"),[this](const QJsonObject &o){m_minecraftToken=o.value("access_token").toString();const auto seconds=o.value("expires_in").toDouble();if(!tokenValid(m_minecraftToken)||seconds<60||seconds>7*24*3600){fail(QCoreApplication::translate("M2CL", "Minecraft 登录数据无效"),QCoreApplication::translate("M2CL", "请重新登录。"),"invalid_minecraft_token");return;}m_expires=QDateTime::currentDateTimeUtc().addSecs(qint64(seconds));entitlements();});}
void MicrosoftAuth::entitlements(){QUrl url("https://api.minecraftservices.com/entitlements/license");QUrlQuery query;query.addQueryItem("requestId",QUuid::createUuid().toString(QUuid::WithoutBraces));url.setQuery(query);send(url,{},m_minecraftToken,QCoreApplication::translate("M2CL", "检查 Minecraft 游戏资格"),[this](const QJsonObject &o){bool playable=false;for(const auto &v:o.value("items").toArray())if(v.toObject().value("name")=="game_minecraft")playable=true;if(!playable){fail(QCoreApplication::translate("M2CL", "此账号没有可用的 Java 版游戏资格"),QCoreApplication::translate("M2CL", "请在 Minecraft 官网确认购买或订阅状态。"),"no_entitlement");return;}profile();});}
void MicrosoftAuth::profile(){send(QUrl("https://api.minecraftservices.com/minecraft/profile"),{},m_minecraftToken,QCoreApplication::translate("M2CL", "读取 Minecraft 角色"),[this](const QJsonObject &o){AuthSession result;auto &a=result.account;a.microsoft=true;a.uuid=o.value("id").toString().remove('-').toLower();a.id="microsoft:"+a.uuid;a.name=o.value("name").toString();a.clientId=m_clientId;for(const auto &skin:o.value("skins").toArray()){const auto s=skin.toObject();const QUrl url(s.value("url").toString());if(s.value("state")=="ACTIVE"&&url.scheme()=="https"&&url.host()=="textures.minecraft.net"&&url.userInfo().isEmpty())a.skinUrl=url;}
        if(!accountProblem(a).isEmpty()){fail(QCoreApplication::translate("M2CL", "Minecraft 角色信息无效"),QCoreApplication::translate("M2CL", "请在官网检查 Java 版角色。"),"invalid_profile");return;}result.identity={a.name,a.uuid,m_minecraftToken,m_xuid,m_clientId,m_expires,true};result.refreshToken=m_refreshToken;cancel();emit succeeded(result);});}
}
