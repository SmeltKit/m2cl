// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "installplandialog.h"
#include "macwindow.h"
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
namespace mc {
namespace {
class TargetInstanceBanner final : public QFrame {
public:
    explicit TargetInstanceBanner(const QPixmap &cover):m_cover(cover) {}
    QWidget *information=nullptr;
protected:
    void resizeEvent(QResizeEvent *event) override {
        QFrame::resizeEvent(event);
        if(information)information->setMaximumWidth(m_cover.isNull()?QWIDGETSIZE_MAX:qMax(240,int(width()*.60)-48));
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;clip.addRoundedRect(QRectF(rect()),8,8);painter.setClipPath(clip);
        const QColor base=palette().color(QPalette::Window);painter.fillRect(rect(),base);
        if(m_cover.isNull())return;
        const QRectF area(width()*.40,0,width()*.60,height());
        const qreal scale=qMax(area.width()/m_cover.width(),area.height()/m_cover.height());
        const QSizeF visible(area.width()/scale,area.height()/scale);
        const QRectF source((m_cover.width()-visible.width())/2,(m_cover.height()-visible.height())/2,visible.width(),visible.height());
        painter.setRenderHint(QPainter::SmoothPixmapTransform);painter.drawPixmap(area,m_cover,source);
        QLinearGradient fade(0,0,width(),0);auto tint=base;
        fade.setColorAt(0,base);fade.setColorAt(.48,base);tint.setAlphaF(.94);fade.setColorAt(.59,tint);
        tint.setAlphaF(.30);fade.setColorAt(.82,tint);tint.setAlphaF(.16);fade.setColorAt(1,tint);
        painter.fillRect(rect(),fade);
    }
private:
    QPixmap m_cover;
};
}

InstallPlanDialog::InstallPlanDialog(QWidget *parent):InstallPlanDialog(QCoreApplication::translate("M2CL", "确认安装方案"),parent){}
InstallPlanDialog::InstallPlanDialog(const QString &caption,QWidget *parent):QDialog(parent){
    setWindowTitle(caption);
    // Native decorations: macOS traffic lights; Windows/Linux native close button.
    setWindowFlags(Qt::Dialog|Qt::WindowTitleHint|Qt::WindowSystemMenuHint|Qt::WindowCloseButtonHint);
    setMinimumWidth(640);
    setProperty("installationDialog",true);
    auto outer=new QVBoxLayout(this);outer->setContentsMargins(22,18,22,18);outer->setSpacing(12);
    auto title=new QLabel(caption);title->setObjectName("sectionTitle");title->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);outer->addWidget(title);
    auto divider=new QFrame;divider->setObjectName("installTitleDivider");divider->setProperty("settingsDivider",true);outer->addWidget(divider);
    m_content=new QVBoxLayout;m_content->setSpacing(8);outer->addLayout(m_content,1);
}
void InstallPlanDialog::addTargetInstance(const QString &name,const QString &configuration,const QPixmap &cover,const QString &captionText){
    auto target=new TargetInstanceBanner(cover);target->setObjectName("installTarget");
    auto layout=new QVBoxLayout(target);layout->setContentsMargins(16,12,16,14);layout->setSpacing(10);
    if(!captionText.isEmpty()){auto caption=new QLabel(captionText);caption->setProperty("muted",true);layout->addWidget(caption);}
    auto row=new QHBoxLayout;row->setSpacing(14);
    if(!captionText.isEmpty()){auto accent=new QFrame;accent->setObjectName("installTargetAccent");accent->setFixedWidth(3);row->addWidget(accent);}
    auto infoHost=new QWidget;target->information=infoHost;auto information=new QVBoxLayout(infoHost);information->setContentsMargins(0,0,0,0);information->setSpacing(4);
    auto title=new QLabel(name);title->setObjectName("installTargetName");title->setTextFormat(Qt::PlainText);title->setWordWrap(true);information->addWidget(title);
    auto detail=new QLabel(configuration);detail->setTextFormat(Qt::PlainText);detail->setWordWrap(true);detail->setProperty("muted",true);information->addWidget(detail);
    row->addWidget(infoHost,1);row->addStretch();layout->addLayout(row);m_content->addWidget(target);
}
QVBoxLayout *InstallPlanDialog::section(QVBoxLayout *parent,const QString &title,const char *tone){
    auto frame=new QFrame;frame->setProperty("installSection",QString::fromLatin1(tone));
    auto body=new QVBoxLayout(frame);body->setContentsMargins(12,10,12,10);body->setSpacing(0);
    auto heading=new QLabel(title);heading->setProperty("installSectionTitle",true);body->addWidget(heading);
    parent->addWidget(frame);return body;
}
QHBoxLayout *InstallPlanDialog::planRow(){
    auto frame=new QFrame;frame->setProperty("installRow",true);frame->setMinimumHeight(34);
    auto row=new QHBoxLayout(frame);row->setContentsMargins(6,3,6,3);row->setSpacing(14);
    return row;
}
void InstallPlanDialog::showEvent(QShowEvent *event){
    QDialog::showEvent(event);
#ifdef Q_OS_MACOS
    hideMacWindowCaption(this);
#endif
}
}
