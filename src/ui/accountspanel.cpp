// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "accountspanel.h"
#include "accountwidgets.h"
#include "skineditor.h"
#include "launcherwidgets.h"
#include "networknotice.h"
#include <QApplication>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace mc {
namespace {
QLabel *text(const QString &value,const char *name=nullptr){auto label=new QLabel(value);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);if(name)label->setObjectName(name);return label;}
QPushButton *action(const QString &value,const char *name,bool primary=false){auto b=new QPushButton(value);b->setObjectName(name);if(primary)b->setProperty("primary",true);return b;}
}
MicrosoftLoginDialog::MicrosoftLoginDialog(AccountManager &manager,const QString &clientId,const QString &accountId,bool refresh,QWidget *parent)
    :InstallPlanDialog(refresh?QCoreApplication::translate("M2CL", "验证登录状态"):QCoreApplication::translate("M2CL", "微软登录"),parent),m_manager(manager),m_clientId(clientId),m_accountId(accountId),m_refresh(refresh){
    setObjectName("microsoftLoginDialog");resize(560,360);auto body=contentLayout();
    body->addWidget(text(QCoreApplication::translate("M2CL", "登录 Minecraft Java 版"),"sectionTitle"));body->addWidget(text(QCoreApplication::translate("M2CL", "在微软官方网页完成验证。登录后将读取游戏资格与角色信息。")));
    auto row=new QHBoxLayout;m_spinner=new BusySpinner;row->addWidget(m_spinner);m_spinner->hide();m_stage=text(refresh?QCoreApplication::translate("M2CL", "正在准备验证登录状态"):QCoreApplication::translate("M2CL", "准备好后，点击下方按钮打开浏览器"));m_stage->setObjectName("accountLoginStage");row->addWidget(m_stage,1);body->addLayout(row);
    m_notice=new NetworkNotice;m_notice->setObjectName("accountLoginNetworkState");body->addWidget(m_notice);
    m_remember=new CheckBox(QCoreApplication::translate("M2CL", "记住登录（保存到%1）").arg(manager.vaultName()));m_remember->setObjectName("rememberMicrosoftAccount");m_remember->setChecked(manager.canRemember());m_remember->setEnabled(manager.canRemember());m_remember->setVisible(!refresh);body->addWidget(m_remember);
    if(!manager.canRemember())body->addWidget(text(QCoreApplication::translate("M2CL", "系统安全凭据库不可用，本次登录仅在启动器运行期间保留。")));
    m_browser=action(QCoreApplication::translate("M2CL", "重新打开登录网页"),"reopenMicrosoftBrowser");m_browser->setProperty("textLink",true);m_browser->hide();body->addWidget(m_browser,0,Qt::AlignLeft);
    m_config=action(QCoreApplication::translate("M2CL", "配置本项目的微软应用"),"configureMicrosoftLogin");m_config->hide();body->addWidget(m_config,0,Qt::AlignLeft);
    m_serviceHelp=action(QCoreApplication::translate("M2CL", "Minecraft 官方 API 访问申请说明 ↗"),"minecraftApplicationAccessHelp");m_serviceHelp->setProperty("textLink",true);m_serviceHelp->hide();body->addWidget(m_serviceHelp,0,Qt::AlignLeft);connect(m_serviceHelp,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://help.minecraft.net/hc/en-us/articles/16254801392141"));});
    m_once=action(QCoreApplication::translate("M2CL", "仅本次使用"),"useMicrosoftSessionOnce");m_once->hide();body->addWidget(m_once,0,Qt::AlignLeft);body->addStretch();
    auto actions=new QHBoxLayout;actions->addStretch();m_cancel=action(QCoreApplication::translate("M2CL", "取消"),"cancelMicrosoftLogin");actions->addWidget(m_cancel);m_start=action(refresh?QCoreApplication::translate("M2CL", "验证登录"):QCoreApplication::translate("M2CL", "在浏览器中登录"),"startMicrosoftLogin",true);actions->addWidget(m_start);body->addLayout(actions);
    auto openBrowser=[this]{if(m_browserUrl.scheme()=="https"&&m_browserUrl.host()=="login.microsoftonline.com"&&!qApp->property("offlineUiTest").toBool()){if(!QDesktopServices::openUrl(m_browserUrl))m_stage->setText(QCoreApplication::translate("M2CL", "未能打开浏览器，可点击“重新打开登录网页”重试"));}};
    connect(m_browser,&QPushButton::clicked,this,openBrowser);connect(m_start,&QPushButton::clicked,this,&MicrosoftLoginDialog::start);connect(m_cancel,&QPushButton::clicked,this,&MicrosoftLoginDialog::reject);connect(m_notice,&NetworkNotice::retryRequested,this,&MicrosoftLoginDialog::start);
    connect(m_config,&QPushButton::clicked,this,[this]{emit configurationRequested();reject();});connect(m_once,&QPushButton::clicked,&manager,&AccountManager::continueWithoutRemembering);
    connect(&manager,&AccountManager::browserRequested,this,[this,openBrowser](const QUrl &url){m_browserUrl=url;m_browser->show();openBrowser();});
    connect(&manager,&AccountManager::progress,this,[this](const QString &stage){m_notice->clear();m_stage->setText(stage);m_stage->show();m_spinner->show();});
    connect(&manager,&AccountManager::busyChanged,this,[this]{const auto busy=m_manager.busy();m_spinner->setVisible(busy);m_start->setEnabled(!busy);m_cancel->setEnabled(m_manager.canCancel());m_remember->setEnabled(!busy&&m_manager.canRemember());});
    connect(&manager,&AccountManager::failed,this,[this](const AuthFailure &failure){m_spinner->hide();m_stage->hide();m_browser->hide();const bool rejected=failure.code=="minecraft_app_registration_rejected";m_notice->failure(failure.title,failure.details,failure.code!="client_not_configured"&&!rejected);m_start->setEnabled(!rejected&&failure.code!="client_not_configured");m_serviceHelp->setVisible(rejected||failure.code=="minecraft_login_forbidden");m_once->setVisible(failure.code=="credential_store_failed"&&m_manager.hasPendingSession());m_config->setVisible(failure.code=="client_not_configured");if(failure.reloginRequired){m_refresh=false;m_remember->show();m_start->setText(QCoreApplication::translate("M2CL", "重新登录"));}});
    connect(&manager,&AccountManager::sessionReady,this,[this](const AuthSession &session){m_session=session;accept();});
    if(refresh)QTimer::singleShot(0,this,&MicrosoftLoginDialog::start);
    else if(!MicrosoftAuth::validClientId(clientId)){m_start->setEnabled(false);m_config->show();m_stage->hide();m_notice->failure(QCoreApplication::translate("M2CL", "此开发构建尚未配置微软登录"),QCoreApplication::translate("M2CL", "项目维护者配置 M²CL 的应用 ID 后，玩家即可直接登录。"),false);}
}
void MicrosoftLoginDialog::start(){if(m_manager.busy())return;m_notice->clear();m_once->hide();m_config->hide();m_browser->hide();m_serviceHelp->hide();m_stage->show();if(m_refresh)m_manager.ensureSession(m_accountId);else m_manager.login(m_clientId,m_remember->isChecked(),m_accountId);}
void MicrosoftLoginDialog::reject(){if(!m_manager.cancel())return;InstallPlanDialog::reject();}
QString AccountsPanel::configuredClientId(const Settings &settings){return settings.microsoftClientId.isEmpty()?QString::fromLatin1(M2CL_MICROSOFT_CLIENT_ID):settings.microsoftClientId;}
AccountsPanel::AccountsPanel(AccountManager &manager,AvatarCache &avatars,Settings &settings,SettingsStore store,QWidget *parent):QWidget(parent),m_manager(manager),m_avatars(avatars),m_settings(settings),m_store(std::move(store)){
    setObjectName("accountsPanel");auto outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);outer->addWidget(scroll);auto page=new QWidget;scroll->setWidget(page);auto body=new QVBoxLayout(page);body->setContentsMargins(0,0,0,0);body->setSpacing(16);
    auto header=new QHBoxLayout;header->addWidget(text(QCoreApplication::translate("M2CL", "账号管理"),"sectionTitle"),1);m_addOffline=action(QCoreApplication::translate("M2CL", "添加离线角色"),"addOfflineAccount");m_addMicrosoft=action(QCoreApplication::translate("M2CL", "添加微软账号"),"addMicrosoftAccount",true);m_addMicrosoft->setIcon(lineIcon(Glyph::Plus));header->addWidget(m_addOffline);header->addWidget(m_addMicrosoft);body->addLayout(header);
    auto selected=new QFrame;selected->setProperty("card",true);auto selectedRow=new QHBoxLayout(selected);selectedRow->setContentsMargins(16,14,16,14);selectedRow->setSpacing(14);
    m_currentAvatar=new AccountAvatar(avatars,48);selectedRow->addWidget(m_currentAvatar);
    auto summary=new QVBoxLayout;summary->setSpacing(4);m_current=text({});m_current->setObjectName("currentAccountSummary");auto currentFont=m_current->font();currentFont.setPixelSize(18);currentFont.setWeight(QFont::DemiBold);m_current->setFont(currentFont);summary->addWidget(m_current);
    m_currentState=text({});m_currentState->setProperty("muted",true);summary->addWidget(m_currentState);selectedRow->addLayout(summary,1);
    auto caption=text(QCoreApplication::translate("M2CL", "当前游戏角色"));caption->setProperty("muted",true);selectedRow->addWidget(caption);body->addWidget(selected);
    m_notice=new NetworkNotice;m_notice->setObjectName("accountsNetworkState");body->addWidget(m_notice);
    m_table=new RoundedTable(0,4);m_table->setObjectName("accountsTable");m_table->setHorizontalHeaderLabels({QCoreApplication::translate("M2CL", "角色"),QCoreApplication::translate("M2CL", "账号类型"),QCoreApplication::translate("M2CL", "登录状态"),QCoreApplication::translate("M2CL", "操作")});m_table->verticalHeader()->hide();m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);m_table->setSelectionBehavior(QAbstractItemView::SelectRows);m_table->setSelectionMode(QAbstractItemView::SingleSelection);m_table->setShowGrid(false);m_table->setIconSize(QSize(48,48));static_cast<RoundedTable *>(m_table)->setColumnLayout({{0,180,3},{1,120,1.5},{2,160,2},{3,88,0,Qt::AlignCenter}});body->addWidget(m_table);
    auto hint=text(QCoreApplication::translate("M2CL", "离线角色用于本地游玩；微软账号使用其真实 Java 版角色与游戏资格。"));hint->setProperty("muted",true);body->addWidget(hint);
    m_configurationToggle=action(QCoreApplication::translate("M2CL", "开发者配置 · 微软登录应用"),"toggleMicrosoftConfiguration");m_configurationToggle->setProperty("textLink",true);m_configurationToggle->setCheckable(true);m_configurationToggle->setIcon(lineIcon(Glyph::Chevron));body->addWidget(m_configurationToggle,0,Qt::AlignLeft);
    auto configuration=new QFrame;m_configuration=configuration;configuration->setProperty("card",true);configuration->setObjectName("microsoftApplicationConfiguration");auto configBody=new QVBoxLayout(configuration);configBody->setContentsMargins(16,14,16,14);auto configHint=text(QCoreApplication::translate("M2CL", "仅供本项目维护者配置。使用 M²CL 自己注册的公共客户端应用，不需要客户端密钥。"));configBody->addWidget(configHint);auto configRow=new QHBoxLayout;m_clientId=new QLineEdit(configuredClientId(settings));m_clientId->setObjectName("microsoftClientId");m_clientId->setPlaceholderText("Application (client) ID");configRow->addWidget(m_clientId,1);auto save=action(QCoreApplication::translate("M2CL", "保存配置"),"saveMicrosoftClientId");configRow->addWidget(save);configBody->addLayout(configRow);auto link=action(QCoreApplication::translate("M2CL", "打开微软应用注册说明 ↗"),"microsoftRegistrationHelp");link->setProperty("textLink",true);configBody->addWidget(link,0,Qt::AlignLeft);configuration->hide();body->addWidget(configuration);connect(m_configurationToggle,&QPushButton::toggled,configuration,&QWidget::setVisible);body->addStretch();
    connect(link,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://learn.microsoft.com/en-us/entra/identity-platform/quickstart-register-app"));});
    connect(save,&QPushButton::clicked,this,[this]{const auto id=m_clientId->text().trimmed();if(!id.isEmpty()&&!MicrosoftAuth::validClientId(id)){m_notice->failure(QCoreApplication::translate("M2CL", "应用 ID 格式不正确"),QCoreApplication::translate("M2CL", "请填写微软应用概述页中的 Application (client) ID。"),false);return;}const auto previous=m_settings.microsoftClientId;m_settings.microsoftClientId=id;QString error;if(!m_store.save(m_settings,&error)){m_settings.microsoftClientId=previous;m_notice->failure(QCoreApplication::translate("M2CL", "应用配置保存失败"),error,false);return;}m_notice->clear();m_configurationToggle->setChecked(false);});
    connect(m_addMicrosoft,&QPushButton::clicked,this,[this]{login();});connect(m_addOffline,&QPushButton::clicked,this,&AccountsPanel::addOffline);
    connect(&m_avatars,&AvatarCache::avatarChanged,this,&AccountsPanel::refresh);
    connect(&manager,&AccountManager::changed,this,&AccountsPanel::refresh);connect(&manager,&AccountManager::busyChanged,this,&AccountsPanel::refresh);
    connect(&manager,&AccountManager::failed,this,[this](const AuthFailure &failure){if(!QApplication::activeModalWidget())m_notice->failure(failure.title,failure.details,false);});refresh();
}
void AccountsPanel::refresh(){
    // Replaced index widgets are deleted later by Qt. Hide them now so that
    // their old buttons cannot remain visible at stale cell coordinates.
    for(int row=0;row<m_table->rowCount();++row)if(auto old=m_table->cellWidget(row,3))old->hide();
    const auto current=m_manager.current();m_currentAvatar->setAccount(current);m_current->setText(current.id.isEmpty()?QCoreApplication::translate("M2CL", "尚未选择角色"):current.name);
    m_currentState->setText(current.id.isEmpty()?QCoreApplication::translate("M2CL", "登录微软账号或添加离线角色"):current.microsoft?QCoreApplication::translate("M2CL", "微软账号 · %1").arg(m_manager.state(current)):QCoreApplication::translate("M2CL", "离线角色 · 本地游玩"));
    const auto accounts=m_manager.accounts();m_table->setRowCount(accounts.size());m_table->setFixedHeight(46+qMax(1,qMin(5,int(accounts.size())))*76);m_addMicrosoft->setEnabled(!m_manager.busy());m_addOffline->setEnabled(!m_manager.busy());m_configuration->setEnabled(!m_manager.busy());m_configurationToggle->setEnabled(!m_manager.busy());
    if(!m_manager.loadError().isEmpty())m_notice->failure(QCoreApplication::translate("M2CL", "账号列表需要处理"),m_manager.loadError(),false);
    for(int row=0;row<accounts.size();++row){const auto a=accounts.at(row);m_table->setRowHeight(row,76);auto name=new QTableWidgetItem(a.name+(a.id==current.id?QCoreApplication::translate("M2CL", " · 当前"):QString()));name->setIcon(QIcon(m_avatars.avatar(a).scaled(48,48,Qt::IgnoreAspectRatio,Qt::FastTransformation)));auto nameFont=name->font();nameFont.setPixelSize(15);nameFont.setWeight(QFont::DemiBold);name->setFont(nameFont);name->setToolTip(a.uuid);m_table->setItem(row,0,name);m_table->setItem(row,1,new QTableWidgetItem(a.microsoft?QStringLiteral("Microsoft"):QCoreApplication::translate("M2CL", "离线")));m_table->setItem(row,2,new QTableWidgetItem(m_manager.state(a)));
        auto more=action({},"accountMore");more->setIcon(lineIcon(Glyph::More));more->setProperty("flatAction",true);more->setEnabled(!m_manager.busy());more->setFixedSize(32,32);auto operation=new QWidget;auto operationLayout=new QHBoxLayout(operation);operationLayout->setContentsMargins(16,0,16,0);operationLayout->addWidget(more,0,Qt::AlignCenter);m_table->setCellWidget(row,3,operation);auto menu=new RoundedMenu(more);
        auto use=menu->addAction(lineIcon(Glyph::Check),QCoreApplication::translate("M2CL", "使用此角色"));use->setEnabled(a.id!=current.id);connect(use,&QAction::triggered,this,[this,a]{const auto error=m_manager.select(a.id);if(error.isEmpty())m_notice->clear();else m_notice->failure(QCoreApplication::translate("M2CL", "无法切换账号"),error,false);});
        if(!a.microsoft)connect(menu->addAction(lineIcon(Glyph::Image),QCoreApplication::translate("M2CL","编辑皮肤")),&QAction::triggered,this,[this,a]{editSkin(a.id);});
        if(a.microsoft)connect(menu->addAction(lineIcon(Glyph::Refresh),QCoreApplication::translate("M2CL", "重新登录")),&QAction::triggered,this,[this,a]{login(a.id);});
        connect(menu->addAction(lineIcon(Glyph::Trash),QCoreApplication::translate("M2CL", "移除账号")),&QAction::triggered,this,[this,a]{InstallPlanDialog confirm(QCoreApplication::translate("M2CL", "移除账号"),this);auto layout=confirm.contentLayout();layout->addWidget(text(QCoreApplication::translate("M2CL", "移除 %1？").arg(a.name),"sectionTitle"));layout->addWidget(text(QCoreApplication::translate("M2CL", "将移除启动器中的账号记录及已保存登录凭据。游戏实例与存档不受影响。")));auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText(QCoreApplication::translate("M2CL", "移除"));buttons->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("M2CL", "取消"));layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&confirm,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&confirm,&QDialog::reject);if(confirm.exec()==QDialog::Accepted){m_notice->clear();m_manager.remove(a.id);}});
        connect(more,&QPushButton::clicked,this,[more,menu]{menu->popup(more->mapToGlobal(QPoint(0,more->height())));});
    }
}
void AccountsPanel::showConfiguration(){m_configurationToggle->setChecked(true);m_clientId->setFocus();}
void AccountsPanel::editSkin(const QString &id){for(const auto &account:m_manager.accounts())if(account.id==id&&!account.microsoft){SkinEditor dialog(m_manager,account,m_store.directory(),this);dialog.exec();return;}}
void AccountsPanel::login(const QString &id){MicrosoftLoginDialog dialog(m_manager,configuredClientId(m_settings),id,false,this);connect(&dialog,&MicrosoftLoginDialog::configurationRequested,this,[this]{emit configurationRequested();showConfiguration();});dialog.exec();refresh();}
void AccountsPanel::addOffline(){
    InstallPlanDialog dialog(QCoreApplication::translate("M2CL", "添加离线角色"),this);dialog.setObjectName("addOfflinePlayerDialog");dialog.setMinimumWidth(420);dialog.resize(460,270);
    auto body=dialog.contentLayout();body->setSpacing(14);
    auto row=new QHBoxLayout;row->setSpacing(16);auto avatar=new AccountAvatar(m_avatars,56);row->addWidget(avatar,0,Qt::AlignTop);
    auto fields=new QWidget;fields->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);auto form=new QVBoxLayout(fields);form->setContentsMargins(0,0,0,0);form->setSpacing(7);
    auto caption=text(QCoreApplication::translate("M2CL", "玩家名"));caption->setProperty("fieldLabel",true);form->addWidget(caption);
    auto input=new QLineEdit;input->setObjectName("newOfflinePlayer");input->setMaxLength(16);input->setPlaceholderText(QCoreApplication::translate("M2CL", "输入玩家名"));form->addWidget(input);
    auto hint=text(QCoreApplication::translate("M2CL", "1–16 位英文字母、数字或下划线"));hint->setProperty("muted",true);form->addWidget(hint);row->addWidget(fields,1);body->addLayout(row);
    auto error=new NetworkNotice;body->addWidget(error);body->addStretch();
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);auto add=buttons->button(QDialogButtonBox::Ok);add->setText(QCoreApplication::translate("M2CL", "添加并使用"));add->setProperty("primary",true);add->setEnabled(false);buttons->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("M2CL", "取消"));body->addWidget(buttons);
    connect(input,&QLineEdit::textChanged,&dialog,[=](const QString &name){error->clear();add->setEnabled(validPlayerName(name.trimmed()));});
    connect(input,&QLineEdit::returnPressed,add,&QPushButton::click);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{const auto problem=m_manager.addOffline(input->text().trimmed());if(problem.isEmpty())dialog.accept();else error->failure(QCoreApplication::translate("M2CL", "无法添加角色"),problem,false);});
    input->setFocus();dialog.exec();
}
}
