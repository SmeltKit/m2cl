// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "networknotice.h"
#include "modversionview.h"
#include "launcherwidgets.h"
#include "projectreadme.h"
#include <QDesktopServices>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QStackedWidget>
#include <QCollator>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QLineEdit>
#include <QLayout>
#include <algorithm>

namespace mc {
namespace {
// Version labels wrap as the content width changes; every exact version stays visible.
class VersionFlowLayout final : public QLayout {
public:
    explicit VersionFlowLayout(QWidget *parent):QLayout(parent){setContentsMargins(0,0,0,0);setSpacing(6);}
    ~VersionFlowLayout() override {while(auto item=takeAt(0))delete item;}
    void addItem(QLayoutItem *item) override {m_items.append(item);}
    int count() const override {return m_items.size();}
    QLayoutItem *itemAt(int index) const override {return m_items.value(index);}
    QLayoutItem *takeAt(int index) override {return index>=0&&index<m_items.size()?m_items.takeAt(index):nullptr;}
    Qt::Orientations expandingDirections() const override {return {};}
    bool hasHeightForWidth() const override {return true;}
    int heightForWidth(int width) const override {return arrange(QRect(0,0,width,0),false);}
    QSize sizeHint() const override {return {320,heightForWidth(320)};}
    QSize minimumSize() const override {return {0,m_items.isEmpty()?0:m_items.first()->sizeHint().height()};}
    void setGeometry(const QRect &rect) override {QLayout::setGeometry(rect);arrange(rect,true);}
private:
    int arrange(const QRect &rect,bool place) const {
        const auto area=rect.marginsRemoved(contentsMargins());int x=area.left(),y=area.top(),lineHeight=0;
        for(auto item:m_items){
            const int width=qMin(item->sizeHint().width(),qMax(1,area.width()));
            if(x>area.left()&&x+width>area.left()+area.width()){x=area.left();y+=lineHeight+spacing();lineHeight=0;}
            const int height=item->hasHeightForWidth()?item->heightForWidth(width):item->sizeHint().height();
            if(place)item->setGeometry(QRect(x,y,width,height));
            x+=width+spacing();lineHeight=qMax(lineHeight,height);
        }
        return y-rect.top()+lineHeight+contentsMargins().bottom();
    }
    QList<QLayoutItem *> m_items;
};
QLabel *text(const QString &value, bool secondary = false) {
    auto label = new QLabel(value); label->setTextFormat(Qt::PlainText); label->setWordWrap(true);
    label->setProperty("secondaryText",secondary); label->setProperty("muted",secondary); return label;
}
}
ModVersionView::ModVersionView(const QString &projectName,const QString &,const Instance *target,const QList<ResourceFile> &files,QWidget *parent):QWidget(parent) {
    setObjectName("modVersionView"); setMinimumWidth(0);
    const auto instance = target ? *target : Instance{};
    auto root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(12);
    auto toolbar = new QHBoxLayout;
    auto back = new QPushButton(QCoreApplication::translate("M2CL", "← 返回模组列表")); back->setObjectName("backToMods"); toolbar->addWidget(back); toolbar->addStretch();
    auto website = new QPushButton(QCoreApplication::translate("M2CL", "项目主页 ↗")); website->setObjectName("modProjectWebsite"); website->setProperty("textLink",true); toolbar->addWidget(website);
    root->addLayout(toolbar);
    connect(back,&QPushButton::clicked,this,&ModVersionView::backRequested);
    connect(website,&QPushButton::clicked,this,[this]{const auto slug=m_project.slug.isEmpty()?m_project.id:m_project.slug;if(!slug.isEmpty())QDesktopServices::openUrl(QUrl("https://modrinth.com/"+(property("resourceType").toString().isEmpty()?QString("mod"):property("resourceType").toString())+"/"+QString::fromLatin1(QUrl::toPercentEncoding(slug))));});
    auto hero = new QHBoxLayout; hero->setSpacing(14);
    m_projectIcon = new QLabel; m_projectIcon->setObjectName("modProjectIcon"); m_projectIcon->setFixedSize(64,64); m_projectIcon->setScaledContents(true); m_projectIcon->setProperty("resourceIcon",true); m_projectIcon->setPixmap(lineIcon(Glyph::Mods).pixmap(48,48)); hero->addWidget(m_projectIcon,0,Qt::AlignTop);
    auto headings = new QVBoxLayout; headings->setSpacing(4);
    auto title = text(projectName); title->setObjectName("sectionTitle"); headings->addWidget(title);
    m_author = text({},true); headings->addWidget(m_author);
    m_description = text({},true); headings->addWidget(m_description);
    hero->addLayout(headings,1); root->addLayout(hero);
    auto tabs = new QTabWidget; tabs->setObjectName("modDetailSections"); root->addWidget(tabs,1);
    m_readme = new ProjectReadme; m_readme->setObjectName("modProjectReadme"); m_readme->setFrameShape(QFrame::NoFrame); m_readme->setOpenLinks(false); m_readme->setOpenExternalLinks(false);
    m_readme->document()->setDefaultFont(font());
    m_readme->document()->setDocumentMargin(14);
    connect(m_readme,&QTextBrowser::anchorClicked,this,[this](const QUrl &url){
        if(url.isRelative() && url.path().isEmpty() && !url.fragment().isEmpty()){m_readme->scrollToAnchor(url.fragment());return;}
        const auto target=m_readme->document()->baseUrl().resolved(url);
        if(target.scheme()=="https" || target.scheme()=="http")QDesktopServices::openUrl(target);
    });
    auto intro=new QWidget;auto introBody=new QVBoxLayout(intro);introBody->setContentsMargins(0,0,0,0);introBody->setSpacing(8);m_projectNotice=new NetworkNotice;m_projectNotice->setObjectName("modProjectNetworkState");introBody->addWidget(m_projectNotice);introBody->addWidget(m_readme,1);
    connect(m_projectNotice,&NetworkNotice::retryRequested,this,[this]{m_projectNotice->loading();emit retryProjectRequested();});
    auto imageFeedback=new NetworkNotice;imageFeedback->setObjectName("modImageNetworkState");introBody->addWidget(imageFeedback);
    auto retryImages=imageFeedback->findChild<QPushButton *>("networkRetry");retryImages->setObjectName("retryReadmeImages");retryImages->setText(QCoreApplication::translate("M2CL", "重试图片"));
    connect(imageFeedback,&NetworkNotice::retryRequested,m_readme,&ProjectReadme::retryImages);
    connect(m_readme,&ProjectReadme::imageStatusChanged,this,[=](int pending,int failed){if(pending)imageFeedback->loading();else if(failed)imageFeedback->failure(QCoreApplication::translate("M2CL", "%1 张介绍图片未能加载").arg(failed),m_readme->imageErrorDetails());else imageFeedback->clear();});
    tabs->addTab(intro,QCoreApplication::translate("M2CL", "介绍"));
    auto downloads = new QWidget; auto body = new QVBoxLayout(downloads); body->setContentsMargins(0,12,0,0); body->setSpacing(10);
    m_fileLoaders=new QTabBar;m_fileLoaders->setObjectName("modFileLoaders");m_fileLoaders->setExpanding(false);m_fileLoaders->setDrawBase(false);m_fileLoaders->addTab(QCoreApplication::translate("M2CL", "全部加载器"));m_fileLoaders->setTabData(0,QString());
    QStringList loaderNames,gameVersions;for(const auto &file:files){loaderNames.append(file.loaders);gameVersions.append(file.gameVersions);}loaderNames.removeDuplicates();gameVersions.removeDuplicates();
    QStringList ordered;for(const auto &loader:QStringList{"forge","neoforge","fabric","quilt"})if(loaderNames.removeAll(loader))ordered.append(loader);loaderNames.sort();ordered.append(loaderNames);
    for(const auto &loader:ordered){const auto index=m_fileLoaders->addTab(resourceTagLabel(loader));m_fileLoaders->setTabData(index,loader);}if(m_fileLoaders->count()>1)m_fileLoaders->setCurrentIndex(1);body->addWidget(m_fileLoaders,0,Qt::AlignLeft);
    auto channel = new QTabBar; channel->setDrawBase(false); channel->setExpanding(false); channel->setObjectName("modReleaseChannel");
    for (const auto &type : QStringList{"release","beta","alpha"}) {
        int count=0; for(const auto &file:files)if(file.releaseType==type)++count;
        const auto index=channel->addTab(resourceTagLabel(type)+QCoreApplication::translate("M2CL", "（%1）").arg(count));channel->setTabData(index,type);
    }
    auto filters=new QHBoxLayout;filters->setSpacing(10);filters->addWidget(channel);filters->addStretch();filters->addWidget(text(QCoreApplication::translate("M2CL", "游戏版本"),true));
    m_fileGameVersions=new SelectBox;m_fileGameVersions->setObjectName("modFileGameVersion");m_fileGameVersions->setMinimumWidth(130);m_fileGameVersions->setMaximumWidth(200);m_fileGameVersions->addItem(QCoreApplication::translate("M2CL", "全部版本"),QString());
    QCollator versionOrder;versionOrder.setNumericMode(true);std::sort(gameVersions.begin(),gameVersions.end(),[&](const QString&a,const QString&b){return versionOrder.compare(a,b)>0;});for(const auto &version:gameVersions){m_fileGameVersions->addItem(version,version);m_fileGameVersions->setItemData(m_fileGameVersions->count()-1,QRegularExpression("^\\d+(?:\\.\\d+)+$").match(version).hasMatch()?"release":"test",SelectBox::PopupGroupRole);}filters->addWidget(m_fileGameVersions,1);body->addLayout(filters);
    m_list = new RoundedTable(files.size(),4); m_list->setObjectName("modFileVersions");
    m_list->setHorizontalHeaderLabels({QCoreApplication::translate("M2CL", "模组版本"),QCoreApplication::translate("M2CL", "Minecraft / 加载器"),QCoreApplication::translate("M2CL", "大小"),QCoreApplication::translate("M2CL", "发布日期")});
    m_list->verticalHeader()->hide();m_list->setShowGrid(false);m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);m_list->setSelectionBehavior(QAbstractItemView::SelectRows);m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    static_cast<RoundedTable *>(m_list)->setColumnLayout({{0,200,3},{1,200,3},{2,100,1},{3,128,1.5}});
    m_list->setWordWrap(false);m_list->setMinimumHeight(100);m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);body->addWidget(m_list,1);
    for(int i=0;i<files.size();++i){const auto &file=files.at(i);
        QStringList loaders;for(const auto &loader:file.loaders)loaders.append(resourceTagLabel(loader));
        const QStringList values{file.versionNumber.isEmpty()?file.versionName.simplified():file.versionNumber,resourceVersionSummary(file.gameVersions,2)+" · "+loaders.join(" / "),QStringLiteral("%1 MiB").arg(file.size/1048576.,0,'f',2),file.published.isEmpty()?QStringLiteral("—"):file.published.left(10)};
        for(int col=0;col<values.size();++col){auto item=new QTableWidgetItem(values.at(col));item->setToolTip(file.versionName+"\n"+file.fileName);m_list->setItem(i,col,item);}m_list->setRowHeight(i,44);
    }
    auto empty = text(QCoreApplication::translate("M2CL", "当前条件下没有正式版，可主动切换到测试版或预览版。"),true);empty->setObjectName("noStableModVersions");body->addWidget(empty);
    auto selected = text({},true);selected->setObjectName("selectedModFile");selected->setWordWrap(false);selected->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);body->addWidget(selected);
    auto compatibility = text(QCoreApplication::translate("M2CL", "安装时选择匹配的实例，并确认需要下载的前置。"),true);compatibility->setObjectName("modFileCompatibility");body->addWidget(compatibility);
    tabs->addTab(downloads,QCoreApplication::translate("M2CL", "下载"));
    auto support=new QWidget;support->setObjectName("modSupportedVersions");auto supportBody=new QVBoxLayout(support);supportBody->setContentsMargins(0,12,0,0);supportBody->setSpacing(10);
    auto supportToolbar=new QHBoxLayout;supportToolbar->setSpacing(16);
    m_supportedChannels=new QTabBar;m_supportedChannels->setObjectName("supportedGameVersionChannels");m_supportedChannels->setExpanding(false);m_supportedChannels->setDrawBase(false);supportToolbar->addWidget(m_supportedChannels);
    supportToolbar->addStretch();
    m_supportedSearch=new QLineEdit;m_supportedSearch->setObjectName("supportedVersionSearch");m_supportedSearch->setPlaceholderText(QCoreApplication::translate("M2CL", "搜索版本…"));m_supportedSearch->setAccessibleName(QCoreApplication::translate("M2CL", "搜索支持的 Minecraft 版本"));m_supportedSearch->setClearButtonEnabled(true);m_supportedSearch->setMaximumWidth(260);m_supportedSearch->setMinimumWidth(120);supportToolbar->addWidget(m_supportedSearch,1);
    supportBody->addLayout(supportToolbar);
    m_supportedPages=new QStackedWidget;m_supportedPages->setObjectName("supportedGameVersionPages");m_supportedPages->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);supportBody->addWidget(m_supportedPages,1);
    connect(m_supportedChannels,&QTabBar::currentChanged,m_supportedPages,&QStackedWidget::setCurrentIndex);
    connect(m_supportedSearch,&QLineEdit::textChanged,this,[this]{rebuildSupportedVersions();});
    tabs->addTab(support,QCoreApplication::translate("M2CL", "支持版本"));
    rebuildSupportedVersions();
    auto actions = new QHBoxLayout;actions->addStretch();
    auto install = new QPushButton(QCoreApplication::translate("M2CL", "选择版本并安装"));install->setObjectName("downloadModVersion");install->setProperty("primary",true);install->setIcon(lineIcon(Glyph::Download));actions->addWidget(install);root->addLayout(actions);
    auto update = [=]{const int row=selectedIndex();const bool valid=row>=0&&row<files.size();
        const bool installed=valid && m_installedVersions.contains(files.at(row).versionId);
        install->setText(tabs->currentIndex()!=1?QCoreApplication::translate("M2CL", "选择版本并安装"):installed?QCoreApplication::translate("M2CL", "在实例中管理"):QCoreApplication::translate("M2CL", "安装到游戏实例"));
        install->setEnabled(tabs->currentIndex()!=1 || valid);
        selected->setVisible(valid);selected->setText(valid?files.at(row).fileName:QString());selected->setToolTip(selected->text());
        compatibility->setVisible(valid);
    };
    m_update=update;
    auto revealSelection=[this]{QTimer::singleShot(0,this,[this]{const auto row=selectedIndex();if(row>=0){m_list->doItemsLayout();m_list->scrollToItem(m_list->item(row,0),QAbstractItemView::PositionAtCenter);}});};
    connect(tabs,&QTabWidget::currentChanged,this,[=]{update();if(tabs->currentIndex()==1)revealSelection();});
    connect(m_list,&QTableWidget::currentCellChanged,this,[=](int,int,int,int){update();});
    connect(install,&QPushButton::clicked,this,[this,tabs,files]{if(tabs->currentIndex()!=1){tabs->setCurrentIndex(1);return;}const auto row=selectedIndex();if(row<0)return;if(m_installedVersions.contains(files.at(row).versionId))emit manageRequested();else emit downloadRequested(row);});
    auto filter = [=]{int first=-1,match=-1;const auto type=channel->tabData(channel->currentIndex()).toString();
        const auto loader=m_fileLoaders->tabData(m_fileLoaders->currentIndex()).toString(),version=m_fileGameVersions->currentData().toString();QHash<QString,int> counts;const auto selected=selectedIndex();
        for(int i=0;i<files.size();++i){const auto &file=files.at(i);const bool environment=(loader.isEmpty()||file.loaders.contains(loader))&&(version.isEmpty()||file.gameVersions.contains(version));if(environment)++counts[file.releaseType];const bool visible=environment&&file.releaseType==type;m_list->setRowHidden(i,!visible);if(visible){if(first<0)first=i;if(i==selected)match=i;else if(match<0&&target&&ModrinthProvider::compatible(file,instance.version,instance.loader))match=i;}}
        for(int i=0;i<channel->count();++i){const auto id=channel->tabData(i).toString();channel->setTabText(i,resourceTagLabel(id)+QCoreApplication::translate("M2CL", "（%1）").arg(counts.value(id)));}
        empty->setVisible(first<0);empty->setText(type=="release"?QCoreApplication::translate("M2CL", "当前条件下没有正式版，可主动切换到测试版或预览版。"):QCoreApplication::translate("M2CL", "当前分类没有可用文件版本。"));
        const auto row=match>=0?match:first;m_list->setCurrentCell(row,0);if(row>=0){m_list->selectRow(row);m_list->scrollToItem(m_list->item(row,0),QAbstractItemView::EnsureVisible);}update();revealSelection();
    };
    m_fileFilter=filter;connect(channel,&QTabBar::currentChanged,this,[=]{filter();});connect(m_fileLoaders,&QTabBar::currentChanged,this,[=]{filter();});connect(m_fileGameVersions,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[=]{filter();});filter();if(target)setDownloadFilter(instance.version,instance.loader);
}
void ModVersionView::setDownloadFilter(const QString &gameVersion,const QString &loader){
    m_fileGameVersions->setCurrentIndex(qMax(0,m_fileGameVersions->findData(gameVersion)));if(!loader.isEmpty()){int selected=0;for(int i=0;i<m_fileLoaders->count();++i)if(m_fileLoaders->tabData(i)==loader)selected=i;m_fileLoaders->setCurrentIndex(selected);}if(m_fileFilter)m_fileFilter();
}
void ModVersionView::setProjectInfo(const ResourceProject &project) {
    m_projectNotice->clear();m_project=project;
    m_author->setText(project.author.isEmpty()?QString():QCoreApplication::translate("M2CL", "作者：%1").arg(project.author));m_author->setVisible(!project.author.isEmpty());
    m_description->setText(project.description);m_projectIcon->setProperty("iconUrl",project.iconUrl);
    auto markdown=project.body.isEmpty()?project.description:project.body.left(512*1024);
    const auto slug=project.slug.isEmpty()?project.id:project.slug;
    m_readme->setProjectMarkdown(markdown,QUrl("https://modrinth.com/"+(property("resourceType").toString().isEmpty()?QString("mod"):property("resourceType").toString())+"/"+QString::fromLatin1(QUrl::toPercentEncoding(slug))));
    rebuildSupportedVersions();
}
void ModVersionView::setSupportedVersionMetadata(const ResourceTags &tags){m_versionTags=tags;for(int i=1;i<m_fileGameVersions->count();++i){const auto version=m_fileGameVersions->itemData(i).toString();const auto type=tags.gameVersionTypes.value(version);if(!type.isEmpty())m_fileGameVersions->setItemData(i,type=="release"?"release":"test",SelectBox::PopupGroupRole);}rebuildSupportedVersions();}
void ModVersionView::rebuildSupportedVersions(){
    const auto selected=m_supportedChannels->currentIndex()<0?QString("release"):m_supportedChannels->tabData(m_supportedChannels->currentIndex()).toString();
    const QSignalBlocker blocker(m_supportedChannels);
    while(m_supportedChannels->count())m_supportedChannels->removeTab(0);
    while(auto page=m_supportedPages->widget(0)){m_supportedPages->removeWidget(page);page->deleteLater();}
    QStringList versions=m_project.gameVersions;versions.removeDuplicates();QHash<QString,int> order;
    for(int i=0;i<m_versionTags.gameVersions.size();++i)order.insert(m_versionTags.gameVersions.at(i),i);
    QCollator collator;collator.setNumericMode(true);
    std::stable_sort(versions.begin(),versions.end(),[&](const QString &a,const QString &b){if(order.contains(a)&&order.contains(b))return order.value(a)<order.value(b);if(order.contains(a)!=order.contains(b))return order.contains(a);return collator.compare(a,b)>0;});
    const QRegularExpression stable("^\\d+(?:\\.\\d+)+$"),test("(?:^\\d{2}w\\d{2}|snapshot|pre|rc|^[ab]\\d|^rd-|^inf|^c0)",QRegularExpression::CaseInsensitiveOption);
    QMap<QString,QStringList> categories;
    for(const auto &version:versions){const auto type=m_versionTags.gameVersionTypes.value(version);const auto channel=!type.isEmpty()?(type=="release"?QString("release"):QString("test")):stable.match(version).hasMatch()?QString("release"):test.match(version).hasMatch()?QString("test"):QString("unknown");categories[channel].append(version);}
    for(const auto &channel:QStringList{"release","test","unknown"}){
        if(channel=="unknown"&&categories.value(channel).isEmpty())continue;
        const auto values=categories.value(channel);const auto caption=channel=="release"?QCoreApplication::translate("M2CL", "正式版"):channel=="test"?QCoreApplication::translate("M2CL", "测试版"):QCoreApplication::translate("M2CL", "未分类");
        const auto index=m_supportedChannels->addTab(caption+QCoreApplication::translate("M2CL", "（%1）").arg(values.size()));m_supportedChannels->setTabData(index,channel);
        auto scroll=new QScrollArea;scroll->setObjectName("supportedVersionsScroll");scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto panel=new QWidget;auto groups=new QVBoxLayout(panel);groups->setContentsMargins(0,0,8,0);groups->setSpacing(0);
        QMap<QString,QStringList> families;QStringList familyOrder;
        for(const auto &version:values){
            QString family;const auto numeric=QRegularExpression("^(\\d+\\.\\d+)").match(version),weekly=QRegularExpression("^(\\d{2})w").match(version);
            if(numeric.hasMatch())family=numeric.captured(1);else if(weekly.hasMatch())family=QCoreApplication::translate("M2CL", "20%1 年快照").arg(weekly.captured(1));else family=QCoreApplication::translate("M2CL", "其他版本");
            if(!families.contains(family))familyOrder.append(family);families[family].append(version);
        }
        const auto query=m_supportedSearch->text().trimmed();int visibleFamilies=0;
        for(const auto &family:familyOrder){
            QStringList matching;for(const auto &version:families.value(family))if(query.isEmpty()||version.contains(query,Qt::CaseInsensitive)||family.contains(query,Qt::CaseInsensitive))matching.append(version);
            if(matching.isEmpty())continue;++visibleFamilies;
            auto row=new QFrame;row->setObjectName("supportedVersionRow");row->setProperty("family",family);row->setProperty("channel",channel);
            auto content=new QHBoxLayout(row);content->setContentsMargins(6,9,6,9);content->setSpacing(12);
            auto heading=text(family);heading->setObjectName("supportedVersionSeries");heading->setFixedWidth(94);heading->setFixedHeight(26);content->addWidget(heading,0,Qt::AlignTop);
            auto count=text(QCoreApplication::translate("M2CL", "%1 个版本").arg(matching.size()),true);count->setFixedWidth(72);count->setFixedHeight(26);content->addWidget(count,0,Qt::AlignTop);
            auto labels=new QWidget;labels->setObjectName("supportedVersionLabels");labels->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);auto flow=new VersionFlowLayout(labels);
            for(const auto &version:matching){auto label=text(version);label->setObjectName("supportedVersionValues");label->setProperty("channel",channel);label->setTextInteractionFlags(Qt::TextSelectableByMouse);label->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);flow->addWidget(label);}
            content->addWidget(labels,1);groups->addWidget(row);
        }
        if(!visibleFamilies){
            const auto message=!query.isEmpty()?QCoreApplication::translate("M2CL", "未找到匹配的版本"):channel=="release"?QCoreApplication::translate("M2CL", "项目未声明支持正式版，可切换到测试版查看。"):channel=="test"?QCoreApplication::translate("M2CL", "项目未声明支持测试版。"):QCoreApplication::translate("M2CL", "项目未声明其他版本。");
            auto empty=text(message,true);empty->setObjectName("supportedVersionEmpty");empty->setAlignment(Qt::AlignCenter);groups->addWidget(empty,1);
        }
        groups->addStretch();scroll->setWidget(panel);m_supportedPages->addWidget(scroll);
    }
    int index=0;for(int i=0;i<m_supportedChannels->count();++i)if(m_supportedChannels->tabData(i)==selected)index=i;m_supportedChannels->setCurrentIndex(index);m_supportedPages->setCurrentIndex(index);
}
void ModVersionView::setProjectLoading(){m_projectNotice->loading();}
void ModVersionView::setProjectError(const QString &message) {
    m_projectNotice->failure(QCoreApplication::translate("M2CL", "项目介绍未能加载"),message);
}
int ModVersionView::selectedIndex() const {const auto row=m_list->currentRow();return row>=0&&!m_list->isRowHidden(row)?row:-1;}
}
