// Copyright (C) 2026 ppm203; GPL-3.0-only with ADDITIONAL_TERMS.md.
#include "skineditor.h"
#include "launcherwidgets.h"
#include "networknotice.h"
#include "network/accountmanager.h"
#include <QApplication>
#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTransform>
#include <algorithm>
#include <array>
#include <cmath>
namespace mc {
class SkinPreview final:public QWidget {
public:
    explicit SkinPreview(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(220,290);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);setCursor(Qt::OpenHandCursor);setAccessibleName(QCoreApplication::translate("M2CL","皮肤模型预览"));}
    void setSkin(const QImage &image,bool slim){m_skin=expandedSkin(image);m_slim=slim;update();}
protected:
    void mousePressEvent(QMouseEvent *event)override{m_last=event->pos();setCursor(Qt::ClosedHandCursor);}
    void mouseReleaseEvent(QMouseEvent *)override{setCursor(Qt::OpenHandCursor);}
    void mouseMoveEvent(QMouseEvent *event)override{if(event->buttons()&Qt::LeftButton){m_yaw+=(event->pos().x()-m_last.x())*.012;m_last=event->pos();update();}}
    void paintEvent(QPaintEvent *)override{
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);auto background=palette().color(QPalette::AlternateBase);painter.setPen(Qt::NoPen);painter.setBrush(background);painter.drawRoundedRect(rect(),8,8);
        if(m_skin.isNull())return;
        struct Point{double x,y,z;};struct Face{QPolygonF quad;QRect texture;double depth;int shade;};QList<Face> faces;
        const double scale=qMin(width()/22.,(height()-38)/35.);const double c=std::cos(m_yaw),s=std::sin(m_yaw);
        auto transform=[&](Point p){const double z=p.x*s+p.z*c;return Point{p.x*c-p.z*s,p.y-z*.12,z+p.y*.12};};
        auto cube=[&](double x,double y,double z,int w,int h,int d,int u,int v,double expand=0){
            const double l=x-expand,r=x+w+expand,t=y-expand,b=y+h+expand,f=z-expand,back=z+d+expand;
            const std::array<Point,8> points{{{l,t,f},{r,t,f},{r,b,f},{l,b,f},{l,t,back},{r,t,back},{r,b,back},{l,b,back}}};
            const std::array<std::array<int,4>,6> indices{{{{0,1,2,3}},{{5,4,7,6}},{{4,0,3,7}},{{1,5,6,2}},{{4,5,1,0}},{{3,2,6,7}}}};
            const std::array<QRect,6> textures{{QRect(u+d,v+d,w,h),QRect(u+2*d+w,v+d,w,h),QRect(u,v+d,d,h),QRect(u+d+w,v+d,d,h),QRect(u+d,v,w,d),QRect(u+d+w,v,w,d)}};
            for(int side=0;side<6;++side){Face face;face.texture=textures[side];face.depth=0;face.shade=side==4?0:side==0?8:side==1?24:40;
                for(int i:indices[side]){const auto p=transform(points[i]);face.quad<<QPointF(width()/2.+p.x*scale,18+p.y*scale);face.depth+=p.z;}
                // Only outward-facing polygons are visible; transparent overlay backs must not bleed through.
                const auto a=face.quad[1]-face.quad[0],b2=face.quad[2]-face.quad[1];if(a.x()*b2.y()-a.y()*b2.x()>0)faces.append(face);
            }
        };
        cube(-4,0,-4,8,8,8,0,0);cube(-4,8,-2,8,12,4,16,16);
        const int arm=m_slim?3:4;cube(-4-arm,8,-2,arm,12,4,40,16);cube(4,8,-2,arm,12,4,32,48);
        cube(-4,20,-2,4,12,4,0,16);cube(0,20,-2,4,12,4,16,48);
        cube(-4,0,-4,8,8,8,32,0,.25);cube(-4,8,-2,8,12,4,16,32,.15);
        cube(-4-arm,8,-2,arm,12,4,40,32,.15);cube(4,8,-2,arm,12,4,48,48,.15);
        cube(-4,20,-2,4,12,4,0,32,.15);cube(0,20,-2,4,12,4,0,48,.15);
        std::stable_sort(faces.begin(),faces.end(),[](const Face &a,const Face &b){return a.depth>b.depth;});painter.setRenderHint(QPainter::SmoothPixmapTransform,false);
        for(const auto &face:faces){const auto pixels=m_skin.copy(face.texture);QPolygonF source;source<<QPointF(0,0)<<QPointF(pixels.width(),0)<<QPointF(pixels.width(),pixels.height())<<QPointF(0,pixels.height());QTransform mapping;
            if(!QTransform::quadToQuad(source,face.quad,mapping))continue;painter.save();painter.setTransform(mapping);painter.drawImage(0,0,pixels);if(face.shade){QImage shade(pixels.size(),QImage::Format_ARGB32);shade.fill(QColor(0,0,0,face.shade));QPainter mask(&shade);mask.setCompositionMode(QPainter::CompositionMode_DestinationIn);mask.drawImage(0,0,pixels);mask.end();painter.drawImage(0,0,shade);}painter.restore();
        }
    }
private:
    QImage m_skin;bool m_slim=false;double m_yaw=-.27;QPoint m_last;
};
SkinEditor::SkinEditor(AccountManager &manager,const Account &account,const QString &directory,QWidget *parent)
    :InstallPlanDialog(QCoreApplication::translate("M2CL","编辑皮肤"),parent),m_manager(manager),m_account(account),m_store(directory){
    setObjectName("skinEditor");resize(680,480);auto body=contentLayout();body->setSpacing(16);
    auto caption=new QLabel(account.name);caption->setTextFormat(Qt::PlainText);caption->setObjectName("sectionTitle");body->addWidget(caption);
    auto columns=new QHBoxLayout;columns->setSpacing(22);auto previewColumn=new QVBoxLayout;m_preview=new SkinPreview;m_preview->setObjectName("skinPreview");previewColumn->addWidget(m_preview,1);
    auto drag=new QLabel(QCoreApplication::translate("M2CL","拖动预览以旋转"));drag->setProperty("muted",true);drag->setAlignment(Qt::AlignCenter);previewColumn->addWidget(drag);columns->addLayout(previewColumn,1);
    auto form=new QVBoxLayout;form->setSpacing(12);form->addWidget(new QLabel(QCoreApplication::translate("M2CL","皮肤来源")));
    m_source=new SelectBox;m_source->setObjectName("offlineSkinSource");m_source->addItem(QCoreApplication::translate("M2CL","游戏默认"),"default");m_source->addItem("Steve","steve");m_source->addItem("Alex","alex");m_source->addItem(QCoreApplication::translate("M2CL","本地 PNG 皮肤"),"local");form->addWidget(m_source);
    form->addWidget(new QLabel(QCoreApplication::translate("M2CL","手臂模型")));m_model=new SelectBox;m_model->setObjectName("offlineSkinModel");m_model->addItem(QCoreApplication::translate("M2CL","经典 · 4 像素宽"),"classic");m_model->addItem(QCoreApplication::translate("M2CL","纤细 · 3 像素宽（1.8+）"),"slim");form->addWidget(m_model);
    m_browse=new QPushButton(QCoreApplication::translate("M2CL","选择皮肤 PNG…"));m_browse->setObjectName("importSkinPng");m_browse->setIcon(lineIcon(Glyph::Folder));form->addWidget(m_browse);
    m_file=new QLabel;m_file->setWordWrap(true);m_file->setProperty("muted",true);form->addWidget(m_file);
    m_hint=new QLabel;m_hint->setWordWrap(true);m_hint->setProperty("muted",true);form->addWidget(m_hint);form->addStretch();
    auto effect=new QLabel(QCoreApplication::translate("M2CL","只修改此离线角色，下次启动生效。多人游戏中的显示取决于服务器。"));effect->setWordWrap(true);effect->setProperty("muted",true);form->addWidget(effect);columns->addLayout(form,1);body->addLayout(columns,1);
    m_notice=new NetworkNotice;body->addWidget(m_notice);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);m_save=buttons->button(QDialogButtonBox::Save);m_save->setObjectName("saveOfflineSkin");m_save->setText(QCoreApplication::translate("M2CL","保存"));m_save->setProperty("primary",true);buttons->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("M2CL","取消"));body->addWidget(buttons);
    if(skinSource(account.offlineSkin)=="local"){m_local=account.offlineSkin;m_file->setText(QCoreApplication::translate("M2CL","已保存本地皮肤副本"));}
    m_source->setCurrentIndex(m_source->findData(skinSource(account.offlineSkin)));m_model->setCurrentIndex(slimSkin(account.offlineSkin)?1:0);
    connect(m_source,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{const auto source=m_source->currentData().toString();if(source=="steve"||source=="alex")m_model->setCurrentIndex(source=="alex"?1:0);updatePreview();});
    connect(m_model,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{updatePreview();});connect(m_browse,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,QCoreApplication::translate("M2CL","选择皮肤 PNG"),{},"PNG (*.png)");if(!path.isEmpty())importFile(path);});
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,this,[this]{
        QJsonObject settings=m_source->currentData()=="local"?m_local:QJsonObject{{"source",m_source->currentData().toString()}};settings.insert("model",m_model->currentData().toString());
        const auto error=m_manager.setOfflineSkin(m_account.id,settings);if(!error.isEmpty()){m_notice->failure(QCoreApplication::translate("M2CL","无法保存皮肤"),error,false);return;}accept();
    });updatePreview();
}
bool SkinEditor::importFile(const QString &path){
    QString error;const auto settings=m_store.importPng(path,m_model->currentData()=="slim",&error);if(settings.isEmpty()){m_notice->failure(QCoreApplication::translate("M2CL","无法导入皮肤"),error,false);return false;}
    m_local=settings;m_source->setCurrentIndex(m_source->findData("local"));m_file->setText(QFileInfo(path).fileName());updatePreview();return true;
}
void SkinEditor::updatePreview(){
    const auto source=m_source->currentData().toString();m_model->setEnabled(source!="default");m_browse->setVisible(source=="local");m_file->setVisible(source=="local");m_notice->clear();
    QJsonObject settings=source=="local"?m_local:QJsonObject{{"source",source}};settings.insert("model",m_model->currentData().toString());QString error;
    const auto bytes=source=="default"?m_store.texture({{"source","steve"},{"model","classic"}},&error):m_store.texture(settings,&error);
    const bool valid=source!="local"||!m_local.isEmpty();if(!valid)error=QCoreApplication::translate("M2CL","请选择一张本地皮肤 PNG");
    m_save->setEnabled(valid&&error.isEmpty());if(!error.isEmpty())m_notice->failure(QCoreApplication::translate("M2CL","皮肤需要处理"),error,false);
    m_preview->setSkin(QImage::fromData(bytes,"PNG"),source!="default"&&m_model->currentData()=="slim");
    m_hint->setText(source=="default"?QCoreApplication::translate("M2CL","默认外观由游戏版本与角色 UUID 决定，此处为示意预览"):QCoreApplication::translate("M2CL","支持 64×64 PNG；64×32 旧版皮肤使用经典手臂。皮肤副本保存在启动器中。"));
}
}
