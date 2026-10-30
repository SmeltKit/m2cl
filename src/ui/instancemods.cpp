// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include <QLocale>
#include "core/i18n.h"
#include "network/networkerror.h"
#include "networknotice.h"
#include "core/compatibility.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "instancemods.h"
#include "instancepacks.h"
#include <QTabWidget>
#include "core/resources.h"
#include "launcherwidgets.h"
#include "network/resourceclient.h"
#include <QApplication>
#include <QBuffer>
#include <QDesktopServices>
#include <QDir>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QEvent>
#include <QScrollBar>
#include <QImageReader>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

namespace mc {
namespace {
class InstanceCoverHeader final:public QWidget {
 QPixmap cover;
public:
 explicit InstanceCoverHeader(const QString &version):cover(CoverStore::image(version,QSize(1100,260),{})){setMinimumHeight(128);}
protected:
 void paintEvent(QPaintEvent *)override{
  QPainter p(this);p.setRenderHint(QPainter::SmoothPixmapTransform);auto base=palette().color(QPalette::Window);
  if(!cover.isNull()){auto scaled=cover.scaled(size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);p.drawPixmap(rect(),scaled,QRect((scaled.width()-width())/2,(scaled.height()-height())/2,width(),height()));}
  QLinearGradient fade(0,0,width(),0);fade.setColorAt(0,base);fade.setColorAt(.38,base);auto clear=base;clear.setAlpha(0);fade.setColorAt(.88,clear);p.fillRect(rect(),fade);
  QLinearGradient bottom(0,height()*.6,0,height());bottom.setColorAt(0,clear);bottom.setColorAt(1,base);p.fillRect(rect(),bottom);
 }
};
constexpr int IdRole=Qt::UserRole+1;
constexpr int DisabledRole=Qt::UserRole+2;
constexpr int WarningRole=Qt::UserRole+3;
QImage readIcon(const QByteArray &bytes){
    if(bytes.isEmpty())return {};
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);const auto size=reader.size();
    if(!size.isValid()||size.width()>4096||size.height()>4096)return {};
    reader.setScaledSize(size.scaled(96,96,Qt::KeepAspectRatio));return reader.read();
}
// Preserve semantic colours even when the row is selected.
class ModRowDelegate final:public TableItemDelegate {
public:
    using TableItemDelegate::TableItemDelegate;
    void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        if(index.column()!=0&&index.column()!=4){TableItemDelegate::paint(p,option,index);return;}
        QStyleOptionViewItem base(option);initStyleOption(&base,index);
        const auto text=base.text;const auto icon=base.icon;base.text.clear();base.icon={};
        const auto widget=base.widget;const auto style=widget?widget->style():QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem,&base,p,widget);
        p->save();const auto area=option.rect.adjusted(RoundedTable::CellInset,0,-RoundedTable::CellInset,0);
        const auto muted=qApp->palette().color(QPalette::Disabled,QPalette::Text);
        if(index.column()==0){
            icon.paint(p,QRect(area.left(),area.center().y()-20,40,40),Qt::AlignCenter,QIcon::Normal);
            const auto textRect=area.adjusted(52,0,0,0);const auto id=index.data(IdRole).toString();
            QFont title=option.font;title.setPixelSize(15);title.setWeight(QFont::DemiBold);p->setFont(title);
            p->setPen(index.data(DisabledRole).toBool()?muted:option.palette.color(QPalette::Text));
            const int top=area.center().y()-(id.isEmpty()?10:21);
            p->drawText(QRect(textRect.left(),top,textRect.width(),23),Qt::AlignVCenter,QFontMetrics(title).elidedText(text,Qt::ElideRight,textRect.width()));
            if(!id.isEmpty()){
                auto secondary=option.font;secondary.setPixelSize(12);p->setFont(secondary);p->setPen(muted);
                p->drawText(QRect(textRect.left(),top+24,textRect.width(),18),Qt::AlignVCenter,QFontMetrics(secondary).elidedText(id,Qt::ElideRight,textRect.width()));
            }
        }else{
            const bool disabled=index.data(DisabledRole).toBool(),warning=index.data(WarningRole).toBool();
            const QColor colour=disabled?muted:warning?(option.palette.color(QPalette::Base).lightness()<128?QColor("#e4b96e"):QColor("#946116")):option.palette.color(QPalette::Link);
            p->setRenderHint(QPainter::Antialiasing);p->setPen(Qt::NoPen);p->setBrush(colour);p->drawEllipse(QPointF(area.left()+4,area.center().y()),3.5,3.5);
            p->setPen(colour);p->setFont(option.font);const auto rect=area.adjusted(16,0,0,0);
            p->drawText(rect,Qt::AlignVCenter,QFontMetrics(option.font).elidedText(text,Qt::ElideRight,rect.width()));
        }
        p->restore();
    }
};
}

InstanceModsView::InstanceModsView(const Instance &instance,const QString &name,QWidget *parent)
    :QWidget(parent),m_instance(instance){
    setObjectName("instanceModsView");
    auto body=new QVBoxLayout(this);body->setContentsMargins(0,0,0,0);body->setSpacing(14);
    auto back=new QPushButton(QCoreApplication::translate("M2CL", "‹ 返回模组市场"));back->setObjectName("backToMarket");back->setProperty("textLink",true);
    body->addWidget(back,0,Qt::AlignLeft);connect(back,&QPushButton::clicked,this,&InstanceModsView::backRequested);
    auto banner=new InstanceCoverHeader(instance.version);body->addWidget(banner);auto heading=new QHBoxLayout(banner);heading->setContentsMargins(16,18,16,18);auto titles=new QVBoxLayout;titles->setSpacing(5);
    auto title=new QLabel(name);title->setObjectName("pageTitle");title->setStyleSheet("font-size:28px;font-weight:600;");titles->addWidget(title);
    auto subtitle=new QLabel(QStringLiteral("Minecraft %1 · %2 %3").arg(instance.version,instance.loaderName(),instance.loaderVersion).trimmed());subtitle->setProperty("muted",true);titles->addWidget(subtitle);heading->addLayout(titles,1);
    auto add=new QPushButton(QCoreApplication::translate("M2CL", "添加模组"));add->setObjectName("addInstanceMods");add->setIcon(lineIcon(Glyph::Plus));add->setProperty("primary",true);
    auto menu=new RoundedMenu(add);
    connect(menu->addAction(QCoreApplication::translate("M2CL", "添加本地模组…")),&QAction::triggered,this,[this]{emit importRequested(m_instance.path);});
    connect(menu->addAction(QCoreApplication::translate("M2CL", "浏览模组市场")),&QAction::triggered,this,[this]{emit marketRequested(m_instance.path);});
    connect(add,&QPushButton::clicked,this,[add,menu]{menu->popup(add->mapToGlobal(QPoint(0,add->height()+4)));});
    auto components=new QPushButton(QCoreApplication::translate("M2CL", "版本组件"));components->setObjectName("instanceVersionComponents");components->setIcon(lineIcon(Glyph::Mods));heading->addWidget(components);connect(components,&QPushButton::clicked,this,[this]{emit versionComponentsRequested(m_instance.path);});
    auto settings=new QPushButton(QCoreApplication::translate("M2CL", "实例设置"));settings->setIcon(lineIcon(Glyph::Settings));heading->addWidget(settings);connect(settings,&QPushButton::clicked,this,[this]{emit settingsRequested(m_instance.path);});
    auto open=new QPushButton(QCoreApplication::translate("M2CL", "打开目录"));open->setIcon(lineIcon(Glyph::Folder));
    connect(open,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(m_instance.gameDirectory).filePath("mods")));});
    auto tabs=new QTabWidget;tabs->setObjectName("instanceResourceTabs");body->addWidget(tabs,1);auto modsPage=new QWidget;tabs->addTab(modsPage,QCoreApplication::translate("M2CL", "模组"));body=new QVBoxLayout(modsPage);body->setContentsMargins(0,12,0,0);body->setSpacing(14);
    for(auto kind:{PackKind::Shader,PackKind::Resource}){auto packs=new InstancePacksView(instance,kind);packs->browseMarket=[this,kind]{emit resourceMarketRequested(m_instance.path,kind==PackKind::Shader?"shader":"resourcepack");};packs->setMutationGuard([this]{return !m_mutationGuard||m_mutationGuard();});packs->configureSupport=[this]{if(m_instance.loader=="fabric"||m_instance.loader=="neoforge")emit findCompatibleRequested(m_instance.path,QStringLiteral("Iris"));else if(m_instance.loader=="vanilla"||m_instance.loader=="forge")emit versionComponentsRequested(m_instance.path);else QMessageBox::information(this,QCoreApplication::translate("M2CL", "光影支持"),QCoreApplication::translate("M2CL", "请先核对当前游戏版本与光影支持组件的兼容性。"));};tabs->addTab(packs,kind==PackKind::Shader?QCoreApplication::translate("M2CL", "光影"):QCoreApplication::translate("M2CL", "材质包"));connect(tabs,&QTabWidget::currentChanged,packs,[packs,tabs](int){if(tabs->currentWidget()==packs)packs->refresh();});}
    if(instance.loader=="quilt"){
        auto row=new QHBoxLayout;auto info=new QLabel;info->setObjectName("compatibilityListStatus");info->setWordWrap(true);row->addWidget(info,1);
        auto update=new QPushButton(QCoreApplication::translate("M2CL", "更新兼容列表"));update->setObjectName("updateCompatibilityList");row->addWidget(update);
        auto source=new QPushButton(QCoreApplication::translate("M2CL", "官方列表 ↗"));row->addWidget(source);body->addLayout(row);
        connect(source,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://quiltmc.org/en/usage/incompatible-mods/"));});
        auto describe=[=]{auto list=compatibilityList(m_instance);info->setText(list.cached?QCoreApplication::translate("M2CL", "Quilt 兼容列表 · 同步于 %1 · 仅提示已知问题").arg(QLocale().toString(list.fetched.toLocalTime(),QLocale::ShortFormat)):QCoreApplication::translate("M2CL", "Quilt 兼容列表 · 内置快照 2026-10-29 · 未在线同步"));};describe();
        auto syncState=new NetworkNotice;syncState->setObjectName("compatibilityNetworkState");body->addWidget(syncState);
        auto manager=new QNetworkAccessManager(this);
        auto fetch=[=]{if(!update->isEnabled())return;recordCompatibilityAttempt(m_instance);update->setEnabled(false);syncState->loading();
            auto reply=manager->get(QNetworkRequest(QUrl(compatibilitySource())));auto timer=new QTimer(reply);timer->setSingleShot(true);connect(timer,&QTimer::timeout,reply,[reply]{reply->setProperty("networkTimedOut",true);reply->abort();});timer->start(15000);
            connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>256*1024)reply->abort();});
            connect(reply,&QNetworkReply::finished,this,[=]{timer->stop();QString error;
                error=networkReplyError(reply);if(error.isEmpty())saveCompatibilityList(m_instance,reply->readAll(),&error);
                describe();syncState->clear();if(!error.isEmpty())syncState->failure(QCoreApplication::translate("M2CL", "兼容列表同步失败，继续使用已有列表"),error+QCoreApplication::translate("M2CL", "\n24 小时内不自动重试，可手动重试。"));update->setEnabled(true);reply->deleteLater();renderRows();});
        };connect(update,&QPushButton::clicked,this,fetch);connect(syncState,&NetworkNotice::retryRequested,this,fetch);
        if(compatibilityUpdateDue(instance))QTimer::singleShot(0,this,[=]{if(compatibilityUpdateDue(m_instance))fetch();});
    }
    auto tools=new ResourceToolbar(QCoreApplication::translate("M2CL", "模组"));body->addWidget(tools);
    // Reuse the same toolbar as shaders and resource packs.
    tools->add->hide();tools->open->hide();auto actionLayout=qobject_cast<QVBoxLayout *>(tools->layout())->itemAt(0)->layout();actionLayout->replaceWidget(tools->add,add);actionLayout->replaceWidget(tools->open,open);add->setParent(tools);open->setParent(tools);add->setText(QCoreApplication::translate("M2CL", "添加模组  ⌄"));add->setIcon({});add->show();open->show();
    m_count=tools->count;m_search=tools->search;m_search->setObjectName("installedModSearch");m_search->setPlaceholderText(QCoreApplication::translate("M2CL", "搜索名称、Mod ID 或文件名…"));
    m_filter=new SelectBox;m_filter->setObjectName("installedModFilter");m_filter->addItem(QCoreApplication::translate("M2CL", "全部状态"),"all");m_filter->addItem(QCoreApplication::translate("M2CL", "已启用"),"enabled");m_filter->addItem(QCoreApplication::translate("M2CL", "已禁用"),"disabled");m_filter->addItem(QCoreApplication::translate("M2CL", "需要处理"),"issues");tools->filters->insertWidget(1,m_filter);
    auto refreshButton=tools->refresh;
    connect(m_search,&QLineEdit::textChanged,this,&InstanceModsView::filterRows);
    connect(m_filter,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&InstanceModsView::filterRows);
    connect(refreshButton,&QPushButton::clicked,this,&InstanceModsView::refresh);
    auto content=new QFrame;auto contentLayout=new QGridLayout(content);contentLayout->setContentsMargins(0,0,0,0);
    m_table=new RoundedTable(0,7);m_table->setObjectName("installedModsTable");
    m_table->setHorizontalHeaderLabels({QCoreApplication::translate("M2CL", "名称"),QCoreApplication::translate("M2CL", "版本"),QCoreApplication::translate("M2CL", "加载器"),QCoreApplication::translate("M2CL", "启用"),QCoreApplication::translate("M2CL", "状态"),QCoreApplication::translate("M2CL", "操作"),QString()});
    m_table->verticalHeader()->hide();m_table->verticalHeader()->setDefaultSectionSize(76);m_table->setShowGrid(false);m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);m_table->setSelectionBehavior(QAbstractItemView::SelectRows);m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);m_table->setWordWrap(false);m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);m_table->setItemDelegate(new ModRowDelegate(m_table));
    auto header=m_table->horizontalHeader();header->setSectionResizeMode(QHeaderView::Fixed);header->setMinimumSectionSize(40);header->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);header->setFixedHeight(44);
    header->moveSection(header->visualIndex(6),0);header->moveSection(header->visualIndex(4),4);
    static_cast<RoundedTable *>(m_table)->setColumnLayout({{6,48,0,Qt::AlignCenter},{0,240,4},{1,176,2.2},{2,112,1.5},{4,160,2},{3,96,0,Qt::AlignCenter},{5,88,0,Qt::AlignCenter}});
    m_table->viewport()->installEventFilter(this);
    m_selectAll=new CheckBox({},header);m_selectAll->setObjectName("selectAllInstalledMods");m_selectAll->setAccessibleName(QCoreApplication::translate("M2CL", "选择当前筛选结果中的全部模组"));m_selectAll->setToolTip(QCoreApplication::translate("M2CL", "全选当前筛选结果"));
    connect(m_table->horizontalScrollBar(),&QScrollBar::valueChanged,this,[this]{resizeColumns();});
    connect(m_selectAll,&QCheckBox::clicked,this,[this](bool on){const QSignalBlocker block(m_table);for(int row=0;row<m_mods.size();++row)if(!m_table->isRowHidden(row))m_table->selectionModel()->select(m_table->model()->index(row,0),(on?QItemSelectionModel::Select:QItemSelectionModel::Deselect)|QItemSelectionModel::Rows);selectRow();});
    contentLayout->addWidget(m_table,0,0);
    m_spinner=new BusySpinner;m_spinner->setObjectName("installedModsSpinner");contentLayout->addWidget(m_spinner,0,0,Qt::AlignCenter);
    m_empty=new QLabel;m_empty->setObjectName("installedModsEmpty");m_empty->setAlignment(Qt::AlignCenter);m_empty->setProperty("muted",true);m_empty->setWordWrap(true);m_empty->setAttribute(Qt::WA_TransparentForMouseEvents);contentLayout->addWidget(m_empty,0,0,Qt::AlignCenter);m_empty->hide();body->addWidget(content,1);
    m_selection=new QFrame;m_selection->setObjectName("selectedModActions");m_selection->setProperty("card",true);auto selectedBody=new QHBoxLayout(m_selection);selectedBody->setContentsMargins(12,10,12,10);
    m_selectedIcon=new QLabel;m_selectedIcon->setFixedSize(40,40);selectedBody->addWidget(m_selectedIcon);
    auto selectedLabels=new QVBoxLayout;selectedLabels->setSpacing(4);m_selectedTitle=new QLabel;m_selectedTitle->setProperty("fieldLabel",true);selectedLabels->addWidget(m_selectedTitle);m_selectedText=new QLabel;m_selectedText->setWordWrap(true);m_selectedText->setProperty("muted",true);selectedLabels->addWidget(m_selectedText);selectedBody->addLayout(selectedLabels,1);
    m_components=new QPushButton(QCoreApplication::translate("M2CL", "管理版本组件"));m_components->setObjectName("manageSelectedVersionComponent");m_components->setIcon(lineIcon(Glyph::Settings));selectedBody->addWidget(m_components);m_components->hide();connect(m_components,&QPushButton::clicked,this,[this]{emit versionComponentsRequested(m_instance.path);});
    m_find=new QPushButton(QCoreApplication::translate("M2CL", "查找适配版本"));m_find->setObjectName("findCompatibleMod");m_find->setIcon(lineIcon(Glyph::Search));selectedBody->addWidget(m_find);
    m_disable=new QPushButton(QCoreApplication::translate("M2CL", "禁用"));m_disable->setObjectName("disableSelectedMod");m_disable->setIcon(lineIcon(Glyph::Ban));selectedBody->addWidget(m_disable);
    m_remove=new QPushButton(QCoreApplication::translate("M2CL", "移除"));m_remove->setObjectName("removeInstalledMod");m_remove->setIcon(lineIcon(Glyph::Trash,QColor("#c7515c")));m_remove->setProperty("dangerAction",true);selectedBody->addWidget(m_remove);body->addWidget(m_selection);m_selection->hide();
    connect(m_table,&QTableWidget::currentCellChanged,this,[this]{selectRow();});connect(m_table,&QTableWidget::itemSelectionChanged,this,&InstanceModsView::selectRow);
    connect(m_find,&QPushButton::clicked,this,[this]{const auto rows=selectedRows();if(rows.size()==1)emit findCompatibleRequested(m_instance.path,m_mods.at(rows.first()).name);});
    connect(m_disable,&QPushButton::clicked,this,&InstanceModsView::changeSelectedEnabled);
    connect(m_remove,&QPushButton::clicked,this,&InstanceModsView::removeSelected);
    m_status=new QLabel;m_status->setObjectName("installedModsNotice");m_status->setWordWrap(true);m_status->setProperty("secondaryText",true);m_status->hide();body->addWidget(m_status);body->addStretch();
    m_icons=new ResourceClient(this);
    connect(m_icons,&ResourceClient::projectIconReady,this,[this](const QString &id,const QUrl &url){
        QStringList files;for(const auto &mod:m_mods)if(mod.projectId==id&&!m_localIcons.contains(mod.fileName))files.append(mod.fileName);
        if(files.isEmpty())return;m_remoteIcons[url]=files;m_icons->icon(url);
    });
    connect(m_icons,&ResourceClient::iconReady,this,[this](const QUrl &url,const QByteArray &bytes){
        const auto image=readIcon(bytes);if(image.isNull())return;
        for(int row=0;row<m_mods.size();++row)if(m_remoteIcons.value(url).contains(m_mods.at(row).fileName)&&!m_localIcons.contains(m_mods.at(row).fileName))m_table->item(row,0)->setIcon(QPixmap::fromImage(image));
        selectRow();
    });
    connect(&m_scan,&QFutureWatcher<ScanResult>::finished,this,[this]{
        m_spinner->hide();m_table->setEnabled(true);const auto result=m_scan.result();
        if(!result.error.isEmpty()){m_status->setText(QCoreApplication::translate("M2CL", "读取失败：%1，可点击刷新重试。").arg(result.error));m_status->show();m_empty->setText(QCoreApplication::translate("M2CL", "模组列表读取失败\n请点击刷新重试"));m_empty->setVisible(m_mods.isEmpty());return;}
        m_mods=result.mods;m_localIcons=result.icons;m_selection->setEnabled(true);renderRows();
        if(!qApp->property("offlineUiTest").toBool()&&!qApp->arguments().contains("--smoke-test")){
            QStringList projects;for(const auto &mod:m_mods)if(!m_localIcons.contains(mod.fileName)&&!mod.projectId.isEmpty())projects.append(mod.projectId);m_icons->projectIcons(projects);
        }
    });refresh();
}
InstanceModsView::~InstanceModsView(){m_scan.waitForFinished();}
void InstanceModsView::renderRows(){
    m_rendering=true;const QSignalBlocker block(m_table);m_selection->hide();m_problems.clear();m_remoteIcons.clear();m_table->setRowCount(0);m_table->setRowCount(m_mods.size());int enabledCount=0,issues=0;
    for(int row=0;row<m_mods.size();++row){
        const auto &mod=m_mods.at(row);const bool component=isVersionComponentFile(m_instance,mod.fileName);if(mod.enabled)++enabledCount;
        auto problem=modEnvironmentProblem(mod,m_instance);if(problem.isEmpty()&&mod.enabled)problem=modDependenciesProblem(mod,m_mods);
        for(const auto &other:m_mods)if(mod.enabled&&other.enabled&&other.fileName!=mod.fileName){const auto duplicate=duplicateModProblem(mod,other);if(!duplicate.isEmpty())problem=duplicate;}
        auto warnings=modCompatibilityWarnings(mod,m_mods);warnings.append(knownCompatibilityWarnings(m_instance,mod));if(problem.isEmpty()&&!warnings.isEmpty())problem=warnings.join(QCoreApplication::translate("M2CL", "；"));m_problems.append(problem);if(!problem.isEmpty())++issues;
        auto name=new QTableWidgetItem(mod.name);name->setData(IdRole,mod.id);name->setData(DisabledRole,!mod.enabled);name->setIcon(m_localIcons.contains(mod.fileName)?QIcon(QPixmap::fromImage(m_localIcons.value(mod.fileName))):lineIcon(Glyph::Mods));
        QStringList dependents;for(const auto &other:m_mods)for(const auto &id:mod.ids)if(other.requiredIds.contains(id)&&!dependents.contains(other.name))dependents.append(other.name);
        name->setToolTip(mod.fileName+"\nMod ID: "+mod.ids.join(", ")+QCoreApplication::translate("M2CL", "\n来源：")+(mod.projectId.isEmpty()?QCoreApplication::translate("M2CL", "本地文件（未关联网站）"):QStringLiteral("Modrinth"))+(dependents.isEmpty()?QString():QCoreApplication::translate("M2CL", "\n被依赖：%1").arg(dependents.join(QCoreApplication::translate("M2CL", "、")))));m_table->setItem(row,0,name);
        const auto version=mod.version.isEmpty()||mod.version.contains("${")?QCoreApplication::translate("M2CL", "未知"):mod.version;auto versionItem=new QTableWidgetItem(version);versionItem->setToolTip(mod.version);m_table->setItem(row,1,versionItem);
        auto loader=new QTableWidgetItem(mod.loader.isEmpty()?QCoreApplication::translate("M2CL", "未知"):resourceTagLabel(mod.loader));QStringList formats;for(const auto &key:mod.metadata.keys())formats.append(resourceTagLabel(key));loader->setToolTip(formats.size()>1?QCoreApplication::translate("M2CL", "包内格式：%1\n当前采用：%2").arg(formats.join(QCoreApplication::translate("M2CL", "、")),resourceTagLabel(mod.loader)):loader->text());m_table->setItem(row,2,loader);
        auto toggleCell=new QWidget;auto toggleLayout=new QHBoxLayout(toggleCell);toggleLayout->setContentsMargins(0,0,0,0);auto toggle=new ToggleSwitch;toggle->setFixedSize(40,28);toggle->setObjectName("installedModToggle");toggle->setAccessibleName(mod.name+QCoreApplication::translate("M2CL", " 启用"));toggle->setChecked(mod.enabled);if(component){toggle->setEnabled(false);toggle->setToolTip(QCoreApplication::translate("M2CL", "此文件属于版本组件，请通过“管理版本组件”修改"));}toggleLayout->addWidget(toggle,0,Qt::AlignCenter);m_table->setCellWidget(row,3,toggleCell);
        QString state;if(problem.isEmpty())state=mod.enabled?QCoreApplication::translate("M2CL", "已识别"):QCoreApplication::translate("M2CL", "已禁用");else if(diagnosticContains(problem, QT_TRANSLATE_NOOP("M2CL", "加载器")))state=QCoreApplication::translate("M2CL", "加载器不匹配");else if(diagnosticContains(problem, QT_TRANSLATE_NOOP("M2CL", "缺少")))state=QCoreApplication::translate("M2CL", "缺少前置");else if(diagnosticContains(problem, QT_TRANSLATE_NOOP("M2CL", "重复")))state=QCoreApplication::translate("M2CL", "重复模组");else if(diagnosticContains(problem, QT_TRANSLATE_NOOP("M2CL", "版本")))state=QCoreApplication::translate("M2CL", "版本需检查");else state=QCoreApplication::translate("M2CL", "需要检查");
        if(component&&problem.isEmpty())state=QCoreApplication::translate("M2CL", "版本组件");
        auto status=new QTableWidgetItem(state);status->setData(DisabledRole,!mod.enabled&&problem.isEmpty());status->setData(WarningRole,!problem.isEmpty());status->setToolTip(problem.isEmpty()?QCoreApplication::translate("M2CL", "已读取本地元数据；不代表运行时一定兼容。"):problem);m_table->setItem(row,4,status);
        auto moreCell=new QWidget;auto moreLayout=new QHBoxLayout(moreCell);moreLayout->setContentsMargins(0,0,0,0);auto more=new QPushButton;more->setIcon(lineIcon(Glyph::More));more->setProperty("flatAction",true);more->setFixedSize(32,32);more->setAccessibleName(mod.name+QCoreApplication::translate("M2CL", " 更多操作"));moreLayout->addWidget(more,0,Qt::AlignCenter);m_table->setCellWidget(row,5,moreCell);
        connect(more,&QPushButton::clicked,this,[this,row,more]{m_table->clearSelection();m_table->setCurrentCell(row,0);showRowMenu(row,more->mapToGlobal(QPoint(0,more->height())));});connect(toggle,&QCheckBox::toggled,this,[this,row](bool value){changeEnabled(row,value);});
        auto selectCell=new QWidget;auto selectLayout=new QHBoxLayout(selectCell);selectLayout->setContentsMargins(0,0,0,0);auto checked=new CheckBox;checked->setObjectName("installedModSelection");checked->setAccessibleName(QCoreApplication::translate("M2CL", "选择 %1").arg(mod.name));checked->setProperty("fileName",mod.fileName);selectLayout->addWidget(checked,0,Qt::AlignCenter);m_table->setCellWidget(row,6,selectCell);
        connect(checked,&QCheckBox::clicked,this,[this,row](bool on){m_table->selectionModel()->select(m_table->model()->index(row,0),(on?QItemSelectionModel::Select:QItemSelectionModel::Deselect)|QItemSelectionModel::Rows);if(on)m_table->setCurrentCell(row,0,QItemSelectionModel::NoUpdate);selectRow();});
        if(!mod.enabled)for(int column:{1,2})m_table->item(row,column)->setForeground(palette().color(QPalette::Disabled,QPalette::Text));
    }
    m_count->setText(QCoreApplication::translate("M2CL", "%1 个模组 · %2 个已启用").arg(m_mods.size()).arg(enabledCount));m_count->setToolTip(QCoreApplication::translate("M2CL", "%1 项需要检查；来源和兼容性说明可悬停查看。").arg(issues));
    for(int row=0;row<m_mods.size();++row){if(m_restoreChecked.contains(m_mods.at(row).fileName))m_table->selectionModel()->select(m_table->model()->index(row,0),QItemSelectionModel::Select|QItemSelectionModel::Rows);if(m_mods.at(row).fileName==m_restoreSelection)m_table->setCurrentCell(row,0,QItemSelectionModel::NoUpdate);}m_restoreSelection.clear();m_restoreChecked.clear();m_rendering=false;resizeColumns();filterRows();
}
void InstanceModsView::filterRows(){
    const QSignalBlocker block(m_table);const auto query=m_search->text().trimmed();const auto filter=m_filter->currentData().toString();int visible=0;
    for(int row=0;row<m_mods.size();++row){const auto &mod=m_mods.at(row);const bool matches=query.isEmpty()||(mod.name+" "+mod.ids.join(" ")+" "+mod.fileName).contains(query,Qt::CaseInsensitive);const bool state=filter=="all"||(filter=="enabled"&&mod.enabled)||(filter=="disabled"&&!mod.enabled)||(filter=="issues"&&!m_problems.value(row).isEmpty());m_table->setRowHidden(row,!matches||!state);if(matches&&state)++visible;else m_table->selectionModel()->select(m_table->model()->index(row,0),QItemSelectionModel::Deselect|QItemSelectionModel::Rows);}
    m_table->parentWidget()->setFixedHeight(46+qMax(1,qMin(6,visible))*76);
    m_empty->setText(m_mods.isEmpty()?QCoreApplication::translate("M2CL", "还没有模组\n点击“添加模组”开始添加"):QCoreApplication::translate("M2CL", "没有符合条件的模组\n试试其他名称或状态"));m_empty->setVisible(!m_scan.isRunning()&&visible==0);selectRow();
}
QList<int> InstanceModsView::selectedRows() const{
    QList<int> rows;for(const auto &index:m_table->selectionModel()->selectedRows(0))if(index.row()<m_mods.size()&&!m_table->isRowHidden(index.row()))rows.append(index.row());std::sort(rows.begin(),rows.end());return rows;
}
void InstanceModsView::selectRow(){
    if(m_rendering)return;const auto rows=selectedRows();QSet<int> selected(rows.begin(),rows.end());int visible=0,checked=0;
    for(int row=0;row<m_mods.size();++row){if(!m_table->isRowHidden(row)){++visible;if(selected.contains(row))++checked;}if(auto cell=m_table->cellWidget(row,6))if(auto choice=cell->findChild<QCheckBox *>()){const QSignalBlocker block(choice);choice->setChecked(selected.contains(row));}}
    {const QSignalBlocker block(m_selectAll);m_selectAll->setCheckState(!checked?Qt::Unchecked:checked==visible?Qt::Checked:Qt::PartiallyChecked);m_selectAll->setEnabled(visible>0&&m_table->isEnabled());}
    m_selection->setVisible(!rows.isEmpty());if(rows.isEmpty())return;
    const auto &mod=m_mods.at(rows.first());m_selectedTitle->setText(QCoreApplication::translate("M2CL", "已选择 %1 个模组").arg(rows.size()));
    QStringList names;bool enabled=false;for(int row:rows){names.append(m_mods.at(row).name);enabled|=m_mods.at(row).enabled;}
    m_selectedText->setText(rows.size()==1?mod.name+(mod.id.isEmpty()?QString():" ("+mod.id+")")+(m_problems.value(rows.first()).isEmpty()?QString():"\n"+m_problems.value(rows.first())):names.mid(0,2).join(QCoreApplication::translate("M2CL", "、"))+(rows.size()>2?QCoreApplication::translate("M2CL", " 等"):QString()));
    m_selectedIcon->setPixmap(rows.size()==1?m_table->item(rows.first(),0)->icon().pixmap(40,40):lineIcon(Glyph::Mods).pixmap(40,40));
    bool hasComponent=false;for(int row:rows)hasComponent|=isVersionComponentFile(m_instance,m_mods.at(row).fileName);m_components->setVisible(hasComponent);m_disable->setEnabled(!hasComponent);m_remove->setEnabled(!hasComponent);
    m_disable->setText(enabled?QCoreApplication::translate("M2CL", "禁用"):QCoreApplication::translate("M2CL", "启用"));m_disable->setIcon(lineIcon(enabled?Glyph::Ban:Glyph::Check));m_find->setEnabled(rows.size()==1&&!mod.name.isEmpty()&&!hasComponent);m_find->setToolTip(rows.size()>1?QCoreApplication::translate("M2CL", "请单独选择一个模组查找适配版本"):QString());
}
void InstanceModsView::changeEnabled(int row,bool enabled){
    if(row<0||row>=m_mods.size())return;const auto mod=m_mods.at(row);
    const auto error=m_mutationGuard&&!m_mutationGuard()?QCoreApplication::translate("M2CL", "请先等待安装完成或关闭游戏"):setModEnabled(m_instance,mod.fileName,enabled);
    if(!error.isEmpty()){auto toggle=m_table->cellWidget(row,3)->findChild<QCheckBox *>();if(toggle){const QSignalBlocker block(toggle);toggle->setChecked(mod.enabled);}m_status->setText(error);m_status->show();return;}
    reload({{mod.fileName,enabled?mod.fileName.left(mod.fileName.size()-9):mod.fileName+".disabled"}});
}
void InstanceModsView::changeSelectedEnabled(){
    const auto rows=selectedRows();if(rows.isEmpty())return;QStringList files;bool enabled=false;for(int row:rows){files.append(m_mods.at(row).fileName);enabled|=m_mods.at(row).enabled;}
    const bool target=!enabled;const auto error=m_mutationGuard&&!m_mutationGuard()?QCoreApplication::translate("M2CL", "请先等待安装完成或关闭游戏"):setModsEnabled(m_instance,files,target);
    if(!error.isEmpty()){m_status->setText(error);m_status->show();return;}
    QHash<QString,QString> renamed;for(int row:rows){const auto &mod=m_mods.at(row);if(mod.enabled!=target)renamed.insert(mod.fileName,target?mod.fileName.left(mod.fileName.size()-9):mod.fileName+".disabled");}reload(renamed);
}
void InstanceModsView::removeSelected(){
    const auto rows=selectedRows();if(rows.isEmpty())return;QStringList files;QHash<QString,QString> removed;for(int row:rows){files.append(m_mods.at(row).fileName);removed.insert(m_mods.at(row).fileName,{});}
    const auto error=m_mutationGuard&&!m_mutationGuard()?QCoreApplication::translate("M2CL", "请先等待安装完成或关闭游戏"):removeLocalMods(m_instance,files);
    if(!error.isEmpty()){m_status->setText(error);m_status->show();return;}reload(removed);m_status->setText(QCoreApplication::translate("M2CL", "已将 %1 个模组移到系统废纸篓／回收站").arg(files.size()));m_status->show();
}
void InstanceModsView::resizeColumns(){
    if(!m_selectAll)return;
    const auto size=m_selectAll->sizeHint();m_selectAll->setGeometry(m_table->horizontalHeader()->sectionViewportPosition(6)+(m_table->columnWidth(6)-size.width())/2,(m_table->horizontalHeader()->height()-size.height())/2,size.width(),size.height());
}
bool InstanceModsView::eventFilter(QObject *object,QEvent *event){if(object==m_table->viewport()&&event->type()==QEvent::Resize)resizeColumns();return QWidget::eventFilter(object,event);}
void InstanceModsView::showRowMenu(int row,const QPoint &position){
    auto menu=new RoundedMenu(this);menu->setAttribute(Qt::WA_DeleteOnClose);
    const bool component=isVersionComponentFile(m_instance,m_mods.at(row).fileName);
    if(component)connect(menu->addAction(lineIcon(Glyph::Settings),QCoreApplication::translate("M2CL", "管理版本组件")),&QAction::triggered,this,[this]{emit versionComponentsRequested(m_instance.path);});
    if(!component)connect(menu->addAction(lineIcon(Glyph::Search),QCoreApplication::translate("M2CL", "查找适配版本")),&QAction::triggered,this,[this,row]{emit findCompatibleRequested(m_instance.path,m_mods.at(row).name);});
    if(!component)connect(menu->addAction(lineIcon(m_mods.at(row).enabled?Glyph::Ban:Glyph::Check),m_mods.at(row).enabled?QCoreApplication::translate("M2CL", "禁用"):QCoreApplication::translate("M2CL", "启用")),&QAction::triggered,this,[this,row]{changeEnabled(row,!m_mods.at(row).enabled);});
    connect(menu->addAction(lineIcon(Glyph::Folder),QCoreApplication::translate("M2CL", "打开目录")),&QAction::triggered,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(m_instance.gameDirectory).filePath("mods")));});
    if(!m_mods.at(row).projectId.isEmpty())connect(menu->addAction(QCoreApplication::translate("M2CL", "Modrinth 项目主页 ↗")),&QAction::triggered,this,[this,row]{QDesktopServices::openUrl(QUrl("https://modrinth.com/mod/"+m_mods.at(row).projectId));});
    if(!component){menu->addSeparator();connect(menu->addAction(lineIcon(Glyph::Trash),QCoreApplication::translate("M2CL", "移到系统废纸篓／回收站")),&QAction::triggered,this,&InstanceModsView::removeSelected);}menu->popup(position);
}
void InstanceModsView::refresh(){reload();}
void InstanceModsView::updateInstance(const Instance &instance){
    const bool changed=m_instance.optifineVersion!=instance.optifineVersion||m_instance.optifineFile!=instance.optifineFile||m_instance.optifineSha512!=instance.optifineSha512;
    m_instance=instance;if(!changed)return;
    auto tabs=findChild<QTabWidget *>("instanceResourceTabs");for(int i=1;i<tabs->count();++i)if(auto packs=dynamic_cast<InstancePacksView *>(tabs->widget(i)))packs->updateInstance(instance);
    reload();
}
void InstanceModsView::reload(const QHash<QString,QString> &renamed){
    if(m_scan.isRunning())return;m_restoreChecked.clear();for(int row:selectedRows()){const auto file=m_mods.at(row).fileName;const auto next=renamed.value(file,file);if(!next.isEmpty())m_restoreChecked.insert(next);}const int row=m_table->currentRow();m_restoreSelection=row>=0&&row<m_mods.size()?renamed.value(m_mods.at(row).fileName,m_mods.at(row).fileName):QString();
    m_spinner->show();m_empty->hide();m_status->hide();m_table->setEnabled(false);m_selection->setEnabled(false);m_selectAll->setEnabled(false);const auto instance=m_instance;
    m_scan.setFuture(QtConcurrent::run([instance]{ScanResult result;try{result.mods=localMods(instance);for(const auto &mod:result.mods){const auto image=readIcon(localModIcon(QDir(instance.gameDirectory).filePath("mods/"+mod.fileName),mod));if(!image.isNull())result.icons.insert(mod.fileName,image);}}catch(const std::exception &error){result.error=QString::fromUtf8(error.what());}return result;}));
}
}
