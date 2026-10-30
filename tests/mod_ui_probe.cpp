// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

// Opt-in real Modrinth UI review; never run by CTest, never installs a mod.
#include "ui/mainwindow.h"
#include "ui/modversionview.h"
#include "ui/projectreadme.h"
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <functional>
int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setApplicationName("mclauncher-mod-ui-probe");app.setStyle("Fusion");app.setFont(mc::applicationFont());
    const auto args=app.arguments();if(args.size()<3){qCritical()<<"Usage: mod_ui_probe DATA_DIR OUTPUT.png [small|normal] [QUERY] [--target] [--dark]";return 2;}
    const auto output=args.at(2);QDir().mkpath(QFileInfo(output).path());
    const auto query=args.size()>4&&!args.at(4).startsWith("--")?args.at(4):QString("sodium");
    if(args.contains("--failure-state"))app.setProperty("offlineUiTest",true);
    mc::MainWindow window{mc::SettingsStore(args.at(1))};window.setAppearance(args.contains("--dark")?"dark":"light","forest");if(args.contains("small"))window.resize(840,620);window.show();
    auto client=window.findChild<mc::ResourceClient *>();if(!client)return 2;
    if(args.contains("--failure-state")){
        QTimer::singleShot(100,&window,[&]{window.changePage(2);window.findChild<QPushButton *>("searchMods")->click();
            client->tagsFailed(QStringLiteral("游戏版本加载失败：连接超时，请检查网络后重试"));client->failed(QStringLiteral("Modrinth 请求失败：连接超时，请检查网络后重试"));
            window.grab().save(output);auto combo=window.findChild<QComboBox *>("modGameVersion");combo->showPopup();
            QTimer::singleShot(60,&window,[&,combo]{auto popup=combo->findChild<QFrame *>("selectPopup");auto screenshot=window.grab();QPainter painter(&screenshot);painter.drawPixmap(window.mapFromGlobal(popup->pos()),popup->grab());painter.end();screenshot.save(output+".version-menu.png");app.exit(0);});
        });return app.exec();
    }
    bool configured=false,capturing=false,bodyLoaded=false;
    QObject::connect(client,&mc::ResourceClient::failed,&app,[&](const QString&e){qCritical().noquote()<<e;app.exit(1);});
    QObject::connect(client,&mc::ResourceClient::tagsFailed,&app,[&](const QString&e){qCritical().noquote()<<e;app.exit(1);});
    QObject::connect(client,&mc::ResourceClient::projectFailed,&app,[&](const QString&e){qCritical().noquote()<<e;app.exit(1);});
    QObject::connect(client,&mc::ResourceClient::projectReady,&app,[&](const mc::ResourceProject&p){bodyLoaded=true;qInfo()<<"Read official project body:"<<p.body.size()<<"characters";});
    QObject::connect(client,&mc::ResourceClient::tagsReady,&window,[&](const mc::ResourceTags&){
        if(configured)return;configured=true;
        auto version=window.findChild<QComboBox *>("modGameVersion");version->setCurrentIndex(version->findData("1.20.1"));auto loader=window.findChild<QComboBox *>("modLoader");loader->setCurrentIndex(loader->findData("fabric"));
        window.findChild<QLineEdit *>("modQuery")->setText(query);
        auto categories=window.findChild<QListWidget *>("modCategories");{const QSignalBlocker block(categories);categories->setCurrentRow(0);}
        window.findChild<QPushButton *>("searchMods")->click();
        QTimer::singleShot(50,&window,[&]{if(window.findChild<mc::BusySpinner *>("modSearchSpinner")->isVisible())window.grab().save(output+".loading.png");});
    });
    auto finish=[&]{
        window.findChild<QPushButton *>("backToMods")->click();
        QTimer::singleShot(180,&app,[&]{auto views=window.findChild<QStackedWidget *>("modViews");if(views->currentIndex()!=0){qCritical()<<"Back did not restore browse page";app.exit(1);return;}qInfo()<<"Full-page details, selected release channel, install-time selection and back navigation passed";app.exit(0);});
    };
    std::function<void()> inspectDetails=[&]{
        auto list=window.findChild<QListWidget *>("modResults");list->setCurrentRow(0);
        auto wait=new QTimer(&window);wait->setInterval(200);
        QObject::connect(wait,&QTimer::timeout,&window,[&,wait]{
            auto view=window.findChild<mc::ModVersionView *>();if(!view||!view->isVisible()||!bodyLoaded)return;
            if(view->graphicsEffect())return;
            auto readme=view->findChild<mc::ProjectReadme *>("modProjectReadme");
            if(args.contains("--readme-images")&&readme->pendingImages())return;
            if(args.contains("--readme-images")){qInfo()<<"Description images loaded:"<<readme->loadedImages()<<"failed:"<<readme->failedImages()<<"characters:"<<readme->toPlainText().size();if(!readme->loadedImages()){qCritical()<<"No description images rendered";app.exit(1);return;}}
            wait->stop();if(QApplication::activeModalWidget()){app.exit(1);return;}
            window.grab().save(output+".intro.png");
            if(args.contains("--readme-images")&&readme->find("FAQs")){
                if(!readme->toPlainText().contains("Yes, it is safe to update")){qCritical()<<"FAQ answer missing";app.exit(1);return;}
                readme->verticalScrollBar()->setValue(readme->verticalScrollBar()->value()+readme->cursorRect().top()-12);auto cursor=readme->textCursor();cursor.clearSelection();readme->setTextCursor(cursor);window.grab().save(output+".faq.png");readme->verticalScrollBar()->setValue(0);
            }
            if(args.contains("--readme-images")){
                for(const auto &entry:QList<QPair<QString,QString>>{{"Features","features"},{"Modpacks and Other","conditions"}}){
                    readme->moveCursor(QTextCursor::Start);if(!readme->find(entry.first))continue;
                    readme->verticalScrollBar()->setValue(readme->verticalScrollBar()->value()+readme->cursorRect().top()-12);auto cursor=readme->textCursor();cursor.clearSelection();readme->setTextCursor(cursor);window.grab().save(output+"."+entry.second+".png");
                }
                readme->verticalScrollBar()->setValue(0);
            }
            auto tabs=view->findChild<QTabWidget *>("modDetailSections");tabs->setCurrentIndex(2);
            QCoreApplication::processEvents();window.grab().save(output+".supported-release.png");
            auto search=view->findChild<QLineEdit *>("supportedVersionSearch");search->setText("1.20");QCoreApplication::processEvents();window.grab().save(output+".supported-search.png");search->clear();
            auto supported=view->findChild<QTabBar *>("supportedGameVersionChannels");supported->setCurrentIndex(1);QCoreApplication::processEvents();window.grab().save(output+".supported-test.png");tabs->setCurrentIndex(1);
            if(args.contains("--show-beta"))view->findChild<QTabBar *>("modReleaseChannel")->setCurrentIndex(1);
            if(args.contains("--all-file-versions"))view->findChild<QComboBox *>("modFileGameVersion")->setCurrentIndex(0);
            QTimer::singleShot(80,&window,[&,view]{
                auto channel=view->findChild<QTabBar *>("modReleaseChannel");auto install=view->findChild<QPushButton *>("downloadModVersion");
                auto versions=view->findChild<QTableWidget *>("modFileVersions");bool hasStable=false;for(int row=0;row<versions->rowCount();++row)if(!versions->isRowHidden(row))hasStable=true;
                if(channel->tabData(channel->currentIndex())!=(args.contains("--show-beta")?"beta":"release")||install->isEnabled()!=hasStable){qCritical()<<"Release channel file selection failed";app.exit(1);return;}
                if(hasStable){const auto row=view->selectedIndex();const auto rect=versions->visualItemRect(versions->item(row,0));qInfo()<<"Selected file row / visible rect:"<<row<<rect<<versions->viewport()->rect();if(!rect.intersects(versions->viewport()->rect())||!versions->item(row,0)->isSelected()){qCritical()<<"Selected file is outside the visible table";app.exit(1);return;}}
                window.grab().save(output+".download.png");
                if(args.contains("--review-loaders")){auto loaders=view->findChild<QTabBar *>("modFileLoaders");const auto current=loaders->currentIndex();for(int i=1;i<loaders->count();++i){loaders->setCurrentIndex(i);QCoreApplication::processEvents();window.grab().save(output+".loader-"+loaders->tabData(i).toString()+".png");}loaders->setCurrentIndex(current);}
                if(!args.contains("--target")){finish();return;}
                int candidates=-1;
                QTimer::singleShot(0,&window,[&]{auto dialog=window.findChild<QDialog *>("modInstallTargetDialog");if(!dialog)return;candidates=dialog->findChild<QListWidget *>("compatibleModInstances")->count();dialog->grab().save(output+".instances.png");dialog->reject();});
                install->click();if(candidates<=0){qCritical()<<"No matching preview instance";app.exit(1);return;}qInfo()<<"Matching install targets:"<<candidates;finish();
            });
        });wait->start();list->itemWidget(list->item(0))->findChild<QPushButton *>()->click();
    };
    auto capturePopup=[&](const QString &name,const QString &suffix,std::function<void()> next){
        auto combo=window.findChild<QComboBox *>(name);combo->showPopup();
        QTimer::singleShot(80,&window,[&,combo,suffix,next]{
            auto popup=combo->findChild<QFrame *>("selectPopup");if(!popup){app.exit(1);return;}
            auto screenshot=window.grab();QPainter painter(&screenshot);painter.drawPixmap(window.mapFromGlobal(popup->pos()),popup->grab());painter.end();screenshot.save(output+suffix);
            combo->hidePopup();next();
        });
    };
    QObject::connect(client,&mc::ResourceClient::projectsReady,&window,[&](const QList<mc::ResourceProject>&projects){
        if(!configured||capturing)return;if(projects.isEmpty()){qCritical()<<"No filtered projects";app.exit(1);return;}capturing=true;
        auto wait=new QTimer(&window);wait->setInterval(100);
        QObject::connect(wait,&QTimer::timeout,&window,[&,wait,ticks=0]()mutable{
            auto icon=window.findChild<QListWidget *>("modResults")->findChild<QLabel *>("modProjectIcon");
            if(!icon||!icon->property("iconLoaded").toBool()){if(++ticks<150)return;window.grab().save(output+".icon-failure.png");qCritical()<<"Project icon failed"<<(icon?icon->property("iconUrl"):QVariant());app.exit(1);return;}
            wait->stop();window.grab().save(output);
            qInfo()<<"Real project metadata, category and source filters loaded";
            capturePopup("modGameVersion",".version-menu.png",[&]{capturePopup("modLoader",".loader-menu.png",inspectDetails);});
        });wait->start();
    });
    QTimer::singleShot(0,&window,[&]{window.changePage(2);});QTimer::singleShot(90000,&app,[&]{qCritical()<<"Mod UI review timed out";app.exit(1);});return app.exec();
}
