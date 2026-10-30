// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "networknotice.h"
#include "instancepacks.h"
#include "launcherwidgets.h"
#include "installplandialog.h"
#include "modplanview.h"
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QJsonDocument>
#include "network/resourceclient.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QLineEdit>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QTemporaryDir>
#include <QDir>
#include <QDialogButtonBox>
#include <algorithm>
#include <QPainter>
#include <QTimer>
#include <QApplication>
#include <QStyledItemDelegate>
namespace mc {
namespace {
class PackRowDelegate final:public TableItemDelegate {
public:
 using TableItemDelegate::TableItemDelegate;
 void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index)const override{
  if(index.column()!=0){TableItemDelegate::paint(p,option,index);return;}
  QStyleOptionViewItem base(option);initStyleOption(&base,index);auto text=base.text;auto icon=base.icon;base.text.clear();base.icon={};
  QApplication::style()->drawControl(QStyle::CE_ItemViewItem,&base,p,base.widget);
  p->save();auto area=option.rect.adjusted(16,0,-16,0);
  icon.paint(p,QRect(area.left(),area.center().y()-20,40,40));
  auto rect=area.adjusted(52,0,0,0);auto font=option.font;font.setPixelSize(15);font.setWeight(QFont::DemiBold);p->setFont(font);p->setPen(option.palette.color(QPalette::Text));
  p->drawText(QRect(rect.left(),rect.center().y()-22,rect.width(),23),Qt::AlignVCenter,QFontMetrics(font).elidedText(text,Qt::ElideRight,rect.width()));
  font.setPixelSize(12);font.setWeight(QFont::Normal);p->setFont(font);p->setPen(option.palette.color(QPalette::Disabled,QPalette::Text));
  p->drawText(QRect(rect.left(),rect.center().y()+3,rect.width(),21),Qt::AlignVCenter,QFontMetrics(font).elidedText(index.data(Qt::UserRole).toString(),Qt::ElideRight,rect.width()));p->restore();
 }
};
class PackConfirmation final:public InstallPlanDialog {
public:
 PackConfirmation(const Instance &instance,const ResourceFile &file,PackKind kind,QWidget *parent):InstallPlanDialog(parent){
    addTargetInstance(instance.id,QStringLiteral("Minecraft %1 · %2 %3").arg(instance.version,instance.loaderName(),instance.loaderVersion),CoverStore::image(instance.version,QSize(1000,250),{}));
    auto group=section(contentLayout(),QCoreApplication::translate("M2CL", "将添加"),"add");auto row=new QHBoxLayout;auto icon=new QLabel;icon->setPixmap(lineIcon(Glyph::Folder).pixmap(24,24));row->addWidget(icon);auto title=new QLabel(file.fileName);title->setWordWrap(true);row->addWidget(title,1);row->addWidget(new QLabel(file.versionNumber));group->addLayout(row);
    if(kind==PackKind::Shader){auto support=shaderSupport(instance);auto state=section(contentLayout(),support.available?QCoreApplication::translate("M2CL", "已有光影支持"):QCoreApplication::translate("M2CL", "光影支持需要配置"),support.available?"reuse":"warning");auto message=new QLabel(support.summary+QCoreApplication::translate("M2CL", "\n光影文件不会自动启用，请在游戏内选择。"));message->setWordWrap(true);state->addWidget(message);}
    auto footer=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);footer->button(QDialogButtonBox::Ok)->setText(QCoreApplication::translate("M2CL", "确认并安装"));footer->button(QDialogButtonBox::Ok)->setProperty("primary",true);footer->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("M2CL", "取消"));contentLayout()->addWidget(footer);connect(footer,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(footer,&QDialogButtonBox::rejected,this,&QDialog::reject);resize(700,sizeHint().height());
 }
};
}

InstancePacksView::InstancePacksView(const Instance &i,PackKind k,QWidget *parent):QWidget(parent),m_instance(i),m_kind(k){
 auto body=new QVBoxLayout(this);body->setContentsMargins(0,12,0,0);body->setSpacing(14);
 auto supportCard=new QFrame;supportCard->setObjectName("packSupportCard");
 auto supportLayout=new QHBoxLayout(supportCard);supportLayout->setContentsMargins(14,10,14,10);
 auto check=new QLabel;check->setPixmap(lineIcon(Glyph::Check,QColor("#287c4d")).pixmap(24,24));supportLayout->addWidget(check);
 m_status=new QLabel;m_status->setWordWrap(true);m_status->setProperty("fieldLabel",true);supportLayout->addWidget(m_status,1);
 auto support=new QPushButton(QCoreApplication::translate("M2CL", "管理支持组件"));support->setObjectName("manageShaderSupport");supportLayout->addWidget(support);support->setVisible(k==PackKind::Shader);
 connect(support,&QPushButton::clicked,this,[this]{if(configureSupport)configureSupport();});
 supportCard->setStyleSheet("QFrame#packSupportCard {background: rgba(82,160,112,16);border:1px solid rgba(82,160,112,50);border-radius:7px;}");
 supportCard->setVisible(k==PackKind::Shader);
 auto tools=new ResourceToolbar(k==PackKind::Shader?QCoreApplication::translate("M2CL", "光影"):QCoreApplication::translate("M2CL", "材质包"));body->addWidget(tools);m_count=tools->count;m_search=tools->search;
 auto menu=new RoundedMenu(tools->add);
 connect(menu->addAction(lineIcon(Glyph::Folder),QCoreApplication::translate("M2CL", "从本地导入…")),&QAction::triggered,this,[this]{importLocal();});
 connect(menu->addAction(lineIcon(Glyph::Grid),QCoreApplication::translate("M2CL", "从资源市场…")),&QAction::triggered,this,[this]{if(browseMarket)browseMarket();});
 connect(tools->add,&QPushButton::clicked,this,[tools,menu]{menu->popup(tools->add->mapToGlobal(QPoint(0,tools->add->height()+4)));});
 connect(tools->open,&QPushButton::clicked,this,[this]{auto path=QDir(m_instance.gameDirectory).filePath(packFolder(m_kind));QDir().mkpath(path);QDesktopServices::openUrl(QUrl::fromLocalFile(path));});
 connect(tools->refresh,&QPushButton::clicked,this,[this]{refresh();});
 connect(m_search,&QLineEdit::textChanged,this,[this]{filterRows();});
 body->addWidget(supportCard);
 auto table=new RoundedTable(0,4);m_table=table;table->setHorizontalHeaderLabels({QCoreApplication::translate("M2CL", "名称"),QCoreApplication::translate("M2CL", "版本"),QCoreApplication::translate("M2CL", "状态"),QCoreApplication::translate("M2CL", "操作")});
 table->setColumnLayout({{0,300,5},{1,120,1.3},{2,150,1.5},{3,90,0,Qt::AlignCenter}});
 table->setItemDelegate(new PackRowDelegate(table));table->setShowGrid(false);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->verticalHeader()->hide();table->horizontalHeader()->setFixedHeight(44);table->verticalHeader()->setDefaultSectionSize(76);table->setEditTriggers(QAbstractItemView::NoEditTriggers);auto tableHost=new QWidget;auto tableLayout=new QGridLayout(tableHost);tableLayout->setContentsMargins(0,0,0,0);tableLayout->addWidget(table,0,0);m_empty=new QLabel;m_empty->setAlignment(Qt::AlignCenter);m_empty->setContentsMargins(12,44,12,0);m_empty->setProperty("muted",true);m_empty->setAttribute(Qt::WA_TransparentForMouseEvents);tableLayout->addWidget(m_empty,0,0);body->addWidget(tableHost);tableHost->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
 auto hint=new QLabel(k==PackKind::Shader?QCoreApplication::translate("M2CL", "ⓘ  在游戏内选择光影和调整画质。"):QCoreApplication::translate("M2CL", "ⓘ  在游戏内选择材质包并调整加载顺序。"));hint->setProperty("muted",true);body->addWidget(hint);body->addStretch();
 refresh();
}
bool InstancePacksView::allowed(){if(!m_guard||m_guard())return true;QMessageBox::information(this,QCoreApplication::translate("M2CL", "暂时无法修改"),QCoreApplication::translate("M2CL", "请等待安装完成或关闭游戏后再修改资源。"));return false;}
void InstancePacksView::refresh(){
 try{m_packs=localPacks(m_instance,m_kind);m_table->setRowCount(m_packs.size());for(int r=0;r<m_packs.size();++r){const auto p=m_packs[r];auto name=new QTableWidgetItem(p.name);QPixmap icon;if(p.icon.size()<2*1024*1024&&icon.loadFromData(p.icon))name->setIcon(icon);else name->setIcon(lineIcon(Glyph::Image));name->setData(Qt::UserRole,p.description.isEmpty()?p.fileName:p.description);name->setToolTip(p.fileName);m_table->setItem(r,0,name);m_table->setItem(r,1,new QTableWidgetItem(p.version.isEmpty()?QStringLiteral("—"):p.version));auto state=new QTableWidgetItem(p.problem.isEmpty()?QCoreApplication::translate("M2CL", "已安装"):QCoreApplication::translate("M2CL", "需要检查"));state->setToolTip(p.problem);state->setIcon(lineIcon(p.problem.isEmpty()?Glyph::Check:Glyph::Ban,QColor("#287c4d")));m_table->setItem(r,2,state);auto remove=new QPushButton;remove->setIcon(lineIcon(Glyph::More));remove->setToolTip(QCoreApplication::translate("M2CL", "更多操作"));remove->setProperty("flatAction",true);m_table->setCellWidget(r,3,remove);auto actions=new RoundedMenu(remove);connect(remove,&QPushButton::clicked,this,[remove,actions]{actions->popup(remove->mapToGlobal(QPoint(0,remove->height())));});connect(actions->addAction(lineIcon(Glyph::Trash),QCoreApplication::translate("M2CL", "移到废纸篓／回收站")),&QAction::triggered,this,[this,p]{if(!allowed())return;if(QMessageBox::question(this,QCoreApplication::translate("M2CL", "移除资源"),QCoreApplication::translate("M2CL", "将 %1 移到系统废纸篓／回收站？").arg(p.fileName))!=QMessageBox::Yes)return;auto error=removePack(m_instance,m_kind,p.fileName);if(!error.isEmpty())QMessageBox::warning(this,QCoreApplication::translate("M2CL", "移除失败"),error);refresh();});}
 m_count->setText(QCoreApplication::translate("M2CL", "%1 个已安装").arg(m_packs.size()));
 if(m_kind==PackKind::Shader){const auto support=shaderSupport(m_instance);QStringList names;for(const auto &component:support.components)names.append(component.section(' ',0,0));m_status->setText(support.available?QCoreApplication::translate("M2CL", "光影支持：%1 已安装").arg(names.join(" + ")):QCoreApplication::translate("M2CL", "尚未配置可用的光影支持"));m_status->setToolTip(support.summary+"\n"+support.conflicts.join("\n"));}
 else m_status->setText(QCoreApplication::translate("M2CL", "材质包 · %1 个已安装").arg(m_packs.size()));
 filterRows();

 }catch(const std::exception &e){m_status->setText(QString::fromUtf8(e.what()));}
}
void InstancePacksView::filterRows(){
 int visible=0;const auto query=m_search->text().trimmed();
 for(int r=0;r<m_packs.size();++r){const auto &p=m_packs[r];bool match=(p.name+" "+p.fileName).contains(query,Qt::CaseInsensitive);m_table->setRowHidden(r,!match);visible+=match;}
 m_table->setFixedHeight(46+qMax(1,qMin(6,visible))*76);m_empty->setVisible(!visible);
 m_empty->setText(m_packs.isEmpty()?QCoreApplication::translate("M2CL", "尚未添加资源\n从本地导入，或浏览资源市场"):QCoreApplication::translate("M2CL", "没有符合搜索条件的资源"));m_empty->setMinimumHeight(76);
}
void InstancePacksView::importLocal(){if(!allowed())return;const auto files=QFileDialog::getOpenFileNames(this,QCoreApplication::translate("M2CL", "添加资源包"),{},"ZIP (*.zip)");for(const auto &f:files){auto error=importPack(m_instance,m_kind,f);if(!error.isEmpty())QMessageBox::warning(this,QCoreApplication::translate("M2CL", "未导入"),error);}refresh();}
void InstancePacksView::installResource(const ResourceProject &project,const ResourceFile &requested){
 if(!allowed())return;
 InstallPlanDialog dialog(QCoreApplication::translate("M2CL", "安装资源"),this);dialog.resize(640,240);
 auto body=dialog.contentLayout();auto heading=new QLabel(project.name);heading->setObjectName("sectionTitle");body->addWidget(heading);
 auto busy=new BusySpinner;body->addWidget(busy,0,Qt::AlignCenter);auto notice=new QLabel(QCoreApplication::translate("M2CL", "正在准备安装…"));notice->setWordWrap(true);body->addWidget(notice);
 auto installState=new NetworkNotice;installState->setObjectName("resourceInstallNetworkState");body->addWidget(installState);
 auto report=[&](const QString &title,const QString &error){busy->hide();notice->hide();installState->failure(title,error);};
 auto install=new QPushButton(QCoreApplication::translate("M2CL", "关闭"));body->addWidget(install);connect(install,&QPushButton::clicked,&dialog,&QDialog::reject);
 ResourceClient client;client.setResourceType(m_kind==PackKind::Shader?"shader":"resourcepack");QTemporaryDir staging;
 ResourceProject selectedProject=project;QByteArray selectedIcon;ResourceFile pending;ModPlan supportPlan;bool installSupport=false;QString stagedPath;auto cancel=std::make_shared<std::atomic_bool>(false);QFutureWatcher<ModPlan> preparation;QFutureWatcher<ModInstallResult> application;
 connect(&client,&ResourceClient::iconReady,&dialog,[&](const QUrl &url,const QByteArray &bytes){if(url==project.iconUrl)selectedIcon=bytes;});
 if(!project.iconUrl.isEmpty())client.icon(project.iconUrl);
 auto finishPack=[&]{auto error=allowed()?importPack(m_instance,m_kind,stagedPath):QCoreApplication::translate("M2CL", "实例正在使用，请稍后重试");if(error.isEmpty())error=savePackMetadata(m_instance,m_kind,selectedProject,pending,selectedIcon);if(error.isEmpty()){installState->clear();notice->setText(QCoreApplication::translate("M2CL", "已安装到当前实例"));notice->show();}else report(QCoreApplication::translate("M2CL", "资源安装未完成"),error);busy->hide();install->setEnabled(true);refresh();};
 connect(&preparation,&QFutureWatcher<ModPlan>::finished,&dialog,[&]{busy->hide();supportPlan=preparation.result();if(!supportPlan.error.isEmpty()){report(QCoreApplication::translate("M2CL", "光影支持方案加载失败"),supportPlan.error);install->setEnabled(true);return;}ModPlanView confirmation(supportPlan,&dialog,{},pending);if(confirmation.exec()!=QDialog::Accepted){dialog.reject();return;}supportPlan=confirmation.selectedPlan();installSupport=true;busy->show();client.download(pending,staging.path());});
 connect(&application,&QFutureWatcher<ModInstallResult>::finished,&dialog,[&]{const auto result=application.result();if(!result.error.isEmpty()){busy->hide();report(QCoreApplication::translate("M2CL", "支持组件安装未完成"),result.error);install->setEnabled(true);return;}installState->clear();notice->setText(QCoreApplication::translate("M2CL", "已安装到当前实例"));notice->show();busy->hide();install->setEnabled(true);refresh();});
 connect(&client,&ResourceClient::downloadFailed,&dialog,[&](const QString &error){report(QCoreApplication::translate("M2CL", "资源下载未完成"),error);install->setEnabled(true);});
 auto beginInstall=[&]{installState->clear();notice->show();notice->setText(QCoreApplication::translate("M2CL", "正在准备安装…"));if(!allowed()){dialog.reject();return;}const auto file=requested;if(std::any_of(file.dependencies.cbegin(),file.dependencies.cend(),[](const ResourceDependency &d){return d.type=="required";})){notice->setText(QCoreApplication::translate("M2CL", "此资源声明了额外前置，当前不能自动核对，请先从项目主页确认。"));return;}pending=file;installSupport=false;
 if(m_kind==PackKind::Shader&&!shaderSupport(m_instance).available){
    if(m_instance.loader!="fabric"&&m_instance.loader!="neoforge"){notice->setText(QCoreApplication::translate("M2CL", "此加载器不能使用 Iris 自动方案。请先配置对应版本的光影支持组件，再安装光影包。"));busy->hide();return;}
    install->setEnabled(false);busy->show();const auto instance=m_instance;preparation.setFuture(QtConcurrent::run([instance,cancel]{ModPlan plan;try{const auto bytes=fetchInstallData(ModrinthProvider().versionsUrl("iris",instance.version,instance.loader),cancel);const auto candidates=ModrinthProvider::files(QJsonDocument::fromJson(bytes).array());ResourceFile iris;for(const auto &candidate:candidates)if(candidate.releaseType=="release"){iris=candidate;break;}if(iris.projectId.isEmpty()){plan.error=QCoreApplication::translate("M2CL", "此版本没有匹配的 Iris 正式构建，请手动配置光影支持。 ");return plan;}return planModInstall(instance,iris,cancel);}catch(const std::exception &e){plan.error=QString::fromUtf8(e.what());return plan;}}));return;
 }
 PackConfirmation confirmation(m_instance,file,m_kind,&dialog);if(confirmation.exec()!=QDialog::Accepted){dialog.reject();return;}install->setEnabled(false);busy->show();client.download(file,staging.path());};
 connect(installState,&NetworkNotice::retryRequested,&dialog,beginInstall);
 connect(&client,&ResourceClient::downloadFinished,&dialog,[&](const QString &path){stagedPath=path;const auto pack=inspectPack(path,m_kind);if(!pack.problem.isEmpty()){busy->hide();report(QCoreApplication::translate("M2CL", "资源包校验失败"),pack.problem);install->setEnabled(true);return;}if(installSupport){if(!allowed()){busy->hide();install->setEnabled(true);return;}const auto plan=supportPlan;const AttachedPack pack{stagedPath,m_kind,selectedProject,pending,selectedIcon};application.setFuture(QtConcurrent::run([plan,cancel,pack]{return applyModPlan(plan,cancel,{},nullptr,&pack);}));}else finishPack();});connect(&dialog,&QDialog::finished,&client,[&]{client.cancelDownload();});QTimer::singleShot(0,&dialog,beginInstall);dialog.exec();cancel->store(true);preparation.waitForFinished();application.waitForFinished();
}
}
