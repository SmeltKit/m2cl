// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "network/accountmanager.h"
#include "core/gameplan.h"
#include "core/gamelog.h"
#include <QProcess>
#include <QThread>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrlQuery>
#include <cstring>

namespace {
const QString client="11111111-2222-4333-8444-555555555555";
const QString uuid="0123456789abcdef0123456789abcdef";
struct Route{QByteArray body;int status=200,delay=0;};
class Reply:public QNetworkReply {
public:
    Reply(const QNetworkRequest &request,const Route &r,QObject *parent):QNetworkReply(parent),body(r.body){setRequest(request);setUrl(request.url());setAttribute(QNetworkRequest::HttpStatusCodeAttribute,r.status);open(QIODevice::ReadOnly|QIODevice::Unbuffered);QTimer::singleShot(r.delay,this,[this]{if(isFinished())return;ready=true;emit readyRead();if(isFinished())return;setFinished(true);emit finished();});}
    void abort() override{if(isFinished())return;setError(OperationCanceledError,"Cancelled");setFinished(true);emit finished();}
    qint64 bytesAvailable() const override{return (ready?body.size()-pos:0)+QIODevice::bytesAvailable();}
protected:
    qint64 readData(char *data,qint64 max) override{if(!ready||pos>=body.size())return -1;const auto n=qMin(max,qint64(body.size()-pos));std::memcpy(data,body.constData()+pos,size_t(n));pos+=n;return n;}
private:QByteArray body;qint64 pos=0;bool ready=false;
};
class Network:public QNetworkAccessManager {
public:
    QHash<QString,Route> routes;QList<QUrl> requested;QList<QByteArray> bodies,bearers;
    void defaults(){
        auto put=[this](const QString &path,const QJsonObject &o){routes.insert(path,{QJsonDocument(o).toJson(QJsonDocument::Compact)});};
        put("/consumers/oauth2/v2.0/token",{{"access_token","msa-access-secret"},{"refresh_token","rotated-refresh-secret"}});
        put("/user/authenticate",{{"Token","xbox-user-secret"},{"DisplayClaims",QJsonObject{{"xui",QJsonArray{QJsonObject{{"uhs","userhash"}}}}}}});
        put("/xsts/authorize",{{"Token","xsts-secret"},{"DisplayClaims",QJsonObject{{"xui",QJsonArray{QJsonObject{{"uhs","userhash"},{"xid","123456789"}}}}}}});
        put("/launcher/login",{{"access_token","minecraft-access-secret"},{"expires_in",3600}});
        put("/entitlements/license",{{"items",QJsonArray{QJsonObject{{"name","game_minecraft"}}}}});
        put("/minecraft/profile",{{"id",uuid},{"name","RealPlayer"},{"skins",QJsonArray{QJsonObject{{"state","ACTIVE"},{"url","https://textures.minecraft.net/texture/test"}}}}});
    }
protected:
    QNetworkReply *createRequest(Operation,const QNetworkRequest &request,QIODevice *body) override{requested.append(request.url());bodies.append(body?body->readAll():QByteArray());bearers.append(request.rawHeader("Authorization"));return new Reply(request,routes.value(request.url().path(),{"{}",404}),this);}
};
class Vault:public mc::CredentialVault {
public:
    QHash<QString,QString> values;QString writeError,removeError;int writes=0,reads=0;
    QString name() const override{return "Test secure store";}bool available() const override{return true;}
    mc::CredentialResult read(const QString &id) override{++reads;return {values.value(id),values.contains(id)?QString():QString("Missing credential")};}
    QString write(const QString &id,const QString &value) override{++writes;if(!writeError.isEmpty())return writeError;values[id]=value;return {};}
    QString remove(const QString &id) override{if(!removeError.isEmpty())return removeError;values.remove(id);return {};}
};
mc::Account microsoft(){mc::Account a;a.microsoft=true;a.uuid=uuid;a.id="microsoft:"+uuid;a.name="RealPlayer";a.clientId=client;a.remembered=true;return a;}
QString credential(){return QString::fromUtf8(QJsonDocument(QJsonObject{{"clientId",client},{"refreshToken","saved-refresh-secret"}}).toJson());}
void seed(const QString &directory,const std::shared_ptr<Vault> &vault){const auto a=microsoft();mc::AccountStore(directory).save({mc::offlineAccount("Player"),a},a.id);vault->values[a.id]=credential();}
}
class AuthTests:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){qRegisterMetaType<mc::AuthSession>();qRegisterMetaType<mc::AuthFailure>();}
    void refreshVerifiesEveryServiceAndOnlySendsBearerToMinecraft(){
        Network network;network.defaults();mc::MicrosoftAuth auth(nullptr,&network);QSignalSpy done(&auth,&mc::MicrosoftAuth::succeeded),failure(&auth,&mc::MicrosoftAuth::failed);auth.refresh(client,"saved-refresh-secret");QTRY_COMPARE(done.size(),1);QCOMPARE(failure.size(),0);QCOMPARE(network.requested.size(),6);
        const auto session=qvariant_cast<mc::AuthSession>(done.first().first());QCOMPARE(session.account.name,QString("RealPlayer"));QCOMPARE(session.account.uuid,uuid);QCOMPARE(session.identity.xuid,QString("123456789"));QCOMPARE(session.refreshToken,QString("rotated-refresh-secret"));QVERIFY(session.identity.microsoft);QVERIFY(!auth.busy());
        for(int n=0;n<4;++n)QVERIFY(network.bearers.at(n).isEmpty());for(int n=4;n<6;++n){QCOMPARE(network.requested.at(n).host(),QString("api.minecraftservices.com"));QCOMPARE(network.bearers.at(n),QByteArray("Bearer minecraft-access-secret"));}
        QUrlQuery token(QString::fromUtf8(network.bodies.first()));QCOMPARE(token.queryItemValue("grant_type"),QString("refresh_token"));QVERIFY(!token.hasQueryItem("client_secret"));
    }
    void browserUsesPkceAndIgnoresWrongState(){
        Network network;network.defaults();mc::MicrosoftAuth auth(nullptr,&network);QSignalSpy urls(&auth,&mc::MicrosoftAuth::browserRequested),done(&auth,&mc::MicrosoftAuth::succeeded);auth.login(client);QCOMPARE(urls.size(),1);const auto url=urls.first().first().toUrl();QCOMPARE(url.host(),QString("login.microsoftonline.com"));QUrlQuery q(url);QCOMPARE(q.queryItemValue("code_challenge_method"),QString("S256"));QVERIFY(q.queryItemValue("state").size()>=40);const QUrl redirect(q.queryItemValue("redirect_uri"));
        auto callback=[&](const QString &state){QTcpSocket socket;socket.connectToHost(QHostAddress::LocalHost,quint16(redirect.port()));QVERIFY(socket.waitForConnected(1000));QUrlQuery query;query.addQueryItem("code","callback-code-secret");query.addQueryItem("state",state);socket.write("GET /?"+query.toString(QUrl::FullyEncoded).toUtf8()+" HTTP/1.1\r\nHost: localhost\r\n\r\n");socket.flush();QTRY_VERIFY(socket.bytesAvailable()>0);};
        callback("wrong-state");QVERIFY(network.requested.isEmpty());QVERIFY(auth.busy());callback(q.queryItemValue("state"));QTRY_COMPARE(done.size(),1);QUrlQuery token(QString::fromUtf8(network.bodies.first()));const auto verifier=token.queryItemValue("code_verifier");QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(verifier.toLatin1(),QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)),q.queryItemValue("code_challenge"));QVERIFY(!url.toString().contains(verifier));
    }
    void deniedOwnershipAndMismatchedXboxIdentityNeverProduceASession(){
        Network network;network.defaults();network.routes["/entitlements/license"]={"{\"items\":[{\"name\":\"product_minecraft\"}]}"};mc::MicrosoftAuth auth(nullptr,&network);QSignalSpy failure(&auth,&mc::MicrosoftAuth::failed),done(&auth,&mc::MicrosoftAuth::succeeded);auth.refresh(client,"refresh");QTRY_COMPARE(failure.size(),1);QCOMPARE(qvariant_cast<mc::AuthFailure>(failure.first().first()).code,QString("no_entitlement"));QCOMPARE(network.requested.size(),5);QVERIFY(done.isEmpty());
        network.defaults();network.requested.clear();network.routes["/xsts/authorize"]={"{\"Token\":\"xsts\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"different\"}]}}"};auth.refresh(client,"refresh");QTRY_COMPARE(failure.size(),2);QCOMPARE(network.requested.size(),3);QVERIFY(done.isEmpty());
    }
    void aBrowserCallbackCannotBeConsumedTwice(){
        Network network;network.defaults();network.routes["/consumers/oauth2/v2.0/token"].delay=150;mc::MicrosoftAuth auth(nullptr,&network);QSignalSpy urls(&auth,&mc::MicrosoftAuth::browserRequested),done(&auth,&mc::MicrosoftAuth::succeeded);auth.login(client);QCOMPARE(urls.size(),1);const QUrlQuery authQuery(urls.first().first().toUrl());const QUrl redirect(authQuery.queryItemValue("redirect_uri"));QTcpSocket first,second;for(auto socket:{&first,&second}){socket->connectToHost(QHostAddress::LocalHost,quint16(redirect.port()));QVERIFY(socket->waitForConnected(1000));}QTest::qWait(10);
        auto submit=[](QTcpSocket &socket,const QString &state){QUrlQuery q;q.addQueryItem("code","callback-code-secret");q.addQueryItem("state",state);socket.write("GET /?"+q.toString(QUrl::FullyEncoded).toUtf8()+" HTTP/1.1\r\nHost: localhost\r\n\r\n");socket.flush();};submit(first,authQuery.queryItemValue("state"));QTRY_VERIFY(first.bytesAvailable()>0);QVERIFY(first.readAll().startsWith("HTTP/1.1 200"));submit(second,{});QTRY_VERIFY(second.bytesAvailable()>0);QVERIFY(second.readAll().startsWith("HTTP/1.1 400"));QTRY_COMPARE(done.size(),1);QCOMPARE(network.requested.size(),6);
    }
    void failuresKeepHttpCodesButNeverEchoTokensOrServerBodies(){
        Network network;network.defaults();network.routes["/consumers/oauth2/v2.0/token"]={"{\"error\":\"invalid_grant\",\"error_description\":\"saved-refresh-secret and access-secret\"}",400};mc::MicrosoftAuth auth(nullptr,&network);QSignalSpy failure(&auth,&mc::MicrosoftAuth::failed);auth.refresh(client,"saved-refresh-secret");QTRY_COMPARE(failure.size(),1);const auto error=qvariant_cast<mc::AuthFailure>(failure.first().first());QVERIFY(error.details.contains("HTTP 400"));QVERIFY(error.details.contains("invalid_grant"));QVERIFY(!error.details.contains("saved-refresh-secret"));QVERIFY(!error.details.contains("access-secret"));QVERIFY(error.reloginRequired);
        network.defaults();network.routes["/launcher/login"]={"{\"error\":\"FORBIDDEN\",\"access_token\":\"never-expose-this\"}",403};auth.refresh(client,"refresh");QTRY_COMPARE(failure.size(),2);const auto denied=qvariant_cast<mc::AuthFailure>(failure.last().first());QVERIFY(denied.details.contains("HTTP 403"));QVERIFY(denied.details.contains("FORBIDDEN"));QVERIFY(!denied.details.contains("never-expose-this"));QCOMPARE(denied.code,QString("minecraft_login_forbidden"));QVERIFY(denied.details.contains(QStringLiteral("不足以确定原因")));
        network.routes["/launcher/login"]={"{\"error\":\"FORBIDDEN\",\"errorMessage\":\"Invalid app registration, see https://aka.ms/AppRegInfo; never-expose-this\"}",403};auth.refresh(client,"refresh");QTRY_COMPARE(failure.size(),3);const auto registration=qvariant_cast<mc::AuthFailure>(failure.last().first());QCOMPARE(registration.code,QString("minecraft_app_registration_rejected"));QVERIFY(registration.details.contains("HTTP 403"));QVERIFY(!registration.details.contains("never-expose-this"));QVERIFY(!registration.reloginRequired);
        network.defaults();network.routes["/entitlements/license"]={"{\"error\":\"FORBIDDEN\"}",403};auth.refresh(client,"refresh");QTRY_COMPARE(failure.size(),4);const auto entitlement=qvariant_cast<mc::AuthFailure>(failure.last().first());QCOMPARE(entitlement.code,QString("FORBIDDEN"));QVERIFY(!entitlement.details.contains(QStringLiteral("微软与 Xbox 验证已通过")));
    }
    void timeoutStopsAndCancellationCannotRestartAuthentication(){
        Network network;network.defaults();network.routes["/consumers/oauth2/v2.0/token"].delay=60000;mc::MicrosoftAuth auth(nullptr,&network,20);QSignalSpy failure(&auth,&mc::MicrosoftAuth::failed),done(&auth,&mc::MicrosoftAuth::succeeded);auth.refresh(client,"refresh");QTRY_COMPARE(failure.size(),1);QVERIFY(qvariant_cast<mc::AuthFailure>(failure.first().first()).details.contains("Timeout"));QTest::qWait(50);QCOMPARE(network.requested.size(),1);auth.refresh(client,"refresh");auth.cancel();QTest::qWait(50);QCOMPARE(failure.size(),1);QVERIFY(done.isEmpty());QVERIFY(!auth.busy());
    }
    void repositoryAndSettingsNeverContainCredentials(){
        QTemporaryDir dir;mc::AccountStore store(dir.path());const auto a=microsoft();QString error;QVERIFY(store.save({a},a.id,&error));QFile f(dir.filePath("accounts.json"));QVERIFY(f.open(QIODevice::ReadOnly));const auto data=f.readAll();QVERIFY(!data.contains("refresh_token"));QVERIFY(!data.contains("access_token"));QVERIFY(!data.contains("refreshToken"));QVERIFY(!(f.permissions()&QFile::ReadOther));const auto loaded=store.load();QVERIFY(loaded.error.isEmpty());QCOMPARE(loaded.accounts.first().uuid,uuid);
        f.close();QVERIFY(f.open(QIODevice::WriteOnly));f.write("broken JSON");f.close();QVERIFY(!store.load().error.isEmpty());
    }
    void managerRotatesCredentialsAndRestoresSessionWithoutSilentOfflineFallback(){
        QTemporaryDir dir;auto vault=std::make_shared<Vault>();seed(dir.path(),vault);Network network;network.defaults();mc::AccountManager manager(dir.path(),"Player",nullptr,vault,&network);QVERIFY(network.requested.isEmpty());QSignalSpy done(&manager,&mc::AccountManager::sessionReady);manager.ensureSession(microsoft().id);QTRY_COMPARE(done.size(),1);QCOMPARE(vault->writes,1);QVERIFY(vault->values.value(microsoft().id).contains("rotated-refresh-secret"));QCOMPARE(manager.current().id,microsoft().id);QVERIFY(manager.state(manager.current()).contains(QStringLiteral("已登录")));
        const auto count=network.requested.size();manager.ensureSession(microsoft().id);QTRY_COMPARE(done.size(),2);QCOMPARE(network.requested.size(),count);
        const auto offline=mc::offlineAccount("Player");QVERIFY(manager.select(offline.id).isEmpty());QCOMPARE(manager.current().id,offline.id);
    }
    void secureStoreFailureRequiresExplicitSessionOnlyChoice(){
        QTemporaryDir dir;auto vault=std::make_shared<Vault>();seed(dir.path(),vault);vault->writeError="Store locked";Network network;network.defaults();mc::AccountManager manager(dir.path(),"Player",nullptr,vault,&network);QSignalSpy done(&manager,&mc::AccountManager::sessionReady),failed(&manager,&mc::AccountManager::failed);manager.ensureSession(microsoft().id);QTRY_COMPARE(failed.size(),1);QVERIFY(done.isEmpty());QVERIFY(manager.hasPendingSession());QCOMPARE(qvariant_cast<mc::AuthFailure>(failed.first().first()).code,QString("credential_store_failed"));manager.continueWithoutRemembering();QTRY_COMPARE(done.size(),1);QVERIFY(!manager.current().remembered);QVERIFY(!vault->values.contains(microsoft().id));
        mc::AccountManager restarted(dir.path(),"Player",nullptr,vault,&network);QSignalSpy login(&restarted,&mc::AccountManager::failed);restarted.ensureSession(microsoft().id);QCOMPARE(login.size(),1);QVERIFY(qvariant_cast<mc::AuthFailure>(login.first().first()).reloginRequired);
    }
    void wrongRoleCannotReplaceAnExistingAccountAndRemovalClearsVault(){
        QTemporaryDir dir;auto vault=std::make_shared<Vault>();seed(dir.path(),vault);Network network;network.defaults();network.routes["/minecraft/profile"]={"{\"id\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"name\":\"OtherPlayer\"}"};mc::AccountManager manager(dir.path(),"Player",nullptr,vault,&network);QSignalSpy failure(&manager,&mc::AccountManager::failed);manager.ensureSession(microsoft().id);QTRY_COMPARE(failure.size(),1);QCOMPARE(manager.accounts().size(),2);QCOMPARE(manager.current().uuid,uuid);QCOMPARE(vault->writes,0);
        manager.remove(microsoft().id);QTRY_COMPARE(manager.accounts().size(),1);QVERIFY(!vault->values.contains(microsoft().id));QVERIFY(!manager.current().microsoft);
    }
    void onlineLaunchUsesRealIdentityAndRedactsSplitSecrets(){
        QTemporaryDir dir;const auto root=QFileInfo(dir.path()).canonicalFilePath();
        const auto metadata=QJsonObject{{"id","test"},{"mainClass","net.minecraft.client.main.Main"},{"arguments",QJsonObject{{"game",QJsonArray{"--username","${auth_player_name}","--uuid","${auth_uuid}","--accessToken","${auth_access_token}","--userType","${user_type}","--clientId","${clientid}","--xuid","${auth_xuid}"}}}}};mc::GameIdentity identity{"RealPlayer",uuid,"minecraft-access-secret","123",client,QDateTime::currentDateTimeUtc().addSecs(3600),true};const auto command=mc::gameCommand(metadata,{},root,"test",root+"/game","java",identity,2048,{"osx","aarch64",{}},{});QVERIFY(command.online);QVERIFY(command.arguments.contains(uuid));QVERIFY(command.arguments.contains("msa"));QVERIFY(command.arguments.contains(client));QVERIFY(command.arguments.contains("123"));QVERIFY(command.arguments.contains(identity.accessToken));QVERIFY(command.redactionSecrets.contains(identity.accessToken));
        const auto bytes=("before "+identity.accessToken+" after\n").toUtf8();for(int n=0;n<=bytes.size();++n){mc::SecretRedactor redactor({identity.accessToken});auto output=redactor.append(bytes.left(n));output+=redactor.append(bytes.mid(n));output+=redactor.append({},true);QCOMPARE(output,QByteArray("before [REDACTED] after\n"));}
        auto bad=metadata;bad.insert("arguments",QJsonObject{{"game",QJsonArray{"${auth_access_token}-${unknown}"}}});try{mc::gameCommand(bad,{},root,"test",root+"/game","java",identity,2048,{"osx","aarch64",{}},{});QFAIL("Expected invalid variable");}catch(const std::exception &e){QVERIFY(!QString::fromUtf8(e.what()).contains(identity.accessToken));}
        identity.expires=QDateTime::currentDateTimeUtc().addSecs(-1);bool rejected=false;try{mc::gameCommand(metadata,{},root,"test",root+"/game","java",identity,2048,{"osx","aarch64",{}},{});}catch(const std::exception &){rejected=true;}QVERIFY(rejected);
    }
    void processLogsAreRedactedBeforeWritingAndFlushAtExit(){
        QTemporaryDir dir;QProcess process;process.setProcessChannelMode(QProcess::MergedChannels);auto writer=new mc::GameLogWriter(&process);const auto path=dir.filePath("launcher.log");QVERIFY(writer->open(path,{"minecraft-access-secret"}).isEmpty());QSignalSpy errors(writer,&mc::GameLogWriter::failed),done(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished));process.start(QCoreApplication::applicationFilePath(),{"--emit-test-output"});QTRY_COMPARE(done.size(),1);QCOMPARE(process.exitCode(),0);QVERIFY(errors.isEmpty());QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("stdout before [REDACTED] after\nstderr [REDACTED]\ntail"));
        QProcess invalid;auto missing=new mc::GameLogWriter(&invalid);QVERIFY(!missing->open(dir.filePath("missing/log"),{"secret"}).isEmpty());
    }
};
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    if(app.arguments().contains("--emit-test-output")){
        QFile out;if(!out.open(stdout,QIODevice::WriteOnly))return 2;out.write("stdout before minecraft-access-");out.flush();QThread::msleep(30);out.write("secret after\n");out.flush();
        QFile err;if(!err.open(stderr,QIODevice::WriteOnly))return 2;err.write("stderr minecraft-access-secret\ntail");err.flush();return 0;
    }
    AuthTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "auth_tests.moc"
