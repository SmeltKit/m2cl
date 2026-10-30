// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "networknotice.h"
#include "launcherwidgets.h"
#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QScreen>
#include <QScrollBar>
#include <QTabBar>
#include <QVBoxLayout>

namespace mc {
namespace {
class ChoicePopup final : public QFrame {
public:
    using QFrame::QFrame;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        auto border=palette().color(QPalette::Text);border.setAlpha(45);
        painter.setPen(QPen(border,1));painter.setBrush(palette().color(QPalette::Base));
        painter.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),8,8);
    }
};
}
void SelectBox::setOptionsState(bool loading, const QString &error) {
    setProperty("optionsLoading",loading);setProperty("optionsError",error);
    if(m_popup && m_popup->isVisible())showPopup();
}
void SelectBox::showPopup() {
    if (!count()) return;
    const int selectedIndex = isEditable() ? findText(currentText()) : currentIndex();
    if (m_popup) { m_popup->close(); m_popup->deleteLater(); }
    m_popupLists.clear();
    auto popup = new ChoicePopup(this, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_popup = popup;popup->setObjectName("selectPopup");popup->setProperty("choicePopup",true);
    popup->setAttribute(Qt::WA_TranslucentBackground);popup->setAccessibleName(accessibleName());popup->installEventFilter(this);
    auto body=new QVBoxLayout(popup);body->setContentsMargins(7,7,7,7);body->setSpacing(7);
    const auto mode=property("popupMode").toString();
    const bool versions=mode=="versions",grouped=mode=="loaders";
    const bool clearChoice=(versions||grouped)&&itemData(0).metaType().id()==QMetaType::QString && itemData(0).toString().isEmpty();
    auto choose=[this](int row){
        if(row<0 || row>=count() || !(model()->flags(model()->index(row,modelColumn()))&Qt::ItemIsEnabled))return;
        setCurrentIndex(row);emit activated(row);emit textActivated(itemText(row));hidePopup();
    };
    QLineEdit *search=nullptr;
    if(versions || grouped || count()>8){
        search=new QLineEdit;search->setObjectName("popupSearch");search->setPlaceholderText(versions?QCoreApplication::translate("M2CL", "搜索游戏版本…"):grouped?QCoreApplication::translate("M2CL", "搜索加载器…"):QCoreApplication::translate("M2CL", "搜索选项…"));
        search->installEventFilter(this);body->addWidget(search);
    }
    QTabBar *channels=nullptr;
    if(versions){
        channels=new QTabBar;channels->setObjectName("versionChannels");channels->setExpanding(true);channels->setDrawBase(false);
        for(const auto &entry:QList<QPair<QString,QString>>{{QCoreApplication::translate("M2CL", "全部"),"all"},{QCoreApplication::translate("M2CL", "正式版"),"release"},{QCoreApplication::translate("M2CL", "测试版"),"test"},{QCoreApplication::translate("M2CL", "特殊版本"),"special"},{QCoreApplication::translate("M2CL", "远古版本"),"historical"}}){const auto i=channels->addTab(entry.first);channels->setTabData(i,entry.second);}
        channels->setCurrentIndex(1);
        for(int i=0;i<channels->count();++i)if(selectedIndex>=0 && channels->tabData(i)==itemData(selectedIndex,PopupGroupRole))channels->setCurrentIndex(i);channels->setVisible(count()>1);body->addWidget(channels);
    }
    auto list=new QListWidget;list->setObjectName(versions?"versionChoices":"popupChoices");list->setProperty("popupList",true);list->setMinimumSize(0,0);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);list->setCursor(Qt::PointingHandCursor);list->installEventFilter(this);
    body->addWidget(list,1);m_popupLists.append(list);
    QString lastGroup="__start__";
    for(int row=0;row<count();++row){
        const auto group=itemData(row,PopupGroupRole).toString();
        if(grouped && row>0 && group!=lastGroup){
            auto heading=new QListWidgetItem(group=="common"?QCoreApplication::translate("M2CL", "常用"):QCoreApplication::translate("M2CL", "其他"),list);heading->setData(Qt::UserRole,-1);heading->setFlags(Qt::NoItemFlags);
            auto font=heading->font();font.setBold(true);heading->setFont(font);heading->setSizeHint(QSize(0,30));lastGroup=group;
        }
        auto entry=new QListWidgetItem(versions && clearChoice && row==0?QCoreApplication::translate("M2CL", "不限游戏版本"):itemText(row),list);
        entry->setData(Qt::UserRole,row);entry->setData(PopupGroupRole,group);entry->setSizeHint(QSize(0,34));entry->setToolTip(itemData(row,Qt::ToolTipRole).toString());
        if(!(model()->flags(model()->index(row,modelColumn()))&Qt::ItemIsEnabled))entry->setFlags(entry->flags()&~Qt::ItemIsEnabled);
        if(row==selectedIndex){entry->setIcon(lineIcon(Glyph::Check));list->setCurrentItem(entry);}
    }
    connect(list,&QListWidget::itemClicked,popup,[choose](QListWidgetItem *item){choose(item->data(Qt::UserRole).toInt());});
    connect(list,&QListWidget::itemActivated,popup,[choose](QListWidgetItem *item){choose(item->data(Qt::UserRole).toInt());});
    auto empty=new QLabel;empty->setObjectName("popupEmptyState");empty->setProperty("muted",true);empty->setAlignment(Qt::AlignCenter);empty->setWordWrap(true);empty->setContentsMargins(14,16,14,16);body->addWidget(empty,1);
    auto spinner=new BusySpinner;spinner->setObjectName("popupOptionsSpinner");body->addWidget(spinner,0,Qt::AlignHCenter);
    auto failure=new NetworkNotice;failure->setObjectName("popupNetworkState");body->addWidget(failure);failure->findChild<QPushButton *>("networkRetry")->setObjectName("retryVersionOptions");
    connect(failure,&NetworkNotice::retryRequested,popup,[this]{hidePopup();emit retryOptionsRequested();});
    const bool unavailable=versions && clearChoice && count()<=1;
    const bool loading=unavailable && property("optionsLoading").toBool();
    const auto error=property("optionsError").toString();
    if(search)search->setEnabled(!unavailable);
    auto filter=[=]{
        const auto query=search?search->text().trimmed():QString();const auto channel=channels?channels->tabData(channels->currentIndex()).toString():QString("all");int matches=0;
        for(int row=0;row<list->count();++row){auto item=list->item(row);const int index=item->data(Qt::UserRole).toInt();const bool reset=clearChoice&&index==0;
            const bool typeMatch=!versions || reset || channel=="all" || item->data(PopupGroupRole).toString()==channel;
            const bool visible=typeMatch && (index<0 || item->text().contains(query,Qt::CaseInsensitive));item->setHidden(!visible);if(visible&&index>=0&&!reset)++matches;
        }
        list->setVisible(!unavailable);empty->setVisible(!unavailable && matches==0);spinner->setVisible(loading);
        if(unavailable&&!loading)failure->failure(QCoreApplication::translate("M2CL", "游戏版本尚未加载"),error);else failure->clear();
        empty->setText(QCoreApplication::translate("M2CL", "没有符合条件的选项"));
    };
    if(search){connect(search,&QLineEdit::textChanged,popup,[=]{filter();});connect(search,&QLineEdit::returnPressed,popup,[=]{
        for(int row=0;row<list->count();++row){auto item=list->item(row);if(!item->isHidden()&&(item->flags()&Qt::ItemIsEnabled)&&item->data(Qt::UserRole).toInt()>=0){choose(item->data(Qt::UserRole).toInt());return;}}
    });}
    if(channels)connect(channels,&QTabBar::currentChanged,popup,[=]{filter();});filter();
    auto bounds=screen()?screen()->availableGeometry():QGuiApplication::primaryScreen()->availableGeometry();
    const auto appBounds=window()->frameGeometry().adjusted(8,8,-8,-8);if(appBounds.width()>200&&appBounds.height()>200)bounds=bounds.intersected(appBounds);
    const int desiredWidth=qMax(width(),versions?320:grouped?260:180);
    const int desiredHeight=unavailable?280:versions||grouped?390:qMin(390,count()*34+16+(search?43:0));
    const auto anchor=mapToGlobal(QPoint(0,height()+4));const int below=bounds.bottom()-anchor.y(),above=mapToGlobal(QPoint()).y()-bounds.top()-4;
    const int popupHeight=qMin(desiredHeight,qMax(80,qMax(below,above))),popupWidth=qMin(desiredWidth,bounds.width());
    const int y=below>=popupHeight?anchor.y():mapToGlobal(QPoint()).y()-popupHeight-4;
    popup->setGeometry(qBound(bounds.left(),anchor.x(),bounds.right()-popupWidth+1),qBound(bounds.top(),y,bounds.bottom()-popupHeight+1),popupWidth,popupHeight);popup->show();
    if(search&&search->isEnabled())search->setFocus();else list->setFocus();
}
void SelectBox::hidePopup() {
    if (m_popup) m_popup->hide();
    QComboBox::hidePopup();
}
bool SelectBox::eventFilter(QObject *object, QEvent *event) {
    if (m_popup && m_popup->isVisible() && event->type() == QEvent::KeyPress) {
        auto key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Escape) { hidePopup(); return true; }
        if(key==Qt::Key_Down && qobject_cast<QLineEdit *>(object) && !m_popupLists.isEmpty()){
            auto list=m_popupLists.first();list->setFocus();
            if(!list->currentItem()||list->currentItem()->isHidden())for(int row=0;row<list->count();++row)if(auto item=list->item(row);!item->isHidden()&&(item->flags()&Qt::ItemIsEnabled)){list->setCurrentItem(item);break;}
            return true;
        }
    }
    return QComboBox::eventFilter(object, event);
}
}
