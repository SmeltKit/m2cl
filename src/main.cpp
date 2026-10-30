// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "ui/mainwindow.h"
#include <QApplication>
#include "core/i18n.h"
#include <QTabWidget>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QDialog>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("mclauncher"); app.setOrganizationName("mclauncher"); app.setApplicationVersion("0.1.0");
    app.setApplicationDisplayName("Mercury MC Launcher");
    app.setWindowIcon(mc::applicationIcon());
#ifdef Q_OS_LINUX
    app.setDesktopFileName(QStringLiteral("app.mclauncher"));
#endif
    app.setStyle("Fusion");
    app.setFont(mc::applicationFont());
    QCommandLineParser parser; parser.setApplicationDescription(QStringLiteral("Mercury MC Launcher (M2CL)"));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"data-dir", "Store settings in this directory", "directory"});
    parser.addOption({"language", "UI language: system, zh_CN, zh_TW or en_US (does not change saved settings)", "language"});
    parser.addOption({"smoke-test", "Render the window and exit without network requests"});
    parser.addOption({"screenshot", "Save a PNG of the window (use with --smoke-test)", "path"});
    parser.addOption({"screenshot-widget", "Capture a named widget instead of the full window", "name"});
    parser.addOption({"appearance", "Display mode: light, dark or system", "mode"});
    parser.addOption({"theme", "Theme: forest, blue, sand or violet", "name"});
    parser.addOption({"page", "Initial page: home, library, mods or settings", "name"});
    parser.addOption({"window-size", "Initial window size, for example 840x620", "size"});
    parser.addOption({"accounts", "Open account management"});
    parser.addOption({"java-settings", "Open Java management"});
    parser.addOption({"java-downloads", "Open the Java build download tab"});
    parser.addOption({"downloads", "Open the download panel"});
    parser.addOption({"instance-java-settings", "Open the selected instance Java settings"});
    parser.addOption({"instance-launch-settings", "Open the selected instance memory, window and launch options"});
    parser.addOption({"instance-components", "Open version components for an instance path", "path"});
    parser.addOption({"instance-settings", "Open the selected instance settings"});
    parser.addOption({"instance-resources", "Open resources for an instance path", "path"});
    parser.addOption({"resource-tab", "Resource tab: mods, shaders, resourcepacks", "name"});
    parser.process(app);
    QTemporaryDir temporary;
    QString directory = parser.value("data-dir");
    if (directory.isEmpty()) directory = parser.isSet("smoke-test") ? temporary.path() : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const auto language=parser.isSet("language")?parser.value("language"):mc::SettingsStore(directory).load().language;
    if(!mc::installLanguage(app,language)){std::fputs("Unsupported language or missing translation catalog\n",stderr);return 2;}
    app.setFont(mc::applicationFont());
    mc::MainWindow window{mc::SettingsStore(directory)};
    const auto size=QRegularExpression("^(\\d{3,4})x(\\d{3,4})$").match(parser.value("window-size"));
    if(size.hasMatch())window.resize(qBound(840,size.captured(1).toInt(),3840),qBound(620,size.captured(2).toInt(),2160));
    if (parser.isSet("appearance") || parser.isSet("theme")) {
        const auto settings = mc::SettingsStore(directory).load();
        window.setAppearance(parser.value("appearance").isEmpty() ? settings.appearance : parser.value("appearance"), parser.value("theme").isEmpty() ? settings.theme : parser.value("theme"));
    }
    window.show();
    if (parser.isSet("page")) window.changePage(QStringList{"home", "library", "mods", "settings"}.indexOf(parser.value("page")));
    if (parser.isSet("java-settings") || parser.isSet("java-downloads")) window.showJavaSettings(parser.isSet("java-downloads"));
    if(!parser.isSet("smoke-test")&&parser.isSet("instance-launch-settings")){
        auto open=new QTimer(&window);open->setInterval(100);QObject::connect(open,&QTimer::timeout,&window,[&window,open]{if(window.ready()){open->stop();window.editInstance({},3);open->deleteLater();}});open->start();
    }
    if(parser.isSet("accounts"))window.showAccounts();
    if(parser.isSet("instance-components")){
        auto open=new QTimer(&window);open->setInterval(50);QObject::connect(open,&QTimer::timeout,&window,[&,open]{if(window.ready()){open->stop();window.showVersionComponents(parser.value("instance-components"));open->deleteLater();}});open->start();
    }
    if(parser.isSet("instance-resources")){
        auto open=new QTimer(&window);open->setInterval(50);QObject::connect(open,&QTimer::timeout,&window,[&,open]{if(window.ready()){open->stop();window.showInstanceMods(parser.value("instance-resources"));if(auto tabs=window.findChild<QTabWidget *>("instanceResourceTabs"))tabs->setCurrentIndex(qMax(0,QStringList{"mods","shaders","resourcepacks"}.indexOf(parser.value("resource-tab"))));open->deleteLater();}});open->start();
    }
    if (parser.isSet("smoke-test")) {
        auto timer = new QTimer(&app); timer->setInterval(100);
        QObject::connect(timer, &QTimer::timeout, &window, [&, timer, ticks = 0, panelOpened = false]() mutable {
            if (!window.ready() && ++ticks < 100) return;
            if(!panelOpened&&(parser.isSet("downloads")||parser.isSet("instance-settings")||parser.isSet("instance-java-settings")||parser.isSet("instance-launch-settings"))){
                panelOpened=true;if(parser.isSet("downloads"))window.showDownloads();else window.editInstance({}, parser.isSet("instance-launch-settings")?3:parser.isSet("instance-java-settings") ? 2 : 0);return;
            }
            if (!window.ready()) { std::fputs("Catalog scan timed out\n", stderr); app.exit(2); return; }
            if(parser.isSet("instance-resources"))if(auto spinner=window.findChild<mc::BusySpinner *>("installedModsSpinner");spinner&&spinner->isVisible()&&++ticks<100)return;
            timer->stop();
            const auto path = parser.value("screenshot");
            if (!path.isEmpty()) {
                QDir().mkpath(QFileInfo(path).absolutePath());
                QWidget *capture=QApplication::activeModalWidget()?QApplication::activeModalWidget():&window;
                if(parser.isSet("screenshot-widget"))capture=window.findChild<QWidget *>(parser.value("screenshot-widget"));
                if(!capture){std::fputs("Screenshot widget not found\n",stderr);app.exit(4);return;}
                QPixmap screenshot=capture->grab();
                if(capture==&window)if(auto popup=window.findChild<mc::DownloadPanel *>();popup&&popup->isVisible()){
                    QPainter painter(&screenshot);painter.drawPixmap(window.mapFromGlobal(popup->pos()),popup->grab());
                }
                if (!screenshot.save(path)) { std::fputs("Cannot save screenshot\n", stderr); app.exit(3); return; }
                window.writeDiagnostics(path + ".json");
            }
            app.exit(0);
        });
        timer->start();
    }
    return app.exec();
}
