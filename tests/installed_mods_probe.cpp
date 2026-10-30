// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

// Opt-in real-file UI review. Downloads only into an automatically removed test instance.
#include "ui/mainwindow.h"
#include "ui/instancemods.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc,char **argv){
    QApplication app(argc,argv);app.setApplicationName("m2cl-installed-mods-review");app.setStyle("Fusion");app.setFont(mc::applicationFont());
    const auto args=app.arguments();if(args.size()<2){qCritical()<<"Usage: installed_mods_probe OUTPUT_DIR [--stay]";return 2;}
    const auto output=QFileInfo(args.at(1)).absoluteFilePath();QDir().mkpath(output);QTemporaryDir temporary;if(!temporary.isValid())return 2;
    const auto base=QFileInfo(temporary.path()).canonicalFilePath();
    const auto root=base+"/minecraft",instancePath=root+"/versions/1.20.1-fabric";
    QDir().mkpath(instancePath+"/mods");
    QFile manifest(instancePath+"/1.20.1-fabric.json");if(!manifest.open(QIODevice::WriteOnly))return 2;
    manifest.write(QJsonDocument(QJsonObject{{"id","1.20.1"},{"libraries",QJsonArray{QJsonObject{{"name","net.fabricmc:fabric-loader:0.16.10"}}}}}).toJson());manifest.close();
    mc::Instance instance;instance.id="1.20.1-fabric";instance.path=instancePath;instance.gameDirectory=instancePath;instance.root=root;instance.version="1.20.1";instance.loader="fabric";instance.loaderVersion="0.16.10";
    QNetworkAccessManager network;const auto cancel=std::make_shared<std::atomic_bool>(false);
    try{
        for(const auto &project:QStringList{"comforts","lithium","appleskin"}){
            auto bytes=mc::fetchInstallData(mc::ModrinthProvider().versionsUrl(project,instance.version,instance.loader),cancel,{},&network);
            const auto files=mc::ModrinthProvider::files(QJsonDocument::fromJson(bytes).array());mc::ResourceFile selected;
            for(const auto &file:files)if(file.releaseType=="release"&&mc::ModrinthProvider::compatible(file,instance.version,instance.loader)){selected=file;break;}
            if(selected.versionId.isEmpty()){qCritical()<<"No compatible stable build:"<<project;return 1;}
            const auto plan=mc::planModInstall(instance,selected,cancel,&network);if(!plan.error.isEmpty()){qCritical().noquote()<<plan.error;return 1;}
            const auto result=mc::applyModPlan(plan,cancel,{},&network);if(!result.error.isEmpty()){qCritical().noquote()<<result.error;return 1;}
        }
        const auto mods=mc::localMods(instance);if(mods.size()<4)return 1;
        for(const auto &mod:mods)qInfo().noquote()<<"Actual JAR:"<<mod.name<<mod.version<<"local icon bytes:"<<mc::localModIcon(instancePath+"/mods/"+mod.fileName,mod).size();
        for(const auto &mod:mods)if(mod.id=="lithium"){const auto error=mc::setModEnabled(instance,mod.fileName,false);if(!error.isEmpty()){qCritical()<<error;return 1;}}
    }catch(const std::exception &error){qCritical()<<error.what();return 1;}
    mc::Settings settings;settings.gameRoots=QStringList{root};settings.selectedInstance=instancePath;settings.instanceNames.insert(instancePath,QStringLiteral("模组管理 · 实测实例"));settings.appearance="light";
    mc::SettingsStore store(temporary.path()+"/settings");if(!store.save(settings))return 2;
    mc::MainWindow window(store);window.resize(1280,800);window.setAppearance("light","forest");window.show();
    auto wait=new QTimer(&window);wait->setInterval(100);
    QObject::connect(wait,&QTimer::timeout,&window,[&,opened=false,captured=false,ticks=0]()mutable{
        if(++ticks>300){qCritical()<<"UI review timed out";app.exit(1);return;}
        if(!window.ready())return;if(!opened){opened=true;window.showInstanceMods(instancePath);return;}
        auto view=window.findChild<mc::InstanceModsView *>();if(!view)return;
        auto table=view->findChild<QTableWidget *>("installedModsTable");if(table->rowCount()<4||!table->isEnabled()||captured)return;
        captured=true;wait->stop();
        QTimer::singleShot(500,&window,[&,view,table]{
            window.grab().save(output+"/actual-light.png");
            auto search=view->findChild<QLineEdit *>("installedModSearch");search->setText("comforts");int visible=0;for(int row=0;row<table->rowCount();++row)if(!table->isRowHidden(row))++visible;
            if(visible!=1){qCritical()<<"Real file search failed";app.exit(1);return;}
            window.grab().save(output+"/actual-search.png");search->clear();
            int comforts=-1;for(int row=0;row<table->rowCount();++row)if(table->item(row,0)->text()=="Comforts")comforts=row;
            if(comforts<0){app.exit(1);return;}table->setCurrentCell(comforts,0);
            auto toggle=table->cellWidget(comforts,3)->findChild<QCheckBox *>();toggle->click();
            QTimer::singleShot(400,&window,[&,view,table]{
                bool disabled=false;for(const auto &mod:mc::localMods(instance))if(mod.id=="comforts")disabled=!mod.enabled;
                if(!disabled){qCritical()<<"Real disable failed";app.exit(1);return;}
                int row=table->currentRow();if(row<0){app.exit(1);return;}auto toggle=table->cellWidget(row,3)->findChild<QCheckBox *>();toggle->click();
                QTimer::singleShot(500,&window,[&]{
                    bool enabled=false;for(const auto &mod:mc::localMods(instance))if(mod.id=="comforts")enabled=mod.enabled;
                    if(!enabled){qCritical()<<"Real enable failed";app.exit(1);return;}
                    window.grab().save(output+"/actual-selected.png");
                    auto table=window.findChild<QTableWidget *>("installedModsTable");if(table->horizontalHeader()->visualIndex(4)>=table->horizontalHeader()->visualIndex(3)){qCritical()<<"Status must precede the enable switch";app.exit(1);return;}
                    for(int row=0;row<table->rowCount();++row)if(!table->selectionModel()->isRowSelected(row,{})){table->cellWidget(row,6)->findChild<QCheckBox *>()->click();break;}
                    QEventLoop settle;QTimer::singleShot(150,&settle,&QEventLoop::quit);settle.exec();window.grab().save(output+"/actual-batch-selected.png");window.setAppearance("dark","forest");
                    QTimer::singleShot(100,&window,[&]{window.grab().save(output+"/actual-dark.png");window.resize(840,620);
                        QTimer::singleShot(100,&window,[&]{window.grab().save(output+"/actual-small-dark.png");window.setAppearance("light","forest");
                            QTimer::singleShot(100,&window,[&]{window.grab().save(output+"/actual-small-light.png");window.resize(1280,800);qInfo()<<"PASS: actual downloads, local icons, search, disable/enable, light/dark and small window. No game or JAR executed.";if(!args.contains("--stay"))app.exit(0);});
                        });
                    });
                });
            });
        });
    });wait->start();return app.exec();
}
