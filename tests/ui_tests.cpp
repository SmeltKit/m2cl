// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "ui/accountspanel.h"
#include "ui/javamanager.h"
#include <QtConcurrent/QtConcurrentRun>
#include "ui/networknotice.h"
#include "ui/mainwindow.h"
#include "ui/projectlegal.h"
#include "ui/versioncomponentsdialog.h"
#include "ui/modversionview.h"
#include "ui/modplanview.h"
#include "ui/instancemods.h"
#include "core/compatibility.h"
#include <QTabWidget>
#include "ui/launchoptionswidget.h"
#include "ui/launcherwidgets.h"
#ifndef Q_MOC_RUN
#include "mods_test_helpers.h"
#endif
#include "ui/projectreadme.h"
#include <QRadioButton>
#include <QBuffer>
#include <QApplication>
#include <QPainter>
#include <QAccessible>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLabel>
#include <QImageReader>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QSpinBox>
#include <QSlider>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QScrollBar>
#include <QScrollArea>
#include <QStackedWidget>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrlQuery>
#include <QTableWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>

class UiTests : public QObject {
    Q_OBJECT
    static bool fixture(const QString &directory,int count=0) {
        const auto root=directory+"/minecraft";QDir().mkpath(root+"/versions");
        const QStringList versions{"1.20.1","1.18.2","1.7.10","1.8.9"};
        for(int i=0;i<count;++i){const auto id=versions.at(i);QDir().mkpath(root+"/versions/"+id);QFile file(root+"/versions/"+id+"/"+id+".json");
            if(!file.open(QIODevice::WriteOnly))return false;file.write(QJsonDocument(QJsonObject{{"id",id}}).toJson());}
        mc::Settings settings;settings.gameRoots=QStringList{root};return mc::SettingsStore(directory).save(settings);
    }
private slots:
    void projectAttributionAndFullLicensesAreAvailableOffline() {
        QTemporaryDir dir;
        QVERIFY(fixture(dir.path()));
        mc::MainWindow window{mc::SettingsStore(dir.path())};
        window.show();
        QTRY_VERIFY(window.ready());
        window.changePage(3);
        auto attribution=window.findChild<QLabel *>("aboutProjectAttribution");
        QVERIFY(attribution);
        QVERIFY(attribution->text().contains("ppm203"));
        auto qtNotice=window.findChild<QLabel *>("aboutQtNotice");
        QVERIFY(qtNotice);
        QVERIFY(qtNotice->text().contains(qVersion()));
        QVERIFY(qtNotice->text().contains("The Qt Company Ltd."));
        QVERIFY(qtNotice->text().contains("LGPLv3"));
        auto legal=window.findChild<QPushButton *>("showProjectLicenses");
        QVERIFY(legal);
        bool opened=false;
        QTimer::singleShot(20,&window,[&]{
            if(auto dialog=window.findChild<QDialog *>("projectLegalDialog")){
                opened=dialog->property("installationDialog").toBool();
                dialog->reject();
            }
        });
        legal->click();
        QVERIFY(opened);
        mc::ProjectLegalDialog dialog(&window);
        dialog.show();
        auto tabs=dialog.findChild<QTabWidget *>("projectLegalTabs");
        QVERIFY(tabs);
        QCOMPARE(tabs->count(),5);
        const auto gpl=dialog.findChild<QPlainTextEdit *>("projectGplText");
        QVERIFY(gpl->isReadOnly());
        QVERIFY(gpl->toPlainText().contains("Version 3, 29 June 2007"));
        QVERIFY(gpl->toPlainText().contains("17. Interpretation of Sections 15 and 16."));
        QVERIFY(gpl->toPlainText().contains("How to Apply These Terms to Your New Programs"));
        const auto terms=dialog.findChild<QPlainTextEdit *>("projectTermsText")->toPlainText();
        QVERIFY(terms.contains("Third-party modified version based on M²CL"));
        QVERIFY(terms.contains("do not\nprohibit forks, commercial distribution"));
        QVERIFY(dialog.findChild<QPlainTextEdit *>("projectNoticeText")->toPlainText().contains("Original project author: ppm203"));
        QVERIFY(dialog.findChild<QPlainTextEdit *>("projectMd4cText")->toPlainText().contains("Permission is hereby granted"));
        auto qtSelector=dialog.findChild<mc::SelectBox *>("qtLegalDocument");
        auto qtText=dialog.findChild<QPlainTextEdit *>("qtLegalText");
        QVERIFY(qtSelector&&qtText&&qtText->isReadOnly());
        QCOMPARE(qtSelector->count(),6);
        QVERIFY(qtText->toPlainText().contains("dynamically linked"));
        qtSelector->setCurrentIndex(1);
        QVERIFY(qtText->toPlainText().contains("GNU LESSER GENERAL PUBLIC LICENSE"));
        QVERIFY(qtText->toPlainText().contains("6. Revised Versions of the GNU Lesser General Public License."));
        qtSelector->setCurrentIndex(3);
        QVERIFY(qtText->toPlainText().contains("Google Inc."));
        QVERIFY(qtText->toPlainText().contains("Mozilla Public License"));
        qtSelector->setCurrentIndex(4);
        QVERIFY(qtText->toPlainText().contains("383dc907816338f0cba72088a524c07458dfc69ce684ca9132fcc4fe91c24b0b"));
        QVERIFY(qtText->toPlainText().contains("reverse engineering"));
        qtSelector->setCurrentIndex(5);
        QVERIFY(qtText->toPlainText().contains("libqwebp.dylib"));
        qtSelector->setCurrentIndex(0);
        for(bool dark:{false,true}){
            window.setAppearance(dark?"dark":"light","forest");
            dialog.resize(680,520);
            for(int i=0;i<tabs->count();++i){
                tabs->setCurrentIndex(i);QCoreApplication::processEvents();
                auto text=qobject_cast<QPlainTextEdit *>(tabs->currentWidget());
                if(!text)text=tabs->currentWidget()->findChild<QPlainTextEdit *>();
                QVERIFY(text&&text->isVisible());
                QVERIFY(text->viewport()->width()>500);
                QVERIFY(text->viewport()->height()>150);
            }
            tabs->setCurrentIndex(4);
            if(qEnvironmentVariableIsSet("M2CL_LEGAL_REVIEW")){
                const auto output=qEnvironmentVariable("M2CL_LEGAL_REVIEW");QDir().mkpath(output);
                dialog.grab().save(output+(dark?"/legal-dark.png":"/legal-light.png"));
            }
        }
        window.setAppearance("light","forest");
        dialog.reject();
    }
    void accountsUseSharedNavigationAndOpeningLoginHasNoNetworkSideEffects() {
        if(qEnvironmentVariableIsSet("M2CL_ACCOUNT_REVIEW"))QDir().mkpath(qEnvironmentVariable("M2CL_ACCOUNT_REVIEW"));
        QTemporaryDir dir;QVERIFY(fixture(dir.path(),1));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());window.showAccounts();auto panel=window.findChild<mc::AccountsPanel *>();QVERIFY(panel->isVisible());QVERIFY(window.findChild<QPushButton *>("accountsSettingsTab")->isChecked());auto manager=window.findChild<mc::AccountManager *>();QCOMPARE(manager->accounts().size(),1);QVERIFY(!manager->current().microsoft);QVERIFY(!manager->busy());
        const bool configured=mc::MicrosoftAuth::validClientId(mc::AccountsPanel::configuredClientId(mc::Settings{}));const auto before=mc::AccountStore(dir.path()).load().selected;bool inspected=false;QTimer::singleShot(20,&window,[&]{auto dialog=window.findChild<mc::MicrosoftLoginDialog *>();if(!dialog)return;inspected=dialog->property("installationDialog").toBool()&&dialog->findChild<mc::NetworkNotice *>("accountLoginNetworkState")->isVisible()!=configured&&dialog->findChild<QPushButton *>("startMicrosoftLogin")->isEnabled()==configured;if(qEnvironmentVariableIsSet("M2CL_ACCOUNT_REVIEW"))dialog->grab().save(qEnvironmentVariable("M2CL_ACCOUNT_REVIEW")+(configured?"/login-ready.png":"/login-unconfigured.png"));dialog->reject();});panel->findChild<QPushButton *>("addMicrosoftAccount")->click();QVERIFY(inspected);QVERIFY(!manager->busy());QCOMPARE(mc::AccountStore(dir.path()).load().selected,before);
        for(bool dark:{false,true}){window.setAppearance(dark?"dark":"light","forest");for(const auto size:{QSize(1140,780),QSize(840,620)}){window.resize(size);QCoreApplication::processEvents();auto table=panel->findChild<QTableWidget *>("accountsTable");QVERIFY(table->isVisible());auto add=panel->findChild<QPushButton *>("addMicrosoftAccount");QVERIFY(add->mapTo(&window,QPoint(add->width(),0)).x()<window.width());if(qEnvironmentVariableIsSet("M2CL_ACCOUNT_REVIEW")){QDir().mkpath(qEnvironmentVariable("M2CL_ACCOUNT_REVIEW"));window.grab().save(qEnvironmentVariable("M2CL_ACCOUNT_REVIEW")+QString("/accounts-%1-%2.png").arg(dark?"dark":"light").arg(size.width()));}}}
        window.setAppearance("light","forest");
    }
    void missingMicrosoftApplicationStillShowsConfigurationInsteadOfStartingLogin() {
        QTemporaryDir dir;mc::AccountManager manager(dir.path(),"Player");mc::MicrosoftLoginDialog dialog(manager,{});dialog.show();QVERIFY(dialog.findChild<mc::NetworkNotice *>("accountLoginNetworkState")->isVisible());QVERIFY(!dialog.findChild<QPushButton *>("startMicrosoftLogin")->isEnabled());QVERIFY(dialog.findChild<QPushButton *>("configureMicrosoftLogin")->isVisible());QVERIFY(!manager.busy());dialog.reject();
    }
    void minecraftApplicationDenialsHaveHelpWithoutMisclassifyingEveryForbiddenResponse() {
        QTemporaryDir dir;mc::AccountManager manager(dir.path(),"Player");mc::MicrosoftLoginDialog dialog(manager,"796724ab-c62a-48dd-983b-d6fecf88ce48");dialog.show();auto help=dialog.findChild<QPushButton *>("minecraftApplicationAccessHelp");auto start=dialog.findChild<QPushButton *>("startMicrosoftLogin");QVERIFY(!help->isVisible());manager.failed({QStringLiteral("访问被拒绝"),"HTTP 403","minecraft_app_registration_rejected",false});QVERIFY(help->isVisible());QVERIFY(!start->isEnabled());manager.failed({QStringLiteral("访问被拒绝"),"HTTP 403","minecraft_login_forbidden",false});QVERIFY(help->isVisible());QVERIFY(start->isEnabled());manager.failed({QStringLiteral("网络错误"),"Timeout","0",false});QVERIFY(!help->isVisible());QVERIFY(start->isEnabled());dialog.reject();
    }
    void configuredMicrosoftLoginUsesTheSelectedPublicApplicationAndCancellationStopsIt() {
        QTemporaryDir dir;mc::Settings settings;settings.microsoftClientId="796724ab-c62a-48dd-983b-d6fecf88ce48";mc::AccountManager manager(dir.path(),"Player");QSignalSpy requested(&manager,&mc::AccountManager::browserRequested);mc::MicrosoftLoginDialog dialog(manager,mc::AccountsPanel::configuredClientId(settings));dialog.show();auto start=dialog.findChild<QPushButton *>("startMicrosoftLogin");QVERIFY(start->isEnabled());start->click();QCOMPARE(requested.size(),1);const QUrlQuery query(requested.first().first().toUrl());QCOMPARE(query.queryItemValue("client_id"),settings.microsoftClientId);QCOMPARE(query.queryItemValue("redirect_uri").section(':',0,1),QString("http://localhost"));QVERIFY(manager.busy());dialog.reject();QVERIFY(!manager.busy());QVERIFY(!manager.current().microsoft);
    }
    void offlineAccountSelectionPersistsAndCancelledCreationIsInert() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path(),1));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());window.showAccounts();auto add=window.findChild<QPushButton *>("addOfflineAccount");bool filled=false;QTimer::singleShot(20,&window,[&]{auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog)return;auto input=dialog->findChild<QLineEdit *>("newOfflinePlayer");if(!input)return;input->setText("Second_Player");filled=true;dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});add->click();QVERIFY(filled);auto manager=window.findChild<mc::AccountManager *>();QCOMPARE(manager->current().name,QString("Second_Player"));QCOMPARE(manager->accounts().size(),2);QCOMPARE(mc::AccountStore(dir.path()).load().selected,manager->current().id);QVERIFY(window.findChild<QPushButton *>("accountSummaryButton")->text().contains("Second"));
        QTimer::singleShot(20,&window,[&]{if(auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget()))dialog->reject();});add->click();QCOMPARE(manager->accounts().size(),2);
    }
    void expiredMicrosoftAccountOffersExplicitRelogin() {
        QTemporaryDir dir;mc::Account a;a.microsoft=true;a.name="RealPlayer";a.uuid="0123456789abcdef0123456789abcdef";a.id="microsoft:"+a.uuid;a.clientId="11111111-2222-4333-8444-555555555555";QVERIFY(mc::AccountStore(dir.path()).save({a},a.id));mc::AccountManager manager(dir.path(),"Player");mc::MicrosoftLoginDialog dialog(manager,a.clientId,a.id,true);dialog.show();auto notice=dialog.findChild<mc::NetworkNotice *>("accountLoginNetworkState");QTRY_VERIFY(notice->isVisible());QCOMPARE(dialog.findChild<QPushButton *>("startMicrosoftLogin")->text(),QStringLiteral("重新登录"));QVERIFY(!manager.busy());QVERIFY(manager.current().microsoft);QVERIFY(dialog.findChild<QCheckBox *>("rememberMicrosoftAccount")->isVisible());dialog.reject();QVERIFY(!manager.busy());
    }
    void publicMicrosoftApplicationConfigurationExpandsAndPersists() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());window.showAccounts();auto panel=window.findChild<mc::AccountsPanel *>();auto config=panel->findChild<QWidget *>("microsoftApplicationConfiguration");QVERIFY(!config->isVisible());panel->showConfiguration();QVERIFY(config->isVisible());auto input=panel->findChild<QLineEdit *>("microsoftClientId");input->setText("invalid");panel->findChild<QPushButton *>("saveMicrosoftClientId")->click();QVERIFY(mc::SettingsStore(dir.path()).load().microsoftClientId.isEmpty());QVERIFY(config->isVisible());input->setText("11111111-2222-4333-8444-555555555555");panel->findChild<QPushButton *>("saveMicrosoftClientId")->click();QCOMPARE(mc::SettingsStore(dir.path()).load().microsoftClientId,input->text());QVERIFY(!config->isVisible());QVERIFY(!panel->findChild<mc::NetworkNotice *>("accountsNetworkState")->isVisible());
    }
    void gameDownloadSourcePersistsThroughTheSharedSelector() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));mc::SettingsStore store(dir.path());
        mc::MainWindow window{store};window.show();QTRY_VERIFY(window.ready());window.changePage(3);
        auto choice=window.findChild<mc::SelectBox *>("downloadSource");QVERIFY(choice);QCOMPARE(choice->count(),3);QCOMPARE(choice->currentData().toString(),QString("auto"));
        choice->setCurrentIndex(choice->findData("bmclapi"));QCOMPARE(store.load().downloadSource,QString("bmclapi"));
        choice->setCurrentIndex(choice->findData("official"));QCOMPARE(store.load().downloadSource,QString("official"));
    }
    void installVersionPopupPreservesManifestData() {
        mc::SelectBox box; box.setProperty("popupMode", "versions");
        const QJsonObject release{{"id","1.21.1"},{"type","release"},{"url","https://example.test/version.json"}};
        box.addItem("1.21.1",release); box.setItemData(0,"release",mc::SelectBox::PopupGroupRole);
        box.resize(640,40);box.show();box.showPopup();
        auto popup=box.findChild<QFrame *>("selectPopup");
        auto list=popup->findChild<QListWidget *>("versionChoices");
        QVERIFY(list->isVisible());QCOMPARE(list->item(0)->text(),QString("1.21.1"));
        list->itemClicked(list->item(0));QCOMPARE(box.currentData().toJsonObject(),release);
        box.addItem("24w14potato",QJsonObject{{"id","24w14potato"},{"type","snapshot"}});
        box.setItemData(1,"special",mc::SelectBox::PopupGroupRole);box.setCurrentIndex(1);
        box.showPopup();popup=qobject_cast<QFrame *>(QApplication::activePopupWidget());QVERIFY(popup);
        auto tabs=popup->findChild<QTabBar *>("versionChannels");QCOMPARE(tabs->tabData(tabs->currentIndex()).toString(),QString("special"));
        list=popup->findChild<QListWidget *>("versionChoices");QVERIFY(list->item(0)->isHidden());QVERIFY(!list->item(1)->isHidden());
        box.hidePopup();
    }

    void managedVersionComponentsHaveASeparateActionAndShaderRoute() {
        QTemporaryDir dir;auto instance=target(dir.filePath("game"));instance.loader="forge";instance.loaderVersion="47.2.18";instance.optifineVersion="HD_U_I6";instance.optifineForgeVersion="47.2.18";instance.optifineFile="OptiFine.jar";
        const QByteArray of=zip({{"META-INF/mods.toml","modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\n[[mods]]\nmodId=\"optifine\"\nversion=\"HD_U_I6\"\ndisplayName=\"OptiFine\"\n"}});
        QVERIFY(save(instance.gameDirectory+"/mods/OptiFine.jar",of));instance.optifineSha512=QString::fromLatin1(mc::fileSha512(instance.gameDirectory+"/mods/OptiFine.jar"));
        mc::InstanceModsView view(instance,QStringLiteral("日常生存"));view.resize(1120,720);view.show();auto table=view.findChild<QTableWidget *>("installedModsTable");QTRY_COMPARE(table->rowCount(),1);
        QCOMPARE(table->item(0,4)->text(),QStringLiteral("版本组件"));auto toggle=table->cellWidget(0,3)->findChild<QCheckBox *>();QVERIFY(toggle&&!toggle->isEnabled());
        table->selectRow(0);auto manage=view.findChild<QPushButton *>("manageSelectedVersionComponent");QVERIFY(manage->isVisible());QVERIFY(!view.findChild<QPushButton *>("disableSelectedMod")->isEnabled());QVERIFY(!view.findChild<QPushButton *>("removeInstalledMod")->isEnabled());
        QSignalSpy requests(&view,&mc::InstanceModsView::versionComponentsRequested);manage->click();QCOMPARE(requests.size(),1);
        auto tabs=view.findChild<QTabWidget *>("instanceResourceTabs");tabs->setCurrentIndex(1);tabs->currentWidget()->findChild<QPushButton *>("manageShaderSupport")->click();QCOMPARE(requests.size(),2);QCOMPARE(requests.last().first().toString(),instance.path);
        auto cleared=instance;cleared.optifineVersion.clear();cleared.optifineFile.clear();cleared.optifineSha512.clear();view.updateInstance(cleared);QTRY_VERIFY(table->isEnabled());QTRY_VERIFY(table->cellWidget(0,3)->findChild<QCheckBox *>()->isEnabled());
    }
    void versionComponentDialogUsesSharedShellAndCancelIsInert() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path(),1));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());
        const auto path=QFileInfo(dir.path()+"/minecraft/versions/1.20.1").canonicalFilePath();bool inspected=false;
        QTimer::singleShot(50,&window,[&]{auto dialog=window.findChild<QDialog *>("versionComponents");if(!dialog)return;inspected=dialog->property("installationDialog").toBool()&&dialog->findChild<QListWidget *>("optifineBuildList");
            auto source=dialog->findChild<QComboBox *>("componentDownloadSource");inspected=inspected&&source&&source->count()==3;if(source)source->setCurrentIndex(source->findData("bmclapi"));dialog->reject();});
        window.showVersionComponents(path);QVERIFY(inspected);QCOMPARE(mc::SettingsStore(dir.path()).load().downloadSource,QString("bmclapi"));QCOMPARE(window.findChild<QComboBox *>("downloadSource")->currentData().toString(),QString("bmclapi"));QVERIFY(!QDir(path+"/mods").exists());QVERIFY(!QFile::exists(path+"/.m2cl-instance.json"));
    }
    void componentLayoutSeparatesInstalledReinstallAndIncompatibleBuilds() {
        mc::applyTheme(mc::makeTheme("forest",false));mc::Instance instance;instance.version="1.20.1";instance.loader="forge";instance.loaderVersion="47.2.18";instance.optifineVersion="HD_U_I6";
        mc::VersionComponentsDialog dialog(instance,QStringLiteral("OptiFine 实测实例"),"bmclapi");dialog.show();
        const QJsonObject installed{{"game","1.20.1"},{"version","HD_U_I6"},{"forge","47.2.18"},{"preview",false}};
        const QJsonObject mismatch{{"game","1.20.1"},{"version","HD_U_I5"},{"forge","47.0.35"},{"preview",false}};
        auto preview=installed;preview.insert("version","HD_U_I6_pre1");preview.insert("preview",true);
        dialog.setBuilds({QJsonArray{installed,mismatch,preview},{}});
        auto list=dialog.findChild<QListWidget *>("optifineBuildList");auto primary=dialog.findChild<QPushButton *>("reviewOptifinePlan");auto channels=dialog.findChild<QTabBar *>("optifineChannels");
        QCOMPARE(list->count(),2);QCOMPARE(dialog.selectedBuild(),installed);QVERIFY(!primary->isEnabled());QCOMPARE(primary->text(),QStringLiteral("已安装"));
        list->setCurrentRow(1);QVERIFY(!primary->isEnabled());QVERIFY(dialog.findChild<QLabel *>("optifineSelectionMessage")->text().contains("47.0.35"));
        channels->setCurrentIndex(1);QCOMPARE(list->count(),1);QVERIFY(primary->isEnabled());QCOMPARE(dialog.selectedBuild(),preview);
        auto reinstall=dialog.findChild<QAction *>("reinstallOptifine");QVERIFY(reinstall&&reinstall->isEnabled());reinstall->trigger();QCOMPARE(channels->currentIndex(),0);QCOMPARE(dialog.selectedBuild(),installed);QVERIFY(primary->isEnabled());QCOMPARE(primary->text(),QStringLiteral("重新安装"));
        // Changing selection cancels explicit reinstall intent; it cannot leak to other builds.
        list->setCurrentRow(1);list->setCurrentRow(0);QVERIFY(!primary->isEnabled());
        for(const auto &appearance:{QString("light"),QString("dark")})for(const auto &size:{QSize(840,730),QSize(700,630)}){
            mc::applyTheme(mc::makeTheme("forest",appearance=="dark"));dialog.resize(size);QCoreApplication::processEvents();
            auto close=dialog.findChild<QPushButton *>("closeVersionComponents");const auto corner=primary->mapTo(&dialog,QPoint(primary->width(),primary->height()));QVERIFY(corner.x()<=dialog.width()-12);QVERIFY(corner.y()<=dialog.height()-12);QVERIFY(primary->mapTo(&dialog,QPoint()).x()>close->mapTo(&dialog,QPoint()).x());
            if(qEnvironmentVariableIsSet("M2CL_COMPONENT_REVIEW")){const auto out=qEnvironmentVariable("M2CL_COMPONENT_REVIEW");QDir().mkpath(out);dialog.grab().save(out+QString("/components-%1-%2.png").arg(appearance).arg(size.width()));}
        }
        mc::applyTheme(mc::makeTheme("forest",false));
        auto source=dialog.findChild<QComboBox *>("componentDownloadSource");QSignalSpy changed(&dialog,&mc::VersionComponentsDialog::downloadSourceChanged);source->setCurrentIndex(source->findData("official"));QCOMPARE(changed.size(),1);QCOMPARE(changed.first().first().toString(),QString("official"));QCOMPARE(list->count(),0);QVERIFY(!primary->isEnabled());
        dialog.setBuilds({{},"Offline"});QVERIFY(!primary->isEnabled());QVERIFY(dialog.findChild<mc::NetworkNotice *>("optifineNetworkState")->isVisible());QVERIFY(!reinstall->isEnabled());
        dialog.setBuilds({QJsonArray{installed},{}});QVERIFY(dialog.findChild<mc::NetworkNotice *>("optifineNetworkState")->isHidden());QVERIFY(!primary->isEnabled());
    }
    void resourceToolbarDoesNotStretchVertically() {
        QWidget page;auto body=new QVBoxLayout(&page);auto toolbar=new mc::ResourceToolbar(QStringLiteral("模组"));body->addWidget(toolbar);body->addStretch();
        page.resize(1000,600);page.show();QTest::qWait(10);const int height=toolbar->height();
        page.resize(1400,1100);QTest::qWait(10);QCOMPARE(toolbar->height(),height);
        QVERIFY(toolbar->search->geometry().top()-toolbar->add->geometry().bottom()<24);
    }
    void instanceResourceTabsShowLocalPacks(){
        mc::applyTheme(mc::makeTheme("forest",false));
        QTemporaryDir dir;auto instance=target(dir.filePath("game"));
        QVERIFY(save(instance.gameDirectory+"/shaderpacks/Example.zip",zip({{"shaders/main.fsh","void main(){}"}})));
        mc::InstanceModsView view(instance,QStringLiteral("日常生存"));view.resize(1120,720);view.show();
        auto tabs=view.findChild<QTabWidget *>("instanceResourceTabs");QVERIFY(tabs);QCOMPARE(tabs->count(),3);tabs->setCurrentIndex(1);QCoreApplication::processEvents();
        auto table=tabs->currentWidget()->findChild<QTableWidget *>();QVERIFY(table);QCOMPARE(table->rowCount(),1);QCOMPARE(table->item(0,2)->text(),QStringLiteral("已安装"));QCOMPARE(table->item(0,1)->text(),QStringLiteral("—"));
        if(qEnvironmentVariableIsSet("M2CL_RESOURCE_REVIEW")){auto out=qEnvironmentVariable("M2CL_RESOURCE_REVIEW");QDir().mkpath(out);view.grab().save(out+"/shaders-light.png");mc::applyTheme(mc::makeTheme("forest",true));QCoreApplication::processEvents();view.grab().save(out+"/shaders-dark.png");mc::applyTheme(mc::makeTheme("forest",false));}
    }

    void sharedChoicesKeepNativeKeyboardAndAccessibility(){
        QWidget host;auto layout=new QVBoxLayout(&host);auto check=new mc::CheckBox(QStringLiteral("自定义窗口大小"));layout->addWidget(check);
        auto first=new mc::RadioButton(QStringLiteral("自动分配"));auto second=new mc::RadioButton(QStringLiteral("手动指定"));layout->addWidget(first);layout->addWidget(second);host.show();
        auto accessible=QAccessible::queryAccessibleInterface(check);QVERIFY(accessible);QCOMPARE(accessible->role(),QAccessible::CheckBox);
        QTest::keyClick(check,Qt::Key_Space);QVERIFY(check->isChecked());QVERIFY(accessible->state().checked);
        QTest::mouseClick(check,Qt::LeftButton,Qt::NoModifier,QPoint(check->width()-8,check->height()/2));QVERIFY(!check->isChecked());
        check->setTristate(true);check->setCheckState(Qt::PartiallyChecked);QVERIFY(accessible->state().checkStateMixed);QTest::keyClick(check,Qt::Key_Space);QCOMPARE(check->checkState(),Qt::Checked);
        check->setEnabled(false);check->click();QVERIFY(check->isChecked());QVERIFY(accessible->state().disabled);
        first->setChecked(true);second->setFocus(Qt::TabFocusReason);QTRY_VERIFY(second->hasFocus());QTest::keyClick(second,Qt::Key_Space);QVERIFY(second->isChecked());QVERIFY(!first->isChecked());QTest::keyClick(second,Qt::Key_Up);QVERIFY(first->isChecked());QVERIFY(!second->isChecked());
    }
    void automaticMemoryCanBeEditedRestoredAndRefreshed(){
        mc::MemorySnapshot snapshot;snapshot.totalMiB=16384;snapshot.availableMiB=10240;snapshot.source="test";
        mc::MemoryContext context;context.gameVersion="1.20.1";context.javaArchitecture="aarch64";
        mc::LaunchOptions options;options.autoMemory=true;options.maxMemoryMiB=8192;options.minMemoryMiB=512;options.width=1280;options.jvmArguments="-XX:+UseG1GC";
        mc::LaunchOptionsWidget view(options,context,nullptr,[&]{return snapshot;});view.resize(560,900);view.show();
        auto maximum=view.findChild<QSpinBox *>("instanceMaxMemory");auto initial=view.findChild<QSpinBox *>("instanceMinMemory");auto automatic=view.findChild<QRadioButton *>("instanceMemoryAuto");auto manual=view.findChild<QRadioButton *>("instanceMemoryManual");auto use=view.findChild<QPushButton *>("useRecommendedMemory");
        QVERIFY(automatic->isChecked());QCOMPARE(maximum->value(),3072);QCOMPARE(view.options().maxMemoryMiB,8192);QVERIFY(view.options().autoMemory);
        maximum->setValue(6144);QVERIFY(manual->isChecked());QVERIFY(!view.options().autoMemory);QCOMPARE(view.options().maxMemoryMiB,6144);
        snapshot.availableMiB=4096;view.refreshRecommendation();QCOMPARE(maximum->value(),6144);QVERIFY(manual->isChecked());
        use->click();QVERIFY(automatic->isChecked());QCOMPARE(maximum->value(),2048);QCOMPARE(initial->value(),0);QCOMPARE(view.options().width,1280);QCOMPARE(view.options().jvmArguments,QString("-XX:+UseG1GC"));
        auto editor=initial->findChild<QLineEdit *>();QVERIFY(editor);QVERIFY(editor->text().isEmpty());QCOMPARE(editor->placeholderText(),QStringLiteral("由 Java 决定"));
        initial->setValue(1024);editor->setFocus();editor->selectAll();QTest::keyClick(editor,Qt::Key_Backspace);QTest::keyClick(editor,Qt::Key_Tab);QCOMPARE(view.options().minMemoryMiB,0);QVERIFY(editor->text().isEmpty());
        editor->setFocus();QTest::keyClicks(editor,"512");QTest::keyClick(editor,Qt::Key_Tab);QCOMPARE(view.options().minMemoryMiB,512);editor->selectAll();QTest::keyClick(editor,Qt::Key_Backspace);QTest::keyClick(editor,Qt::Key_Tab);QCOMPARE(view.options().minMemoryMiB,0);
        initial->stepUp();QCOMPARE(initial->value(),256);initial->stepDown();QVERIFY(editor->text().isEmpty());
        manual->click();QCOMPARE(maximum->value(),6144);automatic->click();QCOMPARE(maximum->value(),2048);
        snapshot.availableMiB=10240;view.refreshRecommendation();QCOMPARE(maximum->value(),3072);QVERIFY(automatic->isChecked());
        context.javaArchitecture="x86";view.setMemoryContext(context);QCOMPARE(maximum->value(),768);initial->setValue(1024);QVERIFY(!view.selectionError().isEmpty());use->click();QVERIFY(view.selectionError().isEmpty());
        snapshot.error="unavailable";view.refreshRecommendation();QVERIFY(!use->isEnabled());QVERIFY(view.findChild<QLabel *>("memoryRecommendationHint")->text().contains("unavailable"));manual->click();QCOMPARE(maximum->value(),6144);
    }
    void instanceLaunchOptionsSaveCancelAndRemainSeparate(){
        QTemporaryDir dir;QVERIFY(fixture(dir.path(),2));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());
        const auto first=QFileInfo(dir.path()+"/minecraft/versions/1.20.1").canonicalFilePath(),second=QFileInfo(dir.path()+"/minecraft/versions/1.18.2").canonicalFilePath();
        window.editInstance(first,3);auto dialog=window.findChild<QDialog *>("instanceSettings");QVERIFY(dialog);auto options=dialog->findChild<mc::LaunchOptionsWidget *>();QVERIFY(options);
        if(qEnvironmentVariableIsSet("M2CL_AUTO_MEMORY_REVIEW")){
            dialog->resize(730,760);
            const auto output=qEnvironmentVariable("M2CL_AUTO_MEMORY_REVIEW");QDir().mkpath(output);window.setAppearance("light","forest");QCoreApplication::processEvents();dialog->grab().save(output+"/automatic-light.png");window.setAppearance("dark","forest");QCoreApplication::processEvents();dialog->grab().save(output+"/automatic-dark.png");window.setAppearance("light","forest");
            auto resolution=dialog->findChild<QCheckBox *>("instanceCustomResolution");resolution->setChecked(true);dialog->findChild<QSpinBox *>("instanceWindowWidth")->setValue(1280);dialog->findChild<QSpinBox *>("instanceWindowHeight")->setValue(720);QTest::qWait(150);dialog->grab().save(output+"/window-enabled-light.png");dialog->findChild<QSlider *>("instanceMemorySlider")->grab().save(output+"/slider-light.png");window.setAppearance("dark","forest");QCoreApplication::processEvents();dialog->grab().save(output+"/window-enabled-dark.png");dialog->findChild<QSlider *>("instanceMemorySlider")->grab().save(output+"/slider-dark.png");window.setAppearance("light","forest");resolution->setChecked(false);
        }
        dialog->findChild<QSpinBox *>("instanceMaxMemory")->setValue(4096);dialog->findChild<QSpinBox *>("instanceMinMemory")->setValue(512);dialog->findChild<QPlainTextEdit *>("instanceJvmArguments")->setPlainText("-Dlabel=\"hello world\"");dialog->findChild<QCheckBox *>("instanceCustomResolution")->setChecked(true);dialog->findChild<QSpinBox *>("instanceWindowWidth")->setValue(1280);
        if(qEnvironmentVariableIsSet("M2CL_LAUNCH_UI_REVIEW")){
            const auto output=qEnvironmentVariable("M2CL_LAUNCH_UI_REVIEW");QDir().mkpath(output);window.setAppearance("light","forest");QCoreApplication::processEvents();dialog->grab().save(output+"/settings-light.png");
            auto scroll=dialog->findChild<QScrollArea *>("instanceLaunchScroll");scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());QCoreApplication::processEvents();dialog->grab().save(output+"/arguments-light.png");window.setAppearance("dark","forest");QCoreApplication::processEvents();dialog->grab().save(output+"/arguments-dark.png");scroll->verticalScrollBar()->setValue(0);QCoreApplication::processEvents();dialog->grab().save(output+"/settings-dark.png");window.setAppearance("light","forest");
        }
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();QTRY_VERIFY(!dialog->isVisible());QCOMPARE(mc::loadInstanceLaunchOptions(first).maxMemoryMiB,4096);QCOMPARE(mc::loadInstanceLaunchOptions(second).maxMemoryMiB,2048);QCOMPARE(mc::SettingsStore(dir.path()).load().memoryMiB,2048);
        QTest::qWait(1);window.editInstance(first,3);dialog=window.findChild<QDialog *>("instanceSettings");QCOMPARE(dialog->findChild<QSpinBox *>("instanceMaxMemory")->value(),4096);dialog->findChild<QSpinBox *>("instanceMaxMemory")->setValue(8192);dialog->reject();QCOMPARE(mc::loadInstanceLaunchOptions(first).maxMemoryMiB,4096);
        QTest::qWait(1);window.editInstance(second,3);dialog=window.findChild<QDialog *>("instanceSettings");dialog->findChild<QPlainTextEdit *>("instanceJvmArguments")->setPlainText("-Xmx8G");dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();QVERIFY(dialog->isVisible());QVERIFY(!dialog->findChild<mc::LaunchOptionsWidget *>()->selectionError().isEmpty());QCOMPARE(mc::loadInstanceLaunchOptions(second).maxMemoryMiB,2048);dialog->reject();
        QTest::qWait(1);window.editInstance(first,3);dialog=window.findChild<QDialog *>("instanceSettings");dialog->findChild<QPushButton *>("useRecommendedMemory")->click();dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();QTRY_VERIFY(!dialog->isVisible());const auto automatic=mc::loadInstanceLaunchOptions(first);QVERIFY(automatic.autoMemory);QCOMPARE(automatic.maxMemoryMiB,4096);QCOMPARE(automatic.minMemoryMiB,0);QCOMPARE(automatic.jvmArguments,QString("-Dlabel=\"hello world\""));
    }
    void initTestCase() { qApp->setFont(mc::applicationFont()); qApp->setProperty("offlineUiTest",true); }
    void offlineLaunchValidatesNameAndCancelDoesNotSave() {
        QTemporaryDir dir; QVERIFY(fixture(dir.path(), 1));
        mc::MainWindow window{mc::SettingsStore(dir.path())}; window.show(); QTRY_VERIFY(window.ready());
        bool visited = false, invalidKeptOpen = false;
        QTimer::singleShot(0, &window, [&] {
            auto dialog = window.findChild<QDialog *>();
            if (!dialog) return;
            auto player = dialog->findChild<QLineEdit *>("offlinePlayer");
            auto actions = dialog->findChild<QDialogButtonBox *>();
            if (!player || !actions) { dialog->reject(); return; }
            visited = true; player->setText("bad name");
            actions->button(QDialogButtonBox::Ok)->click(); invalidKeptOpen = dialog->isVisible();
            player->setText("NewPlayer"); actions->button(QDialogButtonBox::Cancel)->click();
        });
        window.findChild<QPushButton *>("heroAction")->click();
        QVERIFY(visited); QVERIFY(invalidKeptOpen);
        QCOMPARE(mc::SettingsStore(dir.path()).load().offlinePlayer, QString("Player"));
    }
    void javaSettingsExposeAllFiltersWithoutNetwork() {
        QTemporaryDir dir; QVERIFY(fixture(dir.path())); mc::MainWindow window{mc::SettingsStore(dir.path())}; window.show(); QTRY_VERIFY(window.ready());
        window.showJavaSettings(true);
        auto major = window.findChild<QComboBox *>("javaMajorVersion"); auto vendor = window.findChild<QComboBox *>("javaDistribution"); auto arch = window.findChild<QComboBox *>("javaArchitecture");
        auto system = window.findChild<QComboBox *>("javaOperatingSystem");
        auto detect = window.findChild<QPushButton *>("detectJavaPlatform");
        QVERIFY(system && detect);
        const auto native = mc::GamePlatform::current();
        QCOMPARE(system->currentData().toString(), native.os);
        QCOMPARE(arch->currentData().toString(), native.arch);
        system->setCurrentIndex(system->findData("linux"));
        QVERIFY(arch->findData("riscv64") >= 0);
        arch->setCurrentIndex(arch->findData("riscv64"));
        auto tabs = window.findChild<QTabWidget *>("javaTabs");
        tabs->setCurrentIndex(0); tabs->setCurrentIndex(1);
        QCOMPARE(arch->currentData().toString(), QString("riscv64"));
        detect->click();
        QCOMPARE(system->currentData().toString(), native.os);
        QCOMPARE(arch->currentData().toString(), native.arch);
        QVERIFY(major && vendor && arch); QVERIFY(major->isEditable()); QCOMPARE(vendor->count(), 5);QCOMPARE(vendor->findData("mojang"),-1); QVERIFY(vendor->findData("temurin") >= 0); QVERIFY(vendor->findData("zulu") >= 0);
        auto install = window.findChild<QPushButton *>("installJavaBuild"); QVERIFY(install); QVERIFY(!install->isEnabled()); QTRY_VERIFY(install->mapTo(&window, QPoint()).y() + install->height() <= window.height() - 8);
        auto table = window.findChild<QTableWidget *>("javaBuildList"); QVERIFY(table); QCOMPARE(table->columnCount(), 7);
        vendor->setCurrentIndex(vendor->findData("oracle-openjdk")); QVERIFY(window.findChild<QPushButton *>("javaProviderSite")->isVisible()); QVERIFY(table->isVisible()); QVERIFY(window.findChild<QPushButton *>("queryJavaBuilds")->isEnabled());
        auto site = window.findChild<QPushButton *>("javaProviderSite");
        for (const auto &entry : QList<QPair<QString, QString>>{{"zulu", "www.azul.com"}, {"temurin", "adoptium.net"}, {"microsoft", "learn.microsoft.com"}, {"oracle-openjdk", "jdk.java.net"}}) {
            vendor->setCurrentIndex(vendor->findData(entry.first));
            QVERIFY(site->isVisible());
            QCOMPARE(site->property("targetUrl").toUrl().host(), entry.second);
        }
        vendor->setCurrentIndex(vendor->findData("all")); QVERIFY(!site->isVisible()); QVERIFY(table->isVisible()); QVERIFY(!install->isEnabled());
        QVERIFY(window.findChild<QPushButton *>("fullScanJava")); QVERIFY(!window.findChild<QPushButton *>("stopJavaScan")->isEnabled());
        window.resize(840, 620); QTest::qWait(10); QVERIFY(major->width() > 20); QVERIFY(arch->width() > 20);
        auto architectureLabel = window.findChild<QLabel *>("javaArchitectureLabel");
        for (auto size : {QSize(840, 620), QSize(1140, 780)}) {
            window.resize(size); QTest::qWait(20);
            QCOMPARE(architectureLabel->mapTo(&window, QPoint(0, architectureLabel->height()/2)).y(), arch->mapTo(&window, QPoint(0, arch->height()/2)).y());
            const QList<QWidget *> toolbar{window.findChild<QLabel *>("javaOperatingSystemLabel"), system, architectureLabel, arch, detect, window.findChild<QPushButton *>("queryJavaBuilds")};
            for (int i=0;i<toolbar.size();++i){
                const QRect current(toolbar.at(i)->mapTo(&window,QPoint()),toolbar.at(i)->size());
                QVERIFY(current.right()<window.width());
                for(int j=0;j<i;++j){const QRect other(toolbar.at(j)->mapTo(&window,QPoint()),toolbar.at(j)->size());QVERIFY(!current.adjusted(-2,-2,2,2).intersects(other));}
            }
            QVERIFY(!window.findChild<QLabel *>("javaPackageStatus"));
        }
    }
    void javaNavigationDoesNotMoveWhenSwitchingDownloadTab() {
        QTemporaryDir dir;
        QVERIFY(fixture(dir.path()));
        mc::MainWindow window{mc::SettingsStore(dir.path())};
        window.show();
        QTRY_VERIFY(window.ready());
        window.showJavaSettings();
        auto tabs = window.findChild<QTabWidget *>("javaTabs");
        QVERIFY(tabs);
        auto bar = tabs->findChild<QTabBar *>();
        QVERIFY(bar);
        for (auto size : {QSize(1140, 780), QSize(840, 620)}) {
            window.resize(size);
            for (auto appearance : {QString("light"), QString("dark")}) {
                window.setAppearance(appearance, "forest");
                tabs->setCurrentIndex(0);
                QTest::qWait(30);
                const auto position = bar->mapTo(&window, QPoint());
                tabs->setCurrentIndex(1);
                QTest::qWait(30);
                QCOMPARE(bar->mapTo(&window, QPoint()), position);
                QCOMPARE(window.findChild<QScrollArea *>("downloadJavaScroll")->horizontalScrollBar()->maximum(), 0);
                auto install = window.findChild<QPushButton *>("installJavaBuild");
                auto details = window.findChild<QPushButton *>("javaBuildDetails");
                auto manager = window.findChild<mc::JavaManager *>();
                const auto installPosition = install->mapTo(&window, QPoint());
                QVERIFY(install->mapTo(&window, QPoint(0, install->height())).y() <= window.height() - 8);
                QCOMPARE(install->mapTo(manager, QPoint(install->width(), 0)).x(), manager->width());
                QVERIFY(details->mapTo(&window, QPoint(details->width(), 0)).x() < installPosition.x());
                auto vendor = window.findChild<QComboBox *>("javaDistribution");
                for (const auto &provider : {QString("oracle-openjdk"), QString("temurin")}) {
                    vendor->setCurrentIndex(vendor->findData(provider));
                    QTest::qWait(20);
                    QCOMPARE(install->mapTo(&window, QPoint()), installPosition);
                }
                tabs->setCurrentIndex(0);
                QTest::qWait(20);
                QCOMPARE(bar->mapTo(&window, QPoint()), position);
            }
        }
    }
    void settingsHeadingAndHorizontalSectionsKeepTheirPosition() {
        QTemporaryDir dir;
        QVERIFY(fixture(dir.path()));
        mc::MainWindow window{mc::SettingsStore(dir.path())};
        window.show();
        QTRY_VERIFY(window.ready());
        auto general = window.findChild<QPushButton *>("generalSettingsTab");
        auto java = window.findChild<QPushButton *>("javaSettingsTab");
        auto sections = window.findChild<QStackedWidget *>("settingsSections");
        QVERIFY(general && java && sections);
        auto title = general->parentWidget()->findChild<QLabel *>("pageTitle");
        QVERIFY(title);
        for (auto size : {QSize(1140, 780), QSize(840, 620)}) {
            window.resize(size);
            for (auto appearance : {QString("light"), QString("dark")}) {
                window.setAppearance(appearance, "forest");
                window.changePage(3);
                general->click();
                QTest::qWait(20);
                const auto titlePosition = title->mapTo(&window, QPoint());
                const auto generalPosition = general->mapTo(&window, QPoint());
                const auto javaPosition = java->mapTo(&window, QPoint());
                QVERIFY(titlePosition.x() + title->width() < generalPosition.x());
                QVERIFY(generalPosition.x() + general->width() < javaPosition.x());
                QCOMPARE(generalPosition.y(), javaPosition.y());
                QVERIFY(qAbs(titlePosition.y() + title->height() / 2 - generalPosition.y() - general->height() / 2) <= 1);
                QCOMPARE(sections->mapTo(&window, QPoint()).x(), titlePosition.x());
                QVERIFY(sections->mapTo(&window, QPoint()).y() >= generalPosition.y() + general->height() + 8);
                QVERIFY(java->mapTo(&window, QPoint(java->width(), 0)).x() <= window.width() - 24);
                for (bool downloads : {false, true}) {
                    window.showJavaSettings(downloads);
                    QTest::qWait(20);
                    QCOMPARE(title->mapTo(&window, QPoint()), titlePosition);
                    QCOMPARE(general->mapTo(&window, QPoint()), generalPosition);
                    QCOMPARE(java->mapTo(&window, QPoint()), javaPosition);
                    QCOMPARE(window.findChild<mc::JavaManager *>()->mapTo(&window, QPoint()).x(), titlePosition.x());
                    QVERIFY(java->isChecked());
                    QVERIFY(!general->isChecked());
                    java->click();
                    QVERIFY(java->isChecked());
                }
                general->click();
                QCOMPARE(sections->currentIndex(), 0);
                QVERIFY(general->isChecked());
                QVERIFY(!java->isChecked());
            }
        }
    }
    void runtimeLicenseMenuReadsTheSelectedRuntime() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));auto settings=mc::SettingsStore(dir.path()).load();QList<mc::JavaRuntime> runtimes;for(const auto &name:QStringList{"A","B"}){auto home=dir.filePath(name);QDir().mkpath(home+"/bin");QFile java(home+"/bin/java");QVERIFY(java.open(QIODevice::WriteOnly));java.write("metadata-only UI fixture");java.close();QFile license(home+"/LICENSE");QVERIFY(license.open(QIODevice::WriteOnly));license.write(("license-of-"+name).toUtf8());license.close();mc::JavaRuntime runtime;runtime.executable=home+"/bin/java";runtime.major=17;runtime.version="17.0.1";runtime.runtimeVersion="17.0.1+1";runtimes.append(runtime);}settings.javaCatalog=mc::javaCatalogJson(runtimes);QVERIFY(mc::SettingsStore(dir.path()).save(settings));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());window.showJavaSettings();auto table=window.findChild<QTableWidget *>("localJavaList");QVERIFY(table);QCOMPARE(table->rowCount(),2);auto more=table->cellWidget(1,4)->findChild<QPushButton *>("javaRuntimeMore");QVERIFY(more);bool triggered=false;QTimer::singleShot(0,&window,[&]{auto menu=qobject_cast<QMenu *>(QApplication::activePopupWidget());if(!menu)return;for(auto action:menu->actions())if(action->text()==QStringLiteral("查看包内许可证")){triggered=action->isEnabled();menu->close();if(triggered)action->trigger();return;}menu->close();});more->click();QVERIFY(triggered);auto dialog=window.findChild<QDialog *>("javaRuntimeLicensesDialog");QVERIFY(dialog);QCOMPARE(dialog->findChild<QPlainTextEdit *>("runtimeLicenseText")->toPlainText(),QString("license-of-B"));dialog->close();
    }
    void instanceJavaChoicesPersistInSeparateConfigs() {
        QTemporaryDir dir; QVERIFY(fixture(dir.path(), 2)); auto store = mc::SettingsStore(dir.path()); auto settings = store.load();
        mc::JavaRuntime a; a.executable = "/test/java-a"; a.version = "17.0.10"; a.runtimeVersion = "17.0.10+7"; a.vendor = "Temurin"; a.architecture = "aarch64"; a.major = 17;
        auto b = a; b.executable = "/test/java-b"; b.runtimeVersion = "17.0.10+8"; settings.javaCatalog = mc::javaCatalogJson({a, b}); QVERIFY(store.save(settings));
        mc::MainWindow window{store}; window.show(); QTRY_VERIFY(window.ready()); window.showJavaSettings();
        auto manager = window.findChild<mc::JavaManager *>(); QVERIFY(manager);
        QVERIFY(!manager->findChild<mc::JavaChoiceWidget *>()); QVERIFY(!manager->findChild<QPushButton *>("setGlobalJava"));
        const auto instances = mc::scanInstances(settings.gameRoots).instances; QCOMPARE(instances.size(), 2);
        const auto first = instances.at(0).path, second = instances.at(1).path;
        window.editInstance(first, 2); auto dialog = window.findChild<QDialog *>("instanceSettings"); QVERIFY(dialog);
        auto choice = dialog->findChild<mc::JavaChoiceWidget *>("instanceJavaChoice"); QVERIFY(choice);
        QVERIFY(!choice->findChild<QCheckBox *>("overrideInstanceJava"));
        QCOMPARE(choice->findChild<QComboBox *>("javaChoiceMode")->findData("inherit"), -1);
        QVERIFY(choice->findChild<QRadioButton *>("automaticJavaPolicy")->isVisible());
        if(qEnvironmentVariableIsSet("M2CL_COMPONENT_REVIEW")){
            const auto output=qEnvironmentVariable("M2CL_COMPONENT_REVIEW");QDir().mkpath(output);window.setAppearance("light","forest");QCoreApplication::processEvents();dialog->grab().save(output+"/instance-java-light.png");window.setAppearance("dark","forest");QCoreApplication::processEvents();dialog->grab().save(output+"/instance-java-dark.png");window.setAppearance("light","forest");
        }
        QCOMPARE(choice->choice().value("mode").toString(), QString("auto"));
        choice->setChoice(mc::javaPin(b)); dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); QTRY_VERIFY(!dialog->isVisible());
        QCOMPARE(mc::loadInstanceJavaChoice(first), mc::javaPin(b));
        QVERIFY(QFileInfo::exists(mc::instanceConfigPath(first)));
        QCOMPARE(mc::loadInstanceJavaChoice(second).value("mode").toString(), QString("auto"));
        QTest::qWait(1); window.editInstance(second, 2); dialog = window.findChild<QDialog *>("instanceSettings");
        choice = dialog->findChild<mc::JavaChoiceWidget *>("instanceJavaChoice"); choice->setChoice(mc::javaPin(a));
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); QTRY_VERIFY(!dialog->isVisible());
        QCOMPARE(mc::loadInstanceJavaChoice(first), mc::javaPin(b)); QCOMPARE(mc::loadInstanceJavaChoice(second), mc::javaPin(a));
        QTest::qWait(1); window.editInstance(first, 2); dialog = window.findChild<QDialog *>("instanceSettings");
        choice = dialog->findChild<mc::JavaChoiceWidget *>("instanceJavaChoice"); QCOMPARE(choice->choice(), mc::javaPin(b));
        choice->setChoice({{"mode", "auto"}}); dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
        QCOMPARE(mc::loadInstanceJavaChoice(first), mc::javaPin(b));
        QCOMPARE(mc::loadInstanceJavaChoice(second), mc::javaPin(a));
    }
    void instanceCanChooseHigherAndExplicitlyForceLowerJava() {
        mc::JavaRuntime j17;j17.executable="/test/java17";j17.version="17.0.1";j17.runtimeVersion="17.0.1+1";j17.major=17;auto j21=j17;j21.executable="/test/java21";j21.version="21.0.1";j21.runtimeVersion="21.0.1+1";j21.major=21;auto j8=j17;j8.executable="/test/java8";j8.version="1.8.0_402";j8.runtimeVersion="1.8.0_402-b06";j8.major=8;
        mc::JavaChoiceWidget choice(mc::javaPin(j17),{j17,j21,j8},17);choice.show();auto table=choice.findChild<QTableWidget *>("javaRuntimePicker");auto all=choice.findChild<QCheckBox *>("showAllJavaVersions");auto force=choice.findChild<QCheckBox *>("forceLowerJava");QVERIFY(table&&all&&force);QVERIFY(!table->isRowHidden(0));QVERIFY(!table->isRowHidden(1));QVERIFY(table->isRowHidden(2));table->setCurrentCell(1,0);QCOMPARE(choice.choice().value("major").toInt(),21);QVERIFY(choice.selectionError().isEmpty());all->setChecked(true);QVERIFY(!table->isRowHidden(2));table->setCurrentCell(2,0);QVERIFY(!choice.selectionError().isEmpty());QVERIFY(force->isVisible());force->click();QVERIFY(choice.selectionError().isEmpty());auto pin=choice.choice();QVERIFY(pin.value("allowBelowRecommended").toBool());choice.setRuntimes({j17,j21,j8});QCOMPARE(choice.choice(),pin);choice.setChoice(mc::javaPin(j17));QVERIFY(!choice.choice().value("allowBelowRecommended").toBool());
    }
    void instanceJavaLabelDoesNotOverlapChoiceCards() {
        QTemporaryDir dir; QVERIFY(fixture(dir.path(), 1)); mc::MainWindow window{mc::SettingsStore(dir.path())}; window.show(); QTRY_VERIFY(window.ready());
        window.editInstance({}, 2); auto dialog = window.findChild<QDialog *>("instanceSettings"); QVERIFY(dialog); auto choice = dialog->findChild<mc::JavaChoiceWidget *>("instanceJavaChoice"); QVERIFY(choice);
        choice->setChoice({{"mode", "fixed"}, {"path", "/test/java"}});
        for (const auto &appearance : {QString("light"), QString("dark")}) {
            window.setAppearance(appearance, "forest"); QTest::qWait(20);
            auto label = choice->findChild<QLabel *>("javaSelectionLabel"); auto combo = choice->findChild<QTableWidget *>("javaRuntimePicker"); auto cards = choice->findChild<QWidget *>("javaModeCards");
            QVERIFY(label && combo && cards); QVERIFY(label->isVisible());
            const auto labelTop = label->mapTo(choice, QPoint()).y(), cardBottom = cards->mapTo(choice, QPoint(0, cards->height())).y();
            QVERIFY2(labelTop >= cardBottom + 8, qPrintable(QString("Label %1, card bottom %2").arg(labelTop).arg(cardBottom)));
            QVERIFY(combo->mapTo(choice, QPoint()).y() >= labelTop + label->height() + 6);
            QVERIFY(choice->height() >= choice->minimumSizeHint().height());
            auto manage = dialog->findChild<QPushButton *>("manageInstanceJava"); QVERIFY(manage);
            QVERIFY(manage->mapTo(dialog, QPoint()).y() >= choice->mapTo(dialog, QPoint(0, choice->height())).y() + 6);
            auto buttons = dialog->findChild<QDialogButtonBox *>(); QVERIFY(buttons);
            QVERIFY(manage->mapTo(dialog, QPoint(0, manage->height())).y() + 8 <= buttons->mapTo(dialog, QPoint()).y() || dialog->findChild<QScrollArea *>("instanceJavaScroll")->verticalScrollBar()->maximum()>0);
            QCOMPARE(label->font().pixelSize(), 14); QCOMPARE(combo->font().family(), qApp->font().family());
        }
        dialog->reject();
    }
    void sparseInstanceCardsAndSmallSettingsStayInsidePage() {
        QTemporaryDir dir; QVERIFY(fixture(dir.path(), 1)); mc::MainWindow window{mc::SettingsStore(dir.path())};
        window.resize(1140, 780); window.show(); QTRY_VERIFY(window.ready());
        auto grid = window.findChild<mc::InstanceGrid *>("homeInstances"); QVERIFY(grid);
        auto cards = grid->findChildren<mc::InstanceCard *>(); QCOMPARE(cards.size(), 1);
        QVERIFY(cards.first()->width() < grid->width() / 2);
        QVERIFY(cards.first()->height() > qRound((cards.first()->width() - 14) * .5625) + 100);
        window.resize(840, 620); window.changePage(3); QTest::qWait(20);
        for (auto choice : window.findChildren<mc::AppearanceChoice *>()) {
            QVERIFY(choice->isVisible());
            QVERIFY(choice->mapTo(&window, QPoint(choice->width(), 0)).x() <= window.width() - 16);
        }
    }
    void marketFiltersAreIndependentAndVersionsUseSourceChannels() {
        QTemporaryDir dir; QVERIFY(fixture(dir.path(),1)); mc::MainWindow window{mc::SettingsStore(dir.path())}; window.show(); QTRY_VERIFY(window.ready()); window.changePage(2);
        auto client=window.findChild<mc::ResourceClient *>(); auto version=window.findChild<mc::SelectBox *>("modGameVersion"); auto loader=window.findChild<QComboBox *>("modLoader");
        auto categories=window.findChild<QListWidget *>("modCategories"); QVERIFY(client&&version&&loader&&categories);
        QVERIFY(!window.findChild<QComboBox *>("modTarget")); QVERIFY(!window.findChild<QCheckBox *>("modFollowInstance"));
        QVERIFY(window.findChild<QComboBox *>("modSource")->isVisible());
        const auto launch=mc::SettingsStore(dir.path()).load().selectedInstance;
        QVERIFY(!version->isEditable());version->addItem("1.21.1","1.21.1");version->setCurrentIndex(version->findData("1.21.1")); loader->setCurrentIndex(loader->findData("neoforge"));
        mc::ResourceTags tags; tags.categories={{"optimization",QStringLiteral("性能优化")}};
        tags.loaders={{"fabric","Fabric"},{"rift","Rift"},{"quilt","Quilt"},{"forge","Forge"},{"neoforge","NeoForge"}};
        tags.gameVersions=QStringList{"26.4-snapshot-6","1.21.1","1.20.1"}; tags.gameVersionTypes={{"26.4-snapshot-6","snapshot"},{"1.21.1","release"},{"1.20.1","release"}};
        client->tagsReady(tags);
        QCOMPARE(version->currentText(),QString("1.21.1")); QCOMPARE(loader->currentData().toString(),QString("neoforge"));
        QCOMPARE(categories->flow(),QListView::TopToBottom); QCOMPARE(loader->itemData(1).toString(),QString("forge")); QCOMPARE(loader->itemData(2).toString(),QString("neoforge"));
        QCOMPARE(mc::SettingsStore(dir.path()).load().selectedInstance,launch);
        version->showPopup(); QTest::qWait(20);
        auto choices=version->findChild<QListWidget *>("versionChoices");auto channels=version->findChild<QTabBar *>("versionChannels");QVERIFY(choices&&channels);
        QCOMPARE(choices->count(),4);QVERIFY(choices->item(1)->isHidden());QVERIFY(!choices->item(2)->isHidden());
        channels->setCurrentIndex(2);QVERIFY(!choices->item(1)->isHidden());QVERIFY(choices->item(2)->isHidden());
        choices->itemClicked(choices->item(1)); QCOMPARE(version->currentText(),QString("26.4-snapshot-6"));
        QVERIFY(!version->findChild<QFrame *>("selectPopup")->isVisible());
    }
    void instanceModEntriesPrefillFiltersWithoutLockingThem() {
        QTemporaryDir dir; QVERIFY(fixture(dir.path(),2)); mc::MainWindow window{mc::SettingsStore(dir.path())}; window.show(); QTRY_VERIFY(window.ready());
        auto original=mc::SettingsStore(dir.path()).load().selectedInstance; auto cards=window.findChild<mc::InstanceGrid *>("homeInstances")->findChildren<mc::InstanceCard *>(); QCOMPARE(cards.size(),2);
        for(auto card:cards)QVERIFY(card->findChild<QPushButton *>("addInstanceMods"));
        auto destination=cards.first()->instancePath()==original?cards.last()->instancePath():cards.first()->instancePath(); window.browseModsForInstance(destination);
        auto settings=mc::SettingsStore(dir.path()).load(); QCOMPARE(settings.selectedInstance,original); QCOMPARE(settings.modInstallTarget,destination);
        QVERIFY(!window.findChild<QComboBox *>("modTarget")); QVERIFY(window.findChild<QComboBox *>("modSource")->isVisible());
        auto version=window.findChild<QComboBox *>("modGameVersion"); QVERIFY(version->isEnabled()); QVERIFY(!version->isEditable());version->addItem("1.21.1","1.21.1");version->setCurrentIndex(version->findData("1.21.1")); QCOMPARE(version->currentText(),QString("1.21.1"));
        window.showInstanceMods(destination); QVERIFY(window.findChild<QWidget *>("instanceModsView")->isVisible()); QCOMPARE(mc::SettingsStore(dir.path()).load().selectedInstance,original);
    }
    void resourceMarketsShareNavigationWithoutBrowseDialogs(){
        QTemporaryDir dir;QVERIFY(fixture(dir.path(),1));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());
        const auto path=mc::SettingsStore(dir.path()).load().selectedInstance;
        window.showInstanceMods(path);auto instance=window.findChild<mc::InstanceModsView *>();QVERIFY(instance);
        instance->resourceMarketRequested(path,"shader");
        auto type=window.findChild<QComboBox *>("marketResourceType");QCOMPARE(type->currentData().toString(),QString("shader"));QVERIFY(!QApplication::activeModalWidget());
        QVERIFY(window.findChild<QPushButton *>("shaderMarketNav")->isChecked());
        QVERIFY(!window.findChild<QComboBox *>("modLoader")->isVisible());QVERIFY(window.findChild<QComboBox *>("modSource")->isVisible());
        window.findChild<QPushButton *>("resourcepackMarketNav")->click();QCOMPARE(type->currentData().toString(),QString("resourcepack"));QVERIFY(!QApplication::activeModalWidget());
        window.browseModsForInstance(path);QCOMPARE(type->currentData().toString(),QString("mod"));QVERIFY(window.findChild<QComboBox *>("modLoader")->isVisible());
    }
    void failedVersionOptionsExplainTheFailureAndCanRetry() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());window.changePage(2);
        auto version=window.findChild<mc::SelectBox *>("modGameVersion");QVERIFY(!version->isEditable());auto client=window.findChild<mc::ResourceClient *>();
        client->tagsFailed(QStringLiteral("游戏版本加载失败：连接超时"));version->showPopup();QTest::qWait(10);
        auto popup=version->findChild<QFrame *>("selectPopup");auto notice=popup->findChild<mc::NetworkNotice *>("popupNetworkState");QVERIFY(notice->isVisible());QVERIFY(!popup->findChild<QTabBar *>("versionChannels")->isVisible());
        auto details=notice->findChild<QPlainTextEdit *>();QVERIFY(details->isHidden());QVERIFY(details->toPlainText().contains(QStringLiteral("超时")));
        QSignalSpy retried(version,&mc::SelectBox::retryOptionsRequested);popup->findChild<QPushButton *>("retryVersionOptions")->click();QCOMPARE(retried.size(),1);
        version->hidePopup();
        window.findChild<QPushButton *>("searchMods")->click();client->searchFailed(QStringLiteral("Modrinth 请求失败：连接超时"));
        auto searchNotice=window.findChild<mc::NetworkNotice *>("modSearchNetworkState");QVERIFY(searchNotice->isVisible());
        QVERIFY(!window.findChild<mc::BusySpinner *>("modSearchSpinner")->isVisible());QVERIFY(!window.findChild<QLabel *>("modSearchMessage")->isVisible());
        // Starting a retry removes old errors immediately; a successful empty result is not an error.
        searchNotice->findChild<QPushButton *>("networkRetry")->click();QVERIFY(searchNotice->isHidden());QVERIFY(window.findChild<mc::BusySpinner *>("modSearchSpinner")->isVisible());
        client->projectsReady({});QVERIFY(searchNotice->isHidden());QVERIFY(searchNotice->findChild<QPlainTextEdit *>()->toPlainText().isEmpty());QVERIFY(window.findChild<QLabel *>("modSearchMessage")->isVisible());
    }
    void stoppedDownloadsDoNotKeepSpinningAndRetainHttpStatus() {
        mc::DownloadPanel panel;panel.show();panel.setTask("Minecraft 1.20.1","HTTP 503 · Service Unavailable",-1,false);
        auto notice=panel.findChild<mc::NetworkNotice *>("downloadTaskNetworkState");QVERIFY(notice->isVisible());QVERIFY(!panel.findChild<QProgressBar *>()->isVisible());QVERIFY(notice->findChild<QLabel *>("networkErrorSummary")->text().contains("HTTP 503"));QVERIFY(!notice->findChild<QPushButton *>("networkRetry")->isVisible());
        panel.setTask("Minecraft 1.20.1",QStringLiteral("下载中"),200,true);QVERIFY(notice->isHidden());QVERIFY(notice->findChild<QPlainTextEdit *>()->toPlainText().isEmpty());QVERIFY(panel.findChild<QProgressBar *>()->isVisible());panel.setTask("Minecraft 1.20.1",QStringLiteral("安装完成"),1000,false);QVERIFY(notice->isHidden());
    }
    void networkNoticesKeepDetailsCollapsedAndClearOnRecovery() {
        mc::NetworkNotice notice;notice.resize(680,120);QSignalSpy retried(&notice,&mc::NetworkNotice::retryRequested);
        const auto error=QStringLiteral("分类加载失败：连接超时\n加载器加载失败：连接超时\n游戏版本加载失败：连接超时");
        notice.failure(QStringLiteral("部分筛选项未能加载"),error);QVERIFY(notice.isVisible());auto details=notice.findChild<QPlainTextEdit *>();QVERIFY(details->isHidden());QCOMPARE(details->toPlainText(),error);
        notice.findChild<QPushButton *>("networkShowDetails")->click();QVERIFY(details->isVisible());notice.findChild<QPushButton *>("networkRetry")->click();QCOMPARE(retried.size(),1);
        notice.loading();QVERIFY(details->isHidden());QVERIFY(details->toPlainText().isEmpty());QVERIFY(!notice.findChild<QPushButton *>("networkRetry")->isVisible());QVERIFY(notice.findChild<mc::BusySpinner *>()->isVisible());
        notice.clear();QVERIFY(notice.isHidden());QVERIFY(!notice.findChild<mc::BusySpinner *>()->isVisible());
        notice.failure(QStringLiteral("版本列表加载失败"),QStringLiteral("HTTP 503 · 服务暂时不可用\n请求：https://api.modrinth.com/v2/project/sodium/version"));QVERIFY(notice.findChild<QLabel *>("networkErrorSummary")->text().contains("HTTP 503"));
        if(qEnvironmentVariableIsSet("M2CL_NETWORK_REVIEW"))for(bool dark:{false,true}){mc::applyTheme(mc::makeTheme("forest",dark));notice.resize(680,100);QCoreApplication::processEvents();notice.grab().save(qEnvironmentVariable("M2CL_NETWORK_REVIEW")+(dark?"/http-error-dark.png":"/http-error-light.png"));}mc::applyTheme(mc::makeTheme("forest",false));
    }
    void marketFilterFailureIsScopedAndClearsAfterSuccessfulReload() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));mc::MainWindow window{mc::SettingsStore(dir.path())};window.resize(1120,760);window.show();QTRY_VERIFY(window.ready());window.changePage(2);
        auto client=window.findChild<mc::ResourceClient *>();auto notice=window.findChild<mc::NetworkNotice *>("modFilterNetworkState");
        mc::ResourceProject project;project.id="sodium";project.name="Sodium";project.description="A fast rendering engine";project.categories=QStringList{"optimization","fabric"};client->projectsReady({project});
        client->tagsFailed(QStringLiteral("分类加载失败：连接超时\n加载器加载失败：连接超时\n游戏版本加载失败：连接超时"));QVERIFY(notice->isVisible());
        auto list=window.findChild<QListWidget *>("modResults");QCOMPARE(list->count(),1);QVERIFY(!notice->findChild<QPlainTextEdit *>()->isVisible());QVERIFY(list->isVisible());
        if(qEnvironmentVariableIsSet("M2CL_NETWORK_REVIEW")){const auto out=qEnvironmentVariable("M2CL_NETWORK_REVIEW");QDir().mkpath(out);QTest::qWait(30);window.grab().save(out+"/market-filter-failure.png");}
        client->tagsLoading();QVERIFY(notice->findChild<mc::BusySpinner *>()->isVisible());QVERIFY(notice->findChild<QPlainTextEdit *>()->toPlainText().isEmpty());
        mc::ResourceTags tags;tags.gameVersions=QStringList{"1.20.1"};tags.gameVersionTypes={{"1.20.1","release"}};client->tagsReady(tags);QVERIFY(notice->isHidden());QCOMPARE(list->count(),1);
        if(qEnvironmentVariableIsSet("M2CL_NETWORK_REVIEW"))window.grab().save(qEnvironmentVariable("M2CL_NETWORK_REVIEW")+"/market-recovered.png");
        // A version request must not stop a search spinner or write to the download task.
        window.findChild<QPushButton *>("searchMods")->click();client->versionsFailed("Other request failed");QVERIFY(window.findChild<mc::BusySpinner *>("modSearchSpinner")->isVisible());QVERIFY(window.findChild<QLabel *>("resourceDownloadStatus")->text().isEmpty());
        client->searchFailed("Timeout");auto search=window.findChild<mc::NetworkNotice *>("modSearchNetworkState");QVERIFY(search->isVisible());
        if(qEnvironmentVariableIsSet("M2CL_NETWORK_REVIEW")){QTest::qWait(30);window.grab().save(qEnvironmentVariable("M2CL_NETWORK_REVIEW")+"/market-search-failure.png");}
    }
    void projectFailureDoesNotBecomeReadmeAndRetryCanRecover() {
        mc::ModVersionView view("Sodium",{},nullptr,{});view.resize(840,620);view.show();mc::ResourceProject project;project.id="sodium";project.body="# Introduction\n\nOriginal description";view.setProjectInfo(project);
        auto readme=view.findChild<mc::ProjectReadme *>("modProjectReadme");const auto before=readme->toPlainText();view.setProjectError("Timeout details");QCOMPARE(readme->toPlainText(),before);
        auto notice=view.findChild<mc::NetworkNotice *>("modProjectNetworkState");QVERIFY(notice->isVisible());QSignalSpy retry(&view,&mc::ModVersionView::retryProjectRequested);notice->findChild<QPushButton *>("networkRetry")->click();QCOMPARE(retry.size(),1);QVERIFY(notice->findChild<mc::BusySpinner *>()->isVisible());
        project.body="# Recovered";view.setProjectInfo(project);QVERIFY(notice->isHidden());QVERIFY(readme->toPlainText().contains("Recovered"));QVERIFY(!readme->toPlainText().contains("Timeout"));
    }
    void javaQueryFailureLivesInBuildListAndClearsOnSuccess() {
        QTemporaryDir dir;mc::Settings settings;mc::JavaManager view(settings,mc::SettingsStore(dir.path()));view.resize(1080,700);view.show();view.findChild<QTabWidget *>("javaTabs")->setCurrentIndex(1);
        auto watcher=static_cast<QFutureWatcher<mc::JavaPackageList> *>(view.findChild<QFutureWatcherBase *>("javaPackageQuery"));QVERIFY(watcher);
        auto deliver=[&](const mc::JavaPackageList &result){watcher->setFuture(QtConcurrent::run([result]{return result;}));};
        mc::JavaPackageList failed;failed.error=QStringLiteral("Temurin 连接超时，请检查网络后重试");deliver(failed);
        auto notice=view.findChild<mc::NetworkNotice *>("javaQueryNetworkState");QTRY_VERIFY(notice->isVisible());auto table=view.findChild<QTableWidget *>("javaBuildList");QVERIFY(table->viewport()->isAncestorOf(notice));QCOMPARE(table->rowCount(),0);QCoreApplication::processEvents();
        const auto center=notice->mapTo(table->viewport(),notice->rect().center());QVERIFY(qAbs(center.y()-table->viewport()->height()/2)<12);
        if(qEnvironmentVariableIsSet("M2CL_NETWORK_REVIEW")){QTest::qWait(20);view.grab().save(qEnvironmentVariable("M2CL_NETWORK_REVIEW")+"/java-query-failure.png");}
        mc::JavaPackage package;package.provider="temurin";package.version="17.0.20+1";package.major=17;package.os=mc::GamePlatform::current().os;package.arch=mc::GamePlatform::current().arch;package.label="Temurin 17.0.20+1";
        auto partial=failed;partial.packages={package};deliver(partial);QTRY_COMPARE(table->rowCount(),1);QVERIFY(notice->isVisible());QVERIFY(!table->viewport()->isAncestorOf(notice));
        partial.error.clear();deliver(partial);QTRY_VERIFY(notice->isHidden());QVERIFY(notice->findChild<QPlainTextEdit *>()->toPlainText().isEmpty());QCOMPARE(table->rowCount(),1);
        failed.error="Network error";deliver(failed);QTRY_VERIFY(notice->isVisible());auto major=view.findChild<QComboBox *>("javaMajorVersion");major->setCurrentText("21");QVERIFY(notice->isHidden());
    }
    void earlyProjectTimeoutRemainsVisibleWhenVersionsArrive() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());window.changePage(2);auto client=window.findChild<mc::ResourceClient *>();mc::ResourceProject project;project.id="sodium";project.name="Sodium";project.description="Short description";client->projectsReady({project});
        auto list=window.findChild<QListWidget *>("modResults");list->itemWidget(list->item(0))->findChild<QPushButton *>()->click();client->projectFailed("Timed out");client->versionsReady({});
        auto view=window.findChild<mc::ModVersionView *>();QVERIFY(view);auto notice=view->findChild<mc::NetworkNotice *>("modProjectNetworkState");QTRY_VERIFY(notice->isVisible());QVERIFY(!view->findChild<mc::ProjectReadme *>()->toPlainText().contains("Timed out"));
        project.body="# Loaded after retry";client->projectReady(project);QVERIFY(notice->isHidden());
    }
    void searchSpinnerLivesInsideTheResultsArea() {
        QTemporaryDir dir; QVERIFY(fixture(dir.path())); mc::MainWindow window{mc::SettingsStore(dir.path())}; window.show(); QTRY_VERIFY(window.ready()); window.changePage(2);
        auto list=window.findChild<QListWidget *>("modResults"); auto spinner=window.findChild<mc::BusySpinner *>("modSearchSpinner"); QVERIFY(list&&spinner);
        window.findChild<QPushButton *>("searchMods")->click(); QTest::qWait(10); QVERIFY(spinner->isVisible()); QVERIFY(list->viewport()->isAncestorOf(spinner));
        const auto point=spinner->mapTo(list->viewport(),QPoint(spinner->width()/2,spinner->height()/2)); QVERIFY(qAbs(point.x()-list->viewport()->width()/2)<=2); QVERIFY(qAbs(point.y()-list->viewport()->height()/2)<=10);
        window.findChild<mc::ResourceClient *>()->projectsReady({}); QVERIFY(!spinner->isVisible());
    }
    void webpProjectIconsHaveRuntimeDecoder() {
        QVERIFY2(QImageReader::supportedImageFormats().contains("webp"),"Install and deploy Qt Image Formats (WebP) for Modrinth project icons");
    }
    void modRowsScrollByPixelsAndHaveIconsAndTranslatedTags() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path(),1));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());window.changePage(2);
        auto client=window.findChild<mc::ResourceClient *>();auto list=window.findChild<QListWidget *>("modResults");QVERIFY(client&&list);
        QList<mc::ResourceProject> projects;for(int i=0;i<8;++i){mc::ResourceProject p;p.id=QString::number(i);p.name="Supplementaries";p.categories=QStringList{"decoration","fabric","game-mechanics"};projects.append(p);}client->projectsReady(projects);QTest::qWait(20);
        QCOMPARE(list->verticalScrollMode(),QAbstractItemView::ScrollPerPixel);list->verticalScrollBar()->setValue(7);QCOMPARE(list->verticalScrollBar()->value(),7);
        auto icons=list->findChildren<QLabel *>("modProjectIcon");QCOMPARE(icons.size(),8);
        auto tags=list->findChild<mc::AdaptiveTags *>("modProjectTags");QVERIFY(tags);QVERIFY(tags->labels().contains(QStringLiteral("装饰与建筑")));
        QCOMPARE(mc::resourceTagLabel("release"),QStringLiteral("正式版"));QCOMPARE(mc::resourceTagLabel("fabric"),QString("Fabric"));
    }
    void projectVersionTooltipIsBoundedAndDetailsAreGrouped() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path(),1));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());window.changePage(2);
        mc::ResourceProject p;p.id="mouse";p.name="Mouse Tweaks";p.gameVersions=QStringList{"b1.7.3","1.20.1","1.20.4","26.1.2"};
        for(int i=1;i<40;++i)p.gameVersions.append(QString("1.21.%1").arg(i));window.findChild<mc::ResourceClient *>()->projectsReady({p});
        auto summary=window.findChild<QLabel *>("modSupportSummary");QVERIFY(summary);QVERIFY(summary->toolTip().size()<400);QVERIFY(!summary->toolTip().contains(p.gameVersions.join("、")));
        QVERIFY(!window.findChild<QPushButton *>("allProjectVersions"));
    }
    void adaptiveMetadataUsesAvailableWidth() {
        const QStringList labels{QStringLiteral("冒险"),"Fabric","Forge","NeoForge",QStringLiteral("实用工具"),QStringLiteral("装饰与建筑")};mc::AdaptiveTags tags(labels);tags.resize(900,28);tags.show();QCOMPARE(tags.visibleCount(),6);tags.resize(120,28);QVERIFY(tags.visibleCount()<6);tags.resize(900,28);QCOMPARE(tags.visibleCount(),6);
        mc::FittingVersionLabel versions({"26.3","26.2","26.1","1.21","1.20","1.19"});versions.resize(900,24);versions.show();QTest::qWait(10);QVERIFY(!versions.text().contains(QStringLiteral("等")));QVERIFY(versions.text().contains("1.19"));versions.resize(180,24);QVERIFY(versions.text().contains(QStringLiteral("等")));
    }
    void modVersionPageDefaultsToStableAndSeparatesChannels() {
        mc::ResourceFile stable;stable.versionName="Stable Forge";stable.fileName="forge.jar";stable.gameVersions=QStringList{"1.20.1"};stable.loaders=QStringList{"forge"};stable.releaseType="release";
        auto beta=stable;beta.fileName="beta.jar";beta.releaseType="beta";auto alpha=stable;alpha.releaseType="alpha";
        mc::ModVersionView view("Example",{},nullptr,{beta,stable,alpha});view.resize(800,700);view.show();QTest::qWait(20);
        auto list=view.findChild<QTableWidget *>("modFileVersions");auto filter=view.findChild<QTabBar *>("modReleaseChannel");auto install=view.findChild<QPushButton *>("downloadModVersion");auto sections=view.findChild<QTabWidget *>("modDetailSections");QVERIFY(list&&filter&&install&&sections);
        sections->setCurrentIndex(1);QCOMPARE(filter->tabData(filter->currentIndex()).toString(),QString("release"));QVERIFY(list->isRowHidden(0));QVERIFY(list->isRowHidden(2));QCOMPARE(view.selectedIndex(),1);QVERIFY(install->isEnabled());
        filter->setCurrentIndex(1);QCOMPARE(view.selectedIndex(),0);QVERIFY(list->isRowHidden(1));QCOMPARE(view.findChild<QLabel *>("selectedModFile")->text(),QString("beta.jar"));
        mc::ModVersionView noStable("Preview only",{},nullptr,{beta,alpha});noStable.show();noStable.findChild<QTabWidget *>("modDetailSections")->setCurrentIndex(1);QCOMPARE(noStable.selectedIndex(),-1);QVERIFY(!noStable.findChild<QPushButton *>("downloadModVersion")->isEnabled());QVERIFY(noStable.findChild<QLabel *>("noStableModVersions")->isVisible());
    }
    void fileVersionsSeparateLoadersAndRetainOriginalSelectionIndices(){
        mc::ResourceFile forge;forge.versionNumber="forge-build";forge.loaders=QStringList{"forge"};forge.gameVersions=QStringList{"1.20.1"};forge.releaseType="release";auto neo=forge;neo.loaders=QStringList{"neoforge"};auto fabric=forge;fabric.loaders=QStringList{"fabric","quilt"};auto beta=fabric;beta.gameVersions=QStringList{"1.21"};beta.releaseType="beta";
        mc::ModVersionView view("Test",{},nullptr,{forge,neo,fabric,beta});view.resize(840,720);view.show();view.findChild<QTabWidget *>("modDetailSections")->setCurrentIndex(1);auto rows=view.findChild<QTableWidget *>("modFileVersions");auto channels=view.findChild<QTabBar *>("modReleaseChannel");
        view.setDownloadFilter("1.20.1","fabric");QCOMPARE(view.selectedIndex(),2);QVERIFY(rows->isRowHidden(0)&&rows->isRowHidden(1)&&!rows->isRowHidden(2));QCOMPARE(channels->tabText(0),QStringLiteral("正式版（1）"));
        view.setDownloadFilter("1.20.1","quilt");QCOMPARE(view.selectedIndex(),2);view.setDownloadFilter("1.20.1","neoforge");QCOMPARE(view.selectedIndex(),1);
        view.setDownloadFilter("1.21","forge");QCOMPARE(view.selectedIndex(),-1);QVERIFY(!view.findChild<QPushButton *>("downloadModVersion")->isEnabled());view.setDownloadFilter("1.21","fabric");channels->setCurrentIndex(1);QCOMPARE(view.selectedIndex(),3);
    }
    void expandingVersionFilterKeepsSelectedFileVisible(){
        QList<mc::ResourceFile> files;for(int i=0;i<60;++i){mc::ResourceFile f;f.fileName=QString::number(i)+".jar";f.loaders=QStringList{"fabric"};f.gameVersions=QStringList{i==59?"1.20.1":"1.21"};f.releaseType="release";files.append(f);}mc::ModVersionView view("Test",{},nullptr,files);view.resize(840,620);view.show();view.findChild<QTabWidget *>("modDetailSections")->setCurrentIndex(1);view.setDownloadFilter("1.20.1","fabric");QCOMPARE(view.selectedIndex(),59);view.findChild<QComboBox *>("modFileGameVersion")->setCurrentIndex(0);QCoreApplication::processEvents();auto table=view.findChild<QTableWidget *>("modFileVersions");QCOMPARE(view.selectedIndex(),59);QVERIFY(table->item(59,0)->isSelected());QVERIFY(table->visualItemRect(table->item(59,0)).intersects(table->viewport()->rect()));
    }
    void planChoicesShowDisabledFilesAndPruneOnlyExclusiveChildren(){
        if(qEnvironmentVariableIsSet("M2CL_MOD_UI_REVIEW"))mc::applyTheme(mc::makeTheme("forest",true));
        mc::ModPlan plan;plan.target.id="Test";plan.target.loader="fabric";plan.rootProjectId="root";for(const auto &id:QStringList{"root","left","right","shared","exclusive"}){mc::ResourceFile file;file.projectId=id;file.versionName=id;file.versionNumber="1.0";plan.files.append(file);plan.resolved.append(file);}plan.edges={{"root","left","","required"},{"root","right","","required"},{"left","shared","","required"},{"right","shared","","required"},{"left","exclusive","","required"}};plan.disabledFiles.insert("shared","my-shared.jar.disabled");plan.replacements=QStringList{"left-old.jar"};plan.replacementProjects.insert("left-old.jar","left");plan.replacementVersions.insert("left-old.jar","0.9");
        mc::ModPlanView dialog(plan);dialog.show();QMap<QString,QCheckBox *> choices;for(auto choice:dialog.findChildren<QCheckBox *>("dependencyChoice"))choices.insert(choice->property("projectId").toString(),choice);auto action=dialog.findChild<QComboBox *>("disabledDependencyAction");QVERIFY(action);QCOMPARE(action->currentData().toString(),QString("enable"));QVERIFY(dialog.selectedPlan().enableFiles.contains("shared"));choices.value("left")->setChecked(false);QVERIFY(!choices.value("exclusive")->isChecked());QVERIFY(!choices.value("exclusive")->isEnabled());QVERIFY(choices.value("shared")->isChecked());QVERIFY(dialog.selectedPlan().replacements.isEmpty()||!dialog.selectedPlan().replacements.contains("left-old.jar"));choices.value("left")->setChecked(true);QVERIFY(choices.value("exclusive")->isChecked());QVERIFY(choices.value("exclusive")->isEnabled());action->setCurrentIndex(1);QVERIFY(dialog.selectedPlan().enableFiles.isEmpty());QVERIFY(dialog.selectedPlan().replacements.contains("my-shared.jar.disabled"));
        if(qEnvironmentVariableIsSet("M2CL_MOD_UI_REVIEW")){QDir().mkpath(qEnvironmentVariable("M2CL_MOD_UI_REVIEW"));dialog.grab().save(qEnvironmentVariable("M2CL_MOD_UI_REVIEW")+"/dependency-states-synthetic.png");}
    }
    void mismatchedInstalledModOffersSearchAndDisable(){
        if(qEnvironmentVariableIsSet("M2CL_MOD_UI_REVIEW"))mc::applyTheme(mc::makeTheme("forest",true));
        QTemporaryDir dir;auto instance=target(dir.filePath("game"));QVERIFY(save(instance.gameDirectory+"/mods/renamed.jar",zip({{"META-INF/mods.toml","[[mods]]\nmodId=\"wrong\"\nversion=\"1.0\"\ndisplayName=\"Wrong loader example\"\n"}})));mc::InstanceModsView view(instance,"Test instance");view.resize(1000,620);view.show();auto table=view.findChild<QTableWidget *>("installedModsTable");QTRY_COMPARE(table->rowCount(),1);QVERIFY(table->item(0,4)->text().contains(QStringLiteral("加载器不匹配")));table->setCurrentCell(0,0);auto find=view.findChild<QPushButton *>("findCompatibleMod");auto disable=view.findChild<QPushButton *>("disableSelectedMod");QVERIFY(find->isVisible());QSignalSpy requested(&view,&mc::InstanceModsView::findCompatibleRequested);find->click();QCOMPARE(requested.size(),1);QCOMPARE(requested.first().at(0).toString(),instance.path);
        if(qEnvironmentVariableIsSet("M2CL_MOD_UI_REVIEW"))view.grab().save(qEnvironmentVariable("M2CL_MOD_UI_REVIEW")+"/wrong-loader-synthetic.png");disable->click();QTRY_VERIFY(QFile::exists(instance.gameDirectory+"/mods/renamed.jar.disabled"));QVERIFY(!QFile::exists(instance.gameDirectory+"/mods/renamed.jar"));
    }
    void quiltCompatibilityShowsCachedSourceAndLocalWarning(){
        QTemporaryDir dir;auto instance=target(dir.filePath("game"));instance.loader="quilt";
        QVERIFY(save(instance.gameDirectory+"/mods/user-renamed.jar",jar("local_test","1.2.3")));
        QString error;QVERIFY(mc::saveCompatibilityList(instance,R"([{"ids":["local_test"],"name":"Local Test","type":"GAME"}])",&error));
        mc::InstanceModsView view(instance,"Quilt test");view.resize(1100,720);view.show();
        auto status=view.findChild<QLabel *>("compatibilityListStatus");QVERIFY(status);QVERIFY(status->text().contains(QStringLiteral("同步于")));
        QVERIFY(view.findChild<QPushButton *>("updateCompatibilityList")->isEnabled());
        auto table=view.findChild<QTableWidget *>("installedModsTable");QTRY_COMPARE(table->rowCount(),1);
        QVERIFY(table->item(0,4)->toolTip().contains(QStringLiteral("Quilt 官方兼容性提醒")));
    }
    void installedListWorksOfflineAndFiltersActualFiles(){
        QTemporaryDir dir;auto instance=target(dir.filePath("game"));
        QVERIFY(save(instance.gameDirectory+"/mods/user-renamed.jar",jar("local_test","1.2.3")));
        QVERIFY(save(instance.gameDirectory+"/mods/other.jar.disabled",jar("other","2.0.0")));
        mc::InstanceModsView view(instance,"Local instance");view.resize(1000,620);view.show();
        auto table=view.findChild<QTableWidget *>("installedModsTable");QTRY_COMPARE(table->rowCount(),2);
        auto search=view.findChild<QLineEdit *>("installedModSearch");auto filter=view.findChild<QComboBox *>("installedModFilter");
        int localRow=-1,disabledRow=-1;for(int row=0;row<2;++row){if(table->item(row,0)->text()=="local_test")localRow=row;else disabledRow=row;QVERIFY(!table->item(row,0)->icon().isNull());}
        QVERIFY(localRow>=0);QCOMPARE(table->item(localRow,1)->text(),QString("1.2.3"));QCOMPARE(table->item(disabledRow,4)->text(),QStringLiteral("已禁用"));
        search->setText("user-renamed");QVERIFY(!table->isRowHidden(localRow));QVERIFY(table->isRowHidden(disabledRow));
        search->clear();filter->setCurrentIndex(filter->findData("disabled"));QVERIFY(table->isRowHidden(localRow));QVERIFY(!table->isRowHidden(disabledRow));
        search->setText("absent");QVERIFY(view.findChild<QLabel *>("installedModsEmpty")->isVisible());
        search->clear();filter->setCurrentIndex(0);table->setCurrentCell(localRow,0);
        view.setMutationGuard([]{return false;});auto toggle=table->cellWidget(localRow,3)->findChild<QCheckBox *>();QVERIFY(toggle->isChecked());toggle->click();QVERIFY(toggle->isChecked());QVERIFY(QFile::exists(instance.gameDirectory+"/mods/user-renamed.jar"));
    }
    void installedModCheckboxesDriveBatchActionsAndFilterSelection(){
        QTemporaryDir dir;auto instance=target(dir.filePath("game"));const auto mods=instance.gameDirectory+"/mods/";
        QVERIFY(save(mods+"dep.jar",jar("dep")));QVERIFY(save(mods+"app.jar",jar("app","1.0.0",{{"dep","*"}})));QVERIFY(save(mods+"extra.jar",jar("extra")));
        mc::InstanceModsView view(instance,"Batch instance");view.resize(1100,720);view.show();auto table=view.findChild<QTableWidget *>("installedModsTable");QTRY_COMPARE(table->rowCount(),3);
        auto check=[&](const QString &file){for(auto choice:table->findChildren<QCheckBox *>("installedModSelection"))if(choice->property("fileName")==file)return choice;return static_cast<QCheckBox *>(nullptr);};
        auto all=view.findChild<QCheckBox *>("selectAllInstalledMods");auto disable=view.findChild<QPushButton *>("disableSelectedMod");auto find=view.findChild<QPushButton *>("findCompatibleMod");auto remove=view.findChild<QPushButton *>("removeInstalledMod");auto search=view.findChild<QLineEdit *>("installedModSearch");QVERIFY(all&&disable&&find&&remove);
        check("app.jar")->click();check("dep.jar")->click();QCOMPARE(table->selectionModel()->selectedRows().size(),2);QVERIFY(!find->isEnabled());QCOMPARE(all->checkState(),Qt::PartiallyChecked);
        search->setText("app");QCOMPARE(table->selectionModel()->selectedRows().size(),1);search->clear();QCOMPARE(table->selectionModel()->selectedRows().size(),1);all->click();QCOMPARE(table->selectionModel()->selectedRows().size(),3);
        view.setMutationGuard([]{return false;});disable->click();QVERIFY(QFile::exists(mods+"app.jar"));QVERIFY(QFile::exists(mods+"dep.jar"));
        view.setMutationGuard([]{return true;});disable->click();QTRY_VERIFY(table->isEnabled());QVERIFY(QFile::exists(mods+"app.jar.disabled"));QVERIFY(QFile::exists(mods+"dep.jar.disabled"));QCOMPARE(table->selectionModel()->selectedRows().size(),3);QCOMPARE(disable->text(),QStringLiteral("启用"));
        disable->click();QTRY_VERIFY(table->isEnabled());QVERIFY(QFile::exists(mods+"app.jar"));QVERIFY(QFile::exists(mods+"dep.jar"));QCOMPARE(table->selectionModel()->selectedRows().size(),3);
        remove->click();QTRY_COMPARE(table->rowCount(),0);QVERIFY(mc::localMods(instance).isEmpty());QVERIFY(!QDir(instance.gameDirectory+"/.m2cl-mod-backups").exists());
    }
    void optionalComponentChoicesDefaultOffAndShowCombinationErrors(){
        mc::ModPlan plan;plan.rootProjectId="root";mc::ResourceFile root;root.projectId="root";root.versionName="Main mod";plan.files={root};plan.resolved={root};plan.edges={{"root","extra","","optional"}};mc::ModPlan optional;optional.rootProjectId="extra";mc::ResourceFile extra;extra.projectId="extra";extra.versionName="Optional component";optional.files={extra};optional.resolved={extra};optional.edges={{"extra","root","","incompatible"}};plan.optionalPlans={optional};mc::ModPlanView dialog(plan);dialog.show();auto choice=dialog.findChild<QCheckBox *>("dependencyChoice");QVERIFY(choice);QVERIFY(!choice->isChecked());QVERIFY(choice->isEnabled());QCOMPARE(dialog.selectedPlan().files.size(),1);choice->setChecked(true);QVERIFY(!dialog.findChild<QLabel *>("modSelectionError")->isHidden());QVERIFY(!dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());choice->setChecked(false);QVERIFY(dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());QVERIFY(dialog.findChild<QLabel *>("modDependencyWarning")->isHidden());if(qEnvironmentVariableIsSet("M2CL_MOD_UI_REVIEW"))dialog.grab().save(qEnvironmentVariable("M2CL_MOD_UI_REVIEW")+"/optional-off-synthetic.png");
    }
    void installationConfirmationUsesSharedSections() {
        mc::applyTheme(mc::makeTheme("forest",false));
        mc::ModPlan plan;plan.target.id="survival";plan.displayTargetName=QStringLiteral("日常生存");plan.target.version="1.20.1";plan.target.loader="fabric";plan.target.loaderVersion="0.16.10";plan.rootProjectId="root";
        for(const auto &id:QStringList{"root","api","shared"}){mc::ResourceFile f;f.projectId=id;f.versionNumber=id=="root"?"6.4.0+1.20.1":"0.92.2+1.20.1";plan.resolved.append(f);}
        plan.projectNames={{"root","Comforts"},{"api","Fabric API"},{"shared","Cloth Config API"}};
        plan.files={plan.resolved[0],plan.resolved[1]};plan.reusedProjects=QStringList{"shared"};plan.reusedFiles.insert("shared","cloth-config.jar");plan.edges={{"root","api","","required"},{"root","shared","","required"}};
        mc::ModPlanView view(plan);view.show();QCoreApplication::processEvents();
        QStringList tones;for(auto frame:view.findChildren<QFrame *>())if(frame->property("installSection").isValid())tones.append(frame->property("installSection").toString());QVERIFY(tones.contains("add"));QVERIFY(tones.contains("reuse"));QVERIFY(tones.contains("warning"));
        QCOMPARE(view.selectedPlan().files.size(),2);QVERIFY(!view.windowFlags().testFlag(Qt::FramelessWindowHint));
        if(qEnvironmentVariableIsSet("M2CL_MOD_UI_REVIEW")){QDir().mkpath(qEnvironmentVariable("M2CL_MOD_UI_REVIEW"));view.grab().save(qEnvironmentVariable("M2CL_MOD_UI_REVIEW")+"/confirmation-light.png");view.findChild<QCheckBox *>("dependencyChoice")->setChecked(false);QTest::qWait(200);QCoreApplication::processEvents();view.resize(700,view.sizeHint().height());view.grab().save(qEnvironmentVariable("M2CL_MOD_UI_REVIEW")+"/confirmation-warning-light.png");mc::applyTheme(mc::makeTheme("forest",true));QCoreApplication::processEvents();view.grab().save(qEnvironmentVariable("M2CL_MOD_UI_REVIEW")+"/confirmation-warning-dark.png");mc::applyTheme(mc::makeTheme("forest",false));}
    }
    void nativeWindowToolbarCanDragWithoutMovingOnButtonClicks() {
        QWidget window;auto layout=new QVBoxLayout(&window);auto bar=new mc::WindowBar(&window);layout->addWidget(bar);window.resize(600,120);window.move(100,100);window.show();QCoreApplication::processEvents();
        const auto before=window.pos();const QPoint local(100,24);const QPoint global=bar->mapToGlobal(local);
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(local),QPointF(global),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(bar,&press);
        QMouseEvent move(QEvent::MouseMove,QPointF(local+QPoint(30,20)),QPointF(global+QPoint(30,20)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(bar,&move);
        QCOMPARE(window.pos(),before+QPoint(30,20));
        QTest::mouseRelease(bar,Qt::LeftButton);const auto after=window.pos();QTest::mouseClick(bar->downloads,Qt::LeftButton);QCOMPARE(window.pos(),after);
    }
    void embeddedDependenciesHaveNoRedownloadControl() {
        mc::ModPlan plan;plan.target.id="Example";plan.target.version="1.20.1";plan.target.loader="fabric";plan.target.loaderVersion="0.16.10";plan.rootProjectId="root";
        mc::ResourceFile root;root.projectId="root";plan.files={root};plan.resolved={root};plan.edges={{"root","bundled","","embedded"}};plan.projectNames={{"root","Example mod"},{"bundled","Bundled API"}};
        mc::ModPlanView view(plan);auto row=view.findChild<QFrame *>("embeddedDependency");QVERIFY(row);QVERIFY(row->findChildren<QRadioButton *>().isEmpty());QVERIFY(row->findChildren<QCheckBox *>().isEmpty());
        QStringList labels;for(auto label:row->findChildren<QLabel *>())labels.append(label->text());QVERIFY(labels.contains(QStringLiteral("随模组内置")));QVERIFY(labels.contains("Bundled API"));
        QCOMPARE(view.selectedPlan().files.size(),1);QVERIFY(view.findChildren<QCheckBox *>("dependencyChoice").isEmpty());
    }
    void installationIconsSupportReusedProjectsAndLocalFallback() {
        mc::ModPlan plan;plan.rootProjectId="root";
        for(const auto &id:QStringList{"root","existing","local"}){mc::ResourceFile f;f.projectId=id;plan.resolved.append(f);}
        plan.files={plan.resolved[0]};plan.reusedProjects=QStringList{"existing","local"};plan.edges={{"root","existing","","required"},{"root","local","","required"}};
        const QUrl url("https://cdn.modrinth.com/test.png");plan.projectIcons={{"root",url},{"existing",url},{"local",url}};
        QImage sample(8,8,QImage::Format_ARGB32);sample.fill(Qt::red);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);sample.save(&buffer,"PNG");plan.localProjectIcons.insert("local",bytes);
        mc::ModPlanView view(plan);QMap<QString,QLabel *> labels;for(auto label:view.findChildren<QLabel *>("installProjectIcon"))labels.insert(label->property("projectId").toString(),label);
        QCOMPARE(labels.size(),3);QCOMPARE(labels["local"]->property("iconSource").toString(),QString("archive"));
        auto client=view.findChild<mc::ResourceClient *>();QVERIFY(client);client->iconReady(url,"invalid image");QCOMPARE(labels["existing"]->property("iconSource").toString(),QString("fallback"));
        client->iconReady(url,bytes);QCOMPARE(labels["root"]->property("iconSource").toString(),QString("project"));QCOMPARE(labels["existing"]->property("iconSource").toString(),QString("project"));QCOMPARE(labels["local"]->property("iconSource").toString(),QString("archive"));
    }
    void installPlanOffersExplicitDependencyAndRedownloadChoices(){
        mc::ModPlan plan;plan.target.id="Preview instance";plan.target.version="1.20.1";plan.target.loader="fabric";plan.rootProjectId="root";
        for(const auto &id:QStringList{"root","mid","grand"}){mc::ResourceFile file;file.projectId=id;file.versionName=id;file.versionNumber="1.0";plan.resolved.append(file);}plan.files={plan.resolved.at(0),plan.resolved.at(1)};plan.reusedProjects=QStringList{"grand"};plan.reusedFiles.insert("grand","manually-renamed.jar");plan.edges={{"root","mid","","required"},{"mid","grand","","required"},{"root","grand","","required"},{"grand","root","","required"}};
        mc::ModPlanView dialog(plan);dialog.show();auto choices=dialog.findChildren<QCheckBox *>("dependencyChoice");QCOMPARE(choices.size(),1);QVERIFY(choices.first()->isChecked());QCOMPARE(dialog.selectedPlan().files.size(),2);auto warning=dialog.findChild<QLabel *>("modDependencyWarning");QVERIFY(warning->isHidden());
        choices.first()->setChecked(false);QVERIFY(!warning->isHidden());auto selected=dialog.selectedPlan();QCOMPARE(selected.files.size(),1);QCOMPARE(selected.files.first().projectId,QString("root"));QCOMPARE(selected.skippedProjects,QStringList{"mid"});auto buttons=dialog.findChild<QDialogButtonBox *>();QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
        auto again=dialog.findChild<QRadioButton *>("redownloadChoice");QVERIFY(again);QVERIFY(!again->isChecked());again->setChecked(true);selected=dialog.selectedPlan();QCOMPARE(selected.files.size(),2);QVERIFY(selected.reusedFiles.isEmpty());QVERIFY(selected.replacements.contains("manually-renamed.jar"));QVERIFY(!dialog.findChild<QLabel *>("modReplacementNotice")->isHidden());auto reuse=dialog.findChild<QRadioButton *>("reuseChoice");QVERIFY(reuse);QVERIFY(!reuse->isChecked());reuse->setChecked(true);QVERIFY(!again->isChecked());QVERIFY(dialog.selectedPlan().reusedProjects.contains("grand"));
        dialog.reject();QCOMPARE(dialog.result(),int(QDialog::Rejected));plan.error="Declared hard conflict";mc::ModPlanView blocked(plan);QVERIFY(!blocked.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
    }
    void supportedGameVersionsSeparateReleaseAndTestWithoutAppendingToReadme() {
        mc::ModVersionView view("Test",{},nullptr,{});mc::ResourceTags tags;tags.gameVersions=QStringList{"26.4-snapshot-6","26.3","26.2","26w14a","1.21.1","1.21","1.20.1","1.20-pre1","20w14infinite"};tags.gameVersionTypes={{"26.3","release"},{"26.4-snapshot-6","snapshot"},{"26w14a","snapshot"}};view.setSupportedVersionMetadata(tags);
        mc::ResourceProject project;project.body="Original description";project.gameVersions=tags.gameVersions;project.gameVersions.append("26.3");view.setProjectInfo(project);view.resize(840,620);view.show();
        auto sections=view.findChild<QTabWidget *>("modDetailSections");sections->setCurrentIndex(2);auto channels=view.findChild<QTabBar *>("supportedGameVersionChannels");auto pages=view.findChild<QStackedWidget *>("supportedGameVersionPages");
        QCOMPARE(channels->tabData(channels->currentIndex()).toString(),QString("release"));QCOMPARE(channels->tabText(0),QStringLiteral("正式版（5）"));QCOMPARE(channels->tabText(1),QStringLiteral("测试版（4）"));
        QString stable;for(auto label:pages->currentWidget()->findChildren<QLabel *>("supportedVersionValues"))stable+=label->text();QVERIFY(stable.contains("26.3"));QVERIFY(!stable.contains("snapshot"));QVERIFY(!stable.contains("20w14infinite"));
        channels->setCurrentIndex(1);QString tests;for(auto label:pages->currentWidget()->findChildren<QLabel *>("supportedVersionValues"))tests+=label->text();QVERIFY(tests.contains("26.4-snapshot-6"));QVERIFY(tests.contains("20w14infinite"));QVERIFY(tests.contains("1.20-pre1"));
        QCOMPARE(view.findChild<mc::ProjectReadme *>("modProjectReadme")->toPlainText(),QString("Original description"));
        QVERIFY(!view.findChild<QPushButton *>("supportedVersionFamily"));
        auto search=view.findChild<QLineEdit *>("supportedVersionSearch");QVERIFY(search);search->setText("SNAPSHOT");
        const auto matches=pages->currentWidget()->findChildren<QLabel *>("supportedVersionValues");QCOMPARE(matches.size(),1);QCOMPARE(matches.first()->text(),QString("26.4-snapshot-6"));
        QCOMPARE(channels->tabText(1),QStringLiteral("测试版（4）"));
        channels->setCurrentIndex(0);QVERIFY(pages->currentWidget()->findChildren<QLabel *>("supportedVersionValues").isEmpty());QVERIFY(pages->currentWidget()->findChild<QLabel *>("supportedVersionEmpty"));
        search->clear();QCOMPARE(pages->currentWidget()->findChildren<QLabel *>("supportedVersionValues").size(),5);channels->setCurrentIndex(1);
        tags.gameVersionTypes.insert("26.3","snapshot");view.setSupportedVersionMetadata(tags);QCOMPARE(channels->tabData(channels->currentIndex()).toString(),QString("test"));QCOMPARE(channels->tabText(0),QStringLiteral("正式版（4）"));
    }
    void supportedVersionLabelsWrapWithoutLosingExactValues() {
        mc::ModVersionView view("Test",{},nullptr,{});mc::ResourceProject project;
        for(int i=0;i<24;++i)project.gameVersions.append("1.21."+QString::number(i));
        for(int i=0;i<180;++i)project.gameVersions.append("26.4-snapshot-"+QString::number(i));
        project.gameVersions.append("1.21.2");view.setProjectInfo(project);view.resize(840,680);view.show();
        view.findChild<QTabWidget *>("modDetailSections")->setCurrentIndex(2);auto pages=view.findChild<QStackedWidget *>("supportedGameVersionPages");
        QTest::qWait(30);auto chips=pages->currentWidget()->findChildren<QLabel *>("supportedVersionValues");QCOMPARE(chips.size(),24);
        auto scroll=qobject_cast<QScrollArea *>(pages->currentWidget());QVERIFY(scroll);QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
        auto first=chips.first();bool wrapped=false;for(auto chip:chips){QVERIFY(chip->geometry().right()<chip->parentWidget()->width());if(chip->y()>first->y())wrapped=true;}QVERIFY(wrapped);
        view.resize(560,680);QTest::qWait(30);QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);QCOMPARE(pages->currentWidget()->findChildren<QLabel *>("supportedVersionValues").size(),24);
        view.findChild<QTabBar *>("supportedGameVersionChannels")->setCurrentIndex(1);QTest::qWait(30);chips=pages->currentWidget()->findChildren<QLabel *>("supportedVersionValues");QCOMPARE(chips.size(),180);
        scroll=qobject_cast<QScrollArea *>(pages->currentWidget());QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);QVERIFY(scroll->verticalScrollBar()->maximum()>0);
        for(auto chip:chips)QVERIFY(chip->geometry().right()<chip->parentWidget()->width());
        auto search=view.findChild<QLineEdit *>("supportedVersionSearch");search->setText("26.4-snapshot-179");QCOMPARE(pages->currentWidget()->findChildren<QLabel *>("supportedVersionValues").size(),1);search->clear();QCOMPARE(pages->currentWidget()->findChildren<QLabel *>("supportedVersionValues").size(),180);
    }
    void fullWidthModDetailsReturnWithoutResettingResults() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));mc::MainWindow window{mc::SettingsStore(dir.path())};window.resize(840,620);window.show();QTRY_VERIFY(window.ready());window.changePage(2);
        auto client=window.findChild<mc::ResourceClient *>();QList<mc::ResourceProject> projects;for(int i=0;i<8;++i){mc::ResourceProject p;p.name="Example";p.id=QString::number(i);projects.append(p);}client->projectsReady(projects);
        auto results=window.findChild<QListWidget *>("modResults");results->setCurrentRow(3);QTest::qWait(20);results->verticalScrollBar()->setValue(17);const auto scroll=results->verticalScrollBar()->value();
        results->itemWidget(results->item(3))->findChild<QPushButton *>()->click();
        mc::ResourceFile file;file.releaseType="release";client->versionsReady({file});QTest::qWait(180);
        auto views=window.findChild<QStackedWidget *>("modViews");QVERIFY(views);QCOMPARE(views->currentIndex(),1);QVERIFY(!results->isVisible());QVERIFY(!window.findChild<QSplitter *>("modMasterDetail"));
        auto pane=window.findChild<QWidget *>("modDetailsPane");QCOMPARE(pane->width(),views->width());
        auto view=window.findChild<mc::ModVersionView *>();QVERIFY(view);auto install=view->findChild<QPushButton *>("downloadModVersion");QVERIFY(install->mapTo(&window,QPoint(0,install->height())).y()<=window.height());
        view->findChild<QPushButton *>("backToMods")->click();QTest::qWait(160);QCOMPARE(views->currentIndex(),0);QCOMPARE(results->count(),8);QCOMPARE(results->currentRow(),3);QCOMPARE(results->verticalScrollBar()->value(),scroll);
    }
    void installDialogOnlyListsMatchingInstancesAndCancelIsInert() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path(),2));const auto root=dir.path()+"/minecraft";
        for(const auto &pair:QList<QPair<QString,QString>>{{"fabric-one","fabric"},{"fabric-two","fabric"},{"forge-one","forge"}}){
            const auto path=root+"/versions/"+pair.first;QDir().mkpath(path);QFile file(path+"/"+pair.first+".json");QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QJsonDocument(QJsonObject{{"id",pair.first},{"inheritsFrom","1.20.1"},{"libraries",QJsonArray{QJsonObject{{"name",pair.second=="fabric"?QString("net.fabricmc:fabric-loader:0.16.0"):QString("net.minecraftforge:forge:47.0.0")}}}}}).toJson());
        }
        mc::SettingsStore store(dir.path());mc::MainWindow window{store};window.show();QTRY_VERIFY(window.ready());window.changePage(2);const auto launch=store.load().selectedInstance;
        mc::ResourceProject project;project.id="test";project.name="Test mod";auto client=window.findChild<mc::ResourceClient *>();client->projectsReady({project});auto results=window.findChild<QListWidget *>("modResults");results->itemWidget(results->item(0))->findChild<QPushButton *>()->click();
        mc::ResourceFile file;file.releaseType="release";file.gameVersions=QStringList{"1.20.1"};file.loaders=QStringList{"fabric"};file.fileName="test.jar";client->versionsReady({file});QTest::qWait(180);
        auto view=window.findChild<mc::ModVersionView *>();QVERIFY(view);view->findChild<QTabWidget *>("modDetailSections")->setCurrentIndex(1);
        int candidates=-1;QStringList names;QTimer::singleShot(0,&window,[&]{auto dialog=window.findChild<QDialog *>("modInstallTargetDialog");if(!dialog)return;auto list=dialog->findChild<QListWidget *>("compatibleModInstances");if(list){candidates=list->count();for(int i=0;i<list->count();++i)names.append(QFileInfo(list->item(i)->data(Qt::UserRole).toString()).fileName());}dialog->reject();});
        view->findChild<QPushButton *>("downloadModVersion")->click();QCOMPARE(candidates,2);QVERIFY(names.contains("fabric-one")&&names.contains("fabric-two"));QCOMPARE(store.load().selectedInstance,launch);QVERIFY(store.load().modInstallTarget.isEmpty());
        QVERIFY(!QDir(root+"/versions/fabric-one/mods").exists());
    }
    void createGameUsesSharedDialogAndHidesVanillaBuilds() {
        QTemporaryDir dir;mc::applyTheme(mc::makeTheme("forest",false));
        mc::MainWindow window{mc::SettingsStore(dir.path())};window.setAppearance("light","forest");window.show();QTRY_VERIFY(window.ready());
        bool inspected=false;
        QTimer::singleShot(100,&window,[&]{
            auto dialog=window.findChild<QDialog *>("createGameDialog");
            if(!dialog)return;
            inspected=dialog->property("installationDialog").toBool();
            auto builds=dialog->findChild<QComboBox *>("loaderBuild");
            inspected=inspected&&builds&&builds->isHidden();
            bool hasQuilt=false;for(auto combo:dialog->findChildren<QComboBox *>())hasQuilt|=combo->findData("quilt")>=0;
            inspected=inspected&&hasQuilt;
            auto actions=dialog->findChild<QDialogButtonBox *>();
            inspected=inspected&&actions&&actions->button(QDialogButtonBox::Cancel)->text()==QStringLiteral("取消")&&actions->button(QDialogButtonBox::Ok)->property("primary").toBool();
            if(qEnvironmentVariableIsSet("M2CL_INSTALL_REVIEW"))dialog->grab().save(qEnvironmentVariable("M2CL_INSTALL_REVIEW"));
            dialog->reject();
        });
        window.findChild<QPushButton *>("createInstance")->click();QVERIFY(inspected);
    }
    void heroBadgesGrowWithFont() {
        mc::applyTheme(mc::makeTheme("forest",false));
        QWidget parent;parent.setStyleSheet("QLabel { color: white; background: #07120b; }");
        mc::HeroBadge badge(mc::Glyph::Java,"Java 17",&parent);
        badge.resize(badge.sizeHint());parent.show();QTest::qWait(10);
        const auto previous=badge.sizeHint();QFont larger=badge.font();larger.setPixelSize(26);badge.setFont(larger);
        QVERIFY(badge.sizeHint().height()>previous.height());QVERIFY(badge.sizeHint().width()>previous.width());
        QCOMPARE(badge.height(),badge.sizeHint().height());
    }
    void navigationAndEmptyState() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());
        auto pages=window.findChild<QStackedWidget *>();QVERIFY(pages);QCOMPARE(pages->count(),4);int index=0;
        for(auto button:window.findChildren<QPushButton *>())if(button->property("mainNavigation").toBool()){
            QTest::mouseClick(button,Qt::LeftButton);QCOMPARE(pages->currentIndex(),index<4?index:2);++index;QVERIFY(button->isChecked());}
        QCOMPARE(index,6);QCOMPARE(mc::SettingsStore(dir.path()).load().gameRoots,QStringList{dir.filePath("minecraft")});
        window.changePage(0);QVERIFY(window.findChild<mc::InstanceGrid *>("homeInstances")->findChild<QLabel *>("emptyInstances")->isVisible());
        QCOMPARE(window.findChildren<mc::InstanceCard *>().size(),0);
    }
    void themeControlsChangePaletteAndPersist() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());window.changePage(3);
        auto dark=window.findChild<mc::AppearanceChoice *>("appearance-dark");auto light=window.findChild<mc::AppearanceChoice *>("appearance-light");QVERIFY(dark);QVERIFY(light);
        auto sections=window.findChild<QStackedWidget *>("settingsSections");auto shortcut=window.findChild<mc::IconButton *>("appearanceButton");QVERIFY(sections&&shortcut);
        for(bool downloads:{false,true}){window.showJavaSettings(downloads);QCOMPARE(sections->currentIndex(),1);window.changePage(0);shortcut->click();QCOMPARE(sections->currentIndex(),0);QVERIFY(light->isVisible());QVERIFY(window.findChild<QPushButton *>("generalSettingsTab")->isChecked());}
        QComboBox *theme=nullptr;for(auto combo:window.findChildren<QComboBox *>())if(combo->findData("violet")>=0)theme=combo;QVERIFY(theme);
        QTest::mouseClick(dark,Qt::LeftButton);theme->setCurrentIndex(theme->findData("violet"));
        QCOMPARE(qApp->palette().color(QPalette::Window),mc::makeTheme("violet",true).background);
        const auto saved=mc::SettingsStore(dir.path()).load();QCOMPARE(saved.appearance,QString("dark"));QCOMPARE(saved.theme,QString("violet"));QVERIFY(dark->isChecked());
        QTest::mouseClick(light,Qt::LeftButton);QCOMPARE(qApp->palette().color(QPalette::Window),mc::makeTheme("violet",false).background);QVERIFY(!dark->isChecked());
        QCOMPARE(mc::SettingsStore(dir.path()).load().appearance,QString("light"));
        auto containsColor=[](const QImage &image,const QColor &color){int count=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y).rgb()==color.rgb()&&++count>=20)return true;return false;};
        for(const auto &name:QStringList{"blue","forest","sand","violet"}){
            window.setAppearance("light",name);QCoreApplication::processEvents();
            for(auto choice:window.findChildren<mc::AppearanceChoice *>()){
                const auto thumbnail=choice->grab(QRect(8,8,choice->width()-16,62)).toImage();
                QVERIFY2(containsColor(thumbnail,mc::makeTheme(name,choice->mode()=="dark").accent),qPrintable(name+" / "+choice->mode()));
                if(choice->mode()=="system")QVERIFY(containsColor(thumbnail,mc::makeTheme(name,true).accent));
            }
            if(qEnvironmentVariableIsSet("M2CL_APPEARANCE_REVIEW")){const auto output=qEnvironmentVariable("M2CL_APPEARANCE_REVIEW");QDir().mkpath(output);window.grab().save(output+"/appearance-"+name+"-light.png");window.setAppearance("dark",name);QCoreApplication::processEvents();window.grab().save(output+"/appearance-"+name+"-dark.png");}
        }
    }
    void embeddedCoversFollowBaseVersionAndIgnoreWorkingDirectory() {
        QVERIFY2(QImageReader::supportedImageFormats().contains("webp"),"Deployment must include the Qt WebP image plugin");
        QStringList versions;for(int i=0;i<=21;++i)versions.append(QStringLiteral("1.%1").arg(i));versions<<"26.1"<<"26.2"<<"26.3";
        QTemporaryDir dir;const auto previous=QDir::currentPath();QVERIFY(QDir::setCurrent(dir.path()));
        QList<QPixmap> pictures;for(const auto &version:versions)pictures.append(mc::CoverStore::image(version,QSize(320,180)));
        QDir::setCurrent(previous);
        for(int i=0;i<pictures.size();++i){const auto &picture=pictures[i];QVERIFY2(!picture.isNull(),qPrintable(versions[i]));QVERIFY(picture.width()<=384);QVERIFY(picture.height()<=256);}
        QCOMPARE(mc::CoverStore::family("1.7.10"),QString("1.7"));QVERIFY(mc::CoverStore::family("1.70.1").isEmpty());
        QCOMPARE(mc::CoverStore::family("1.19.4"),QString("1.19"));QCOMPARE(mc::CoverStore::family("26.1.2"),QString("26.1"));
        QCOMPARE(mc::CoverStore::family("26.3-rc-1"),QString("26.3"));QVERIFY(mc::CoverStore::family("26.4").isEmpty());
        if(qEnvironmentVariableIsSet("M2CL_COVER_REVIEW")){
            QPixmap sheet(1680,1110);sheet.fill(QColor("#f3f5ef"));QPainter painter(&sheet);painter.setFont(QFont("Arial",14));
            for(int i=0;i<pictures.size();++i){int x=16+(i%5)*332,y=16+(i/5)*220;painter.drawPixmap(QRect(x,y,316,178),pictures[i]);painter.setPen(QColor("#24372e"));painter.drawText(QRect(x,y+180,316,30),Qt::AlignCenter,versions[i]);}
            painter.end();QVERIFY(sheet.save(qEnvironmentVariable("M2CL_COVER_REVIEW")));
        }
    }
    void instanceCardsSelectAndFavoritePersistently() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path(),4));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());
        auto grid=window.findChild<mc::InstanceGrid *>("homeInstances");auto cards=grid->findChildren<mc::InstanceCard *>(QString(),Qt::FindDirectChildrenOnly);QCOMPARE(cards.size(),4);
        auto target=cards.at(1);QTest::mouseClick(target,Qt::LeftButton,Qt::NoModifier,QPoint(30,25));
        QCOMPARE(mc::SettingsStore(dir.path()).load().selectedInstance,target->instancePath());
        target->setFocus();auto star=target->findChild<mc::IconButton *>("favoriteInstance");QVERIFY(star);QTest::mouseClick(star,Qt::LeftButton);
        QVERIFY(mc::SettingsStore(dir.path()).load().favoriteInstances.contains(target->instancePath()));
        window.resize(840,620);QTest::qWait(20);for(auto item:cards)QVERIFY(item->geometry().right()<grid->width());
    }
    void instanceEditorSavesAndCancelPreservesValues() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path(),1));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());
        auto hero=window.findChild<QLabel *>("heroTitle");QVERIFY(hero);QCOMPARE(hero->text(),QString("1.20.1"));
        const auto path=mc::SettingsStore(dir.path()).load().selectedInstance;
        window.editInstance();auto dialog=window.findChild<QDialog *>("instanceSettings");QVERIFY(dialog);dialog->findChild<QLineEdit *>("instanceName")->setText("Cancelled rename");
        auto controls=dialog->findChild<QDialogButtonBox *>();QTest::mouseClick(controls->button(QDialogButtonBox::Cancel),Qt::LeftButton);QTRY_VERIFY(!dialog->isVisible());
        QVERIFY(!mc::SettingsStore(dir.path()).load().instanceNames.contains(path));QTest::qWait(1);
        window.editInstance();dialog=window.findChild<QDialog *>("instanceSettings");QVERIFY(dialog);dialog->findChild<QLineEdit *>("instanceName")->setText(QStringLiteral("樱谷生存"));
        controls=dialog->findChild<QDialogButtonBox *>();QTest::mouseClick(controls->button(QDialogButtonBox::Save),Qt::LeftButton);QTRY_VERIFY(window.ready());
        QCOMPARE(mc::SettingsStore(dir.path()).load().instanceNames.value(path).toString(),QStringLiteral("樱谷生存"));QCOMPARE(hero->text(),QStringLiteral("樱谷生存"));
        QVERIFY(QFile::exists(dir.filePath("minecraft/versions/1.20.1/1.20.1.json")));
    }
    void downloadPopoverShowsAndDismisses() {
        QTemporaryDir dir;QVERIFY(fixture(dir.path()));mc::MainWindow window{mc::SettingsStore(dir.path())};window.show();QTRY_VERIFY(window.ready());
        window.showDownloads();auto panel=window.findChild<mc::DownloadPanel *>();QVERIFY(panel);QTRY_VERIFY(panel->isVisible());QVERIFY(!panel->findChild<QProgressBar *>()->isVisible());
        QVERIFY(panel->windowFlags().testFlag(Qt::FramelessWindowHint));QVERIFY(panel->testAttribute(Qt::WA_TranslucentBackground));
        const auto image=panel->grab().toImage();for(const auto &point:{QPoint(0,0),QPoint(image.width()-1,0),QPoint(0,image.height()-1),QPoint(image.width()-1,image.height()-1)})QCOMPARE(image.pixelColor(point).alpha(),0);
        QCOMPARE(image.pixelColor(image.width()/2,qRound(8*panel->devicePixelRatioF())),qApp->palette().color(QPalette::Base));
        if(qEnvironmentVariableIsSet("M2CL_APPEARANCE_REVIEW")){const auto output=qEnvironmentVariable("M2CL_APPEARANCE_REVIEW");QDir().mkpath(output);for(bool dark:{false,true}){window.setAppearance(dark?"dark":"light","forest");QCoreApplication::processEvents();panel->grab().save(output+(dark?"/downloads-dark.png":"/downloads-light.png"));}}
        QTest::keyClick(panel,Qt::Key_Escape);QVERIFY(!panel->isVisible());
        panel->setTask("test.jar",QStringLiteral("正在下载"),500,true);panel->show();QCOMPARE(panel->findChild<QProgressBar *>()->value(),500);
        QSignalSpy cancelled(panel,&mc::DownloadPanel::cancelRequested);for(auto button:panel->findChildren<QPushButton *>())if(button->text()==QStringLiteral("取消下载"))QTest::mouseClick(button,Qt::LeftButton);
        QCOMPARE(cancelled.size(),1);panel->hide();
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
