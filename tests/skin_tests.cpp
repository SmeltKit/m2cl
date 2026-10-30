// Copyright (C) 2026 ppm203; GPL-3.0-only with ADDITIONAL_TERMS.md.
#include "core/skins.h"
#include "network/accountmanager.h"
#include "network/offlineskin.h"
#include "ui/accountwidgets.h"
#include "ui/skineditor.h"
#include "ui/launcherwidgets.h"
#include "ui/theme.h"
#include "core/i18n.h"
#include <QApplication>
#include <QBuffer>
#include <QDialogButtonBox>
#include <QFile>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
class SkinTests:public QObject{
    Q_OBJECT
private slots:
    void init(){qApp->setProperty("offlineUiTest",true);mc::installLanguage(*qApp,"zh_CN");qApp->setFont(mc::applicationFont());mc::applyTheme(mc::makeTheme("forest",false));}
    void importsSurviveSourceDeletionAndValidateFormat(){
        QTemporaryDir dir;mc::SkinStore store(dir.path());QString error;
        for(const auto source:{"steve","alex"})for(const auto model:{"classic","slim"}){
            const auto bytes=store.texture({{"source",source},{"model",model}},&error);QVERIFY2(!bytes.isEmpty(),qPrintable(error));QCOMPARE(QImage::fromData(bytes).size(),QSize(64,64));
        }
        const auto path=dir.path()+"/source.png";QImage image(64,64,QImage::Format_ARGB32);image.fill(Qt::red);QVERIFY(image.save(path));
        auto settings=store.importPng(path,true,&error);QVERIFY2(!settings.isEmpty(),qPrintable(error));QVERIFY(QFile::remove(path));
        QCOMPARE(QImage::fromData(store.texture(settings,&error)).pixelColor(0,0),QColor(Qt::red));QVERIFY(error.isEmpty());
        QImage legacy(64,32,QImage::Format_ARGB32);legacy.fill(Qt::blue);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);legacy.save(&buffer,"PNG");
        QVERIFY(mc::validateSkinPng(bytes,true,&error).isEmpty());QVERIFY(!error.isEmpty());QVERIFY(!mc::validateSkinPng(bytes,false,&error).isEmpty());QCOMPARE(mc::expandedSkin(legacy).size(),QSize(64,64));
        QVERIFY(mc::validateSkinPng("not PNG",false,&error).isEmpty());QImage bad(128,128,QImage::Format_ARGB32);QVERIFY(bad.save(path));QVERIFY(store.importPng(path,false,&error).isEmpty());
        QFile corrupted(dir.path()+"/skins/"+settings.value("sha256").toString()+".png");QVERIFY(corrupted.open(QIODevice::WriteOnly));corrupted.write("corrupt");corrupted.close();QVERIFY(store.texture(settings,&error).isEmpty());QVERIFY(!error.isEmpty());
    }
    void editorSavesPerRoleWithoutChangingUuidOrOtherAccounts(){
        QTemporaryDir dir;mc::AccountManager manager(dir.path(),"Player");const auto first=manager.current();QVERIFY(manager.addOffline("OtherPlayer").isEmpty());const auto second=manager.current();
        {mc::SkinEditor editor(manager,first,dir.path());editor.show();auto source=editor.findChild<mc::SelectBox *>("offlineSkinSource");source->setCurrentIndex(source->findData("alex"));QCOMPARE(editor.findChild<mc::SelectBox *>("offlineSkinModel")->currentData().toString(),QString("slim"));editor.reject();}
        QCOMPARE(mc::skinSource(manager.accounts().first().offlineSkin),QString("default"));
        mc::SkinEditor editor(manager,first,dir.path());editor.show();auto source=editor.findChild<mc::SelectBox *>("offlineSkinSource");source->setCurrentIndex(source->findData("alex"));QCoreApplication::processEvents();
        if(qEnvironmentVariableIsSet("M2CL_SKIN_REVIEW"))editor.grab().save(qEnvironmentVariable("M2CL_SKIN_REVIEW")+"/alex-light.png");
        auto preview=editor.findChild<QWidget *>("skinPreview");QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,QPoint(60,100));QTest::mouseMove(preview,QPoint(200,100));QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,QPoint(200,100));
        if(qEnvironmentVariableIsSet("M2CL_SKIN_REVIEW"))editor.grab().save(qEnvironmentVariable("M2CL_SKIN_REVIEW")+"/alex-rotated.png");
        editor.findChild<QPushButton *>("saveOfflineSkin")->click();QCOMPARE(editor.result(),int(QDialog::Accepted));
        const auto saved=mc::AccountStore(dir.path()).load();QCOMPARE(saved.selected,second.id);QCOMPARE(saved.accounts.first().uuid,first.uuid);QCOMPARE(mc::skinSource(saved.accounts.first().offlineSkin),QString("alex"));QVERIFY(mc::slimSkin(saved.accounts.first().offlineSkin));QVERIFY(saved.accounts.last().offlineSkin.isEmpty());
        mc::AvatarCache avatars(dir.path());QVERIFY(avatars.avatar(saved.accounts.first()).toImage()!=mc::AvatarCache::defaultAvatar().toImage());
        mc::applyTheme(mc::makeTheme("forest",true));mc::SkinEditor dark(manager,saved.accounts.first(),dir.path());dark.show();QCoreApplication::processEvents();if(qEnvironmentVariableIsSet("M2CL_SKIN_REVIEW"))dark.grab().save(qEnvironmentVariable("M2CL_SKIN_REVIEW")+"/alex-dark.png");
        const auto file=dir.path()+"/new.png";QImage custom(64,64,QImage::Format_ARGB32);custom.fill(QColor("#7799cc"));QVERIFY(custom.save(file));QVERIFY(dark.importFile(file));QFile::remove(file);dark.findChild<QPushButton *>("saveOfflineSkin")->click();QCOMPARE(mc::skinSource(manager.accounts().first().offlineSkin),QString("local"));
    }
    void editorFitsAllSupportedLanguages(){
        QTemporaryDir dir;mc::AccountManager manager(dir.path(),"gjpqyPlayer");
        for(const auto language:{"zh_CN","zh_TW","en_US"}){
            QVERIFY(mc::installLanguage(*qApp,language));qApp->setFont(mc::applicationFont());mc::SkinEditor editor(manager,manager.current(),dir.path());editor.show();
            auto source=editor.findChild<mc::SelectBox *>("offlineSkinSource");source->setCurrentIndex(source->findData("alex"));QCoreApplication::processEvents();
            const auto save=editor.findChild<QPushButton *>("saveOfflineSkin");QVERIFY(save->isVisible());QVERIFY(save->mapTo(&editor,save->rect().bottomRight()).y()<editor.height());QVERIFY(editor.width()<=800);QVERIFY(editor.height()<=640);
            if(qEnvironmentVariableIsSet("M2CL_SKIN_REVIEW"))editor.grab().save(qEnvironmentVariable("M2CL_SKIN_REVIEW")+"/editor-"+language+".png");
        }
    }
    void runtimeGuardLeavesOnlineAndDefaultLaunchesAlone(){
        mc::InstallRequest request;request.launch=true;request.offlineSkin={{"source","alex"},{"model","slim"}};request.identity.microsoft=true;mc::JavaRuntime java;java.major=17;
        const auto cancel=std::make_shared<std::atomic_bool>(false);QVERIFY(mc::prepareOfflineSkin(request,java,cancel,{}).isEmpty());request.identity.microsoft=false;request.offlineSkin={};QVERIFY(mc::prepareOfflineSkin(request,java,cancel,{}).isEmpty());
        request.offlineSkin={{"source","alex"},{"model","slim"}};request.version="1.7.10";QVERIFY_EXCEPTION_THROWN(mc::prepareOfflineSkin(request,java,cancel,{}),std::runtime_error);request.version="1.20.1";java.major=7;QVERIFY_EXCEPTION_THROWN(mc::prepareOfflineSkin(request,java,cancel,{}),std::runtime_error);
    }
};
QTEST_MAIN(SkinTests)
#include "skin_tests.moc"
