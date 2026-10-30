// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "core/i18n.h"
#include "accountspanel.h"
#include "accountwidgets.h"
#include "network/networkerror.h"
#include "networknotice.h"
#include "network/downloadsource.h"
#include "versioncomponentsdialog.h"
#include "instancepacks.h"
#include "installplandialog.h"
#include "projectlegal.h"
#include "mainwindow.h"
#include "network/forgeinstaller.h"
#include "network/optifine.h"
#include "launchoptionswidget.h"
#include "modplanview.h"
#include "modversionview.h"
#include "instancemods.h"
#ifdef Q_OS_MACOS
#include "macwindow.h"
#endif
#include "core/gamelog.h"
#include <QApplication>
#include <QBuffer>
#include <QButtonGroup>
#include <algorithm>
#include <QImageReader>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFontInfo>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHash>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QProcess>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QSplitter>
#include <QUuid>
#include <QRegularExpression>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QSysInfo>
#include <QStorageInfo>
#include <QTimer>
#include <QTabWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>
#include <QtConcurrent/QtConcurrentRun>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

namespace mc {
namespace {
QLabel *label(const QString &text, const char *name = nullptr, bool muted = false) {
    auto widget = new QLabel(text);
    widget->setWordWrap(true);
    if (name) widget->setObjectName(name);
    if (muted) { widget->setProperty("muted", true); widget->setProperty("secondaryText", true); }
    return widget;
}
QPushButton *button(const QString &text, bool primary = false) {
    auto widget = new QPushButton(text);
    widget->setCursor(Qt::PointingHandCursor);
    if (primary) widget->setProperty("primary", true);
    return widget;
}
QFrame *card() {
    auto frame = new QFrame;
    frame->setProperty("card", true);
    return frame;
}
QVBoxLayout *column(QWidget *widget, int margin = ui::CardMargin) {
    auto layout = new QVBoxLayout(widget);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(ui::SectionSpace);
    return layout;
}
QWidget *page(QVBoxLayout **layout) {
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto contents = new QWidget;
    *layout = column(contents, ui::PageMargin);
    (*layout)->setContentsMargins(ui::PageMargin, 8, ui::PageMargin, ui::PageMargin);
    scroll->setWidget(contents);
    return scroll;
}
QIcon categoryIcon(const QString &id) {
    static const QHash<QString,Glyph> icons{{"adventure",Glyph::Home},{"decoration",Glyph::Image},{"optimization",Glyph::Leaf},{"technology",Glyph::Settings},{"worldgen",Glyph::Grass},{"magic",Glyph::Star},{"storage",Glyph::Folder},{"library",Glyph::Mods},{"utility",Glyph::Settings},{"management",Glyph::Settings},{"social",Glyph::Home},{"transportation",Glyph::Play},{"equipment",Glyph::Mods},{"food",Glyph::Leaf}};
    return lineIcon(icons.value(id,Glyph::Mods));
}
QLabel *badge(const QString &text,Glyph icon) { return new HeroBadge(icon,text); }
}

MainWindow::MainWindow(SettingsStore store, QWidget *parent)
    : QMainWindow(parent), m_store(std::move(store)), m_systemPalette(qApp->palette()), m_resources(this) {
    QString error;
    m_settings = m_store.load(&error);
    m_accounts=new AccountManager(m_store.directory(),m_settings.offlinePlayer,this);
    m_avatars=new AvatarCache(m_store.directory(),this);
    m_javas = javaCatalogFromJson(m_settings.javaCatalog);
    if (m_settings.gameRoots.isEmpty() && QDir(defaultGameRoot()).exists()) m_settings.gameRoots.append(defaultGameRoot());
    setWindowTitle(QStringLiteral("Mercury MC Launcher · M²CL"));
#ifdef Q_OS_WIN
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);setAttribute(Qt::WA_TranslucentBackground);
#else
    setWindowFlags(Qt::Window);
#ifdef Q_OS_MACOS
    setWindowFlag(Qt::WindowFullscreenButtonHint);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    // Keep native traffic lights, with the application background underneath.
    setWindowFlag(Qt::ExpandedClientAreaHint);
    setWindowFlag(Qt::NoTitleBarBackgroundHint);
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
#endif
#endif
#endif
    resize(1140, 780); setMinimumSize(840, 620);
    auto root = new QWidget; root->setObjectName("appRoot");root->setProperty("nativeWindow",!windowFlags().testFlag(Qt::FramelessWindowHint));setCentralWidget(root);
#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    root->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
#endif
    auto layout = new QHBoxLayout(root); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto sidebar = new QFrame; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(currentLanguage()=="en_US"?192:164);
    auto side = column(sidebar, 14);side->setSpacing(7);
#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    // Reserve the native control strip without pushing down the whole page.
    side->setContentsMargins(14, 42, 14, 14);
#endif
    auto brandHeading = new QHBoxLayout;brandHeading->setSpacing(8);
    auto mark = new BrandMark(36);mark->setObjectName("brandMark");
    setWindowIcon(applicationIcon());
    auto brandText=new BrandWordmark;brandText->setObjectName("brandText");
    brandHeading->addWidget(mark);brandHeading->addWidget(brandText);brandHeading->addStretch();side->addLayout(brandHeading);
    side->addSpacing(22);
    const QStringList nav{QCoreApplication::translate("M2CL", "开始"), QCoreApplication::translate("M2CL", "游戏库"), QCoreApplication::translate("M2CL", "模组"), QCoreApplication::translate("M2CL", "设置")};
    const QList<Glyph> icons{Glyph::Home,Glyph::Grid,Glyph::Mods,Glyph::Settings};
    for (int i = 0; i < nav.size(); ++i) {
        auto item = button(nav.at(i)); item->setCheckable(true); item->setProperty("nav", true); item->setProperty("mainNavigation", true);
        item->setIcon(lineIcon(icons.at(i)));item->setIconSize(QSize(20,20));item->setAccessibleName(nav.at(i));item->setFixedHeight(43);
        m_nav.append(item); side->addWidget(item);
        connect(item, &QPushButton::clicked, this, [this, i] { if(i==2){m_resourceReturnPath.clear();m_marketReturn->hide();setMarketType("mod");closeModDetails();m_modViews->setCurrentIndex(0);searchMods();}changePage(i); });
    }
    for(const auto &entry:QList<QPair<QString,QString>>{{"shader",QCoreApplication::translate("M2CL", "光影")},{"resourcepack",QCoreApplication::translate("M2CL", "材质包")}}){
        auto item=button(entry.second);item->setCheckable(true);item->setProperty("nav",true);item->setProperty("mainNavigation",true);item->setIcon(lineIcon(entry.first=="shader"?Glyph::Image:Glyph::Leaf));item->setIconSize(QSize(20,20));item->setFixedHeight(43);item->setObjectName(entry.first+"MarketNav");side->insertWidget(side->indexOf(m_nav.last()),item);
        if(entry.first=="shader")m_shaderNav=item;else m_packNav=item;
        connect(item,&QPushButton::clicked,this,[this,type=entry.first]{m_resourceReturnPath.clear();m_marketReturn->hide();setMarketType(type);m_modQuery->clear();m_modVersion->setCurrentIndex(0);closeModDetails();m_modViews->setCurrentIndex(0);changePage(2);searchMods();});
    }
    side->addStretch();
    layout->addWidget(sidebar);
    auto main = new QVBoxLayout; main->setContentsMargins(0, 0, 0, 0); main->setSpacing(0);
    m_bar=new WindowBar(this);main->addWidget(m_bar);
    m_pages = new QStackedWidget;
    m_pages->addWidget(homePage()); m_pages->addWidget(libraryPage());
    m_pages->addWidget(modsPage()); m_pages->addWidget(settingsPage());
    main->addWidget(m_pages, 1);
    m_status = label(QCoreApplication::translate("M2CL", "准备就绪"), nullptr, true);
    m_status->setContentsMargins(24, 2, 24, 6);m_status->setWordWrap(false);m_status->setFixedHeight(25);m_status->hide();m_status->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    m_status->setObjectName("statusLabel");m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    main->addWidget(m_status); layout->addLayout(main, 1);
    m_downloadPanel=new DownloadPanel(this);
    connect(m_bar->downloads,&QAbstractButton::clicked,this,&MainWindow::showDownloads);
    connect(m_bar->appearance,&QAbstractButton::clicked,this,[this]{changePage(3);m_settingsSections->setCurrentIndex(0);});
    connect(m_downloadPanel,&DownloadPanel::cancelRequested,this,[this]{ if(m_installCancelled)m_installCancelled->store(true);if(m_javaManager)m_javaManager->cancelInstall();m_resources.cancelDownload();if(m_modCancel)m_modCancel->store(true); });
    connect(m_downloadPanel,&DownloadPanel::openRequested,this,[this]{m_downloadPanel->hide();if(m_lastDownloadJava)showJavaSettings(true);else changePage(2);});
    for(auto grid:{m_homeGrid,m_libraryGrid}){
        connect(grid,&InstanceGrid::addModsRequested,this,&MainWindow::addModsMenu);
        connect(grid,&InstanceGrid::selected,this,&MainWindow::selectInstance);
        connect(grid,&InstanceGrid::favoriteRequested,this,&MainWindow::toggleFavorite);
        connect(grid,&InstanceGrid::menuRequested,this,&MainWindow::instanceMenu);
    }
    connect(&m_catalogWatcher, &QFutureWatcher<CatalogResult>::finished, this, [this] {
        if (m_catalogPending) { m_catalogPending = false; refreshCatalog(); return; }
        const auto result = m_catalogWatcher.result(); m_instances = result.instances;
        QStringList instancePaths; for (const auto &instance : m_instances) instancePaths.append(instance.path);
        const auto migrationErrors = migrateInstanceJavaChoices(m_settings, instancePaths);
        if (!selectedInstance() && !m_instances.isEmpty()) m_settings.selectedInstance = m_instances.first().path;
        refreshInstanceViews(); save();
        if (!migrationErrors.isEmpty()) status(QCoreApplication::translate("M2CL", "实例 Java 配置迁移失败：%1").arg(migrationErrors.join(QCoreApplication::translate("M2CL", "；"))));
        else if (result.warnings.isEmpty()) status(QCoreApplication::translate("M2CL", "已找到 %1 个本地实例").arg(m_instances.size()));
        else status(QCoreApplication::translate("M2CL", "已找到 %1 个实例，%2 个版本配置无法读取").arg(m_instances.size()).arg(result.warnings.size()));
        m_catalogReady = true;
    });
    connect(&m_installWatcher, &QFutureWatcher<InstallResult>::finished, this, [this] {
        const auto result = m_installWatcher.result();
        m_downloadInProgress = false; m_javaManager->setGameTaskBusy(false);
        m_activeDownloadMessage = result.error.isEmpty() ? QCoreApplication::translate("M2CL", "安装完成，已通过文件校验") : result.error;
        m_activeDownloadProgress = result.error.isEmpty() ? 1000 : -1;
        updateDownloadPanel(); status(m_activeDownloadMessage); m_heroLaunch->setEnabled(true);
        if (!result.error.isEmpty()) { refreshCatalog(); QMessageBox::warning(this, QCoreApplication::translate("M2CL", "安装 / 启动未完成"), result.error + QCoreApplication::translate("M2CL", "\n重新点击安装或启动即可重试，已校验文件会复用。")); return; }
        m_settings.selectedInstance = result.instancePath;
        auto catalog = javaCatalogFromJson(m_settings.javaCatalog); bool known = false;
        for (auto &j : catalog) if (j.executable == result.javaRuntime.executable) { j = result.javaRuntime; known = true; break; }
        if (!known) catalog.append(result.javaRuntime); m_settings.javaCatalog = javaCatalogJson(catalog);
        QString configError;
        const auto choice = loadInstanceJavaChoice(result.instancePath, &configError);
        if (choice.value("mode") == "fixed" && choice.value("runtimeVersion").toString().isEmpty() && QFileInfo(choice.value("path").toString()).canonicalFilePath() == result.javaRuntime.executable) {
            auto confirmed=javaPin(result.javaRuntime);if(choice.value("allowBelowRecommended").toBool())confirmed.insert("allowBelowRecommended",true);
            if (configError.isEmpty()) saveInstanceJavaChoice(result.instancePath, confirmed, &configError);
        }
        else if (configError.isEmpty() && !QFileInfo::exists(instanceConfigPath(result.instancePath))) saveInstanceJavaChoice(result.instancePath, choice, &configError);
        m_javaManager->refreshCatalog(); m_javas = m_javaManager->runtimes();
        save(); refreshCatalog();
        if (!configError.isEmpty()) { QMessageBox::warning(this, QCoreApplication::translate("M2CL", "实例配置保存失败"), configError); return; }
        if (m_launchAfterInstall) {
            if(!result.warnings.isEmpty()){
                QMessageBox warning(QMessageBox::Warning,QCoreApplication::translate("M2CL", "启动前兼容性提醒"),result.warnings.join("\n\n"),QMessageBox::NoButton,this);
                warning.setTextFormat(Qt::PlainText);auto proceed=warning.addButton(QCoreApplication::translate("M2CL", "仍然启动"),QMessageBox::AcceptRole);auto cancel=warning.addButton(QCoreApplication::translate("M2CL", "取消"),QMessageBox::RejectRole);warning.setDefaultButton(cancel);warning.exec();if(warning.clickedButton()!=proceed)return;
            }
            runGame(result.command);
        }
    });
    connect(&m_modPlanWatcher,&QFutureWatcher<ModPlan>::finished,this,[this]{
        m_pendingModPlan=m_modPlanWatcher.result();m_pendingModPlan.displayTargetName=instanceName(m_pendingModPlan.target);const auto plan=m_pendingModPlan;
        if(!plan.error.isEmpty()){m_modInstallActive=false;m_downloadInProgress=false;m_javaManager->setGameTaskBusy(false);m_activeDownloadMessage=plan.error;updateDownloadPanel();if(plan.resolved.isEmpty())QMessageBox::information(this,QCoreApplication::translate("M2CL", "无法安装到实例"),plan.error);else{ModPlanView review(plan,this,m_settings.coverOverrides.value(plan.target.path).toString());review.exec();}return;}

        if(plan.resolved.size()>1||!plan.optionalPlans.isEmpty()||!plan.optionalErrors.isEmpty()||!plan.disabledFiles.isEmpty()||!plan.reusedProjects.isEmpty()||!plan.replacements.isEmpty()||!plan.edges.isEmpty()||!plan.notes.isEmpty()){
            ModPlanView review(plan,this,m_settings.coverOverrides.value(plan.target.path).toString());if(review.exec()!=QDialog::Accepted){m_modCancel->store(true);m_modInstallActive=false;m_downloadInProgress=false;m_javaManager->setGameTaskBusy(false);m_activeDownloadMessage=QCoreApplication::translate("M2CL", "已取消安装计划");updateDownloadPanel();return;}m_pendingModPlan=review.selectedPlan();
        }
        commitModPlan();
    });
    connect(&m_modApplyWatcher,&QFutureWatcher<ModInstallResult>::finished,this,[this]{const auto result=m_modApplyWatcher.result();m_modInstallActive=false;m_downloadInProgress=false;m_javaManager->setGameTaskBusy(false);m_activeDownloadMessage=result.error.isEmpty()?QCoreApplication::translate("M2CL", "已完成 → %1 · 下载 %2 项，启用 %3 项，复用 %4 项").arg(m_pendingModPlan.target.id).arg(result.installed.size()-result.enabled.size()).arg(result.enabled.size()).arg(m_pendingModPlan.reusedProjects.size()):result.error;m_activeDownloadProgress=result.error.isEmpty()?1000:-1;updateDownloadPanel();if(result.error.isEmpty())showInstanceMods(m_pendingModPlan.target.path);else QMessageBox::information(this,QCoreApplication::translate("M2CL", "安装未完成"),result.error);});
    connect(&m_resources,&ResourceClient::tagsLoading,this,[this]{qobject_cast<SelectBox *>(m_modVersion)->setOptionsState(true);m_modRefresh->setEnabled(false);m_modTagStatus->clear();m_modCategoryState->show();});
    connect(&m_resources, &ResourceClient::tagsReady, this, [this](const ResourceTags &tags) {
        m_modTags = tags;
        const QSignalBlocker categories(m_modCategories), versions(m_modVersion), loaders(m_modLoader);
        const auto category = m_modCategories->currentItem() ? m_modCategories->currentItem()->data(Qt::UserRole).toString() : QString();
        m_modCategories->clear();
        auto all = new QListWidgetItem(lineIcon(Glyph::Grid),QCoreApplication::translate("M2CL", "全部分类"),m_modCategories); all->setData(Qt::UserRole,"");m_modCategories->setCurrentItem(all);
        for(const auto &tag:tags.categories){auto item=new QListWidgetItem(categoryIcon(tag.id),tag.label,m_modCategories);item->setData(Qt::UserRole,tag.id);item->setToolTip(tag.id);if(tag.id==category)m_modCategories->setCurrentItem(item);}
        const auto version=m_modVersion->currentIndex()==0 && m_modVersion->currentText()==m_modVersion->itemText(0)?QString():m_modVersion->currentText().trimmed();
        const auto loader=m_modLoader->currentData().toString();
        m_modVersion->clear();m_modVersion->addItem(QCoreApplication::translate("M2CL", "全部版本"),"");
        auto addVersion=[this,&tags](const QString &value){
            m_modVersion->addItem(value,value);
            const auto type=tags.gameVersionTypes.value(value);
            m_modVersion->setItemData(m_modVersion->count()-1,gameVersionChannel(value,type),SelectBox::PopupGroupRole);
        };
        for(const auto &value:tags.gameVersions)addVersion(value);
        if(!version.isEmpty()){auto index=m_modVersion->findData(version);if(index<0){addVersion(version);index=m_modVersion->count()-1;}m_modVersion->setCurrentIndex(index);}
        const QStringList common{"forge","neoforge","fabric","quilt"};
        auto ordered=tags.loaders;
        std::stable_sort(ordered.begin(),ordered.end(),[&common](const ResourceTag&a,const ResourceTag&b){auto ai=common.indexOf(a.id),bi=common.indexOf(b.id);return(ai<0?common.size():ai)<(bi<0?common.size():bi);});
        m_modLoader->clear();m_modLoader->addItem(QCoreApplication::translate("M2CL", "全部加载器"),"");
        for(const auto &tag:ordered){m_modLoader->addItem(tag.label,tag.id);m_modLoader->setItemData(m_modLoader->count()-1,common.contains(tag.id)?"common":"other",SelectBox::PopupGroupRole);}
        auto index=m_modLoader->findData(loader);
        if(index<0 && !loader.isEmpty()){m_modLoader->addItem(resourceTagLabel(loader),loader);index=m_modLoader->count()-1;m_modLoader->setItemData(index,common.contains(loader)?"common":"other",SelectBox::PopupGroupRole);}
        m_modLoader->setCurrentIndex(qMax(0,index));
        m_modCategoryState->hide();m_modTagStatus->clear();m_modTagStatus->hide();m_modRefresh->setEnabled(true);m_modRefresh->setText(QCoreApplication::translate("M2CL", "刷新筛选"));qobject_cast<SelectBox *>(m_modVersion)->setOptionsState(false);
        if(auto view=m_modDetails->findChild<ModVersionView *>())view->setSupportedVersionMetadata(tags);
    });
    connect(&m_resources, &ResourceClient::tagsFailed, this, [this](const QString &error){ m_modCategoryState->hide();m_modTagStatus->failure(QCoreApplication::translate("M2CL", "部分筛选项未能加载，仍可浏览已有结果"),error);m_modRefresh->setEnabled(true);m_modRefresh->setText(QCoreApplication::translate("M2CL", "重试筛选"));qobject_cast<SelectBox *>(m_modVersion)->setOptionsState(false,m_modTags.gameVersions.isEmpty()?error:QString()); });
    connect(&m_resources, &ResourceClient::searchPageReady, this, [this](int total, int offset){ m_modTotal=total; m_modOffset=offset; });
    connect(&m_resources, &ResourceClient::iconReady, this, [this](const QUrl &url, const QByteArray &bytes){
        QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);
        const auto size=reader.size();if(!size.isValid() || size.width()>4096 || size.height()>4096) return;
        reader.setScaledSize(size.scaled(128,128,Qt::KeepAspectRatio));const auto image=reader.read();if(image.isNull())return;
        QPixmap pixmap(128,128);pixmap.setDevicePixelRatio(2);pixmap.fill(Qt::transparent);QPainter painter(&pixmap);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath clip;clip.addRoundedRect(QRectF(0,0,64,64),8,8);painter.setClipPath(clip);
        const auto target=image.size().scaled(64,64,Qt::KeepAspectRatio);painter.drawImage(QRectF((64-target.width())/2.,(64-target.height())/2.,target.width(),target.height()),image);painter.end();
        for(auto icon:findChildren<QLabel *>("modProjectIcon")) if(icon->property("iconUrl").toUrl()==url){icon->setPixmap(pixmap);icon->setProperty("iconLoaded",true);}
    });
    connect(&m_resources, &ResourceClient::projectsReady, this, [this](const QList<ResourceProject> &projects) {
        if(auto notice=m_modSearchState->findChild<NetworkNotice *>())notice->clear();
        m_modSearchPending=false;m_modSearchSpinner->hide();m_modSearchState->setVisible(projects.isEmpty());m_modSearchMessage->setText(QCoreApplication::translate("M2CL", "没有符合条件的资源，试试其他版本或分类"));m_modSearchMessage->show();m_modSearchRetry->hide();m_projects = projects; m_modResults->clear();
        for (int i=0;i<projects.size();++i) {
            const auto &project=projects.at(i); auto item = new QListWidgetItem(m_modResults); item->setSizeHint(QSize(0, 140));
            auto frame = card();auto row=new QHBoxLayout(frame);row->setContentsMargins(14,12,14,12);row->setSpacing(12);
            auto icon=new QLabel;icon->setObjectName("modProjectIcon");icon->setFixedSize(64,64);icon->setAlignment(Qt::AlignCenter);icon->setScaledContents(true);icon->setProperty("resourceIcon",true);icon->setProperty("iconUrl",project.iconUrl);icon->setAccessibleName(project.name+QCoreApplication::translate("M2CL", " 图标"));icon->setPixmap(lineIcon(Glyph::Mods).pixmap(36,36));row->addWidget(icon,0,Qt::AlignTop);
            auto content = new QVBoxLayout;content->setContentsMargins(0,0,0,0);content->setSpacing(6);row->addLayout(content,1);
            auto header=new QHBoxLayout; auto name=label(project.name);name->setTextFormat(Qt::PlainText); name->setProperty("projectTitle",true);name->setWordWrap(false);name->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); header->addWidget(name,1);
            auto sideHost=new QWidget;sideHost->setObjectName("modCardActions");auto side=new QVBoxLayout(sideHost);side->setContentsMargins(0,0,0,0);side->setSpacing(8);auto downloads=label(resourceDownloadCount(project.downloads)+QCoreApplication::translate("M2CL", " 下载"),nullptr,true);downloads->setWordWrap(false);downloads->setAlignment(Qt::AlignRight);downloads->setToolTip(QCoreApplication::translate("M2CL", "%1 次下载").arg(project.downloads));side->addWidget(downloads);side->addStretch();
            auto versions=button(QCoreApplication::translate("M2CL", "查看详情"),true);versions->setProperty("compact",true);side->addWidget(versions);row->addWidget(sideHost);content->addLayout(header);connect(versions,&QPushButton::clicked,this,[this,i]{m_modResults->setCurrentRow(i);chooseModVersion();});
            auto description=new SummaryLabel(project.description);description->setObjectName("modProjectDescription");description->setProperty("muted",true);description->setProperty("secondaryText",true);content->addWidget(description);
            QStringList tags;for(const auto &id:project.categories)tags.append(resourceTagLabel(id));auto tagList=new AdaptiveTags(tags);tagList->setObjectName("modProjectTags");content->addWidget(tagList);
            frame->setToolTip(tags.join(" · ")+"\n"+project.description);
            auto supportRow=new QHBoxLayout;supportRow->setSpacing(8);
            auto supported=new FittingVersionLabel(resourceVersionFamilies(project.gameVersions));supported->setProperty("secondaryText",true);supported->setProperty("muted",true);supported->setObjectName("modSupportSummary");supported->setWordWrap(false);supported->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
            supported->setToolTip(QCoreApplication::translate("M2CL", "<div style='width:300px'>项目声明支持 %1 个 Minecraft 版本。<br>系列：%2<br>完整支持版本可在“查看详情”的“支持版本”页查看。</div>").arg(project.gameVersions.size()).arg(resourceVersionOverview(project.gameVersions,4).toHtmlEscaped()));supportRow->addWidget(supported,1);
            content->addLayout(supportRow);
            m_modResults->setItemWidget(item,frame);
            m_resources.icon(project.iconUrl);
        }
        if(m_modDetails->isVisible())updateModBrowseLayout(true);
        m_modPrevious->setEnabled(m_modOffset>0);m_modNext->setEnabled(!projects.isEmpty() && m_modOffset+projects.size()<m_modTotal);
        m_modPageLabel->setText(QCoreApplication::translate("M2CL", "共 %1 个项目 · 第 %2 页").arg(m_modTotal).arg(m_modOffset/20+1));
        status(QCoreApplication::translate("M2CL", "Modrinth · 找到 %1 个项目").arg(m_modTotal));
    });
    connect(&m_resources,&ResourceClient::projectReady,this,[this](const ResourceProject &project){
        if(project.id!=m_versionProject.id)return;
        m_modProjectPending=false;m_versionProjectError.clear();auto author=m_versionProject.author;m_versionProject=project;if(m_versionProject.author.isEmpty())m_versionProject.author=author;
        if(auto view=m_modDetails->findChild<ModVersionView *>())view->setProjectInfo(m_versionProject);
    });
    connect(&m_resources,&ResourceClient::projectFailed,this,[this](const QString &message){m_modProjectPending=false;m_versionProjectError=message;if(auto view=m_modDetails->findChild<ModVersionView *>())view->setProjectError(message);});
    connect(&m_resources, &ResourceClient::versionsReady, this, &MainWindow::showModVersions);
    connect(&m_resources,&ResourceClient::versionsFailed,this,[this](const QString &error){
        if(!m_modVersionPending)return;m_modVersionPending=false;
        if(auto notice=m_modDetails->findChild<NetworkNotice *>("modVersionNetworkState"))notice->failure(QCoreApplication::translate("M2CL", "版本列表加载失败"),error);
        if(auto spinner=m_modDetails->findChild<BusySpinner *>("modVersionSpinner"))spinner->hide();
    });
    connect(&m_resources,&ResourceClient::searchFailed,this,[this](const QString &error){
        if(!m_modSearchPending)return;m_modSearchPending=false;m_modSearchSpinner->hide();m_modSearchState->show();m_modSearchMessage->hide();m_modSearchRetry->hide();
        if(auto notice=m_modSearchState->findChild<NetworkNotice *>("modSearchNetworkState"))notice->failure(QCoreApplication::translate("M2CL", "搜索暂时不可用，请重试"),error);
    });
    connect(&m_resources,&ResourceClient::downloadFailed,this,[this](const QString &error){
        m_downloadStatus->setText(error);
        if (!m_installWatcher.isRunning() && !m_resources.downloading() && !m_modInstallActive) m_javaManager->setGameTaskBusy(false);
        m_cancelDownload->setEnabled(m_resources.downloading());
        if(m_downloadInProgress && !m_installWatcher.isRunning() && !m_javaDownloadActive) {m_downloadInProgress=false;m_activeDownloadMessage=error;updateDownloadPanel();}
    });
    connect(&m_resources, &ResourceClient::downloadProgress, this, [this](qint64 received, qint64 total) {
        m_downloadProgress->setRange(0, total > 0 ? 1000 : 0);
        if (total > 0) m_downloadProgress->setValue(static_cast<int>(received * 1000 / total));
        m_downloadStatus->setText(QCoreApplication::translate("M2CL", "正在下载 · %1 / %2 MiB").arg(received / 1048576., 0, 'f', 1).arg(total / 1048576., 0, 'f', 1));
        m_activeDownloadProgress=total>0?static_cast<int>(received*1000/total):-1;m_activeDownloadMessage=m_downloadStatus->text();updateDownloadPanel();
    });
    connect(&m_resources, &ResourceClient::downloadFinished, this, [this](const QString &path) {
        m_downloadProgress->setRange(0, 1000); m_downloadProgress->setValue(1000);
        m_cancelDownload->setEnabled(false);
        m_downloadStatus->setText(QCoreApplication::translate("M2CL", "已校验并保存：%1").arg(path)); status(QCoreApplication::translate("M2CL", "模组下载完成")); m_javaManager->setGameTaskBusy(false);
        m_downloadInProgress=false;m_activeDownloadProgress=1000;m_activeDownloadMessage=QCoreApplication::translate("M2CL", "下载完成，已通过大小与哈希校验");updateDownloadPanel();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { if (m_settings.appearance == "system") updateAppearance(); });
#endif
    connect(m_accounts,&AccountManager::changed,this,&MainWindow::refreshInstanceViews);
    connect(m_accountCard,&HomeAccountCard::managementRequested,this,&MainWindow::showAccounts);
    connect(m_accountCard,&HomeAccountCard::microsoftLoginRequested,this,[this](const QString &id){m_accountsPanel->login(id);});
    connect(m_accountCard,&HomeAccountCard::offlineCreationRequested,m_accountsPanel,&AccountsPanel::addOffline);
    connect(m_accountCard,&HomeAccountCard::skinEditRequested,m_accountsPanel,&AccountsPanel::editSkin);
    connect(m_accountsPanel,&AccountsPanel::configurationRequested,this,&MainWindow::showAccounts);
    updateAppearance(); changePage(0); refreshCatalog();
    qApp->installEventFilter(this);
    for(auto child:findChildren<QWidget *>())child->setMouseTracking(true);
    if (!error.isEmpty()) status(error);
}
MainWindow::~MainWindow() {
    qApp->removeEventFilter(this);if(m_resizeCursor)qApp->restoreOverrideCursor();
    m_resources.cancelDownload();if(m_modCancel)m_modCancel->store(true);m_modPlanWatcher.waitForFinished();m_modApplyWatcher.waitForFinished();
    if(m_installCancelled)m_installCancelled->store(true);
    m_installWatcher.waitForFinished();
    delete m_javaManager; m_javaManager = nullptr;
    m_catalogWatcher.waitForFinished();
}
QWidget *MainWindow::homePage() {
    QVBoxLayout *body; auto widget=page(&body);body->setContentsMargins(24,8,24,24);body->setSpacing(ui::SectionSpace);
    body->addWidget(label(QCoreApplication::translate("M2CL", "开始"),"pageTitle"));
    m_accountCard=new HomeAccountCard(*m_accounts,*m_avatars);body->addWidget(m_accountCard);
    m_hero=new CoverView;m_hero->setObjectName("heroCover");m_hero->setProperty("fixedPreview",true);m_hero->setFixedHeight(225);
    auto heroLayout=column(m_hero,24);heroLayout->setSpacing(9);heroLayout->addStretch();
    m_heroTitle=label(QCoreApplication::translate("M2CL", "把世界放进书桌"),"heroTitle");heroLayout->addWidget(m_heroTitle);
    auto badges=new QHBoxLayout;badges->setSpacing(6);
    m_heroVersion=badge("Minecraft",Glyph::Mods);m_heroLoader=badge(QCoreApplication::translate("M2CL", "你的方块世界"),Glyph::Settings);m_heroJava=badge("Java",Glyph::Java);
    for(auto item:{m_heroVersion,m_heroLoader,m_heroJava})badges->addWidget(item);
    badges->addStretch();heroLayout->addLayout(badges);
    auto actions=new QHBoxLayout;actions->setSpacing(7);
    m_heroLaunch=button(QCoreApplication::translate("M2CL", "添加游戏目录"),true);m_heroLaunch->setObjectName("heroAction");m_heroLaunch->setIcon(lineIcon(Glyph::Play,Qt::white));m_heroLaunch->setIconSize(QSize(19,19));m_heroLaunch->setMinimumWidth(180);actions->addWidget(m_heroLaunch);
    connect(m_heroLaunch,&QPushButton::clicked,this,[this]{selectedInstance()?launchInstance():createInstance();});
    actions->addStretch();
    m_heroOpen=new GlassButton(QCoreApplication::translate("M2CL", "打开目录"));m_heroOpen->setProperty("onCover",true);m_heroOpen->setProperty("heroAuxiliary",true);m_heroOpen->setIcon(lineIcon(Glyph::Folder,Qt::white));actions->addWidget(m_heroOpen);
    m_heroEdit=new GlassButton(QCoreApplication::translate("M2CL", "编辑实例"));m_heroEdit->setObjectName("editInstance");m_heroEdit->setProperty("onCover",true);m_heroEdit->setProperty("heroAuxiliary",true);m_heroEdit->setIcon(lineIcon(Glyph::Edit,Qt::white));actions->addWidget(m_heroEdit);
    auto addMods=new GlassButton(QCoreApplication::translate("M2CL", "添加模组"));addMods->setObjectName("heroAddMods");addMods->setProperty("onCover",true);addMods->setProperty("heroAuxiliary",true);addMods->setIcon(lineIcon(Glyph::Plus,Qt::white));actions->addWidget(addMods);connect(addMods,&QPushButton::clicked,this,[this,addMods]{if(auto instance=selectedInstance())addModsMenu(instance->path,addMods->mapToGlobal(QPoint(0,addMods->height())));});
    auto more=new GlassButton(QCoreApplication::translate("M2CL", "更多操作"));more->setProperty("onCover",true);more->setIcon(lineIcon(Glyph::More,Qt::white));actions->addWidget(more);
    connect(m_heroOpen,&QPushButton::clicked,this,[this]{if(auto instance=selectedInstance())openDirectory(instance->gameDirectory);});
    connect(m_heroEdit,&QPushButton::clicked,this,[this]{editInstance();});
    connect(more,&QPushButton::clicked,this,[this,more]{if(auto instance=selectedInstance())instanceMenu(instance->path,more->mapToGlobal(QPoint(0,more->height())));});
    heroLayout->addLayout(actions);
    m_heroPath=label(QCoreApplication::translate("M2CL", "安装原版或添加已有目录，开启你的下一段冒险。"));m_heroPath->setObjectName("heroPath");m_heroPath->setWordWrap(false);m_heroPath->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    auto playerRow=new QHBoxLayout;playerRow->setContentsMargins(0,0,0,0);
    m_heroPlayer=label({});m_heroPlayer->setObjectName("heroPlayer");m_heroPlayer->setWordWrap(false);playerRow->addWidget(m_heroPlayer);playerRow->addStretch();m_heroPath->setAlignment(Qt::AlignRight);playerRow->addWidget(m_heroPath,1);
    heroLayout->addLayout(playerRow);
    body->addWidget(m_hero);
    auto title=new QHBoxLayout;title->setSpacing(9);title->addWidget(label(QCoreApplication::translate("M2CL", "我的实例"),"sectionTitle"));
    m_instanceCount=label(QCoreApplication::translate("M2CL", "%n 个实例",nullptr,0),nullptr,true);title->addWidget(m_instanceCount);title->addStretch();
    auto create=button(QCoreApplication::translate("M2CL", "安装原版"),true);create->setObjectName("createInstance");title->addWidget(create);connect(create,&QPushButton::clicked,this,&MainWindow::createInstance);
    auto add=button(QCoreApplication::translate("M2CL", "添加游戏目录"));add->setIcon(lineIcon(Glyph::Plus));title->addWidget(add);connect(add,&QPushButton::clicked,this,&MainWindow::chooseRoot);body->addLayout(title);
    m_homeGrid=new InstanceGrid;m_homeGrid->setObjectName("homeInstances");body->addWidget(m_homeGrid);
    body->addStretch();return widget;
}
QWidget *MainWindow::libraryPage() {
    QVBoxLayout *body;auto widget=page(&body);body->setContentsMargins(24,8,24,24);
    auto top=new QHBoxLayout;top->setSpacing(ui::Space);top->addWidget(label(QCoreApplication::translate("M2CL", "游戏库"),"pageTitle"));m_libraryCount=label(QCoreApplication::translate("M2CL", "%n 个实例",nullptr,0),nullptr,true);top->addWidget(m_libraryCount);top->addStretch();
    auto create=button(QCoreApplication::translate("M2CL", "安装原版"),true);create->setObjectName("createInstance");top->addWidget(create);connect(create,&QPushButton::clicked,this,&MainWindow::createInstance);
    auto add=button(QCoreApplication::translate("M2CL", "添加游戏目录"));add->setIcon(lineIcon(Glyph::Plus));top->addWidget(add);
    auto refresh=new IconButton(Glyph::Refresh,QCoreApplication::translate("M2CL", "刷新实例"));top->addWidget(refresh);body->addLayout(top);
    connect(add,&QPushButton::clicked,this,&MainWindow::chooseRoot);connect(refresh,&QAbstractButton::clicked,this,&MainWindow::refreshCatalog);
    m_libraryGrid=new InstanceGrid;m_libraryGrid->setObjectName("libraryInstances");body->addWidget(m_libraryGrid);
    auto heading=new QHBoxLayout;heading->addWidget(label(QCoreApplication::translate("M2CL", "游戏目录"),"sectionTitle"));heading->addStretch();
    auto manage=button(QCoreApplication::translate("M2CL", "管理目录"));manage->setIcon(lineIcon(Glyph::Folder));heading->addWidget(manage);body->addLayout(heading);
    auto roots=card();auto contents=column(roots,18);roots->setVisible(false);
    m_roots=new QListWidget;m_roots->setMaximumHeight(100);contents->addWidget(m_roots);
    auto remove=button(QCoreApplication::translate("M2CL", "移除所选目录记录"));contents->addWidget(remove,0,Qt::AlignLeft);
    contents->addWidget(label(QCoreApplication::translate("M2CL", "移除记录只取消此处的目录关联，游戏文件会继续保留。"),nullptr,true));body->addWidget(roots);
    connect(manage,&QPushButton::clicked,this,[roots]{roots->setVisible(!roots->isVisible());});
    connect(remove,&QPushButton::clicked,this,[this]{if(auto item=m_roots->currentItem()){m_settings.gameRoots.removeAll(item->text());save();refreshCatalog();}});
    m_libraryDetail=label(QCoreApplication::translate("M2CL", "添加已有 .minecraft 目录，识别本地游戏实例。"),nullptr,true);m_libraryDetail->setTextInteractionFlags(Qt::TextSelectableByMouse);body->addWidget(m_libraryDetail);
    body->addStretch();return widget;
}
QWidget *MainWindow::modsPage() {
    auto marketPage = new QWidget;
    auto root = column(marketPage, ui::PageMargin);
    root->setContentsMargins(ui::PageMargin, 8, ui::PageMargin, ui::PageMargin);
    auto marketTitle=label(QCoreApplication::translate("M2CL", "模组市场"), "pageTitle");m_marketTitle=marketTitle;auto marketHeading=new QHBoxLayout;marketHeading->addWidget(marketTitle,1);m_marketReturn=new QPushButton(QCoreApplication::translate("M2CL", "返回实例"));m_marketReturn->hide();marketHeading->addWidget(m_marketReturn);connect(m_marketReturn,&QPushButton::clicked,this,[this]{const auto type=m_marketType;showInstanceMods(m_resourceReturnPath);if(auto tabs=m_modViews->currentWidget()->findChild<QTabWidget *>("instanceResourceTabs"))tabs->setCurrentIndex(type=="shader"?1:2);});root->addLayout(marketHeading);
    m_modViews = new QStackedWidget;
    m_modViews->setObjectName("modViews");
    m_modViews->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    connect(m_modViews,&QStackedWidget::currentChanged,marketTitle,[marketTitle](int index){marketTitle->setVisible(index<2);});
    root->addWidget(m_modViews, 1);
    auto browse = new QWidget;
    browse->setObjectName("modMarketBrowse");
    auto body = column(browse, 0);
    m_modViews->addWidget(browse);
    auto types=new QHBoxLayout;m_marketTypeChoice=new SelectBox;m_marketTypeChoice->setObjectName("marketResourceType");
    m_marketTypeChoice->addItem(QCoreApplication::translate("M2CL", "模组"),"mod");m_marketTypeChoice->addItem(QCoreApplication::translate("M2CL", "光影"),"shader");m_marketTypeChoice->addItem(QCoreApplication::translate("M2CL", "材质包"),"resourcepack");
    types->addWidget(m_marketTypeChoice);types->addStretch();body->addLayout(types);m_marketTypeChoice->hide();
    connect(m_marketTypeChoice,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{setMarketType(m_marketTypeChoice->currentData().toString());searchMods();});
    auto search = new QHBoxLayout;
    search->setSpacing(8);
    m_modQuery = new QLineEdit;
    m_modQuery->setObjectName("modQuery");
    m_modQuery->setPlaceholderText(QCoreApplication::translate("M2CL", "搜索模组，例如 Sodium、Iris、背包…"));
    auto go = button(QCoreApplication::translate("M2CL", "搜索"), true);
    go->setObjectName("searchMods");
    search->addWidget(m_modQuery, 1); search->addWidget(go);
    body->addLayout(search);
    m_modFilters = new QWidget;
    auto choices = new QGridLayout(m_modFilters);
    choices->setContentsMargins(0, 0, 0, 0);
    choices->setHorizontalSpacing(12); choices->setVerticalSpacing(8);
    m_modVersion = new SelectBox;
    m_modVersion->setObjectName("modGameVersion");
    m_modVersion->setAccessibleName(QCoreApplication::translate("M2CL", "游戏版本"));
    m_modVersion->setProperty("popupMode", "versions");
    m_modVersion->addItem(QCoreApplication::translate("M2CL", "全部版本"), "");
    m_modLoader = new SelectBox;
    m_modLoader->setObjectName("modLoader");
    m_modLoader->setAccessibleName(QCoreApplication::translate("M2CL", "加载器"));
    m_modLoader->setProperty("popupMode", "loaders");
    m_modLoader->addItem(QCoreApplication::translate("M2CL", "全部加载器"), "");
    for (const auto &id : QStringList{"forge", "neoforge", "fabric", "quilt"}) {
        m_modLoader->addItem(resourceTagLabel(id), id);
        m_modLoader->setItemData(m_modLoader->count() - 1, "common", SelectBox::PopupGroupRole);
    }
    auto source = new SelectBox;
    source->setObjectName("modSource"); source->setAccessibleName(QCoreApplication::translate("M2CL", "模组来源")); source->addItem("Modrinth", "modrinth");
    m_modSort = new SelectBox;
    m_modSort->setObjectName("modSort"); m_modSort->setAccessibleName(QCoreApplication::translate("M2CL", "排序"));
    for (const auto &entry : QList<QPair<QString,QString>>{{QCoreApplication::translate("M2CL", "相关度"),"relevance"},{QCoreApplication::translate("M2CL", "下载量"),"downloads"},{QCoreApplication::translate("M2CL", "关注数"),"follows"},{QCoreApplication::translate("M2CL", "最新发布"),"newest"},{QCoreApplication::translate("M2CL", "最近更新"),"updated"}}) m_modSort->addItem(entry.first, entry.second);
    for (const auto &entry : QList<QPair<QString,QComboBox *>>{{QCoreApplication::translate("M2CL", "游戏版本"),m_modVersion},{QCoreApplication::translate("M2CL", "加载器"),m_modLoader},{QCoreApplication::translate("M2CL", "来源"),source},{QCoreApplication::translate("M2CL", "排序"),m_modSort}}) {
        auto field = new QWidget;
        auto row = new QHBoxLayout(field); row->setContentsMargins(0,0,0,0); row->setSpacing(8);
        auto caption = label(entry.first); caption->setWordWrap(false); caption->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
        entry.second->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        entry.second->setMinimumContentsLength(6);
        entry.second->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        row->addWidget(caption); row->addWidget(entry.second,1);
        m_modFilterFields.append(field);
        choices->addWidget(field,0,m_modFilterFields.size()-1);
    }
    body->addWidget(m_modFilters);
    m_modTagStatus = new NetworkNotice;m_modTagStatus->setObjectName("modFilterNetworkState");
    auto results = new QHBoxLayout; results->setSpacing(16);
    m_modCategories = new QListWidget;
    m_modCategories->setObjectName("modCategories"); m_modCategories->setProperty("modCategoryList",true);
    m_modCategories->setIconSize(QSize(16,16)); m_modCategories->setFixedWidth(160);
    m_modCategories->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_modCategories->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto all = new QListWidgetItem(lineIcon(Glyph::Grid),QCoreApplication::translate("M2CL", "全部分类"),m_modCategories);
    all->setData(Qt::UserRole, ""); m_modCategories->setCurrentRow(0);
    auto categoryOverlay=new QVBoxLayout(m_modCategories->viewport());categoryOverlay->setContentsMargins(0,0,0,0);
    m_modCategoryState=new QWidget;m_modCategoryState->setObjectName("modCategoryLoadingState");m_modCategoryState->setAutoFillBackground(true);m_modCategoryState->setBackgroundRole(QPalette::Base);
    auto categoryLoading=new QVBoxLayout(m_modCategoryState);categoryLoading->setContentsMargins(0,0,0,0);categoryLoading->addStretch();
    m_modCategorySpinner=new BusySpinner;m_modCategorySpinner->setObjectName("modCategorySpinner");categoryLoading->addWidget(m_modCategorySpinner,0,Qt::AlignHCenter);categoryLoading->addStretch();
    categoryOverlay->addWidget(m_modCategoryState);m_modCategoryState->hide();
    results->addWidget(m_modCategories);
    auto listing = new QVBoxLayout; listing->setSpacing(8); results->addLayout(listing,1);
    auto pager = new QHBoxLayout;
    m_modPageLabel = label({}, nullptr, true); pager->addWidget(m_modPageLabel,1);
    m_modRefresh = button(QCoreApplication::translate("M2CL", "刷新筛选")); m_modRefresh->setObjectName("refreshModTags"); m_modRefresh->setProperty("compact",true);
    m_modPrevious = button(QStringLiteral("‹")); m_modNext = button(QStringLiteral("›"));
    m_modPrevious->setToolTip(QCoreApplication::translate("M2CL", "上一页")); m_modNext->setToolTip(QCoreApplication::translate("M2CL", "下一页"));
    for (auto control : {m_modRefresh,m_modPrevious,m_modNext}) pager->addWidget(control);
    m_modPrevious->setEnabled(false); m_modNext->setEnabled(false); listing->addLayout(pager);listing->addWidget(m_modTagStatus);
    m_modResults = new QListWidget;
    m_modResults->setObjectName("modResults"); m_modResults->setProperty("resourceCards",true);
    m_modResults->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_modResults->verticalScrollBar()->setSingleStep(24);
    m_modResults->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_modResults->setMinimumHeight(100); listing->addWidget(m_modResults,1);
    auto overlay = new QVBoxLayout(m_modResults->viewport()); overlay->setContentsMargins(12,12,12,12);
    m_modSearchState = new QWidget;
    m_modSearchState->setObjectName("modSearchState");
    auto state = new QVBoxLayout(m_modSearchState); state->setContentsMargins(0,0,0,0); state->setSpacing(12); state->addStretch();
    m_modSearchSpinner = new BusySpinner; m_modSearchSpinner->setObjectName("modSearchSpinner"); m_modSearchSpinner->hide();
    state->addWidget(m_modSearchSpinner,0,Qt::AlignHCenter);
    m_modSearchMessage = label(QCoreApplication::translate("M2CL", "输入关键词或选择分类，发现模组"),nullptr,true);
    m_modSearchMessage->setObjectName("modSearchMessage");m_modSearchMessage->setAlignment(Qt::AlignCenter); state->addWidget(m_modSearchMessage);
    m_modSearchRetry = button(QCoreApplication::translate("M2CL", "重试")); m_modSearchRetry->hide(); state->addWidget(m_modSearchRetry,0,Qt::AlignHCenter); state->addStretch();
    auto searchNotice=new NetworkNotice;searchNotice->setObjectName("modSearchNetworkState");state->insertWidget(state->count()-1,searchNotice);connect(searchNotice,&NetworkNotice::retryRequested,this,&MainWindow::searchMods);
    overlay->addWidget(m_modSearchState);
    body->addLayout(results,1);
    m_downloadProgress = new QProgressBar; m_downloadProgress->setRange(0,1000); m_downloadProgress->setTextVisible(false); m_downloadProgress->hide(); body->addWidget(m_downloadProgress);
    m_downloadStatus = label({},nullptr,true);m_downloadStatus->setObjectName("resourceDownloadStatus"); m_downloadStatus->hide(); body->addWidget(m_downloadStatus);
    m_cancelDownload = button(QCoreApplication::translate("M2CL", "取消下载")); m_cancelDownload->setEnabled(false); m_cancelDownload->hide(); body->addWidget(m_cancelDownload,0,Qt::AlignRight);
    m_modDetails = new QWidget;
    m_modDetails->setObjectName("modDetailsPane"); m_modDetails->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);
    auto details = new QVBoxLayout(m_modDetails); details->setContentsMargins(0,0,0,0);
    m_modViews->addWidget(m_modDetails);
    connect(go,&QPushButton::clicked,this,&MainWindow::searchMods);
    connect(m_modQuery,&QLineEdit::returnPressed,this,&MainWindow::searchMods);
    connect(m_modSearchRetry,&QPushButton::clicked,this,&MainWindow::searchMods);
    connect(m_modTagStatus,&NetworkNotice::retryRequested,m_modRefresh,&QPushButton::click);
    connect(m_modRefresh,&QPushButton::clicked,this,[this]{if(!qApp->property("offlineUiTest").toBool()&&!qApp->arguments().contains("--smoke-test"))m_resources.tags(true);});
    connect(qobject_cast<SelectBox *>(m_modVersion),&SelectBox::retryOptionsRequested,m_modRefresh,&QPushButton::click);
    connect(m_modPrevious,&QPushButton::clicked,this,[this]{searchModPage(qMax(0,m_modOffset-20));});
    connect(m_modNext,&QPushButton::clicked,this,[this]{searchModPage(m_modOffset+20);});
    connect(m_modCategories,&QListWidget::currentRowChanged,this,[this]{searchMods();});
    for(auto combo : {m_modVersion,m_modLoader,m_modSort}) connect(combo,&QComboBox::currentTextChanged,this,[this]{invalidateModResults();});
    connect(m_modQuery,&QLineEdit::textEdited,this,[this]{invalidateModResults();});
    connect(m_modResults,&QListWidget::itemDoubleClicked,this,[this]{chooseModVersion();});
    connect(m_cancelDownload,&QPushButton::clicked,&m_resources,&ResourceClient::cancelDownload);
    return marketPage;
}
QWidget *MainWindow::settingsPage() {
    auto widget = new QWidget;
    auto body = column(widget, ui::PageMargin);
    body->setContentsMargins(ui::PageMargin, 8, ui::PageMargin, ui::PageMargin);
    auto header = new QHBoxLayout;
    header->setSpacing(8);
    auto title = label(QCoreApplication::translate("M2CL", "设置"), "pageTitle");
    title->setWordWrap(false);
    title->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    header->addWidget(title);
    header->addSpacing(20);
    auto sectionGroup = new QButtonGroup(widget);
    sectionGroup->setExclusive(true);
    QList<QPushButton *> sections;
    for (int i = 0; i < 3; ++i) {
        auto item = button(i == 0 ? QCoreApplication::translate("M2CL", "通用与外观") : i==1?QCoreApplication::translate("M2CL", "Java 管理"):QCoreApplication::translate("M2CL", "账号管理"));
        item->setObjectName(i == 0 ? "generalSettingsTab" : i==1?"javaSettingsTab":"accountsSettingsTab");
        item->setProperty("settingsTab", true);
        item->setCheckable(true);
        item->setChecked(i == 0);
        sectionGroup->addButton(item, i);
        sections.append(item);
        header->addWidget(item);
    }
    header->addStretch();
    body->addLayout(header);
    m_settingsSections = new QStackedWidget;
    m_settingsSections->setObjectName("settingsSections");
    m_settingsSections->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    body->addWidget(m_settingsSections, 1);
    auto general = new QWidget; auto generalLayout = column(general, 0);
    auto languageCard=card();auto languageLayout=column(languageCard);
    auto languageRow=new QHBoxLayout;languageRow->addWidget(label(QCoreApplication::translate("M2CL","界面语言"),"sectionTitle"));
    auto languageChoice=new SelectBox;languageChoice->setObjectName("interfaceLanguage");
    languageChoice->setAccessibleName(QCoreApplication::translate("M2CL","界面语言"));
    languageChoice->addItem(QCoreApplication::translate("M2CL","跟随系统"),"system");
    languageChoice->addItem(QT_TRANSLATE_NOOP("LanguageNames","简体中文"),"zh_CN");
    languageChoice->addItem(QT_TRANSLATE_NOOP("LanguageNames","繁體中文"),"zh_TW");
    languageChoice->addItem("English","en_US");
    languageChoice->setCurrentIndex(languageChoice->findData(m_settings.language));
    languageRow->addStretch();languageRow->addWidget(languageChoice);languageLayout->addLayout(languageRow);
    auto languageNote=label(QCoreApplication::translate("M2CL","重启启动器后生效；不会更改游戏内语言。"),nullptr,true);languageNote->setObjectName("languageRestartNotice");languageLayout->addWidget(languageNote);
    connect(languageChoice,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,languageChoice,languageNote]{
        m_settings.language=languageChoice->currentData().toString();QString error;
        if(!m_store.save(m_settings,&error))languageNote->setText(QCoreApplication::translate("M2CL","语言设置保存失败：%1").arg(error));
        else languageNote->setText(QCoreApplication::translate("M2CL","语言已保存，下次启动时生效。"));
    });generalLayout->addWidget(languageCard);
    auto appearance = card(); auto appearanceLayout = column(appearance); appearanceLayout->addWidget(label(QCoreApplication::translate("M2CL", "界面外观"), "sectionTitle"));
    auto choices=new QHBoxLayout;choices->setSpacing(16);
    for(const auto &mode:{QStringLiteral("light"),QStringLiteral("dark"),QStringLiteral("system")}){
        auto choice=new AppearanceChoice(mode);m_appearanceChoices.append(choice);choices->addWidget(choice);
        connect(choice,&QAbstractButton::clicked,this,[this,mode]{setAppearance(mode,m_settings.theme);});
    }
    choices->addStretch();appearanceLayout->addLayout(choices);
    auto row = new QHBoxLayout;
    row->addWidget(label(QCoreApplication::translate("M2CL", "主题配色"))); m_theme = new SelectBox;
    m_theme->addItem(QCoreApplication::translate("M2CL", "水星绿"), "forest"); m_theme->addItem(QCoreApplication::translate("M2CL", "远山蓝"), "blue"); m_theme->addItem(QCoreApplication::translate("M2CL", "暖沙金"), "sand"); m_theme->addItem(QCoreApplication::translate("M2CL", "暮色紫"), "violet");
    m_theme->setCurrentIndex(m_theme->findData(m_settings.theme));m_theme->setMinimumWidth(170);row->addWidget(m_theme);row->addStretch();appearanceLayout->addLayout(row);
    appearanceLayout->addWidget(label(QCoreApplication::translate("M2CL", "外观即时生效，封面会按 Minecraft 版本自动选择。"), nullptr, true)); generalLayout->addWidget(appearance);
    connect(m_theme, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { setAppearance(m_settings.appearance, m_theme->currentData().toString()); });
    auto downloads = card(); auto downloadsLayout = column(downloads);downloadsLayout->addWidget(label(QCoreApplication::translate("M2CL", "游戏与组件下载"),"sectionTitle"));
    auto sourceRow = new QHBoxLayout;sourceRow->addWidget(label(QCoreApplication::translate("M2CL", "下载源")));
    auto downloadSource = new SelectBox;downloadSource->setObjectName("downloadSource");downloadSource->setAccessibleName(QCoreApplication::translate("M2CL", "游戏与组件下载源"));
    downloadSource->addItem(automaticDownloadSourceLabel(),"auto");downloadSource->addItem(QCoreApplication::translate("M2CL", "官方源"),"official");downloadSource->addItem("BMCLAPI","bmclapi");downloadSource->setCurrentIndex(downloadSource->findData(m_settings.downloadSource));
    sourceRow->addWidget(downloadSource);sourceRow->addStretch();downloadsLayout->addLayout(sourceRow);
    downloadsLayout->addWidget(label(QCoreApplication::translate("M2CL", "用于 Minecraft、Fabric、Forge 和 OptiFine。自动模式按系统时区选择：Asia/Shanghai 镜像优先，其余官方优先；失败后只尝试一次备用源。"),nullptr,true));
    generalLayout->addWidget(downloads);connect(downloadSource,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,downloadSource]{m_settings.downloadSource=downloadSource->currentData().toString();save();});
    auto info = card(); auto infoLayout = column(info); infoLayout->addWidget(label(QCoreApplication::translate("M2CL", "关于这个预览版"), "sectionTitle"));
    infoLayout->addWidget(label(QStringLiteral("Mercury MC Launcher"),"applicationFullName"));
    infoLayout->addWidget(label(QCoreApplication::translate("M2CL", "M²CL 0.1.0 开发预览 · 原版安装、实例资源与 Java 管理。微软登录尚在联调。"), nullptr, true));
    auto attribution=label(QCoreApplication::translate("M2CL", "原项目作者：ppm203 · Copyright © 2026 ppm203"));attribution->setObjectName("aboutProjectAttribution");infoLayout->addWidget(attribution);
    auto qtNotice=label(QCoreApplication::translate("M2CL", "本程序使用 Qt %1 · LGPLv3\nCopyright © The Qt Company Ltd. 及其他贡献者。完整许可与对应源码说明见下方。"),nullptr,true);
    qtNotice->setText(qtNotice->text().arg(QString::fromLatin1(qVersion())));qtNotice->setObjectName("aboutQtNotice");infoLayout->addWidget(qtNotice);
    infoLayout->addWidget(label(QCoreApplication::translate("M2CL", "GNU GPL 第 3 版 + 第 7 节附加条款 · 不提供担保。第三方修改版须保留合理署名并明确区分来源。"),nullptr,true));
    auto legal=new QPushButton(QCoreApplication::translate("M2CL", "许可证与署名"));legal->setObjectName("showProjectLicenses");infoLayout->addWidget(legal,0,Qt::AlignLeft);
    connect(legal,&QPushButton::clicked,this,[this]{ProjectLegalDialog dialog(this);dialog.exec();});
    generalLayout->addWidget(info); generalLayout->addStretch();auto generalScroll=new QScrollArea;generalScroll->setWidgetResizable(true);generalScroll->setFrameShape(QFrame::NoFrame);generalScroll->setWidget(general);m_settingsSections->addWidget(generalScroll);
    m_javaManager = new JavaManager(m_settings, m_store); m_settingsSections->addWidget(m_javaManager);
    m_accountsPanel=new AccountsPanel(*m_accounts,*m_avatars,m_settings,m_store);m_settingsSections->addWidget(m_accountsPanel);
    for (int i = 0; i < sections.size(); ++i) connect(sections.at(i), &QPushButton::clicked, this, [this, i] { m_settingsSections->setCurrentIndex(i); });
    connect(m_settingsSections, &QStackedWidget::currentChanged, this, [sections](int index) { for (int i = 0; i < sections.size(); ++i) sections.at(i)->setChecked(i == index); });
    connect(m_javaManager, &JavaManager::changed, this, [this] { m_javas = m_javaManager->runtimes(); refreshInstanceViews(); });
    connect(m_javaManager, &JavaManager::taskProgress, this, [this](const QString &name, const QString &message, int progress, bool active) { m_javaDownloadActive = active; m_lastDownloadJava = true; m_activeDownloadName = name; m_activeDownloadMessage = message; m_activeDownloadProgress = progress; m_heroLaunch->setEnabled(!active && !m_installWatcher.isRunning()); updateDownloadPanel(); status(message); });
    return widget;
}
void MainWindow::showAccounts(){changePage(3);m_settingsSections->setCurrentIndex(2);}
void MainWindow::showJavaSettings(bool downloads) { changePage(3); m_settingsSections->setCurrentIndex(1); if (auto tabs = m_javaManager->findChild<QTabWidget *>("javaTabs")) tabs->setCurrentIndex(downloads ? 1 : 0); }
void MainWindow::save() { QString error; if (!m_store.save(m_settings, &error)) status(QCoreApplication::translate("M2CL", "设置保存失败：%1").arg(error)); }
void MainWindow::status(const QString &message) { if (m_status) {if(!networkErrorSummary(message).isEmpty()){m_status->clear();m_status->hide();return;}m_status->setText(message);m_status->setToolTip(message);const bool actionable=diagnosticContains(message, QT_TRANSLATE_NOOP("M2CL", "失败")) || diagnosticContains(message, QT_TRANSLATE_NOOP("M2CL", "无法")) || diagnosticContains(message, QT_TRANSLATE_NOOP("M2CL", "不存在")) || diagnosticContains(message, QT_TRANSLATE_NOOP("M2CL", "请先")) || diagnosticContains(message, QT_TRANSLATE_NOOP("M2CL", "不兼容")) || diagnosticContains(message, QT_TRANSLATE_NOOP("M2CL", "正在运行"));const bool mods=m_pages && m_pages->currentIndex()==2;m_status->setVisible(actionable && !mods);if(actionable && mods && m_modPageLabel)m_modPageLabel->setText(message);} }
void MainWindow::changePage(int index) {
    if (index < 0 || index >= m_pages->count()) return;
    if(index!=2 && m_modViews && m_modViews->currentIndex()==1)closeModDetails();
    m_pages->setCurrentIndex(index);
    if(index == 2 && !qApp->arguments().contains("--smoke-test") && !qApp->property("offlineUiTest").toBool()){m_resources.tags();if(m_modViews->currentIndex()==0 && m_projects.isEmpty() && !m_modSearchPending)searchMods();}
    for (int i = 0; i < m_nav.size(); ++i) m_nav.at(i)->setChecked(i == index && (i!=2||m_marketType=="mod"));
    m_shaderNav->setChecked(index==2&&m_marketType=="shader");m_packNav->setChecked(index==2&&m_marketType=="resourcepack");
}
void MainWindow::chooseRoot() {
    const auto path = QFileDialog::getExistingDirectory(this, QCoreApplication::translate("M2CL", "选择 .minecraft 目录（包含 versions 文件夹）"));
    if (path.isEmpty()) return;
    if (!QDir(QDir(path).filePath("versions")).exists()) { QMessageBox::information(this, QCoreApplication::translate("M2CL", "没有找到游戏版本"), QCoreApplication::translate("M2CL", "请选择包含 versions 文件夹的游戏目录。")); return; }
    const auto canonical = QFileInfo(path).canonicalFilePath();
    if (!m_settings.gameRoots.contains(canonical)) m_settings.gameRoots.append(canonical);
    save(); refreshCatalog();
}
void MainWindow::refreshCatalog() {
    if (m_catalogWatcher.isRunning()) { m_catalogPending = true; status(QCoreApplication::translate("M2CL", "正在读取游戏目录，请稍候")); return; }
    m_catalogReady = false;
    m_roots->clear(); m_roots->addItems(m_settings.gameRoots);
    const auto roots = m_settings.gameRoots; const auto overrides = m_settings.isolationOverrides;
    status(QCoreApplication::translate("M2CL", "正在读取本地实例…"));
    m_catalogWatcher.setFuture(QtConcurrent::run([roots, overrides] { return scanInstances(roots, overrides); }));
}
const Instance *MainWindow::selectedInstance() const {
    for (const auto &instance : m_instances) if (instance.path == m_settings.selectedInstance) return &instance;
    return nullptr;
}
void MainWindow::selectInstance(const QString &path) {
    if (path.isEmpty() || path == m_settings.selectedInstance) return;
    m_settings.selectedInstance = path; save(); refreshInstanceViews();
}
QString MainWindow::instanceName(const Instance &instance) const { return m_settings.instanceNames.value(instance.path).toString(instance.id); }
void MainWindow::refreshInstanceViews() {
    if(m_modViews)for(auto view:m_modViews->findChildren<InstanceModsView *>())for(const auto &instance:m_instances)if(instance.path==view->instancePath())view->updateInstance(instance);
    for(auto grid:{m_homeGrid,m_libraryGrid})grid->setInstances(m_instances,m_settings.selectedInstance,m_settings.favoriteInstances,m_settings.instanceNames,m_settings.coverOverrides);
    const auto count=QCoreApplication::translate("M2CL", "%n 个实例",nullptr,m_instances.size());m_instanceCount->setText(count);m_libraryCount->setText(count);
    const auto instance=selectedInstance();
    const auto account=m_accounts->current();
    m_heroPlayer->setVisible(instance&&!account.id.isEmpty());
    m_heroPlayer->setText(QCoreApplication::translate("M2CL", "使用角色 %1").arg(account.name));
    for(auto action:m_hero->findChildren<QPushButton *>())if(action->property("onCover").toBool())action->setVisible(instance&&(!action->property("heroAuxiliary").toBool()||width()>=1050));
    for(auto item:{m_heroVersion,m_heroLoader,m_heroJava})item->setVisible(instance);
    if(instance){
        m_hero->setCover(instance->version,m_settings.coverOverrides.value(instance->path).toString());m_heroTitle->setText(instanceName(*instance));
        m_heroVersion->setText(instance->version);m_heroLoader->setText(instance->loaderName());
        QString javaError;const auto javaPolicy=loadInstanceJavaChoice(instance->path,&javaError);const bool fixed=javaPolicy.value("mode")=="fixed";auto selectedMajor=javaPolicy.value("major").toInt();if(!selectedMajor)selectedMajor=parseJavaOutput("java.version = "+javaPolicy.value("runtimeVersion").toString()).major;
        m_heroJava->setText(fixed?(selectedMajor?QCoreApplication::translate("M2CL", "Java %1 · 固定").arg(selectedMajor):QCoreApplication::translate("M2CL", "固定 Java")):instance->javaMajor?QCoreApplication::translate("M2CL", "默认 Java %1").arg(instance->javaMajor):QCoreApplication::translate("M2CL", "Java 待识别"));
        m_heroJava->setToolTip(fixed?QCoreApplication::translate("M2CL", "将使用指定的 Java；游戏默认版本为 %1").arg(instance->javaMajor):QCoreApplication::translate("M2CL", "自动管理优先选择游戏默认版本；实例设置可固定其他版本"));
        if(!javaError.isEmpty()){m_heroJava->setText(QCoreApplication::translate("M2CL", "Java 配置异常"));m_heroJava->setToolTip(javaError);}
        const auto relative=QDir(instance->root).relativeFilePath(instance->gameDirectory);
        const auto shortPath=QFileInfo(instance->root).fileName()+(relative=="."?QString():"/"+relative);
        m_heroPath->setText(QDir::toNativeSeparators(shortPath));m_heroPath->setToolTip(instance->gameDirectory);
        m_heroLaunch->setText(m_accounts->current().microsoft?QCoreApplication::translate("M2CL", "启动游戏"):QCoreApplication::translate("M2CL", "离线启动"));m_heroLaunch->setToolTip(QCoreApplication::translate("M2CL", "使用当前角色启动，先检查登录状态并补齐游戏文件"));
        m_libraryDetail->setText(QCoreApplication::translate("M2CL", "当前实例：%1\n%2").arg(instanceName(*instance),QDir::toNativeSeparators(instance->gameDirectory)));
    }else{
        m_hero->setCover("1.20");m_heroTitle->setText(QCoreApplication::translate("M2CL", "把世界放进书桌"));m_heroPath->setText(QCoreApplication::translate("M2CL", "安装原版或添加已有目录，开启你的下一段冒险。"));
        m_heroLaunch->setText(QCoreApplication::translate("M2CL", "安装原版"));m_heroLaunch->setToolTip(QCoreApplication::translate("M2CL", "选择 Minecraft 版本并创建独立实例"));
        m_libraryDetail->setText(QCoreApplication::translate("M2CL", "添加已有 .minecraft 目录，识别本地游戏实例。"));
    }
}
void MainWindow::toggleFavorite(const QString &path) {
    if(m_settings.favoriteInstances.contains(path))m_settings.favoriteInstances.removeAll(path);else m_settings.favoriteInstances.append(path);
    save();refreshInstanceViews();
}
void MainWindow::openDirectory(const QString &path) {
    if(!QDir(path).exists()){status(QCoreApplication::translate("M2CL", "目录尚不存在：%1").arg(path));return;}
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
void MainWindow::instanceMenu(const QString &path,const QPoint &position) {
    selectInstance(path);const auto selected=selectedInstance();if(!selected)return;const auto instance=*selected;
    RoundedMenu menu(this);menu.setObjectName("instanceContextMenu");
    menu.addAction(lineIcon(Glyph::Play),m_accounts->current().microsoft?QCoreApplication::translate("M2CL", "启动游戏"):QCoreApplication::translate("M2CL", "离线启动"),this,&MainWindow::launchInstance);
    menu.addAction(lineIcon(Glyph::Edit),QCoreApplication::translate("M2CL", "编辑实例"),this,[this,path]{editInstance(path);});
    menu.addAction(lineIcon(Glyph::Star),m_settings.favoriteInstances.contains(path)?QCoreApplication::translate("M2CL", "取消收藏"):QCoreApplication::translate("M2CL", "收藏实例"),this,[this,path]{toggleFavorite(path);});
    menu.addSeparator();menu.addAction(lineIcon(Glyph::Folder),QCoreApplication::translate("M2CL", "打开游戏目录"),this,[this,instance]{openDirectory(instance.gameDirectory);});
    menu.addAction(QCoreApplication::translate("M2CL", "打开 mods 目录"),this,[this,instance]{openDirectory(QDir(instance.gameDirectory).filePath("mods"));});
    menu.addAction(QCoreApplication::translate("M2CL", "打开日志目录"),this,[this,instance]{openDirectory(QDir(instance.gameDirectory).filePath("logs"));});
    menu.exec(position);
}
void MainWindow::editInstance(const QString &path, int section) {
    if(m_modInstallActive||m_installWatcher.isRunning()){status(QCoreApplication::translate("M2CL", "请先等待安装完成或取消任务，再修改实例设置"));return;}
    if(!path.isEmpty())selectInstance(path);const auto selected=selectedInstance();if(!selected)return;const auto instance=*selected;
    QString javaError;const auto ownJava=loadInstanceJavaChoice(instance.path,&javaError);
    if(!javaError.isEmpty()){QMessageBox::warning(this,QCoreApplication::translate("M2CL", "实例配置无法读取"),javaError);return;}
    const auto ownLaunch=loadInstanceLaunchOptions(instance.path,m_settings.memoryMiB,&javaError);
    if(!javaError.isEmpty()){QMessageBox::warning(this,QCoreApplication::translate("M2CL", "实例配置无法读取"),javaError);return;}
    auto dialog=new QDialog(this);dialog->setObjectName("instanceSettings");dialog->setWindowTitle(QCoreApplication::translate("M2CL", "实例设置"));dialog->setWindowModality(Qt::WindowModal);dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(730,600);dialog->setMinimumSize(650,600);
    auto outer=new QVBoxLayout(dialog);outer->setContentsMargins(20,18,20,18);outer->setSpacing(18);outer->addWidget(label(QCoreApplication::translate("M2CL", "实例设置"),"instanceSettingsTitle"));
    auto main=new QHBoxLayout;main->setSpacing(20);auto navigation=new QVBoxLayout;navigation->setSpacing(8);auto pages=new QStackedWidget;
    const QStringList tabs{QCoreApplication::translate("M2CL", "基础信息"),QCoreApplication::translate("M2CL", "游戏目录"),QCoreApplication::translate("M2CL", "Java 设置"),QCoreApplication::translate("M2CL", "启动设置")};
    const QList<Glyph> tabIcons{Glyph::Document,Glyph::Folder,Glyph::Java,Glyph::Gamepad};
    QList<QPushButton *> nav;
    for(int i=0;i<tabs.size();++i){auto item=button(tabs.at(i));item->setProperty("nav",true);item->setProperty("instanceSettingsNav",true);item->setIcon(lineIcon(tabIcons.at(i)));item->setIconSize(QSize(20,20));item->setCheckable(true);item->setChecked(i==section);item->setFixedSize(136,48);navigation->addWidget(item);nav.append(item);
        connect(item,&QPushButton::clicked,dialog,[pages,i]{pages->setCurrentIndex(i);});
    }
    // Keep the section selection coherent for pointer and keyboard activation.
    for(int i=0;i<nav.size();++i)connect(nav.at(i),&QPushButton::clicked,dialog,[nav,i]{for(int j=0;j<nav.size();++j)nav.at(j)->setChecked(i==j);});
    navigation->addStretch();main->addLayout(navigation);main->addWidget(pages,1);
    auto basic=new QWidget;auto basicLayout=column(basic,0);basicLayout->setSpacing(ui::SectionSpace);
    auto coverRow=new QHBoxLayout;auto preview=new CoverView;preview->setShaded(false);preview->setProperty("fixedPreview",true);preview->setMinimumHeight(100);preview->setFixedSize(180,101);preview->setCover(instance.version,m_settings.coverOverrides.value(instance.path).toString());preview->setProperty("customCover",m_settings.coverOverrides.value(instance.path).toString());
    coverRow->addWidget(preview);auto coverActions=new QVBoxLayout;auto cover=button(QCoreApplication::translate("M2CL", "更换封面"));cover->setIcon(lineIcon(Glyph::Image));coverActions->addWidget(cover);
    auto reset=button(QCoreApplication::translate("M2CL", "恢复版本封面"));coverActions->addWidget(reset);coverActions->addStretch();coverRow->addLayout(coverActions);coverRow->addStretch();basicLayout->addLayout(coverRow);
    connect(cover,&QPushButton::clicked,dialog,[dialog,preview,instance]{
        const auto file=QFileDialog::getOpenFileName(dialog,QCoreApplication::translate("M2CL", "选择实例封面"),{},QCoreApplication::translate("M2CL", "图片 (*.png *.jpg *.jpeg)"));if(file.isEmpty())return;
        if(!CoverStore::readable(file)){QMessageBox::information(dialog,QCoreApplication::translate("M2CL", "无法读取封面"),QCoreApplication::translate("M2CL", "请选择可读取的 PNG 或 JPEG 图片（文件不超过 32 MiB，边长不超过 16000 像素）。"));return;}
        preview->setProperty("customCover",file);preview->setCover(instance.version,file);
    });
    connect(reset,&QPushButton::clicked,dialog,[preview,instance]{preview->setProperty("customCover",QString());preview->setCover(instance.version);});
    auto form=new QFormLayout;form->setVerticalSpacing(ui::Space);form->setHorizontalSpacing(ui::SectionSpace);form->setLabelAlignment(Qt::AlignLeft);auto name=new QLineEdit(instanceName(instance));name->setObjectName("instanceName");name->setMaxLength(80);form->addRow(QCoreApplication::translate("M2CL", "实例名称"),name);
    auto version=new QLineEdit(instance.version);version->setReadOnly(true);form->addRow(QCoreApplication::translate("M2CL", "游戏版本"),version);
    auto loader=new QLineEdit(instance.loaderName());loader->setReadOnly(true);form->addRow(QCoreApplication::translate("M2CL", "加载器"),loader);basicLayout->addLayout(form);
    auto components=button(QCoreApplication::translate("M2CL", "管理版本组件"));components->setIcon(lineIcon(Glyph::Settings));components->setObjectName("manageVersionComponents");basicLayout->addWidget(components,0,Qt::AlignLeft);
    connect(components,&QPushButton::clicked,dialog,[this,dialog,instance]{dialog->reject();manageVersionComponents(instance);});
    basicLayout->addWidget(label(QCoreApplication::translate("M2CL", "名称和封面仅用于启动器显示，版本文件夹保持原样。"),nullptr,true));basicLayout->addStretch();pages->addWidget(basic);
    auto directory=new QWidget;auto directoryLayout=column(directory,0);directoryLayout->addWidget(label(QCoreApplication::translate("M2CL", "游戏目录"),"sectionTitle"));
    auto location=new QLineEdit(instance.gameDirectory);location->setReadOnly(true);directoryLayout->addWidget(location);
    auto isolated=new CheckBox(QCoreApplication::translate("M2CL", "此实例使用独立游戏目录"));isolated->setObjectName("instanceIsolation");isolated->setChecked(instance.isolated);directoryLayout->addWidget(isolated);
    directoryLayout->addWidget(label(QCoreApplication::translate("M2CL", "独立目录把存档、模组与配置放在当前版本文件夹内。切换目录选择不会移动已有文件。"),nullptr,true));
    auto folders=new QHBoxLayout;for(const auto &folder:{QString(),QStringLiteral("mods"),QStringLiteral("logs")}){
        auto open=button(folder.isEmpty()?QCoreApplication::translate("M2CL", "打开目录"):folder);folders->addWidget(open);connect(open,&QPushButton::clicked,dialog,[this,instance,folder]{openDirectory(folder.isEmpty()?instance.gameDirectory:QDir(instance.gameDirectory).filePath(folder));});
    }directoryLayout->addLayout(folders);directoryLayout->addStretch();pages->addWidget(directory);
    auto java=new QWidget;auto javaLayout=column(java,0);javaLayout->setSizeConstraint(QLayout::SetMinimumSize);javaLayout->addWidget(label(QCoreApplication::translate("M2CL", "Java 运行环境"),"sectionTitle"));
    auto javaChoice = new JavaChoiceWidget(ownJava, m_javas, instance.javaMajor);javaChoice->setObjectName("instanceJavaChoice"); javaLayout->addWidget(javaChoice);
    auto manageJava=button(QCoreApplication::translate("M2CL", "管理 / 下载 Java"));manageJava->setObjectName("manageInstanceJava");javaLayout->addWidget(manageJava,0,Qt::AlignLeft);connect(manageJava,&QPushButton::clicked,dialog,[this,dialog]{dialog->reject();showJavaSettings();});javaLayout->addStretch();auto javaScroll=new QScrollArea;javaScroll->setObjectName("instanceJavaScroll");javaScroll->setWidgetResizable(true);javaScroll->setFrameShape(QFrame::NoFrame);javaScroll->setWidget(java);pages->addWidget(javaScroll);
    auto context=memoryContext(instance.version,instance.gameDirectory,instance.loader,ownJava.value("mode")=="fixed"?ownJava.value("arch").toString():QString());
    auto launchOptions=new LaunchOptionsWidget(ownLaunch,context);auto launchScroll=new QScrollArea;launchScroll->setObjectName("instanceLaunchScroll");launchScroll->setWidgetResizable(true);launchScroll->setFrameShape(QFrame::NoFrame);launchScroll->setWidget(launchOptions);pages->addWidget(launchScroll);
    auto refreshMemoryContext=[instance,isolated,javaChoice,launchOptions]{const auto choice=javaChoice->choice();launchOptions->setMemoryContext(memoryContext(instance.version,isolated->isChecked()?instance.path:instance.root,instance.loader,choice.value("mode")=="fixed"?choice.value("arch").toString():QString()));};
    connect(javaChoice,&JavaChoiceWidget::changed,dialog,refreshMemoryContext);connect(isolated,&QCheckBox::toggled,dialog,refreshMemoryContext);
    outer->addLayout(main,1);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Save)->setText(QCoreApplication::translate("M2CL", "保存"));buttons->button(QDialogButtonBox::Save)->setProperty("primary",true);buttons->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("M2CL", "取消"));outer->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,dialog,[this,dialog,name,preview,isolated,instance,javaChoice,launchOptions,launchScroll,pages,nav]{
        const auto text=name->text().trimmed();if(text.isEmpty()){name->setFocus();return;}
        const auto choice = javaChoice->choice();if(!javaChoice->selectionError().isEmpty()){QMessageBox::information(dialog,QCoreApplication::translate("M2CL", "请确认 Java 选择"),javaChoice->selectionError());return;}
        if (choice.value("mode") == "fixed" && choice.value("path").toString().isEmpty()) { QMessageBox::information(dialog, QCoreApplication::translate("M2CL", "请选择 Java"), QCoreApplication::translate("M2CL", "固定构建需要选择一个本地 Java")); return; }
        if(!launchOptions->selectionError().isEmpty()){pages->setCurrentIndex(3);for(int i=0;i<nav.size();++i)nav.at(i)->setChecked(i==3);QTimer::singleShot(0,dialog,[launchScroll,launchOptions]{launchScroll->ensureWidgetVisible(launchOptions->findChild<QLabel *>("instanceLaunchError"));});return;}
        QString error;if(!saveInstanceRuntimeSettings(instance.path,choice,launchOptions->options(),&error)){QMessageBox::warning(dialog,QCoreApplication::translate("M2CL", "实例配置保存失败"),error);return;}
        m_settings.legacyInstanceJava.remove(instance.path);
        m_settings.instanceNames.insert(instance.path,text);const auto custom=preview->property("customCover").toString();if(custom.isEmpty())m_settings.coverOverrides.remove(instance.path);else m_settings.coverOverrides.insert(instance.path,custom);
        m_settings.isolationOverrides.insert(instance.path,isolated->isChecked());save();refreshInstanceViews();refreshCatalog();dialog->accept();
    });
    pages->setCurrentIndex(qBound(0, section, pages->count() - 1));
    dialog->open();
}
void MainWindow::updateDownloadPanel() {
    m_bar->downloads->setBadge((m_resources.downloading()||m_installWatcher.isRunning()||m_javaDownloadActive||m_modInstallActive)?1:0);
    m_downloadPanel->setTask(m_activeDownloadName,m_activeDownloadMessage,m_activeDownloadProgress,m_resources.downloading()||m_installWatcher.isRunning()||m_javaDownloadActive||m_modInstallActive);
}
void MainWindow::showDownloads() {
    if(m_downloadPanel->isVisible()){m_downloadPanel->hide();return;}
    const QPoint anchor=m_bar->downloads->mapToGlobal(QPoint(m_bar->downloads->width(),m_bar->downloads->height()+6));
    m_downloadPanel->adjustSize();const int x=qBound(mapToGlobal(QPoint(8,0)).x(),anchor.x()-m_downloadPanel->width(),mapToGlobal(QPoint(width()-m_downloadPanel->width()-8,0)).x());
    m_downloadPanel->move(x,anchor.y());m_downloadPanel->show();
}
void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
#ifdef Q_OS_MACOS
    hideMacWindowCaption(this);
#endif
}
bool MainWindow::eventFilter(QObject *object,QEvent *event) {
    if(object==this&&event->type()==QEvent::Resize&&m_hero){
        for(auto action:m_hero->findChildren<QPushButton *>())if(action->property("heroAuxiliary").toBool())action->setVisible(selectedInstance()&&width()>=1050);
    }
    if(object==this && event->type()==QEvent::Resize && m_modViews)QTimer::singleShot(0,this,[this]{updateModBrowseLayout(m_modViews->currentIndex()==1);});
    const auto widget=qobject_cast<QWidget *>(object);
    if(windowFlags().testFlag(Qt::FramelessWindowHint)&&widget&&widget->window()==this&&!isMaximized()&&!QApplication::activeModalWidget()){
        if(event->type()==QEvent::MouseButtonRelease&&m_dragResizeEdges){m_dragResizeEdges={};releaseMouse();return true;}
        if(event->type()==QEvent::MouseMove||event->type()==QEvent::MouseButtonPress){
            auto mouse=static_cast<QMouseEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
            const QPoint global=mouse->globalPosition().toPoint();
#else
            const QPoint global=mouse->globalPos();
#endif
            const QPoint point=mapFromGlobal(global);
            if(m_dragResizeEdges&&event->type()==QEvent::MouseMove){
                const auto delta=global-m_resizeStartMouse;auto target=m_resizeStartGeometry;
                if(m_dragResizeEdges.testFlag(Qt::LeftEdge))target.setLeft(qMin(target.left()+delta.x(),target.right()-minimumWidth()+1));
                if(m_dragResizeEdges.testFlag(Qt::RightEdge))target.setRight(qMax(target.right()+delta.x(),target.left()+minimumWidth()-1));
                if(m_dragResizeEdges.testFlag(Qt::TopEdge))target.setTop(qMin(target.top()+delta.y(),target.bottom()-minimumHeight()+1));
                if(m_dragResizeEdges.testFlag(Qt::BottomEdge))target.setBottom(qMax(target.bottom()+delta.y(),target.top()+minimumHeight()-1));
                setGeometry(target);return true;
            }
            Qt::Edges edges;if(point.x()<5)edges|=Qt::LeftEdge;if(point.x()>=width()-5)edges|=Qt::RightEdge;if(point.y()<5)edges|=Qt::TopEdge;if(point.y()>=height()-5)edges|=Qt::BottomEdge;
            if(event->type()==QEvent::MouseButtonPress&&mouse->button()==Qt::LeftButton&&edges){
                if(windowHandle()&&windowHandle()->startSystemResize(edges))return true;
                m_dragResizeEdges=edges;m_resizeStartGeometry=geometry();m_resizeStartMouse=global;grabMouse();return true;
            }
            if(event->type()==QEvent::MouseMove){
                if(edges&&rect().contains(point)){
                    const auto cursor=(edges.testFlag(Qt::LeftEdge)&&edges.testFlag(Qt::TopEdge))||(edges.testFlag(Qt::RightEdge)&&edges.testFlag(Qt::BottomEdge))?Qt::SizeFDiagCursor:
                        (edges.testFlag(Qt::LeftEdge)&&edges.testFlag(Qt::BottomEdge))||(edges.testFlag(Qt::RightEdge)&&edges.testFlag(Qt::TopEdge))?Qt::SizeBDiagCursor:
                        (edges.testFlag(Qt::LeftEdge)||edges.testFlag(Qt::RightEdge))?Qt::SizeHorCursor:Qt::SizeVerCursor;
                    if(m_resizeCursor)qApp->changeOverrideCursor(cursor);else{qApp->setOverrideCursor(cursor);m_resizeCursor=true;}
                }else if(m_resizeCursor){qApp->restoreOverrideCursor();m_resizeCursor=false;}
            }
        }else if(event->type()==QEvent::Leave&&m_resizeCursor&&!rect().contains(mapFromGlobal(QCursor::pos()))){qApp->restoreOverrideCursor();m_resizeCursor=false;}
    }
    return QMainWindow::eventFilter(object,event);
}
void MainWindow::createInstance() {
    if (m_installWatcher.isRunning() || m_resources.downloading() || m_javaDownloadActive || m_modInstallActive) { status(QCoreApplication::translate("M2CL", "请先等待或取消当前下载")); return; }
    InstallPlanDialog dialog(QCoreApplication::translate("M2CL", "安装 Minecraft"),this); dialog.setObjectName("createGameDialog"); dialog.resize(640, 360);
    auto layout = dialog.contentLayout(); auto form = new QFormLayout;
    form->setHorizontalSpacing(20); form->setVerticalSpacing(14); form->setLabelAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto versions = new SelectBox; versions->setObjectName("gameVersion"); versions->setProperty("popupMode","versions"); versions->setEnabled(false);
    auto name = new QLineEdit; name->setObjectName("instanceName"); name->setPlaceholderText(QCoreApplication::translate("M2CL", "留空使用版本号 + 原版"));
    auto root = new QLineEdit(m_settings.gameRoots.isEmpty() ? QDir(m_store.directory()).filePath("minecraft") : m_settings.gameRoots.first());
    auto rootRow = new QHBoxLayout; rootRow->addWidget(root); auto browse = button(QCoreApplication::translate("M2CL", "浏览")); rootRow->addWidget(browse);
    form->addRow(QCoreApplication::translate("M2CL", "Minecraft 版本"), versions); form->addRow(QCoreApplication::translate("M2CL", "实例名称"), name); form->addRow(QCoreApplication::translate("M2CL", "游戏根目录"), rootRow); layout->addLayout(form);
    auto loader=new SelectBox;loader->addItem(QCoreApplication::translate("M2CL", "原版"),"vanilla");loader->addItem(QStringLiteral("Fabric"),"fabric");loader->addItem(QStringLiteral("Forge"),"forge");loader->addItem(QStringLiteral("NeoForge"),"neoforge");loader->addItem(QStringLiteral("Quilt"),"quilt");auto builds=new SelectBox;builds->setObjectName("loaderBuild");builds->setEnabled(false);form->addRow(QCoreApplication::translate("M2CL", "加载器"),loader);form->addRow(QCoreApplication::translate("M2CL", "加载器版本"),builds);
    auto gameState=new NetworkNotice;gameState->setObjectName("gameManifestNetworkState");layout->addWidget(gameState);
    auto loaderState=new NetworkNotice;loaderState->setObjectName("loaderBuildNetworkState");layout->addWidget(loaderState);
    auto message=label({},nullptr,true);message->hide();layout->addWidget(message);
    layout->addWidget(label(QCoreApplication::translate("M2CL", "每个实例使用独立的存档和设置目录。下载游戏、依赖和资源后，缺少的 Java 也会自动安装。"), nullptr, true));
    auto actions = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    actions->button(QDialogButtonBox::Ok)->setText(QCoreApplication::translate("M2CL", "创建并安装")); actions->button(QDialogButtonBox::Ok)->setProperty("primary",true);
    actions->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("M2CL", "取消"));
    actions->button(QDialogButtonBox::Ok)->setEnabled(false); layout->addWidget(actions);
    connect(browse, &QPushButton::clicked, &dialog, [&, root] { const auto path = QFileDialog::getExistingDirectory(&dialog, QCoreApplication::translate("M2CL", "选择游戏根目录"), root->text()); if (!path.isEmpty()) root->setText(path); });
    connect(actions, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    const auto cancelled = std::make_shared<std::atomic_bool>(false);
    QFutureWatcher<VersionList> watcher;
    std::function<void()> queryLoader;
    auto queryManifest=[&]{if(watcher.isRunning())return;versions->clear();versions->setEnabled(false);actions->button(QDialogButtonBox::Ok)->setEnabled(false);gameState->loading();watcher.setFuture(QtConcurrent::run([cancelled,source=m_settings.downloadSource]{return fetchGameVersions(cancelled,source);}));};
    connect(gameState,&NetworkNotice::retryRequested,&dialog,queryManifest);
    QStringList existing; for (const auto &instance : m_instances) existing.append(instanceName(instance));
    QString automaticName;
    connect(versions, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [&, name, versions](int) {
        const auto next = defaultInstanceName(versions->currentText(), loader->currentData().toString(), existing);
        if (name->text().isEmpty() || name->text() == automaticName) name->setText(next);
        automaticName = next; name->setPlaceholderText(next);
    });
    connect(&watcher, &QFutureWatcher<VersionList>::finished, &dialog, [&] {
        const auto result = watcher.result();gameState->clear();
        if (!result.error.isEmpty()) { gameState->failure(QCoreApplication::translate("M2CL", "Minecraft 版本列表加载失败"),result.error); return; }
        if(result.versions.isEmpty()){gameState->failure(QCoreApplication::translate("M2CL", "没有取得 Minecraft 版本，请重试"),{});return;}
        int defaultRelease = -1;
        for (const auto &value : result.versions) {
            const auto entry = value.toObject();
            const auto id = entry.value("id").toString();
            if (id.isEmpty()) continue;
            const auto channel = gameVersionChannel(id, entry.value("type").toString());
            versions->addItem(id, entry);
            versions->setItemData(versions->count()-1, channel, SelectBox::PopupGroupRole);
            if (channel == "release" && defaultRelease < 0) defaultRelease = versions->count()-1;
        }
        if (defaultRelease >= 0) versions->setCurrentIndex(defaultRelease);
        versions->setEnabled(versions->count() > 0); actions->button(QDialogButtonBox::Ok)->setEnabled(versions->count() > 0 && (loader->currentData()=="vanilla" || builds->count()>0));
        message->hide();if(queryLoader)queryLoader();
    });
    QFutureWatcher<VersionList> loaderWatcher;
    QString queriedGame,queriedLoader;
    queryLoader=[&]{
        loaderState->clear();message->hide();builds->clear();builds->setEnabled(false);
        const bool vanilla=loader->currentData().toString()=="vanilla";
        builds->setVisible(!vanilla);form->labelForField(builds)->setVisible(!vanilla);
        if(vanilla){actions->button(QDialogButtonBox::Ok)->setEnabled(versions->count()>0);return;}
        actions->button(QDialogButtonBox::Ok)->setEnabled(false);
        if(versions->currentText().isEmpty())return;loaderState->loading();
        if(loaderWatcher.isRunning())return;
        queriedGame=versions->currentText();queriedLoader=loader->currentData().toString();
        loaderWatcher.setFuture(QtConcurrent::run([cancelled,game=queriedGame,kind=queriedLoader,source=m_settings.downloadSource]{return kind=="fabric"?fetchFabricVersions(game,cancelled,source):kind=="quilt"?fetchQuiltVersions(game,cancelled,source):fetchForgeVersions(kind,game,cancelled,source);}));
    };
    connect(loaderState,&NetworkNotice::retryRequested,&dialog,queryLoader);
    builds->hide();form->labelForField(builds)->hide();
    connect(loader,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,queryLoader);
    connect(loader,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{const auto next=defaultInstanceName(versions->currentText(),loader->currentData().toString(),existing);if(name->text().isEmpty()||name->text()==automaticName)name->setText(next);automaticName=next;});
    connect(versions,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,queryLoader);
    connect(&loaderWatcher,&QFutureWatcher<VersionList>::finished,&dialog,[&]{
        if(queriedGame!=versions->currentText()||queriedLoader!=loader->currentData().toString()){queryLoader();return;}
        if(loader->currentData().toString()=="vanilla")return;
        loaderState->clear();const auto result=loaderWatcher.result();for(const auto &v:result.versions){const auto item=v.toObject().value("loader").toObject();builds->addItem(item.value("version").toString()+(item.value("stable").toBool()?QString():QCoreApplication::translate("M2CL", " · 测试版")),item.value("version").toString());}
        builds->setEnabled(builds->count()>0);actions->button(QDialogButtonBox::Ok)->setEnabled(builds->count()>0);
        if(!result.error.isEmpty())loaderState->failure(QCoreApplication::translate("M2CL", "%1 构建加载失败").arg(loader->currentText()),result.error);
        else if(!builds->count()){message->setText(QCoreApplication::translate("M2CL", "此游戏版本暂无支持的构建"));message->show();}
    });
    queryManifest();
    const auto accepted = dialog.exec() == QDialog::Accepted; cancelled->store(true);
    watcher.waitForFinished();loaderWatcher.waitForFinished();
    if (!accepted || versions->currentIndex() < 0) return;
    const auto rawRoot = root->text().trimmed();
    if (rawRoot.isEmpty() || !QDir().mkpath(rawRoot)) { QMessageBox::warning(this, QCoreApplication::translate("M2CL", "无法创建实例"), QCoreApplication::translate("M2CL", "请选择可写的游戏根目录")); return; }
    InstallRequest request; request.root = QFileInfo(rawRoot).canonicalFilePath(); request.version = versions->currentText();
    request.versionEntry = versions->currentData().toJsonObject();
    request.loader=loader->currentData().toString();request.loaderVersion=builds->currentData().toString();
    if(request.loader!="vanilla"&&request.loaderVersion.isEmpty())return;
    request.id = request.version + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    request.directory = QDir(request.root).filePath("versions/" + request.id);
    request.player = m_settings.offlinePlayer; request.memoryMiB = m_settings.memoryMiB;
    QString javaError;if(!configureJava(request,request.directory,&javaError)){QMessageBox::warning(this,QCoreApplication::translate("M2CL", "实例配置无法读取"),javaError);return;}
    const auto displayName = name->text().trimmed().isEmpty() ? defaultInstanceName(request.version, request.loader, existing) : name->text().trimmed();
    m_settings.instanceNames.insert(request.directory, displayName); m_settings.isolationOverrides.insert(request.directory, true);
    m_settings.selectedInstance = request.directory;
    if (!m_settings.gameRoots.contains(request.root)) m_settings.gameRoots.append(request.root);
    save(); startInstall(request);
}

void MainWindow::launchInstance() {
    if(m_accounts->busy()){status(QCoreApplication::translate("M2CL", "请先完成当前账号操作"));return;}
    const auto account=m_accounts->current();if(account.id.isEmpty()){showAccounts();return;}
    if (m_installWatcher.isRunning() || m_resources.downloading() || m_javaDownloadActive || m_modInstallActive) { status(QCoreApplication::translate("M2CL", "请先等待或取消当前下载")); return; }
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) { status(QCoreApplication::translate("M2CL", "游戏正在运行")); return; }
    if (!selectedInstance()) { createInstance(); return; }
    const auto instance = *selectedInstance();
    if(!instance.loaderProblem.isEmpty()){QMessageBox::information(this,QCoreApplication::translate("M2CL", "无法启动此实例"),instance.loaderProblem);return;}
    if (instance.loader!="vanilla"&&instance.loader!="fabric"&&instance.loader!="quilt"&&instance.loader!="forge"&&instance.loader!="neoforge") { QMessageBox::information(this, QCoreApplication::translate("M2CL", "暂不支持此加载器"), QCoreApplication::translate("M2CL", "当前支持原版、Fabric、Quilt、Forge 和 NeoForge；此加载器仍待接入。")); return; }
    QString javaError;const auto ownJava=loadInstanceJavaChoice(instance.path,&javaError);
    if(!javaError.isEmpty()){QMessageBox::warning(this,QCoreApplication::translate("M2CL", "实例配置无法读取"),javaError);return;}
    const auto ownLaunch=loadInstanceLaunchOptions(instance.path,m_settings.memoryMiB,&javaError);
    if(!javaError.isEmpty()){QMessageBox::warning(this,QCoreApplication::translate("M2CL", "实例配置无法读取"),javaError);return;}
    InstallPlanDialog dialog((account.microsoft?QCoreApplication::translate("M2CL", "启动游戏 · %1"):QCoreApplication::translate("M2CL", "离线启动 · %1")).arg(instanceName(instance)),this);dialog.setObjectName("launchInstanceDialog");dialog.resize(520,400);
    auto layout = dialog.contentLayout(); auto form = new QFormLayout;
    auto player = new QLineEdit(account.name);player->setReadOnly(account.microsoft); player->setObjectName("offlinePlayer"); player->setMaxLength(16);
    auto memory = new NumberBox; memory->setObjectName("gameMemory"); memory->setRange(512, 32768); memory->setSingleStep(512); memory->setAccessibleName(QCoreApplication::translate("M2CL", "最大内存（MiB）"));memory->setValue(ownLaunch.maxMemoryMiB);
    auto java = new JavaChoiceWidget(ownJava, m_javas, instance.javaMajor);
    auto memoryRow=new QWidget;auto memoryLayout=new QHBoxLayout(memoryRow);memoryLayout->setContentsMargins(0,0,0,0);memoryLayout->addWidget(memory,1);memoryLayout->addWidget(numberLabel("MiB"));auto memoryMode=new QLabel;memoryMode->setProperty("muted",true);memoryLayout->addWidget(memoryMode);
    bool manualMemoryEdit=false,updatingMemory=false;
    auto updateMemoryPreview=[&]{
        if(ownLaunch.autoMemory&&!manualMemoryEdit){const auto choice=java->choice();const auto rec=recommendMemory(systemMemory(),memoryContext(instance.version,instance.gameDirectory,instance.loader,choice.value("mode")=="fixed"?choice.value("arch").toString():QString()));if(rec.valid()){updatingMemory=true;memory->setValue(rec.maximumMiB);updatingMemory=false;}memoryMode->setText(QCoreApplication::translate("M2CL", "自动 · 启动前重算"));memoryMode->setToolTip(rec.valid()?rec.basis:rec.error);}
        else memoryMode->setText(QCoreApplication::translate("M2CL", "手动"));
    };
    connect(memory,QOverload<int>::of(&QSpinBox::valueChanged),&dialog,[&]{if(!updatingMemory){manualMemoryEdit=true;updateMemoryPreview();}});connect(java,&JavaChoiceWidget::changed,&dialog,updateMemoryPreview);updateMemoryPreview();
    form->addRow(account.microsoft?QCoreApplication::translate("M2CL", "游戏角色"):QCoreApplication::translate("M2CL", "离线玩家名"), player); form->addRow(numberLabel(QCoreApplication::translate("M2CL", "最大内存")), memoryRow); form->addRow(QStringLiteral("Java"), java); layout->addLayout(form);
    auto more=button(QCoreApplication::translate("M2CL", "内存、窗口与附加参数…"));layout->addWidget(more,0,Qt::AlignLeft);connect(more,&QPushButton::clicked,&dialog,[this,&dialog,instance]{dialog.reject();QTimer::singleShot(0,this,[this,instance]{editInstance(instance.path,3);});});
    auto exit = new CheckBox(account.microsoft?QCoreApplication::translate("M2CL", "游戏启动后隐藏启动器（保留安全日志）"):QCoreApplication::translate("M2CL", "游戏启动后退出启动器")); exit->setChecked(m_settings.exitAfterLaunch); layout->addWidget(exit);
    auto message = label(account.microsoft?QCoreApplication::translate("M2CL", "启动前会验证当前账号。游戏日志会过滤本次登录凭据，并保存到实例 logs/launcher.log。"):QCoreApplication::translate("M2CL", "玩家名为 1–16 位英文字母、数字或下划线。启动前会检查并补齐文件，游戏输出保存在实例 logs/launcher.log。"), nullptr, true); layout->addWidget(message);
    auto actions = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); actions->button(QDialogButtonBox::Ok)->setText(account.microsoft?QCoreApplication::translate("M2CL", "启动游戏"):QCoreApplication::translate("M2CL", "离线启动"));actions->button(QDialogButtonBox::Ok)->setProperty("primary",true);actions->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("M2CL", "取消")); layout->addWidget(actions);
    connect(actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(actions, &QDialogButtonBox::accepted, &dialog, [&] {
        if (!QRegularExpression("^[A-Za-z0-9_]{1,16}$").match(player->text()).hasMatch()) { message->setText(QCoreApplication::translate("M2CL", "请输入有效玩家名：1–16 位英文字母、数字或下划线")); return; }
        if (java->choice().value("mode") == "fixed" && java->choice().value("path").toString().isEmpty()) { message->setText(QCoreApplication::translate("M2CL", "请选择固定的 Java 构建或本地可执行文件")); return; }
        if(!java->selectionError().isEmpty()){message->setText(java->selectionError());return;}
        auto launch=ownLaunch;if(manualMemoryEdit){launch.autoMemory=false;launch.maxMemoryMiB=memory->value();}QString error;if(!saveInstanceRuntimeSettings(instance.path,java->choice(),launch,&error)){message->setText(error);return;}m_settings.legacyInstanceJava.remove(instance.path);dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return;
    AuthSession session;
    if(account.microsoft){MicrosoftLoginDialog login(*m_accounts,account.clientId,account.id,true,this);connect(&login,&MicrosoftLoginDialog::configurationRequested,this,[this]{showAccounts();m_accountsPanel->showConfiguration();});if(login.exec()!=QDialog::Accepted)return;session=login.session();}
    else {const auto problem=m_accounts->addOffline(player->text());if(!problem.isEmpty()){QMessageBox::warning(this,QCoreApplication::translate("M2CL", "无法使用此角色"),problem);return;}m_settings.offlinePlayer=player->text();session.identity.name=player->text();}
    m_settings.exitAfterLaunch = exit->isChecked(); save();
    InstallRequest request; request.root = instance.root; request.id = instance.id; request.version = instance.version; request.directory = instance.gameDirectory;
    request.identity=session.identity;request.player = session.identity.name; request.memoryMiB = m_settings.memoryMiB;
    if(!account.microsoft){request.offlineSkin=m_accounts->current().offlineSkin;request.launcherDirectory=m_store.directory();}
    if(!configureJava(request,instance.path,&javaError)){QMessageBox::warning(this,QCoreApplication::translate("M2CL", "实例配置无法读取"),javaError);return;}request.launch = true;
    startInstall(request);
}

void MainWindow::showVersionComponents(const QString &path){for(const auto &instance:m_instances)if(instance.path==path){manageVersionComponents(instance);return;}}
void MainWindow::manageVersionComponents(const Instance &instance) {
    if(m_installWatcher.isRunning()||m_modInstallActive||m_javaDownloadActive||m_resources.downloading()||(m_gameProcess&&m_gameProcess->state()!=QProcess::NotRunning)) {
        QMessageBox::information(this,QCoreApplication::translate("M2CL", "暂时无法修改"),QCoreApplication::translate("M2CL", "请先结束游戏或当前安装任务，再修改版本组件。"));return;
    }
    VersionComponentsDialog dialog(instance,instanceName(instance),m_settings.downloadSource,m_settings.coverOverrides.value(instance.path).toString(),this);
    connect(&dialog,&VersionComponentsDialog::downloadSourceChanged,this,[this](const QString &source){
        m_settings.downloadSource=source;if(auto choice=findChild<SelectBox *>("downloadSource")){const QSignalBlocker block(choice);choice->setCurrentIndex(choice->findData(source));}save();
    });
    if(!qApp->property("offlineUiTest").toBool()&&!qApp->arguments().contains("--smoke-test"))dialog.queryBuilds();
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto action=dialog.action();const auto build=dialog.selectedBuild();const auto localFile=dialog.localFile();
    if(action=="install"&&build.isEmpty())return;
    InstallPlanDialog confirmation(QCoreApplication::translate("M2CL", "确认安装方案"),this);confirmation.resize(600,380);auto content=confirmation.contentLayout();
    confirmation.addTargetInstance(instanceName(instance),QStringLiteral("Minecraft %1 · %2 %3").arg(instance.version,instance.loaderName(),instance.loaderVersion),CoverStore::image(instance.version,QSize(1200,300),m_settings.coverOverrides.value(instance.path).toString()),{});
    auto plan=InstallPlanDialog::section(content,action=="remove"?QCoreApplication::translate("M2CL", "将移除"):QCoreApplication::translate("M2CL", "将安装"),action=="remove"?"warning":"add");
    plan->addWidget(label("OptiFine · "+(action=="remove"?instance.optifineVersion:build.value("version").toString())));
    plan->addWidget(label(action=="remove"?QCoreApplication::translate("M2CL", "恢复安装前的版本配置，保留游戏存档与设置。"):QCoreApplication::translate("M2CL", "保留当前主加载器；如有 Iris、Sodium、Oculus 或 OptiFabric，需先停用。"),nullptr,true));
    auto reuse=InstallPlanDialog::section(content,QCoreApplication::translate("M2CL", "将保留"),"reuse");reuse->addWidget(label(QStringLiteral("Minecraft %1 · %2 %3").arg(instance.version,instance.loaderName(),instance.loaderVersion)));
    auto confirm=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);confirm->button(QDialogButtonBox::Ok)->setText(action=="remove"?QCoreApplication::translate("M2CL", "确认移除"):QCoreApplication::translate("M2CL", "确认并安装"));confirm->button(QDialogButtonBox::Ok)->setProperty("primary",true);confirm->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("M2CL", "取消"));content->addWidget(confirm);
    connect(confirm,&QDialogButtonBox::accepted,&confirmation,&QDialog::accept);connect(confirm,&QDialogButtonBox::rejected,&confirmation,&QDialog::reject);if(confirmation.exec()!=QDialog::Accepted)return;
    InstallRequest request;request.root=instance.root;request.id=instance.id;request.version=instance.version;request.directory=instance.gameDirectory;request.player=m_settings.offlinePlayer.isEmpty()?QString("Player"):m_settings.offlinePlayer;
    request.optifineAction=action;request.optifineBuild=build;request.optifineLocalFile=localFile;QString error;
    if(!configureJava(request,instance.path,&error)){QMessageBox::warning(this,QCoreApplication::translate("M2CL", "实例配置无法读取"),error);return;}startInstall(request);
}

void MainWindow::startInstall(const InstallRequest &request) {
    m_lastDownloadJava = false; m_launchAfterInstall = request.launch; m_installCancelled = std::make_shared<std::atomic_bool>(false);
    m_downloadInProgress = true; m_activeDownloadName = request.version + (request.loader.isEmpty() || request.loader=="vanilla" ? QString() : " · " + request.loader); m_activeDownloadMessage = QCoreApplication::translate("M2CL", "准备安装…"); m_activeDownloadProgress = -1;
    m_heroLaunch->setEnabled(false); m_javaManager->setGameTaskBusy(true);
    const auto flag = m_installCancelled;
    auto effectiveRequest=request;effectiveRequest.downloadSource=m_settings.downloadSource;
    m_installWatcher.setFuture(QtConcurrent::run([this, request=effectiveRequest, flag] {
        return prepareGame(request, flag, [this](const QString &message, int progress) {
            QMetaObject::invokeMethod(this, [this, message, progress] { m_activeDownloadMessage = message; m_activeDownloadProgress = progress; updateDownloadPanel(); status(message); }, Qt::QueuedConnection);
        });
    }));
    updateDownloadPanel(); if (!m_downloadPanel->isVisible()) showDownloads();
}

void MainWindow::runGame(const LaunchCommand &command) {
    const auto logs = QDir(command.directory).filePath("logs");
    if (!QDir().mkpath(logs)) { QMessageBox::warning(this, QCoreApplication::translate("M2CL", "启动失败"), QCoreApplication::translate("M2CL", "无法创建日志目录")); return; }
    const auto logPath = QDir(logs).filePath("launcher.log");
    // No QObject parent: quitting the launcher must not terminate the game.
    auto process = new QProcess; m_gameProcess = process;
    process->setWorkingDirectory(command.directory); process->setProcessChannelMode(QProcess::MergedChannels);
    const bool previousQuit=qApp->quitOnLastWindowClosed();
    GameLogWriter *writer=nullptr;
    if(command.online){
        writer=new GameLogWriter(process);const auto error=writer->open(logPath,command.redactionSecrets);if(!error.isEmpty()){QMessageBox::warning(this,QCoreApplication::translate("M2CL", "无法准备安全日志"),error);process->deleteLater();return;}qApp->setQuitOnLastWindowClosed(false);
    }else process->setStandardOutputFile(logPath, QIODevice::Truncate);
    auto dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("gameLogDialog");dialog->setWindowTitle(QCoreApplication::translate("M2CL", "Minecraft · 游戏日志")); dialog->resize(780, 480);
    auto layout = column(dialog); auto state = label(QCoreApplication::translate("M2CL", "正在启动 Java…")); layout->addWidget(state);
    if(writer)connect(writer,&GameLogWriter::failed,dialog,[state](const QString &error){state->setText(QCoreApplication::translate("M2CL", "游戏日志保存失败：%1").arg(error));});
    auto output = new QPlainTextEdit; output->setReadOnly(true); output->setMaximumBlockCount(2500); layout->addWidget(output, 1);
    auto stop = button(QCoreApplication::translate("M2CL", "停止游戏")); layout->addWidget(stop, 0, Qt::AlignRight);
    auto timer = new QTimer(dialog); auto log = std::make_shared<QFile>(logPath); auto decoder = std::make_shared<GameLogDecoder>();
    connect(timer, &QTimer::timeout, dialog, [log, output, decoder] { if (!log->isOpen() && !log->open(QIODevice::ReadOnly)) return; const auto bytes = log->read(128 * 1024); if (!bytes.isEmpty()) { output->moveCursor(QTextCursor::End); output->insertPlainText(decoder->append(bytes)); } });
    connect(stop, &QPushButton::clicked, process, [process] { process->setProperty("stopRequested", true); process->terminate(); QTimer::singleShot(5000, process, [process] { if (process->state() != QProcess::NotRunning) process->kill(); }); });
    connect(process, &QProcess::started, dialog, [this, state,command] { state->setText(QCoreApplication::translate("M2CL", "游戏进程已启动")+(command.memoryNote.isEmpty()?QString():QStringLiteral(" · ")+command.memoryNote)); status(QCoreApplication::translate("M2CL", "Minecraft 已启动")); if (m_settings.exitAfterLaunch){if(command.online){hide();if(auto logs=findChild<QDialog *>("gameLogDialog"))logs->hide();}else QTimer::singleShot(0,qApp,&QCoreApplication::quit);} });
    connect(process, &QProcess::errorOccurred, dialog, [state, process, stop](QProcess::ProcessError error) { state->setText(QCoreApplication::translate("M2CL", "游戏进程错误：%1").arg(process->errorString())); if (error == QProcess::FailedToStart) stop->setEnabled(false); });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), dialog, [state, stop, process](int code, QProcess::ExitStatus how) { state->setText(process->property("stopRequested").toBool() ? QCoreApplication::translate("M2CL", "游戏已停止") : code == 0 && how == QProcess::NormalExit ? QCoreApplication::translate("M2CL", "游戏已退出") : QCoreApplication::translate("M2CL", "游戏异常退出（%1），请查看下方日志及实例 crash-reports").arg(code)); stop->setEnabled(false); });
    connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) { if (error == QProcess::FailedToStart) process->deleteLater(); });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process, &QObject::deleteLater);
    if(command.online){
        const QPointer<QDialog> logWindow=dialog;
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,logWindow,previousQuit](int,QProcess::ExitStatus){qApp->setQuitOnLastWindowClosed(previousQuit);if(!isVisible()&&(!logWindow||!logWindow->isVisible()))QTimer::singleShot(0,qApp,&QCoreApplication::quit);});
        connect(process,&QProcess::errorOccurred,this,[previousQuit](QProcess::ProcessError error){if(error==QProcess::FailedToStart)qApp->setQuitOnLastWindowClosed(previousQuit);});
    }
    dialog->show(); process->start(command.executable, command.arguments); timer->start(250);
}

bool MainWindow::configureJava(InstallRequest &request, const QString &path, QString *error) const {
    QString problem;request.javaChoice = loadInstanceJavaChoice(path,&problem);
    if(error)*error=problem;if(!problem.isEmpty())return false;
    request.launchOptions=loadInstanceLaunchOptions(path,m_settings.memoryMiB,&problem);request.memoryMiB=request.launchOptions.maxMemoryMiB;
    if(error)*error=problem;if(!problem.isEmpty())return false;
    if(m_settings.legacyInstanceJava.contains(path)){if(error)*error=QCoreApplication::translate("M2CL", "此实例的旧 Java 设置尚未成功迁移，请检查实例配置目录权限。");return false;}
    request.java = request.javaChoice.value("mode") == "fixed" ? request.javaChoice.value("path").toString() : QString();
    request.javaRoots = m_settings.javaFolders; request.javaRoots.append(QDir(m_store.directory()).filePath("java"));
    for (const auto &j : m_javas) request.javaRoots.append(j.executable);
    return true;
}
void MainWindow::invalidateModResults() {
    closeModDetails();
    m_resources.invalidateSearch(); m_projects.clear(); m_modResults->clear();
    m_modSearchPending = false; m_modSearchSpinner->hide(); m_modSearchRetry->hide();
    m_modSearchState->show(); m_modSearchMessage->setText(QCoreApplication::translate("M2CL", "点击搜索，查看符合筛选条件的资源")); m_modSearchMessage->show();
    m_modPrevious->setEnabled(false); m_modNext->setEnabled(false);
    m_modOffset = 0; m_modTotal = 0; m_modPageLabel->clear();
}
void MainWindow::syncModFilters() {
    const auto instance = modTarget(); if (!instance) return;
    {
        const QSignalBlocker version(m_modVersion), loader(m_modLoader);
        auto i = m_modVersion->findData(instance->version);
        if (i < 0) { m_modVersion->addItem(instance->version, instance->version); i = m_modVersion->count()-1; m_modVersion->setItemData(i,"release",SelectBox::PopupGroupRole); }
        m_modVersion->setCurrentIndex(i);
        const auto id = instance->supportsMods() ? instance->loader : QString();
        auto j = m_modLoader->findData(id);
        if (j < 0) { m_modLoader->addItem(resourceTagLabel(id),id); j=m_modLoader->count()-1; }
        m_modLoader->setCurrentIndex(j);
    }
    invalidateModResults();
}
void MainWindow::setMarketType(const QString &type){
    if(type!=m_marketType){m_resources.invalidateSearch();m_modSearchPending=false;}
    if(type=="mod"){m_resourceReturnPath.clear();if(m_marketReturn)m_marketReturn->hide();}
    m_marketType=type=="shader"||type=="resourcepack"?type:QString("mod");m_resources.setResourceType(m_marketType);
    const QSignalBlocker block(m_marketTypeChoice);m_marketTypeChoice->setCurrentIndex(m_marketTypeChoice->findData(m_marketType));
    m_marketTitle->setText(m_marketType=="shader"?QCoreApplication::translate("M2CL", "光影市场"):m_marketType=="resourcepack"?QCoreApplication::translate("M2CL", "材质包市场"):QCoreApplication::translate("M2CL", "模组市场"));
    m_modQuery->setPlaceholderText(m_marketType=="mod"?QCoreApplication::translate("M2CL", "搜索模组…"):m_marketType=="shader"?QCoreApplication::translate("M2CL", "搜索光影，例如 Complementary、BSL…"):QCoreApplication::translate("M2CL", "搜索材质包…"));
    m_modFilterFields.at(1)->setVisible(m_marketType=="mod");updateModBrowseLayout(false);m_modLoader->setEnabled(m_marketType=="mod");if(m_marketType!="mod")m_modLoader->setCurrentIndex(0);
    {const QSignalBlocker categories(m_modCategories);m_modCategories->clear();auto all=new QListWidgetItem(QCoreApplication::translate("M2CL", "全部分类"),m_modCategories);all->setData(Qt::UserRole,QString());m_modCategories->setCurrentRow(0);}
    if(!qApp->property("offlineUiTest").toBool()&&!qApp->arguments().contains("--smoke-test"))m_resources.tags();
}
void MainWindow::browseResourcesForInstance(const QString &path,const QString &type){
    m_resourceReturnPath=path;m_marketReturn->setText(QCoreApplication::translate("M2CL", "← 返回实例"));m_marketReturn->show();m_settings.modInstallTarget=path;setMarketType(type);m_modQuery->clear();
    for(const auto &instance:m_instances)if(instance.path==path){int index=m_modVersion->findText(instance.version);if(index<0){m_modVersion->addItem(instance.version,instance.version);index=m_modVersion->count()-1;}m_modVersion->setCurrentIndex(index);}
    closeModDetails();m_modViews->setCurrentIndex(0);changePage(2);searchMods();
}
void MainWindow::searchMods() { searchModPage(0); }
void MainWindow::searchModPage(int offset) {
    if(auto notice=m_modSearchState->findChild<NetworkNotice *>())notice->clear();
    closeModDetails(); m_modViews->setCurrentIndex(0);
    m_modSearchPending = true; m_projects.clear(); m_modResults->clear();
    m_modSearchState->show(); m_modSearchSpinner->show(); m_modSearchMessage->hide(); m_modSearchRetry->hide();
    m_modPrevious->setEnabled(false); m_modNext->setEnabled(false); m_modPageLabel->clear();
    const auto version = m_modVersion->currentIndex()==0 && m_modVersion->currentText()==m_modVersion->itemText(0) ? QString() : m_modVersion->currentText().trimmed();
    const auto category = m_modCategories->currentItem() ? m_modCategories->currentItem()->data(Qt::UserRole).toString() : QString();
    if (!qApp->property("offlineUiTest").toBool() && !qApp->arguments().contains("--smoke-test"))
        m_resources.search(m_modQuery->text().trimmed(),version,m_marketType=="mod"?m_modLoader->currentData().toString():QString(),category,m_modSort->currentData().toString(),offset);
}
void MainWindow::chooseModVersion() {
    const int row = m_modResults->currentRow(); if (row < 0 || row >= m_projects.size()) return;
    if (m_resources.downloading() || m_installWatcher.isRunning() || m_javaDownloadActive || m_modInstallActive) { status(QCoreApplication::translate("M2CL", "请等待当前下载完成，或先取消下载")); return; }
    m_versionTarget = m_settings.modInstallTarget;
    m_versionProject = m_projects.at(row); m_versionProjectName = m_versionProject.name;
    m_modVersionPending = true;m_modProjectPending=true;m_versionProjectError.clear();
    auto loading = new QWidget; auto body = column(loading,0);
    auto back = button(QCoreApplication::translate("M2CL", "← 返回模组列表")); back->setObjectName("backToMods"); body->addWidget(back,0,Qt::AlignLeft);
    connect(back,&QPushButton::clicked,this,&MainWindow::animateCloseModDetails);
    body->addStretch();
    auto spinner = new BusySpinner; spinner->setObjectName("modVersionSpinner"); body->addWidget(spinner,0,Qt::AlignHCenter);
    auto notice=new NetworkNotice;notice->setObjectName("modVersionNetworkState");body->addWidget(notice);connect(notice,&NetworkNotice::retryRequested,this,&MainWindow::chooseModVersion);body->addStretch();
    setModDetails(loading);
    if (!qApp->property("offlineUiTest").toBool() && !qApp->arguments().contains("--smoke-test")) {
        m_resources.versions(m_versionProject.id,{},{});
        m_resources.project(m_versionProject.id);
    }
}
void MainWindow::showModVersions(const QList<ResourceFile> &files) {
    if (m_pages->currentIndex()!=2 || !m_modVersionPending) return;
    m_modVersionPending=false;
    auto view = new ModVersionView(m_versionProjectName,{},nullptr,files);
    view->setProperty("resourceType",m_marketType);view->setDownloadFilter(m_modVersion->currentData().toString(),m_marketType=="mod"?m_modLoader->currentData().toString():QString());
    if(m_marketType!="mod"){view->findChild<QPushButton *>("backToMods")->setText(QCoreApplication::translate("M2CL", "← 返回资源列表"));if(auto loaders=view->findChild<QWidget *>("modFileLoaders"))loaders->hide();}
    view->setSupportedVersionMetadata(m_modTags);view->setProjectInfo(m_versionProject);
    if(!m_versionProjectError.isEmpty())view->setProjectError(m_versionProjectError);else if(m_modProjectPending)view->setProjectLoading();
    setModDetails(view);
    m_resources.icon(m_versionProject.iconUrl);
    connect(view,&ModVersionView::backRequested,this,&MainWindow::animateCloseModDetails);
    connect(view,&ModVersionView::retryProjectRequested,this,[this]{m_modProjectPending=true;m_versionProjectError.clear();m_resources.project(m_versionProject.id);});
    connect(view,&ModVersionView::downloadRequested,this,[this,files](int i){if(i>=0 && i<files.size())chooseModInstallTarget(files.at(i));});
}
void MainWindow::updateModBrowseLayout(bool) {
    if (!m_modFilters || m_modFilterFields.isEmpty()) return;
    auto grid = qobject_cast<QGridLayout *>(m_modFilters->layout());
    const int columns = width() >= 1100 ? 4 : 2;
    int position=0;for (int i=0;i<m_modFilterFields.size();++i) { grid->removeWidget(m_modFilterFields.at(i));if(i==1&&m_marketType!="mod")continue;grid->addWidget(m_modFilterFields.at(i),position/columns,position%columns);++position; }
    m_modCategories->setFixedWidth(currentLanguage()=="en_US"?(width()<960?164:196):(width()<960?136:160));
}
void MainWindow::setModDetails(QWidget *view) {
    auto layout=m_modDetails->layout();
    while(auto item=layout->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
    view->setMinimumWidth(0); view->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored); layout->addWidget(view);
    m_modViews->setCurrentWidget(m_modDetails);
    auto effect = new QGraphicsOpacityEffect(view); view->setGraphicsEffect(effect);
    auto fade = new QPropertyAnimation(effect,"opacity",view); fade->setDuration(140); fade->setStartValue(0.); fade->setEndValue(1.);
    connect(fade,&QPropertyAnimation::finished,view,[view]{view->setGraphicsEffect(nullptr);});fade->start(QAbstractAnimation::DeleteWhenStopped);
}
void MainWindow::animateCloseModDetails() {
    m_resources.invalidateVersions(); m_modVersionPending=false;
    auto item=m_modDetails->layout()->itemAt(0); if(!item || !item->widget()){closeModDetails();return;}
    QPointer<QWidget> view=item->widget(); view->setEnabled(false);
    auto effect=new QGraphicsOpacityEffect(view);view->setGraphicsEffect(effect);
    auto fade=new QPropertyAnimation(effect,"opacity",view);fade->setDuration(120);fade->setStartValue(1.);fade->setEndValue(0.);
    connect(fade,&QPropertyAnimation::finished,this,[this,view]{auto current=m_modDetails->layout()->itemAt(0);if(view && current && current->widget()==view)closeModDetails();});
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}
void MainWindow::closeModDetails() {
    if(!m_modDetails)return;
    m_resources.invalidateVersions();m_modVersionPending=false;
    if(m_modViews->currentWidget()==m_modDetails)m_modViews->setCurrentIndex(0);
    auto layout=m_modDetails->layout();while(auto item=layout->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
}
void MainWindow::chooseModInstallTarget(const ResourceFile &file) {
    if(m_modInstallActive || m_resources.downloading() || m_installWatcher.isRunning() || m_javaDownloadActive){status(QCoreApplication::translate("M2CL", "请先等待当前任务完成"));return;}
    if(m_marketType!="mod"&&!m_resourceReturnPath.isEmpty()){for(const auto &instance:m_instances)if(instance.path==m_resourceReturnPath&&file.gameVersions.contains(instance.version)){downloadModFile(file,instance.path);return;}}
    QDialog dialog(this); dialog.setObjectName("modInstallTargetDialog");dialog.setWindowTitle(QCoreApplication::translate("M2CL", "安装到哪个实例"));dialog.resize(500,380);
    auto body=column(&dialog);body->addWidget(label(QCoreApplication::translate("M2CL", "安装到哪个实例"),"sectionTitle"));
    body->addWidget(label(file.versionName+QCoreApplication::translate("M2CL", "\n只列出游戏版本和加载器匹配的实例。"),nullptr,true));
    auto targets=new QListWidget;targets->setObjectName("compatibleModInstances");body->addWidget(targets,1);
    for(const auto &instance:m_instances)if(m_marketType=="mod"?ModrinthProvider::compatible(file,instance.version,instance.loader):file.gameVersions.contains(instance.version)){
        auto item=new QListWidgetItem(targets);
        const auto name=instanceName(instance), environment=QStringLiteral("Minecraft %1 · %2").arg(instance.version,instance.loaderName());
        item->setData(Qt::UserRole,instance.path);item->setData(Qt::AccessibleTextRole,name+" · "+environment);item->setToolTip(instance.gameDirectory);item->setSizeHint(QSize(0,72));
        auto entry=new QWidget;entry->setAttribute(Qt::WA_TransparentForMouseEvents);auto row=new QHBoxLayout(entry);row->setContentsMargins(14,8,14,8);row->setSpacing(12);
        auto icon=new QLabel;icon->setPixmap(lineIcon(Glyph::Grass).pixmap(32,32));row->addWidget(icon);
        auto lines=new QVBoxLayout;lines->setSpacing(4);auto title=label(name);title->setTextFormat(Qt::PlainText);title->setProperty("fieldLabel",true);lines->addWidget(title);lines->addWidget(label(environment,nullptr,true));row->addLayout(lines,1);targets->setItemWidget(item,entry);
        if(instance.path==m_versionTarget || instance.path==m_settings.modInstallTarget)targets->setCurrentItem(item);
    }
    if(targets->currentRow()<0 && targets->count())targets->setCurrentRow(0);
    auto empty=label(QCoreApplication::translate("M2CL", "没有匹配的实例。请先在游戏库中准备对应版本与加载器的游戏实例。"),nullptr,true);empty->setVisible(!targets->count());body->addWidget(empty);
    auto actions=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);actions->button(QDialogButtonBox::Ok)->setText(QCoreApplication::translate("M2CL", "安装"));actions->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("M2CL", "取消"));actions->button(QDialogButtonBox::Ok)->setProperty("primary",true);actions->button(QDialogButtonBox::Ok)->setEnabled(targets->count()>0);body->addWidget(actions);
    connect(actions,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(actions,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted || !targets->currentItem())return;
    const auto target=targets->currentItem()->data(Qt::UserRole).toString();
    m_settings.modInstallTarget=target;save();downloadModFile(file,target);
}
void MainWindow::downloadModFile(const ResourceFile &file,const QString &targetPath) {
    if(m_marketType!="mod"){
        if(m_gameProcess&&m_gameProcess->state()!=QProcess::NotRunning){status(QCoreApplication::translate("M2CL", "请先关闭游戏"));return;}
        for(const auto &instance:m_instances)if(instance.path==targetPath&&file.gameVersions.contains(instance.version)){
            InstancePacksView installer(instance,m_marketType=="shader"?PackKind::Shader:PackKind::Resource,this);
            installer.installResource(m_versionProject,file);return;
        }
        status(QCoreApplication::translate("M2CL", "资源与实例版本不匹配"));return;
    }
    const Instance *current=nullptr;for(const auto &instance:m_instances)if(instance.path==targetPath)current=&instance;
    if(!current || !ModrinthProvider::compatible(file,current->version,current->loader)){status(QCoreApplication::translate("M2CL", "文件与安装目标不兼容，请重新选择。"));return;}
    if(m_modInstallActive || m_resources.downloading() || m_installWatcher.isRunning() || m_javaDownloadActive){status(QCoreApplication::translate("M2CL", "请先等待当前任务完成"));return;}
    if(m_gameProcess && m_gameProcess->state()!=QProcess::NotRunning){status(QCoreApplication::translate("M2CL", "请先关闭正在运行的游戏"));return;}
    m_modInstallActive=true;m_modCancel=std::make_shared<std::atomic_bool>(false);m_lastDownloadJava=false;m_downloadInProgress=true;m_activeDownloadName=QCoreApplication::translate("M2CL", "安装模组 → %1").arg(instanceName(*current));m_activeDownloadMessage=QCoreApplication::translate("M2CL", "正在解析兼容版本与必需依赖…");m_activeDownloadProgress=-1;m_javaManager->setGameTaskBusy(true);updateDownloadPanel();
    const auto target=*current;const auto flag=m_modCancel;auto root=file;m_modPlanWatcher.setFuture(QtConcurrent::run([target,root,flag]{return planModInstall(target,root,flag);}));
}
void MainWindow::commitModPlan() {
    const auto plan=m_pendingModPlan;const auto flag=m_modCancel;
    m_modApplyWatcher.setFuture(QtConcurrent::run([this,plan,flag]{return applyModPlan(plan,flag,[this](const QString &message,int progress){QMetaObject::invokeMethod(this,[this,message,progress]{m_activeDownloadMessage=message;m_activeDownloadProgress=progress;updateDownloadPanel();},Qt::QueuedConnection);});}));
}
const Instance *MainWindow::modTarget() const {for(const auto &instance:m_instances)if(instance.path==m_settings.modInstallTarget)return &instance;return nullptr;}
void MainWindow::addModsMenu(const QString &path,const QPoint &position) {
    RoundedMenu menu;menu.addAction(QCoreApplication::translate("M2CL", "添加本地模组"),this,[this,path]{importMods(path);});menu.addAction(QCoreApplication::translate("M2CL", "浏览模组市场"),this,[this,path]{browseModsForInstance(path);});menu.addSeparator();menu.addAction(QCoreApplication::translate("M2CL", "管理已安装模组"),this,[this,path]{showInstanceMods(path);});menu.exec(position);
}
void MainWindow::browseModsForInstance(const QString &path) {
    m_resourceReturnPath.clear();m_marketReturn->hide();setMarketType("mod");
    m_settings.modInstallTarget=path;save();closeModDetails();m_modViews->setCurrentIndex(0);syncModFilters();changePage(2);if(!m_modSearchPending)searchMods();
}
void MainWindow::importMods(const QString &path) {
    const Instance *found=nullptr;for(const auto &instance:m_instances)if(instance.path==path)found=&instance;if(!found)return;const auto instance=*found;
    if(!instance.supportsMods()){QMessageBox::information(this,QCoreApplication::translate("M2CL", "此实例需要加载器"),QCoreApplication::translate("M2CL", "原版实例不能加载这些模组。请先配置 Fabric、Forge 等加载器实例。"));return;}
    if(m_modInstallActive || (m_gameProcess&&m_gameProcess->state()!=QProcess::NotRunning)){status(QCoreApplication::translate("M2CL", "请先等待安装完成或关闭游戏"));return;}
    auto files=QFileDialog::getOpenFileNames(this,QCoreApplication::translate("M2CL", "添加到 %1").arg(instanceName(instance)),{},QCoreApplication::translate("M2CL", "Minecraft 模组 (*.jar)"));if(files.isEmpty())return;
    auto error=importLocalMods(instance,files);if(!error.isEmpty()){QMessageBox::information(this,QCoreApplication::translate("M2CL", "未导入模组"),error);return;}showInstanceMods(path);
}
void MainWindow::showInstanceMods(const QString &path) {
    const Instance *found=nullptr;for(const auto &instance:m_instances)if(instance.path==path)found=&instance;if(!found)return;const int previousPage=m_pages->currentIndex();closeModDetails();
    for(int i=m_modViews->count()-1;i>1;--i){auto old=m_modViews->widget(i);m_modViews->removeWidget(old);old->deleteLater();}
    auto view=new InstanceModsView(*found,instanceName(*found));
    connect(view,&InstanceModsView::resourceMarketRequested,this,&MainWindow::browseResourcesForInstance);
    connect(view,&InstanceModsView::versionComponentsRequested,this,&MainWindow::showVersionComponents);
    connect(view,&InstanceModsView::settingsRequested,this,[this](const QString &path){editInstance(path);});
    view->findChild<QPushButton *>("backToMarket")->setText(QCoreApplication::translate("M2CL", "‹ 返回%1").arg(QStringList{QCoreApplication::translate("M2CL", "开始"),QCoreApplication::translate("M2CL", "游戏库"),QCoreApplication::translate("M2CL", "模组市场"),QCoreApplication::translate("M2CL", "设置")}.value(previousPage)));
    view->setMutationGuard([this]{return !m_modInstallActive&&!m_installWatcher.isRunning()&&(!m_gameProcess||m_gameProcess->state()==QProcess::NotRunning);});m_modViews->addWidget(view);m_modViews->setCurrentWidget(view);changePage(2);connect(view,&InstanceModsView::importRequested,this,&MainWindow::importMods);connect(view,&InstanceModsView::marketRequested,this,&MainWindow::browseModsForInstance);connect(view,&InstanceModsView::findCompatibleRequested,this,[this](const QString &path,const QString &query){m_modQuery->setText(query);m_resources.invalidateSearch();m_modSearchPending=false;browseModsForInstance(path);});connect(view,&InstanceModsView::backRequested,this,[this,previousPage]{m_modViews->setCurrentIndex(0);changePage(previousPage);});
}
void MainWindow::setAppearance(const QString &appearance, const QString &theme) {
    if (QStringList{"system", "light", "dark"}.contains(appearance)) m_settings.appearance = appearance;
    if (QStringList{"forest", "blue", "sand", "violet"}.contains(theme)) m_settings.theme = theme;
    const QSignalBlocker color(m_theme);
    m_theme->setCurrentIndex(m_theme->findData(m_settings.theme));
    updateAppearance(); save();
}
void MainWindow::updateAppearance() {
    bool dark = m_settings.appearance == "dark";
    if (m_settings.appearance == "system") {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        dark = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
        dark = m_systemPalette.color(QPalette::Window).lightness() < 128;
#endif
    }
    const auto theme=makeTheme(m_settings.theme, dark);applyTheme(theme);
    m_heroLaunch->setIcon(lineIcon(Glyph::Play,theme.onAccent));
    for(auto choice:m_appearanceChoices){choice->setTheme(m_settings.theme);choice->setChecked(choice->mode()==m_settings.appearance);}
}
void MainWindow::writeDiagnostics(const QString &path) const {
    QJsonArray instances;
    for (const auto &instance : m_instances) instances.append(QJsonObject{{"id", instance.id}, {"version", instance.version}, {"loader", instance.loader}, {"javaMajor", instance.javaMajor}, {"path", instance.path}, {"gameDirectory", instance.gameDirectory}});
    QJsonObject document{{"devicePixelRatio", devicePixelRatioF()}, {"windowWidth", width()}, {"windowHeight", height()}, {"qtVersion", qVersion()}, {"architecture", QSysInfo::currentCpuArchitecture()}, {"appearance", m_settings.appearance}, {"theme", m_settings.theme}, {"instances", instances}, {"ready", ready()}};
    QJsonArray javaWidgets;
    for (const auto &name : {"javaManager", "javaTabs", "javaBuildList", "installJavaBuild", "localJavaList"}) {
        if (auto widget = findChild<QWidget *>(name)) { const auto pos = widget->mapTo(const_cast<MainWindow *>(this), QPoint()); javaWidgets.append(QJsonObject{{"name", name}, {"x", pos.x()}, {"y", pos.y()}, {"width", widget->width()}, {"height", widget->height()}, {"minimumHeight", widget->minimumSizeHint().height()}, {"hintHeight", widget->sizeHint().height()}}); }
    }
    document.insert("javaWidgets", javaWidgets);
    QJsonArray fonts;
    QWidget *fontRoot = const_cast<MainWindow *>(this);
    if (auto dialog = findChild<QDialog *>("instanceSettings"); dialog && dialog->isVisible()) fontRoot = dialog;
    for (const auto &name : {"javaSelectionLabel", "javaRuntimeChoice", "queryJavaBuilds", "javaPackageStatus"}) {
        if (auto widget = fontRoot->findChild<QWidget *>(name)) { const QFontInfo info(widget->font()); fonts.append(QJsonObject{{"widget", name}, {"family", info.family()}, {"pixelSize", info.pixelSize()}, {"weight", int(widget->font().weight())}}); }
    }
    document.insert("fonts", fonts);

    QSaveFile file(path); if (file.open(QIODevice::WriteOnly)) { file.write(QJsonDocument(document).toJson()); file.commit(); }
}
}
