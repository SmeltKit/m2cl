// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "core/i18n.h"
#include "core/settings.h"
#include "core/mods.h"
#include "core/resources.h"
#include "network/networkerror.h"
#include "ui/mainwindow.h"
#include "ui/launcherwidgets.h"
#include "ui/launchoptionswidget.h"
#include "ui/modplanview.h"
#include <QCheckBox>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

class I18nTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){qApp->setProperty("offlineUiTest",true);}
    void cleanup(){QVERIFY(mc::installLanguage(*qApp,"zh_CN"));}
    void localeResolutionAndFallback(){
        QCOMPARE(mc::resolvedLanguage("system",QLocale("zh_CN")),QString("zh_CN"));
        QCOMPARE(mc::resolvedLanguage("system",QLocale("zh_TW")),QString("zh_TW"));
        QCOMPARE(mc::resolvedLanguage("system",QLocale("zh_HK")),QString("zh_TW"));
        QCOMPARE(mc::resolvedLanguage("system",QLocale("zh_Hant")),QString("zh_TW"));
        QCOMPARE(mc::resolvedLanguage("system",QLocale("de_DE")),QString("en_US"));
        QCOMPARE(mc::resolvedLanguage("en_US",QLocale("zh_CN")),QString("en_US"));
        QVERIFY(!mc::validLanguage("../../other"));
        QVERIFY(mc::installLanguage(*qApp,"en_US"));
        QVERIFY(!mc::installLanguage(*qApp,"invalid"));
        QCOMPARE(mc::currentLanguage(),QString("en_US"));
    }
    void settingsPreserveStableData(){
        QTemporaryDir dir;mc::SettingsStore store(dir.path());mc::Settings settings;
        settings.instanceNames.insert("/example/instance",QStringLiteral("我的存档"));
        settings.microsoftClientId="796724ab-c62a-48dd-983b-d6fecf88ce48";
        QCOMPARE(store.load().language,QString("system"));
        for(const auto language:{"zh_CN","zh_TW","en_US","system"}){
            settings.language=language;QVERIFY(store.save(settings));
            const auto loaded=store.load();QCOMPARE(loaded.language,settings.language);
            QCOMPARE(loaded.instanceNames,settings.instanceNames);QCOMPARE(loaded.microsoftClientId,settings.microsoftClientId);
        }
        QFile file(dir.filePath("settings.json"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("{\"language\":\"invalid\"}");file.close();
        QCOMPARE(store.load().language,QString("system"));
    }
    void localizedDiagnosticsPreserveCompatibilityChecks(){
        mc::Instance instance;instance.version="1.20.1";instance.loader="fabric";instance.loaderVersion="0.16.10";
        mc::LocalMod mod;mod.name="Local mod";mod.id="local";mod.ids=QStringList{"local"};mod.loader="forge";mod.known=true;
        for(const auto language:{"en_US","zh_TW","zh_CN"}){
            QVERIFY(mc::installLanguage(*qApp,language));
            const auto problem=mc::modEnvironmentProblem(mod,instance);QVERIFY(!problem.isEmpty());
            QVERIFY(problem.contains("Forge",Qt::CaseInsensitive));QVERIFY(problem.contains("Fabric",Qt::CaseInsensitive));
            QVERIFY(mc::diagnosticContains(problem,QT_TRANSLATE_NOOP("M2CL","加载器")));
            QCOMPARE(mc::resourceTagLabel("fabric"),QString("Fabric"));
            QCOMPARE(mc::resourceTagLabel("release"),QCoreApplication::translate("M2CL","正式版"));
            const auto network=mc::networkErrorSummary("HTTP 403\nNetwork error 3 (HostNotFoundError)");
            QVERIFY(network.contains("403"));QVERIFY(network.contains("HostNotFoundError"));
            mc::DownloadPanel panel;panel.setTask("Download",QCoreApplication::translate("M2CL","安装已取消"),-1,false);
            auto notice=panel.findChild<QLabel *>("networkNoticeTitle");QVERIFY(notice);
            QCOMPARE(notice->text(),QCoreApplication::translate("M2CL","任务已取消"));
            if(QString(language)=="en_US"){
                QVERIFY(!problem.contains(QRegularExpression("[\\x{3400}-\\x{9fff}]")));
                QCOMPARE(mc::resourceDownloadCount(1234567),QString("1.2M"));
                QVERIFY(network.contains("Access denied"));
                QCOMPARE(QCoreApplication::translate("M2CL","%n 个实例",nullptr,1),QString("1 instance"));
                QCOMPARE(QCoreApplication::translate("M2CL","%n 个实例",nullptr,2),QString("2 instances"));
            }
        }
    }
    void localizedInstallPlansKeepDependencyChoicesAndFitButtons(){
        for(const auto language:{"en_US","zh_TW"}){
            QVERIFY(mc::installLanguage(*qApp,language));mc::applyTheme(mc::makeTheme("forest",false));
            mc::ModPlan plan;plan.rootProjectId="root";plan.displayTargetName="My instance";
            plan.target.version="1.20.1";plan.target.loader="fabric";plan.target.loaderVersion="0.16.10";
            mc::ResourceFile root;root.projectId="root";root.versionName="Main mod";root.versionNumber="1.0";root.fileName="main.jar";
            auto dependency=root;dependency.projectId="api";dependency.versionName="API dependency";dependency.fileName="api.jar";
            plan.files={root,dependency};plan.resolved=plan.files;plan.edges={{"root","api","","required"}};plan.requiredProjects=QStringList{"api"};
            mc::ModPlanView dialog(plan);dialog.resize(700,520);dialog.show();QCoreApplication::processEvents();
            auto choice=dialog.findChild<QCheckBox *>("dependencyChoice");QVERIFY(choice);QVERIFY(choice->isChecked());
            auto buttons=dialog.findChild<QDialogButtonBox *>();QVERIFY(buttons);auto install=buttons->button(QDialogButtonBox::Ok);QVERIFY(install);
            QVERIFY(install->mapTo(&dialog,QPoint(install->width(),install->height())).x()<dialog.width());
            choice->setChecked(false);QCOMPARE(dialog.selectedPlan().files.size(),1);
            choice->setChecked(true);QCOMPARE(dialog.selectedPlan().files.size(),2);
            if(qEnvironmentVariableIsSet("M2CL_I18N_REVIEW")){
                const auto output=qEnvironmentVariable("M2CL_I18N_REVIEW");QDir().mkpath(output);
                dialog.grab().save(output+"/"+language+"-dependencies.png");
            }
            dialog.reject();
        }
    }
    void languageChoicePersistsWithoutChangingTheRunningSession(){
        QVERIFY(mc::installLanguage(*qApp,"en_US"));
        QTemporaryDir dir;mc::SettingsStore store(dir.path());mc::MainWindow window{store};window.show();QTRY_VERIFY(window.ready());window.changePage(3);
        auto choice=window.findChild<mc::SelectBox *>("interfaceLanguage");QVERIFY(choice);QCOMPARE(choice->count(),4);
        QCOMPARE(choice->currentData().toString(),QString("system"));choice->setCurrentIndex(choice->findData("zh_TW"));
        QCOMPARE(store.load().language,QString("zh_TW"));QCOMPARE(mc::currentLanguage(),QString("en_US"));
        QVERIFY(window.findChild<QLabel *>("languageRestartNotice")->text().contains("next launch"));
        QVERIFY(mc::installLanguage(*qApp,store.load().language));
        mc::MainWindow next{store};next.show();QTRY_VERIFY(next.ready());next.changePage(3);
        QCOMPARE(next.findChild<mc::SelectBox *>("interfaceLanguage")->currentData().toString(),QString("zh_TW"));
        bool found=false;for(auto label:next.findChildren<QLabel *>())found|=label->text()==QStringLiteral("介面語言");QVERIFY(found);
        QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);
        QVERIFY(buttons.button(QDialogButtonBox::Cancel)->text().contains(QStringLiteral("取消")));
    }
};
QTEST_MAIN(I18nTests)
#include "i18n_tests.moc"
