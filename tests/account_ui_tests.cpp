// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "ui/accountwidgets.h"
#include "ui/accountspanel.h"
#include "ui/mainwindow.h"
#include "ui/theme.h"
#include "core/i18n.h"
#include <QApplication>
#include <QBuffer>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <cstring>

namespace {
mc::Account microsoft(const QString &name="OnlinePlayer") {
    mc::Account a;a.name=name;a.uuid="0123456789abcdef0123456789abcdef";a.id="microsoft:"+a.uuid;
    a.microsoft=true;a.clientId="11111111-2222-4333-8444-555555555555";
    a.skinUrl=QUrl("https://textures.minecraft.net/texture/"+QString(64,'a'));return a;
}
QByteArray texture(int unit=1) {
    QImage image(64*unit,64*unit,QImage::Format_ARGB32);image.fill(Qt::transparent);
    QPainter p(&image);p.fillRect(8*unit,8*unit,8*unit,8*unit,Qt::red);
    p.fillRect(40*unit,8*unit,unit,unit,Qt::green);p.end();
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");return bytes;
}
class Reply final:public QNetworkReply {
public:
    Reply(const QNetworkRequest &request,const QByteArray &bytes,QObject *parent):QNetworkReply(parent),m_bytes(bytes) {
        setRequest(request);setUrl(request.url());setAttribute(QNetworkRequest::HttpStatusCodeAttribute,200);
        open(QIODevice::ReadOnly|QIODevice::Unbuffered);QTimer::singleShot(0,this,[this]{emit readyRead();if(!isFinished()){setFinished(true);emit finished();}});
    }
    void abort() override {if(!isFinished()){setError(OperationCanceledError,"Cancelled");setFinished(true);emit finished();}}
    qint64 bytesAvailable() const override {return m_bytes.size()-m_position+QIODevice::bytesAvailable();}
protected:
    qint64 readData(char *data,qint64 maximum) override {const auto count=qMin(maximum,qint64(m_bytes.size()-m_position));if(!count)return -1;std::memcpy(data,m_bytes.constData()+m_position,size_t(count));m_position+=count;return count;}
private:
    QByteArray m_bytes;qint64 m_position=0;
};
class Network final:public QNetworkAccessManager {
public:
    QByteArray bytes=texture();QList<QNetworkRequest> requests;
    QNetworkReply *createRequest(Operation,const QNetworkRequest &request,QIODevice *) override {requests.append(request);return new Reply(request,bytes,this);}
};
bool fixture(const QString &directory) {
    const auto path=directory+"/minecraft/versions/1.20.1";QDir().mkpath(path);QFile file(path+"/1.20.1.json");
    if(!file.open(QIODevice::WriteOnly))return false;file.write("{\"id\":\"1.20.1\"}");file.close();
    mc::Settings settings;settings.gameRoots=QStringList{directory+"/minecraft"};return mc::SettingsStore(directory).save(settings);
}
}
class AccountUiTests:public QObject {
    Q_OBJECT
private slots:
    void init(){qApp->setProperty("offlineUiTest",true);mc::installLanguage(*qApp,"zh_CN");qApp->setFont(mc::applicationFont());}
    void cleanup(){qApp->setProperty("offlineUiTest",true);mc::installLanguage(*qApp,"zh_CN");}
    void homeSwitchesRolesWithoutLeavingHomeOrLoggingIn() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));const auto offline=mc::offlineAccount("LocalPlayer"),online=microsoft();
        QVERIFY(mc::AccountStore(dir.path()).save({offline,online},offline.id));
        mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());
        auto manager=window.findChild<mc::AccountManager *>();auto card=window.findChild<mc::HomeAccountCard *>();QVERIFY(card->isVisible());
        QSignalSpy browser(manager,&mc::AccountManager::browserRequested);
        QCOMPARE(window.findChild<QLabel *>("homePlayerName")->text(),offline.name);
        QVERIFY(window.findChild<QLabel *>("heroPlayer")->text().contains(offline.name));
        card->findChild<QPushButton *>("homeMicrosoftMode")->click();QCOMPARE(manager->current().id,online.id);
        QVERIFY(card->findChild<QLabel *>("homeAccountState")->text().contains(QStringLiteral("需要重新登录")));
        QVERIFY(card->findChild<QPushButton *>("homeAccountRelogin")->isVisible());QCOMPARE(browser.size(),0);
        QVERIFY(!window.findChild<mc::AccountsPanel *>()->isVisible());
        QCOMPARE(mc::AccountStore(dir.path()).load().selected,online.id);
        card->findChild<QPushButton *>("homeSwitchAccount")->click();
        auto popup=window.findChild<QFrame *>("homeAccountMenu");QVERIFY(popup&&popup->isVisible());auto list=popup->findChild<QListWidget *>("homeAccountChoices");QVERIFY(list);
        QVERIFY(popup->testAttribute(Qt::WA_TranslucentBackground));QCOMPARE(popup->grab().toImage().pixelColor(0,0).alpha(),0);
        if(qEnvironmentVariableIsSet("M2CL_HOME_ACCOUNT_REVIEW"))popup->grab().save(qEnvironmentVariable("M2CL_HOME_ACCOUNT_REVIEW")+"/account-picker.png");
        for(int row=0;row<list->count();++row)if(list->item(row)->data(Qt::UserRole).toString()==offline.id){QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(row)).center());break;}
        QCOMPARE(manager->current().id,offline.id);QVERIFY(!popup->isVisible());
        QVERIFY(card->isVisible());QVERIFY(!card->findChild<QPushButton *>("homeAccountRelogin")->isVisible());QCOMPARE(browser.size(),0);
        card->findChild<QPushButton *>("homeManageAccounts")->click();QVERIFY(window.findChild<mc::AccountsPanel *>()->isVisible());
        auto table=window.findChild<QTableWidget *>("accountsTable");QCOMPARE(table->iconSize(),QSize(48,48));QCOMPARE(table->rowHeight(0),76);
        const auto avatar=table->item(0,0)->icon().pixmap(48,48);QCOMPARE(avatar.size(),QSize(48,48));
        QCoreApplication::processEvents();auto more=table->cellWidget(0,3)->findChild<QPushButton *>("accountMore");
        QCOMPARE(table->horizontalHeaderItem(3)->textAlignment(),int(Qt::AlignCenter));
        const auto buttonCenter=more->mapTo(table->viewport(),more->rect().center()).x();
        const auto columnCenter=table->columnViewportPosition(3)+table->columnWidth(3)/2;QVERIFY(qAbs(buttonCenter-columnCenter)<=2);
        const auto tableImage=table->viewport()->grab().toImage();const auto first=table->visualItemRect(table->item(0,0));
        if(qEnvironmentVariableIsSet("M2CL_HOME_ACCOUNT_REVIEW"))window.grab().save(qEnvironmentVariable("M2CL_HOME_ACCOUNT_REVIEW")+"/account-management.png");
        const auto background=tableImage.pixelColor(first.left()+12,first.top()+4);
        for(int y=first.top()+4;y<first.center().y()-24;++y)for(int x=first.left()+12;x<first.left()+60;++x)
            QVERIFY2(tableImage.pixelColor(x,y)==background,qPrintable(QString("Unexpected stale control at %1,%2 above the face").arg(x).arg(y)));
        int visibleActions=0;for(auto button:table->findChildren<QPushButton *>("accountMore"))if(button->isVisible())++visibleActions;
        QCOMPARE(visibleActions,table->rowCount());
    }
    void cancelledLoginAndOfflineCreationPreserveCurrentRole() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());
        auto manager=window.findChild<mc::AccountManager *>();const auto original=manager->current().id;
        bool inspected=false;QTimer::singleShot(20,&window,[&]{if(auto dialog=qobject_cast<mc::MicrosoftLoginDialog *>(QApplication::activeModalWidget())){inspected=dialog->findChild<QPushButton *>("startMicrosoftLogin")->isEnabled();dialog->reject();}});
        window.findChild<QPushButton *>("homeMicrosoftMode")->click();QVERIFY(inspected);QVERIFY(!manager->busy());
        QCOMPARE(manager->current().id,original);QVERIFY(window.findChild<QPushButton *>("homeOfflineMode")->isChecked());
        QVERIFY(window.findChild<mc::HomeAccountCard *>()->isVisible());
        auto card=window.findChild<mc::HomeAccountCard *>();bool cancelled=false;
        QTimer::singleShot(20,&window,[&]{auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog)return;auto name=dialog->findChild<QLineEdit *>("newOfflinePlayer");if(!name)return;name->setText("NewPlayer");QVERIFY(dialog->width()<500);if(qEnvironmentVariableIsSet("M2CL_HOME_ACCOUNT_REVIEW"))dialog->grab().save(qEnvironmentVariable("M2CL_HOME_ACCOUNT_REVIEW")+"/add-offline-player.png");dialog->reject();cancelled=true;});
        card->offlineCreationRequested();QVERIFY(cancelled);QCOMPARE(manager->current().id,original);QCOMPARE(manager->accounts().size(),1);
        QTimer::singleShot(20,&window,[&]{if(auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget())){dialog->findChild<QLineEdit *>("newOfflinePlayer")->setText("NewPlayer");dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();}});
        card->offlineCreationRequested();QCOMPARE(manager->current().name,QString("NewPlayer"));
        QCOMPARE(window.findChild<QLabel *>("homePlayerName")->text(),QString("NewPlayer"));QVERIFY(window.findChild<QLabel *>("heroPlayer")->text().contains("NewPlayer"));
    }
    void accountCardFitsBothSizesAndAllLanguages() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));auto a=microsoft("LongPlayerName16");a.remembered=true;
        for(const auto version:{"1.18.2","1.12.2","1.13.2"}){
            const auto path=dir.path()+"/minecraft/versions/"+version;QDir().mkpath(path);QFile file(path+"/"+version+".json");
            QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(QJsonObject{{"id",version}}).toJson());
        }
        QVERIFY(mc::AccountStore(dir.path()).save({a},a.id));
        for(const auto language:{"zh_CN","zh_TW","en_US"}){
            QVERIFY(mc::installLanguage(*qApp,language));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());
            for(bool dark:{false,true})for(const auto size:{QSize(1140,780),QSize(840,620)}){
                window.setAppearance(dark?"dark":"light","forest");window.resize(size);QTest::qWait(20);
                auto card=window.findChild<mc::HomeAccountCard *>();QVERIFY(card->isVisible());
                if(qEnvironmentVariableIsSet("M2CL_HOME_ACCOUNT_REVIEW")){
                    const auto path=qEnvironmentVariable("M2CL_HOME_ACCOUNT_REVIEW");QDir().mkpath(path);
                    window.grab().save(path+QString("/%1-%2-%3.png").arg(language,dark?"dark":"light").arg(size.width()));
                }
                QVERIFY2(card->width()<=window.width()-160,qPrintable(QString("%1: window %2, card %3").arg(language).arg(window.width()).arg(card->width())));
                QVERIFY(card->mapTo(&window,QPoint(card->width(),0)).x()<=window.width());
                for(const auto name:{"homeMicrosoftMode","homeOfflineMode","homeSwitchAccount","homeManageAccounts"}){
                    auto button=card->findChild<QPushButton *>(name);QVERIFY(button&&button->isVisible());
                    const auto right=button->mapTo(card,QPoint(button->width(),button->height()));
                    QVERIFY2(right.x()<=card->width(),name);QVERIFY2(right.y()<=card->height(),name);
                    QVERIFY2(button->width()>=button->fontMetrics().horizontalAdvance(button->text())+20,qPrintable(QString("%1 %2 %3: %4/%5").arg(language).arg(size.width()).arg(name).arg(button->width()).arg(button->fontMetrics().horizontalAdvance(button->text())+20)));
                }
            }
        }
    }
    void avatarUsesTheHeadAndTransparentHatLayer() {
        for(int unit:{1,2}){auto face=mc::AvatarCache::skinFace(texture(unit)).toImage();QCOMPARE(face.size(),QSize(8*unit,8*unit));QCOMPARE(face.pixelColor(0,0),QColor(Qt::green));QCOMPARE(face.pixelColor(2*unit,2*unit),QColor(Qt::red));}
        QVERIFY(mc::AvatarCache::skinFace("not a png").isNull());QVERIFY(mc::AvatarCache::skinFace(QByteArray(1024*1024+1,'x')).isNull());
        QImage invalid(63,64,QImage::Format_ARGB32);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);invalid.save(&buffer,"PNG");QVERIFY(mc::AvatarCache::skinFace(bytes).isNull());
        QVERIFY(!mc::AvatarCache::defaultAvatar().isNull());
        QTemporaryDir dir;mc::AvatarCache cache(dir.path());mc::AccountAvatar avatar(cache,48);
        const auto rendered=avatar.grab().toImage();
        QCOMPARE(rendered.pixelColor(0,0),mc::AvatarCache::defaultAvatar().toImage().pixelColor(0,0));
        QCOMPARE(rendered.pixelColor(47,47),mc::AvatarCache::defaultAvatar().toImage().pixelColor(7,7));
    }
    void publicTexturesAreCachedWithoutCredentialsOrAutomaticRetries() {
        qApp->setProperty("offlineUiTest",false);QTemporaryDir dir;Network network;auto a=microsoft();
        mc::AvatarCache cache(dir.path(),nullptr,&network);QSignalSpy loaded(&cache,&mc::AvatarCache::avatarChanged);
        cache.avatar(a);QTRY_COMPARE(loaded.size(),1);QCOMPARE(network.requests.size(),1);QVERIFY(network.requests.first().rawHeader("Authorization").isEmpty());
        QCOMPARE(cache.avatar(a).toImage().pixelColor(0,0),QColor(Qt::green));cache.avatar(a);QCOMPARE(network.requests.size(),1);
        mc::AvatarCache cached(dir.path(),nullptr,&network);QCOMPARE(cached.avatar(a).toImage().pixelColor(0,0),QColor(Qt::green));QCOMPARE(network.requests.size(),1);
        network.bytes="bad png";a.skinUrl=QUrl("https://textures.minecraft.net/texture/"+QString(64,'b'));cache.avatar(a);QTest::qWait(20);QCOMPARE(network.requests.size(),2);
        for(int n=0;n<3;++n)cache.avatar(a);QCOMPARE(network.requests.size(),2);QCOMPARE(loaded.size(),1);
        for(const auto url:{"https://example.test/skin.png","http://textures.minecraft.net/texture/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","file:///tmp/skin.png"}){a.skinUrl=QUrl(url);cache.avatar(a);}
        QCOMPARE(network.requests.size(),2);
    }
};
QTEST_MAIN(AccountUiTests)
#include "account_ui_tests.moc"
