// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "networknotice.h"
#include "versioncomponentsdialog.h"
#include "launcherwidgets.h"
#include "network/optifine.h"
#include "network/downloadsource.h"
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QDesktopServices>
#include <QFileDialog>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QGridLayout>
#include <QStyle>
#include <stdexcept>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTabBar>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

namespace mc {
namespace {
QLabel *text(const QString &value,const char *role=nullptr) {
    auto label=new QLabel(value);label->setTextFormat(Qt::PlainText);
    if(role)label->setProperty(role,true);
    return label;
}
QLabel *badge(const QString &value,const char *tone="ok") {
    auto label=text(value);label->setProperty("componentBadge",tone);
    label->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    return label;
}
QString displayVersion(QString version) { return version.replace('_',' '); }
QPixmap componentIcon(const QString &kind,const QPalette &palette) {
    if(kind=="minecraft")return lineIcon(Glyph::Grass).pixmap(32,32);
    // Small, recognisable marks stay crisp at desktop scale, like the shared line icons.
    QPixmap result(64,64);result.fill(Qt::transparent);QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);painter.scale(2,2);
    if(kind=="optifine") {
        auto font=QApplication::font();font.setPixelSize(23);font.setWeight(QFont::Bold);
        painter.setFont(font);painter.setPen(QColor("#df5b22"));painter.drawText(QRect(0,0,32,32),Qt::AlignCenter,"OF");
    }else if(kind=="forge") {
        painter.setPen(Qt::NoPen);painter.setBrush(palette.color(QPalette::Text));
        QPainterPath anvil;anvil.moveTo(2,7);anvil.lineTo(30,7);anvil.lineTo(25,14);anvil.lineTo(20,15);
        anvil.lineTo(20,23);anvil.lineTo(25,25);anvil.lineTo(25,28);anvil.lineTo(8,28);anvil.lineTo(8,25);
        anvil.lineTo(13,23);anvil.lineTo(13,15);anvil.lineTo(8,14);anvil.closeSubpath();painter.drawPath(anvil);
    }else return lineIcon(Glyph::Mods).pixmap(32,32);
    result.setDevicePixelRatio(2);return result;
}
}

VersionComponentsDialog::VersionComponentsDialog(const Instance &instance,const QString &name,const QString &source,
                                               const QString &customCover,QWidget *parent)
    :InstallPlanDialog(QCoreApplication::translate("M2CL", "版本组件"),parent),m_instance(instance),m_source(source),
      m_cancel(std::make_shared<std::atomic_bool>(false)) {
    setObjectName("versionComponents");resize(840,730);setMinimumSize(700,630);
    findChild<QLabel *>("sectionTitle")->setProperty("componentPageTitle",true);findChild<QFrame *>("installTitleDivider")->hide();
    auto body=contentLayout();body->setSpacing(12);
    auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
    auto content=new QWidget;auto layout=new QVBoxLayout(content);layout->setContentsMargins(0,0,0,0);layout->setSpacing(12);
    scroll->setWidget(content);body->addWidget(scroll,1);
    // Share the instance banner with install plans, without the installation-specific caption.
    addTargetInstance(name,QStringLiteral("Minecraft %1 · %2 %3").arg(instance.version,instance.loaderName(),instance.loaderVersion).trimmed(),
                      CoverStore::image(instance.version,QSize(1200,300),customCover),{});
    auto banner=body->takeAt(body->count()-1);layout->addWidget(banner->widget());delete banner;

    layout->addWidget(text(QCoreApplication::translate("M2CL", "已安装组件"),"componentSectionTitle"));
    auto installed=new QFrame;installed->setObjectName("installedComponentList");
    auto rows=new QVBoxLayout(installed);rows->setContentsMargins(0,0,0,0);rows->setSpacing(0);
    auto component=[&](const QString &kind,const QString &title,const QString &caption,const QString &version,bool more){
        auto row=new QFrame;row->setProperty("componentRow",true);row->setMinimumHeight(54);
        auto line=new QHBoxLayout(row);line->setContentsMargins(14,8,14,8);line->setSpacing(14);
        auto icon=new QLabel;icon->setProperty("componentIconKind",kind);icon->setPixmap(componentIcon(kind,palette()));icon->setFixedSize(32,32);line->addWidget(icon);
        auto labels=new QVBoxLayout;labels->setContentsMargins(0,0,0,0);labels->setSpacing(1);labels->addWidget(text(title,"componentName"));
        if(!caption.isEmpty())labels->addWidget(text(caption,"componentCaption"));
        auto nameHost=new QWidget;nameHost->setLayout(labels);nameHost->setMinimumWidth(126);line->addWidget(nameHost,2);
        auto number=text(displayVersion(version));number->setMinimumWidth(95);line->addWidget(number,3);
        line->addWidget(badge(QCoreApplication::translate("M2CL", "✓ 已安装")));line->addSpacing(8);
        if(more) {
            auto button=new QPushButton;button->setObjectName("optifineComponentMore");button->setProperty("flatAction",true);
            button->setIcon(lineIcon(Glyph::More));button->setFixedSize(34,32);button->setAccessibleName(QCoreApplication::translate("M2CL", "OptiFine 更多操作"));
            auto menu=new RoundedMenu(button);m_reinstallAction=menu->addAction(lineIcon(Glyph::Refresh),QCoreApplication::translate("M2CL", "重新安装"));m_reinstallAction->setObjectName("reinstallOptifine");m_reinstallAction->setEnabled(false);
            auto remove=menu->addAction(lineIcon(Glyph::Trash,QColor("#c7515c")),QCoreApplication::translate("M2CL", "移除 OptiFine"));remove->setObjectName("removeOptifineComponent");
            connect(remove,&QAction::triggered,this,[this]{m_action="remove";accept();});
            connect(m_reinstallAction,&QAction::triggered,this,[this]{
                for(const auto &v:m_builds)if(v.toObject().value("version")==m_instance.optifineVersion){chooseBuild(v.toObject());m_reinstall=true;updateSelection();break;}
            });
            connect(button,&QPushButton::clicked,this,[button,menu]{menu->popup(button->mapToGlobal(QPoint(button->width()-menu->sizeHint().width(),button->height()+4)));});line->addWidget(button);
        }else {auto empty=text(QStringLiteral("—"),"muted");empty->setFixedWidth(34);empty->setAlignment(Qt::AlignCenter);line->addWidget(empty);}
        rows->addWidget(row);
    };
    m_reinstallAction=nullptr;
    component("minecraft","Minecraft",{},instance.version,false);
    if(instance.loader!="vanilla")component(instance.loader,instance.loaderName(),QCoreApplication::translate("M2CL", "主加载器"),instance.loaderVersion,false);
    if(!instance.optifineVersion.isEmpty())component("optifine","OptiFine",QCoreApplication::translate("M2CL", "可选组件"),instance.optifineVersion,true);
    layout->addWidget(installed);

    auto heading=new QHBoxLayout;heading->addWidget(text(QCoreApplication::translate("M2CL", "安装 / 更新 OptiFine"),"componentSectionTitle"));heading->addStretch();
    auto website=new QPushButton(QCoreApplication::translate("M2CL", "项目官网 ↗"));website->setProperty("textLink",true);heading->addWidget(website);
    connect(website,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://optifine.net/downloads"));});layout->addLayout(heading);
    auto toolbar=new QHBoxLayout;toolbar->setSpacing(10);m_channels=new QTabBar;m_channels->setObjectName("optifineChannels");
    m_channels->addTab(QCoreApplication::translate("M2CL", "正式版"));m_channels->addTab(QCoreApplication::translate("M2CL", "预览版"));m_channels->setExpanding(false);m_channels->setDrawBase(false);toolbar->addWidget(m_channels);toolbar->addStretch();
    m_refresh=new QPushButton(QCoreApplication::translate("M2CL", "刷新"));m_refresh->setObjectName("refreshOptifineBuilds");m_refresh->setIcon(lineIcon(Glyph::Refresh));toolbar->addWidget(m_refresh);
    m_import=new QPushButton(QCoreApplication::translate("M2CL", "导入本地安装包"));m_import->setObjectName("importOptifineBuild");m_import->setIcon(lineIcon(Glyph::Folder));toolbar->addWidget(m_import);layout->addLayout(toolbar);
    auto host=new QWidget;host->setObjectName("optifineBuildHost");auto overlay=new QGridLayout(host);overlay->setContentsMargins(0,0,0,0);
    m_list=new QListWidget;m_list->setObjectName("optifineBuildList");m_list->setAccessibleName(QCoreApplication::translate("M2CL", "OptiFine 构建"));m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);m_list->setMinimumHeight(144);
    overlay->addWidget(m_list,0,0);m_spinner=new BusySpinner;m_spinner->setFixedSize(28,28);m_spinner->setObjectName("optifineBuildSpinner");overlay->addWidget(m_spinner,0,0,Qt::AlignCenter);m_spinner->hide();
    m_empty=text(QCoreApplication::translate("M2CL", "点击“刷新”查询适配的 OptiFine 构建"),"muted");m_empty->setObjectName("optifineEmpty");m_empty->setAlignment(Qt::AlignCenter);m_empty->setWordWrap(true);m_empty->setAttribute(Qt::WA_TransparentForMouseEvents);overlay->addWidget(m_empty,0,0,Qt::AlignCenter);layout->addWidget(host);m_networkNotice=new NetworkNotice;m_networkNotice->setObjectName("optifineNetworkState");layout->addWidget(m_networkNotice);connect(m_networkNotice,&NetworkNotice::retryRequested,this,&VersionComponentsDialog::queryBuilds);
    m_message=text({},"muted");m_message->setObjectName("optifineSelectionMessage");m_message->setWordWrap(true);layout->addWidget(m_message);layout->addStretch();
    auto divider=new QFrame;divider->setProperty("settingsDivider",true);body->addWidget(divider);
    auto footer=new QHBoxLayout;footer->setSpacing(10);footer->addWidget(text(QCoreApplication::translate("M2CL", "下载源"),"muted"));m_sourceChoice=new SelectBox;m_sourceChoice->setObjectName("componentDownloadSource");m_sourceChoice->setAccessibleName(QCoreApplication::translate("M2CL", "组件下载源"));m_sourceChoice->addItem(automaticDownloadSourceLabel(),"auto");m_sourceChoice->addItem(QCoreApplication::translate("M2CL", "官方源"),"official");m_sourceChoice->addItem("BMCLAPI","bmclapi");m_sourceChoice->setCurrentIndex(m_sourceChoice->findData(source));m_sourceChoice->setMinimumWidth(170);footer->addWidget(m_sourceChoice);footer->addStretch();
    auto close=new QPushButton(QCoreApplication::translate("M2CL", "关闭"));close->setObjectName("closeVersionComponents");footer->addWidget(close);
    m_primary=new QPushButton(QCoreApplication::translate("M2CL", "查看安装方案"));m_primary->setObjectName("reviewOptifinePlan");m_primary->setProperty("primary",true);m_primary->setMinimumWidth(126);m_primary->setEnabled(false);footer->addWidget(m_primary);body->addLayout(footer);
    connect(this,&QDialog::finished,this,[this]{m_closing=true;m_requery=false;m_cancel->store(true);});
    connect(close,&QPushButton::clicked,this,&QDialog::reject);connect(m_primary,&QPushButton::clicked,this,[this]{if(m_primary->isEnabled()){m_action="install";accept();}});
    connect(m_channels,&QTabBar::currentChanged,this,[this]{m_reinstall=false;m_localFile.clear();populate();});
    connect(m_list,&QListWidget::currentRowChanged,this,[this]{m_reinstall=false;m_localFile.clear();updateSelection();});
    connect(m_refresh,&QPushButton::clicked,this,&VersionComponentsDialog::queryBuilds);connect(m_import,&QPushButton::clicked,this,&VersionComponentsDialog::importLocal);
    connect(m_sourceChoice,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{
        m_source=m_sourceChoice->currentData().toString();emit downloadSourceChanged(m_source);
        m_reinstall=false;m_localFile.clear();m_builds={};m_list->clear();m_radios.clear();m_primary->setEnabled(false);if(m_reinstallAction)m_reinstallAction->setEnabled(false);
        if(m_loading){m_requery=true;m_cancel->store(true);return;}
        if(m_everQueried)queryBuilds();else{m_empty->setText(QCoreApplication::translate("M2CL", "来源已切换，点击“刷新”查询构建"));m_empty->show();m_message->clear();}
    });
    connect(&m_watcher,&QFutureWatcher<VersionList>::finished,this,[this]{
        if(m_closing)return;
        if(m_requery){m_requery=false;queryBuilds();return;}
        setBuilds(m_watcher.result());
    });
}

void VersionComponentsDialog::changeEvent(QEvent *event){
    InstallPlanDialog::changeEvent(event);
    if(event->type()==QEvent::PaletteChange||event->type()==QEvent::ApplicationPaletteChange)
        for(auto icon:findChildren<QLabel *>())if(!icon->property("componentIconKind").toString().isEmpty())icon->setPixmap(componentIcon(icon->property("componentIconKind").toString(),palette()));
}
VersionComponentsDialog::~VersionComponentsDialog(){m_cancel->store(true);m_watcher.waitForFinished();}
QJsonObject VersionComponentsDialog::selectedBuild() const { return m_list->currentItem()?m_list->currentItem()->data(Qt::UserRole).toJsonObject():QJsonObject(); }
void VersionComponentsDialog::queryBuilds(){
    if(m_watcher.isRunning()||m_closing)return;m_everQueried=true;m_cancel=std::make_shared<std::atomic_bool>(false);m_loading=true;m_reinstall=false;m_localFile.clear();m_list->clear();m_radios.clear();m_message->clear();m_networkNotice->clear();m_empty->hide();m_spinner->show();m_refresh->setEnabled(false);m_import->setEnabled(false);m_primary->setEnabled(false);if(m_reinstallAction)m_reinstallAction->setEnabled(false);
    m_watcher.setFuture(QtConcurrent::run([game=m_instance.version,source=m_source,cancel=m_cancel]{return fetchOptifineBuilds(game,cancel,source);}));
}
void VersionComponentsDialog::setBuilds(const VersionList &result){
    m_networkNotice->clear();m_loading=false;m_spinner->hide();m_refresh->setEnabled(true);m_import->setEnabled(true);m_builds=result.versions;populate();
    if(!result.error.isEmpty()){m_empty->hide();m_message->clear();m_networkNotice->failure(QCoreApplication::translate("M2CL", "OptiFine 构建查询失败"),result.error);}
    if(m_reinstallAction){bool found=false;for(const auto &v:m_builds)found|=v.toObject().value("version")==m_instance.optifineVersion;m_reinstallAction->setEnabled(found);}
}
void VersionComponentsDialog::populate(){
    const QSignalBlocker blocker(m_list);m_list->clear();m_radios.clear();int preferred=-1;
    auto group=new QButtonGroup(m_list);group->setExclusive(true);
    // Groups own only transient controls; delete the prior empty group after its rows are removed.
    for(auto old:m_list->findChildren<QButtonGroup *>(QString(),Qt::FindDirectChildrenOnly))if(old!=group)delete old;
    for(const auto &v:m_builds){const auto build=v.toObject();if(build.value("preview").toBool()!=(m_channels->currentIndex()==1))continue;
        const auto problem=optifineCompatibility(build,m_instance.version,m_instance.loader,m_instance.loaderVersion);const bool installed=build.value("version")==m_instance.optifineVersion;
        auto item=new QListWidgetItem(m_list);item->setData(Qt::UserRole,build);item->setData(Qt::AccessibleTextRole,displayVersion(build.value("version").toString())+" · Minecraft "+m_instance.version+" · Forge "+build.value("forge").toString());item->setSizeHint(QSize(0,62));item->setToolTip(problem);
        auto row=new QFrame;row->setObjectName("optifineBuildRow");row->setProperty("selected",false);auto line=new QHBoxLayout(row);line->setContentsMargins(10,6,10,6);line->setSpacing(12);
        const int index=m_radios.size();auto radio=new RadioButton;radio->setAccessibleName(displayVersion(build.value("version").toString()));group->addButton(radio,index);m_radios.append(radio);line->addWidget(radio);
        auto version=text(displayVersion(build.value("version").toString()),"componentName");version->setMinimumWidth(72);line->addWidget(version);
        auto details=text(QStringLiteral("Minecraft %1%2").arg(m_instance.version,m_instance.loader=="forge"?" · Forge "+build.value("forge").toString():QString()),"muted");details->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);line->addWidget(details,1);
        if(!problem.isEmpty())line->addWidget(badge(m_instance.loader=="forge"?QCoreApplication::translate("M2CL", "Forge 版本不匹配"):QCoreApplication::translate("M2CL", "不支持此加载器"),"warning"));
        else {line->addWidget(badge(QCoreApplication::translate("M2CL", "匹配当前实例")));if(installed)line->addWidget(badge(QCoreApplication::translate("M2CL", "✓ 已安装")));}
        m_list->setItemWidget(item,row);connect(radio,&QRadioButton::clicked,this,[this,index]{m_list->setCurrentRow(index);updateSelection();});
        if(problem.isEmpty()&&(preferred<0||installed))preferred=index;
    }
    m_list->parentWidget()->setFixedHeight(qBound(144,m_list->count()*62+10,208));
    m_list->setCurrentRow(preferred);m_empty->setText(QCoreApplication::translate("M2CL", "此分类暂无适配构建，可切换正式版 / 预览版"));m_empty->setVisible(!m_loading&&m_list->count()==0);updateSelection();
}
void VersionComponentsDialog::updateSelection(){
    const auto build=selectedBuild();const bool installed=!build.isEmpty()&&build.value("version")==m_instance.optifineVersion;
    const auto problem=build.isEmpty()?QString():optifineCompatibility(build,m_instance.version,m_instance.loader,m_instance.loaderVersion);
    for(int row=0;row<m_list->count();++row){const bool selected=row==m_list->currentRow();m_radios.at(row)->setChecked(selected);auto widget=m_list->itemWidget(m_list->item(row));widget->setProperty("selected",selected);widget->style()->unpolish(widget);widget->style()->polish(widget);}
    m_primary->setText(installed&&!m_reinstall?QCoreApplication::translate("M2CL", "已安装"):m_reinstall?QCoreApplication::translate("M2CL", "重新安装"):QCoreApplication::translate("M2CL", "查看安装方案"));m_primary->setEnabled(!m_loading&&!build.isEmpty()&&problem.isEmpty()&&(!installed||m_reinstall));
    if(m_loading)return;
    if(!problem.isEmpty())m_message->setText(problem);
    else if(m_reinstall)m_message->setText(QCoreApplication::translate("M2CL", "将重新安装此构建；确认安装方案后开始下载。"));
    else if(installed)m_message->setText(QCoreApplication::translate("M2CL", "ⓘ 已安装此构建，可在上方更多操作中重新安装。"));
    else m_message->setText(build.isEmpty()?QString():QCoreApplication::translate("M2CL", "ⓘ 安装前将检查本地模组与组件冲突。"));
    if(!m_localFile.isEmpty())m_message->setText(QCoreApplication::translate("M2CL", "本地安装包：%1").arg(QFileInfo(m_localFile).fileName()));
}
void VersionComponentsDialog::chooseBuild(const QJsonObject &build){
    const QSignalBlocker tabs(m_channels);m_channels->setCurrentIndex(build.value("preview").toBool()?1:0);populate();
    for(int row=0;row<m_list->count();++row)if(m_list->item(row)->data(Qt::UserRole).toJsonObject().value("version")==build.value("version")){m_list->setCurrentRow(row);break;}
}
void VersionComponentsDialog::importLocal(){
    const auto file=QFileDialog::getOpenFileName(this,QCoreApplication::translate("M2CL", "选择 OptiFine 安装包"),{},"Java archive (*.jar)");if(file.isEmpty())return;
    try {const auto identity=inspectOptifine(file);if(identity.value("game")!=m_instance.version)throw std::runtime_error(QCoreApplication::translate("M2CL", "安装包与实例 Minecraft 版本不一致").toUtf8().toStdString());
        QJsonObject found;for(const auto &v:m_builds)if(v.toObject().value("version")==identity.value("version")){found=v.toObject();break;}
        if(found.isEmpty())throw std::runtime_error(QCoreApplication::translate("M2CL", "请先刷新构建；安装包未出现在当前游戏的版本列表中").toUtf8().toStdString());
        chooseBuild(found);m_localFile=file;m_reinstall=identity.value("version")==m_instance.optifineVersion;updateSelection();
    } catch(const std::exception &error){QMessageBox::warning(this,QCoreApplication::translate("M2CL", "无法导入安装包"),QString::fromUtf8(error.what()));}
}
}
