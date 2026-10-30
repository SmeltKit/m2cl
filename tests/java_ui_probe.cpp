// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

// Opt-in live UI request check. Never run by CTest.
#include "ui/mainwindow.h"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
int main(int argc, char **argv) {
    QApplication app(argc, argv); app.setApplicationName("mclauncher-ui-probe"); app.setStyle("Fusion");
    app.setFont(mc::applicationFont());
    if (argc < 4) { qCritical() << "Usage: java_ui_probe DATA_DIR PROVIDER OUTPUT.png"; return 2; }
    const auto args = app.arguments(); mc::MainWindow window{mc::SettingsStore(args.at(1))}; window.show(); window.showJavaSettings(true);
    if (args.at(2) == "local") {
        window.showJavaSettings();
        if (args.contains("--dark")) window.setAppearance("dark", "forest");
        QTimer::singleShot(100, &window, [&] {
            auto table = window.findChild<QTableWidget *>("localJavaList");
            if (!table->rowCount()) { app.exit(1); return; }
            if (args.contains("--selected")) table->selectRow(0); else table->clearSelection();
            QTimer::singleShot(50, &window, [&] { QDir().mkpath(QFileInfo(args.at(3)).path()); app.exit(window.grab().save(args.at(3)) ? 0 : 1); });
        });
        return app.exec();
    }
    if (args.at(2) == "menu") {
        window.showJavaSettings();
        QTimer::singleShot(100, &window, [&] {
            auto button = window.findChild<QPushButton *>("quickScanJava");
            auto menu = button->menu();
            menu->popup(button->mapToGlobal(QPoint(0, button->height())));
            QTimer::singleShot(100, menu, [&, menu] { QDir().mkpath(QFileInfo(args.at(3)).path()); app.exit(menu->grab().save(args.at(3)) ? 0 : 1); });
        });
        return app.exec();
    }
    auto provider = window.findChild<QComboBox *>("javaDistribution"); auto query = window.findChild<QPushButton *>("queryJavaBuilds"); auto table = window.findChild<QTableWidget *>("javaBuildList");
    if (provider->findData(args.at(2)) < 0) return 2; provider->setCurrentIndex(provider->findData(args.at(2)));
    QTimer::singleShot(0, query, &QPushButton::click);
    QTimer::singleShot(40,&window,[&]{
        auto spinner=window.findChild<mc::BusySpinner *>("javaQuerySpinner");
        if(!spinner || !spinner->isVisible())return;
        QDir().mkpath(QFileInfo(args.at(3)).path());window.grab().save(args.at(3)+".loading.png");
    }); QTimer timer; timer.setInterval(100); int ticks = 0;
    QObject::connect(&timer, &QTimer::timeout, &window, [&] {
        ++ticks; if (ticks < 3 || !query->isEnabled()||window.findChild<QDialog *>("javaBuildDetailsDialog")) return;
        if (table->rowCount() == 0) { qCritical().noquote() << window.findChild<QLabel *>("javaPackageStatus")->text(); app.exit(1); return; }
        const auto expected = args.at(2) == "zulu" ? "Zulu" : args.at(2) == "temurin" ? "Temurin" : args.at(2) == "microsoft" ? "Microsoft" : args.at(2)=="oracle-openjdk"?"Oracle OpenJDK":"Mojang";
        if (args.at(2) != "all") for (int row = 0; row < table->rowCount(); ++row) if (table->item(row, 2)->text() != expected) { qCritical() << "Provider filter leaked a different distribution"; app.exit(1); return; }
        if (!window.findChild<QPushButton *>("installJavaBuild")->isEnabled()) { qCritical() << "Selected build cannot be installed"; app.exit(1); return; }
        QDir().mkpath(QFileInfo(args.at(3)).path()); if (!window.grab().save(args.at(3))) { app.exit(1); return; }
        qInfo() << "Live query populated" << table->rowCount() << "rows; provider filter and selection passed";if(args.contains("--details")){window.findChild<QPushButton *>("javaBuildDetails")->click();QTimer::singleShot(80,&app,[&]{auto dialog=window.findChild<QDialog *>("javaBuildDetailsDialog");if(!dialog){app.exit(1);return;}dialog->grab().save(args.at(3)+".license.png");app.exit(0);});return;}app.exit(0);
    }); timer.start();
    QTimer::singleShot(120000, &app, [&] { qCritical() << "Live UI query timed out"; app.exit(1); }); return app.exec();
}
