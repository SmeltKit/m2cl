// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "accountwidgets.h"
#include "launcherwidgets.h"
#include "networknotice.h"
#include "network/accountmanager.h"
#include "core/skins.h"
#include <QApplication>
#include <QBoxLayout>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QListWidget>
#include <QScreen>
#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <memory>

namespace mc {
namespace {
constexpr int MaxSkinBytes=1024*1024;
bool officialSkin(const QUrl &url) {
    return url.scheme()=="https" && url.host()=="textures.minecraft.net"
        && url.userInfo().isEmpty() && url.port(-1)==-1 && url.query().isEmpty()
        && url.fragment().isEmpty() && QRegularExpression("^/texture/[0-9a-fA-F]{32,128}$").match(url.path()).hasMatch();
}
}
AvatarCache::AvatarCache(const QString &directory,QObject *parent,QNetworkAccessManager *network)
    :QObject(parent),m_directory(QDir(directory).filePath("cache/avatars")),m_skinDirectory(directory),
      m_network(network?network:new QNetworkAccessManager(this)) {}
QPixmap AvatarCache::defaultAvatar() {
    // A small neutral block character, also used when a profile has no skin.
    static const QPixmap face=[] {
        QImage image(8,8,QImage::Format_ARGB32); image.fill(QColor("#bd8d70"));
        QPainter p(&image);p.fillRect(0,0,8,2,QColor("#57483b"));p.fillRect(0,2,1,1,QColor("#57483b"));p.fillRect(7,2,1,1,QColor("#57483b"));
        p.fillRect(1,4,2,1,QColor("#e6e8dd"));p.fillRect(5,4,2,1,QColor("#e6e8dd"));
        p.fillRect(2,4,1,1,QColor("#385a54"));p.fillRect(5,4,1,1,QColor("#385a54"));
        p.fillRect(3,5,2,1,QColor("#aa745d"));p.fillRect(2,6,4,2,QColor("#775442"));p.fillRect(3,6,2,1,QColor("#c59677"));
        p.end();return QPixmap::fromImage(image);
    }();return face;
}
QPixmap AvatarCache::skinFace(const QByteArray &png) {
    if(png.size()>MaxSkinBytes)return {};
    QBuffer buffer;buffer.setData(png);buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer,"png");const auto size=reader.size();
    if(size.width()<64||size.width()>1024||size.width()%64
       ||(size.height()!=size.width()&&size.height()*2!=size.width()))return {};
    const auto skin=reader.read();if(skin.isNull())return {};
    const int unit=size.width()/64;
    auto face=skin.copy(8*unit,8*unit,8*unit,8*unit);
    QPainter painter(&face);painter.drawImage(0,0,skin.copy(40*unit,8*unit,8*unit,8*unit));painter.end();
    return QPixmap::fromImage(face);
}
QPixmap AvatarCache::avatar(const Account &account) {
    if(!account.microsoft&&skinSource(account.offlineSkin)!="default"){
        const auto face=skinFace(SkinStore(m_skinDirectory).texture(account.offlineSkin));return face.isNull()?defaultAvatar():face;
    }
    if(!account.microsoft||!officialSkin(account.skinUrl))return defaultAvatar();
    const auto key=QString::fromLatin1(QCryptographicHash::hash(account.skinUrl.toEncoded(),QCryptographicHash::Sha256).toHex());
    if(m_faces.contains(key))return m_faces.value(key);
    if(m_attempted.contains(key))return defaultAvatar();
    m_attempted.insert(key);
    const auto path=QDir(m_directory).filePath(key+".png");QFile cached(path);
    if(cached.open(QIODevice::ReadOnly)&&cached.size()<=MaxSkinBytes){const auto face=skinFace(cached.readAll());if(!face.isNull()){m_faces.insert(key,face);return face;}}
    if(qApp->property("offlineUiTest").toBool()||qApp->arguments().contains("--smoke-test"))return defaultAvatar();
    QNetworkRequest request(account.skinUrl);request.setRawHeader("User-Agent","M2CL/0.1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto reply=m_network->get(request);auto bytes=std::make_shared<QByteArray>();
    auto drain=[reply,bytes]{*bytes+=reply->readAll();if(bytes->size()>MaxSkinBytes)reply->abort();};
    connect(reply,&QNetworkReply::readyRead,this,drain);
    auto timeout=new QTimer(reply);timeout->setSingleShot(true);connect(timeout,&QTimer::timeout,reply,&QNetworkReply::abort);timeout->start(8000);
    connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,key,path,drain]{
        drain();const auto face=reply->error()==QNetworkReply::NoError&&reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200?skinFace(*bytes):QPixmap();
        reply->deleteLater();if(face.isNull())return;
        m_faces.insert(key,face);
        if(QDir().mkpath(m_directory)){QSaveFile file(path);if(file.open(QIODevice::WriteOnly)&&file.write(*bytes)==bytes->size())file.commit();}
        emit avatarChanged();
    });
    return defaultAvatar();
}
AccountAvatar::AccountAvatar(AvatarCache &cache,int size,QWidget *parent):QWidget(parent),m_cache(cache) {
    setFixedSize(size,size);connect(&cache,&AvatarCache::avatarChanged,this,[this]{setAccount(m_account);});
    setAccessibleName(QCoreApplication::translate("M2CL","角色头像"));setAccount({});
}
void AccountAvatar::setAccount(const Account &account){m_account=account;m_face=m_cache.avatar(account);update();}
void AccountAvatar::setEditable(bool value){m_editable=value;setCursor(value?Qt::PointingHandCursor:Qt::ArrowCursor);setFocusPolicy(value?Qt::StrongFocus:Qt::NoFocus);setToolTip(value?QCoreApplication::translate("M2CL","编辑皮肤"):QString());}
void AccountAvatar::mouseReleaseEvent(QMouseEvent *event){if(m_editable&&event->button()==Qt::LeftButton&&rect().contains(event->pos()))emit activated();QWidget::mouseReleaseEvent(event);}
void AccountAvatar::keyPressEvent(QKeyEvent *event){if(m_editable&&(event->key()==Qt::Key_Space||event->key()==Qt::Key_Return)){emit activated();event->accept();return;}QWidget::keyPressEvent(event);}
void AccountAvatar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform,false);p.drawPixmap(rect(),m_face);
}

HomeAccountCard::HomeAccountCard(AccountManager &manager,AvatarCache &cache,QWidget *parent)
    :QFrame(parent),m_manager(manager),m_cache(cache) {
    setObjectName("homeAccountCard");setProperty("card",true);setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    auto body=new QVBoxLayout(this);body->setSizeConstraint(QLayout::SetNoConstraint);body->setContentsMargins(20,16,20,16);body->setSpacing(10);
    m_row=new QBoxLayout(QBoxLayout::LeftToRight);m_row->setSpacing(20);body->addLayout(m_row);
    m_identity=new QWidget;auto identity=new QHBoxLayout(m_identity);identity->setContentsMargins(0,0,0,0);identity->setSpacing(14);
    m_avatar=new AccountAvatar(cache);m_avatar->setObjectName("homeAccountAvatar");identity->addWidget(m_avatar);
    auto description=new QVBoxLayout;description->setSpacing(4);m_name=new QLabel;m_name->setTextFormat(Qt::PlainText);m_name->setObjectName("homePlayerName");
    m_name->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);description->addWidget(m_name);
    m_state=new QLabel;m_state->setTextFormat(Qt::PlainText);m_state->setObjectName("homeAccountState");m_state->setProperty("muted",true);description->addWidget(m_state);
    identity->addLayout(description);identity->addStretch();
    m_relogin=new QPushButton(QCoreApplication::translate("M2CL","重新登录"));m_relogin->setObjectName("homeAccountRelogin");m_relogin->setProperty("textLink",true);identity->addWidget(m_relogin);
    m_row->addWidget(m_identity,1);
    m_actions=new QWidget;auto actions=new QHBoxLayout(m_actions);actions->setContentsMargins(0,0,0,0);actions->setSpacing(10);
    auto modes=new QFrame;modes->setObjectName("homeAccountModes");auto modeLayout=new QHBoxLayout(modes);modeLayout->setContentsMargins(2,2,2,2);modeLayout->setSpacing(0);
    m_microsoft=new QPushButton(QCoreApplication::translate("M2CL","微软账号"));m_microsoft->setObjectName("homeMicrosoftMode");
    m_offline=new QPushButton(QCoreApplication::translate("M2CL","离线角色"));m_offline->setObjectName("homeOfflineMode");
    for(auto mode:{m_microsoft,m_offline}){mode->setCheckable(true);mode->setProperty("accountMode",true);modeLayout->addWidget(mode);}
    actions->addWidget(modes);
    m_switch=new QPushButton(QCoreApplication::translate("M2CL","切换角色 ▾"));m_switch->setObjectName("homeSwitchAccount");actions->addWidget(m_switch);
    m_manage=new QPushButton(QCoreApplication::translate("M2CL","管理账号"));m_manage->setIcon(lineIcon(Glyph::Settings));m_manage->setObjectName("homeManageAccounts");m_manage->setAccessibleName(QCoreApplication::translate("M2CL","管理账号"));m_manage->setToolTip(m_manage->accessibleName());actions->addWidget(m_manage);
    m_row->addWidget(m_actions);
    m_notice=new NetworkNotice;m_notice->setObjectName("homeAccountNotice");body->addWidget(m_notice);
    connect(m_microsoft,&QPushButton::clicked,this,[this]{chooseType(true);});connect(m_offline,&QPushButton::clicked,this,[this]{chooseType(false);});
    connect(m_switch,&QPushButton::clicked,this,&HomeAccountCard::showAccounts);
    connect(m_manage,&QPushButton::clicked,this,&HomeAccountCard::managementRequested);
    connect(m_avatar,&AccountAvatar::activated,this,[this]{const auto a=m_manager.current();if(!a.microsoft&&!a.id.isEmpty())emit skinEditRequested(a.id);});
    connect(m_relogin,&QPushButton::clicked,this,[this]{emit microsoftLoginRequested(m_manager.current().id);});
    connect(&manager,&AccountManager::changed,this,[this]{m_notice->clear();refresh();});connect(&manager,&AccountManager::busyChanged,this,&HomeAccountCard::refresh);
    auto clock=new QTimer(this);clock->setInterval(30000);connect(clock,&QTimer::timeout,this,&HomeAccountCard::refresh);clock->start();refresh();
}
void HomeAccountCard::refresh() {
    const auto a=m_manager.current();const bool has=!a.id.isEmpty();
    if(has){if(a.microsoft)m_lastMicrosoft=a.id;else m_lastOffline=a.id;}
    m_avatar->setAccount(a);m_avatar->setEditable(has&&!a.microsoft&&!m_manager.busy());m_name->setText(has?a.name:QCoreApplication::translate("M2CL","选择游戏角色"));
    m_state->setText(!has?QCoreApplication::translate("M2CL","登录微软账号或添加离线角色"):
        a.microsoft?QCoreApplication::translate("M2CL","微软账号 · %1").arg(m_manager.state(a)):QCoreApplication::translate("M2CL","离线角色 · 本地游玩"));
    m_state->setToolTip(m_state->text());m_microsoft->setChecked(has&&a.microsoft);m_offline->setChecked(has&&!a.microsoft);
    m_relogin->setVisible(has&&m_manager.requiresLogin(a));
    for(auto button:{m_microsoft,m_offline,m_switch,m_manage,m_relogin})button->setEnabled(!m_manager.busy());
    if(!m_manager.loadError().isEmpty())m_notice->failure(QCoreApplication::translate("M2CL","账号列表需要处理"),m_manager.loadError(),false);
    updateGeometry();
}
void HomeAccountCard::resizeEvent(QResizeEvent *event) {
    QFrame::resizeEvent(event);
    const bool compact=width()<800;
    m_manage->setText(compact?QString():QCoreApplication::translate("M2CL","管理账号"));
    m_manage->setMinimumWidth(compact?38:0);m_manage->setMaximumWidth(compact?38:QWIDGETSIZE_MAX);
    for(auto mode:{m_microsoft,m_offline})mode->setMinimumWidth(mode->fontMetrics().horizontalAdvance(mode->text())+26);
    const bool stacked=width()<m_actions->sizeHint().width()+m_identity->minimumSizeHint().width()+80;
    m_row->setDirection(stacked?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight);
    m_row->setAlignment(m_actions,stacked?Qt::AlignLeft:Qt::Alignment{});
}
void HomeAccountCard::select(const QString &id) {
    const auto error=m_manager.select(id);if(error.isEmpty())m_notice->clear();else m_notice->failure(QCoreApplication::translate("M2CL","无法切换账号"),error,false);
    refresh();
}
void HomeAccountCard::chooseType(bool microsoft) {
    const auto current=m_manager.current();
    if(!current.id.isEmpty()&&current.microsoft==microsoft){refresh();return;}
    const auto previous=microsoft?m_lastMicrosoft:m_lastOffline;QString candidate;
    for(const auto &a:m_manager.accounts())if(a.microsoft==microsoft){if(candidate.isEmpty()||a.id==previous)candidate=a.id;}
    if(candidate.isEmpty()){refresh();if(microsoft)emit microsoftLoginRequested({});else emit offlineCreationRequested();}
    else select(candidate);
}
void HomeAccountCard::showAccounts() {
    if(m_picker){m_picker->close();m_picker->deleteLater();}
    auto popup=new RoundedPopup(this);m_picker=popup;popup->setObjectName("homeAccountMenu");
    popup->setAccessibleName(QCoreApplication::translate("M2CL","切换角色"));
    auto body=new QVBoxLayout(popup);body->setContentsMargins(9,9,9,9);body->setSpacing(8);
    auto list=new QListWidget;list->setObjectName("homeAccountChoices");list->setProperty("accountPickerList",true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);list->setFrameShape(QFrame::NoFrame);
    list->setCursor(Qt::PointingHandCursor);list->setFocusPolicy(Qt::StrongFocus);body->addWidget(list);
    const auto current=m_manager.current();const auto accounts=m_manager.accounts();
    constexpr int rowHeight=78;
    for(const auto &a:accounts){
        auto item=new QListWidgetItem(list);item->setData(Qt::UserRole,a.id);item->setData(Qt::AccessibleTextRole,a.name);item->setSizeHint(QSize(0,rowHeight));
        auto row=new QWidget;row->setAttribute(Qt::WA_TransparentForMouseEvents);auto cells=new QHBoxLayout(row);cells->setContentsMargins(12,10,12,10);cells->setSpacing(12);
        auto avatar=new AccountAvatar(m_cache,36);avatar->setAccount(a);cells->addWidget(avatar);
        auto labels=new QVBoxLayout;labels->setSpacing(6);labels->setAlignment(Qt::AlignVCenter);auto name=new QLabel(a.name);name->setTextFormat(Qt::PlainText);name->setProperty("accountPickerName",true);name->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);labels->addWidget(name);
        auto detail=new QLabel(a.microsoft?QCoreApplication::translate("M2CL","微软账号 · %1").arg(m_manager.state(a)):QCoreApplication::translate("M2CL","离线角色 · 本地游玩"));
        detail->setProperty("muted",true);detail->setProperty("accountPickerDetail",true);detail->setTextFormat(Qt::PlainText);detail->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);detail->setToolTip(detail->text());labels->addWidget(detail);cells->addLayout(labels,1);
        for(auto label:{name,detail}){label->ensurePolished();label->setIndent(0);label->setContentsMargins(0,2,0,2);label->setMinimumHeight(label->fontMetrics().height()+4);}
        if(a.id==current.id){cells->addWidget(new GlyphView(Glyph::Check,18));list->setCurrentItem(item);}
        list->setItemWidget(item,row);
    }
    list->setFixedHeight(qMax(rowHeight,qMin(4,int(accounts.size()))*rowHeight+4));list->setVisible(!accounts.isEmpty());
    auto choose=[this,popup](QListWidgetItem *item){const auto id=item->data(Qt::UserRole).toString();popup->close();select(id);};
    connect(list,&QListWidget::itemClicked,popup,choose);connect(list,&QListWidget::itemActivated,popup,choose);
    auto rule=new QFrame;rule->setProperty("settingsDivider",true);rule->setVisible(!accounts.isEmpty());body->addWidget(rule);
    auto addMicrosoft=new QPushButton(QCoreApplication::translate("M2CL","添加微软账号"));addMicrosoft->setObjectName("pickerAddMicrosoft");
    auto addOffline=new QPushButton(QCoreApplication::translate("M2CL","添加离线角色"));addOffline->setObjectName("pickerAddOffline");
    for(auto button:{addMicrosoft,addOffline}){button->setProperty("accountPickerAction",true);button->setIcon(lineIcon(Glyph::Plus));body->addWidget(button);}
    connect(addMicrosoft,&QPushButton::clicked,popup,[this,popup]{popup->close();emit microsoftLoginRequested({});});
    connect(addOffline,&QPushButton::clicked,popup,[this,popup]{popup->close();emit offlineCreationRequested();});
    const auto bounds=screen()?screen()->availableGeometry():QGuiApplication::primaryScreen()->availableGeometry();
    popup->setFixedWidth(qMin(370,bounds.width()-16));popup->adjustSize();
    const auto anchor=m_switch->mapToGlobal(QPoint(m_switch->width(),m_switch->height()+6));
    const auto y=anchor.y()+popup->height()<=bounds.bottom()?anchor.y():m_switch->mapToGlobal(QPoint()).y()-popup->height()-6;
    popup->move(qBound(bounds.left()+8,anchor.x()-popup->width(),bounds.right()-popup->width()-8),qBound(bounds.top()+8,y,bounds.bottom()-popup->height()-8));
    popup->show();list->setFocus();
}
}
