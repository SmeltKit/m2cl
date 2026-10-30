// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "core/i18n.h"
#include "networknotice.h"
#include "launcherwidgets.h"
#include "theme.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QEvent>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIconEngine>
#include <QImageReader>
#include <QLabel>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QHeaderView>
#include <QPainterPath>
#include <QPixmapCache>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QTextLayout>
#include <QToolButton>
#include <QLineEdit>
#include <QWindow>
#include <QtMath>
#include <QVector>
#include <QStyleOptionButton>
#include <cmath>

static void initializeCoverResources() { Q_INIT_RESOURCE(covers); }

namespace mc {
namespace {
constexpr int checkIndicator=20,radioIndicator=24,choiceHeight=34;
constexpr int numberStepWidth=44,numberHeight=36,numberGap=4;
QSize choiceSize(const QAbstractButton &button,bool radio){
    const auto label=button.fontMetrics().size(Qt::TextShowMnemonic,button.text());
    return {(radio?radioIndicator:checkIndicator)+(button.text().isEmpty()?0:(radio?10:8)+label.width())+4,qMax(choiceHeight,label.height()+10)};
}
QColor choiceBlend(const QColor &a,const QColor &b,qreal ratio){
    return QColor::fromRgbF(a.redF()*(1-ratio)+b.redF()*ratio,a.greenF()*(1-ratio)+b.greenF()*ratio,a.blueF()*(1-ratio)+b.blueF()*ratio);
}
void paintChoice(QAbstractButton &button,const QStyleOptionButton &option,bool radio,bool partial,qreal mark){
    const int choiceIndicator=radio?radioIndicator:checkIndicator,choiceGap=radio?10:8;
    QPainter painter(&button);painter.setRenderHint(QPainter::Antialiasing);
    const auto group=button.isEnabled()?QPalette::Active:QPalette::Disabled;
    const auto surface=button.palette().color(group,QPalette::Base),accent=button.palette().color(group,QPalette::Highlight);
    const auto outline=button.palette().color(group,QPalette::Mid),text=button.palette().color(group,QPalette::WindowText);
    const bool hover=button.isEnabled()&&option.state.testFlag(QStyle::State_MouseOver);
    const auto logical=QRect(2,(button.height()-choiceIndicator)/2,choiceIndicator,choiceIndicator);
    const QRectF box=QStyle::visualRect(button.layoutDirection(),button.rect(),logical).adjusted(1,1,-1,-1);
    QColor fill=radio?surface:choiceBlend(surface,accent,mark);
    if(hover)fill=choiceBlend(fill,accent,button.isDown()?.16:.07);
    painter.setBrush(fill);painter.setPen(QPen(button.isChecked()||hover?accent:outline,1.6));
    if(radio){
        painter.drawEllipse(box);
        if(button.isChecked()){painter.setPen(Qt::NoPen);painter.setBrush(accent);painter.drawEllipse(box.adjusted(5,5,-5,-5));}
    }else{
        painter.drawRoundedRect(box,5,5);
        painter.save();painter.setOpacity(mark);painter.setPen(QPen(button.palette().color(group,QPalette::HighlightedText),2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        if(partial)painter.drawLine(QPointF(box.left()+box.width()*.25,box.center().y()),QPointF(box.left()+box.width()*.75,box.center().y()));
        else{QPolygonF tick;tick<<QPointF(box.left()+box.width()*.23,box.top()+box.height()*.51)<<QPointF(box.left()+box.width()*.43,box.top()+box.height()*.71)<<QPointF(box.left()+box.width()*.78,box.top()+box.height()*.30);painter.drawPolyline(tick);}painter.restore();
    }
    if(!button.text().isEmpty()){
        const auto label=QStyle::visualRect(button.layoutDirection(),button.rect(),QRect(choiceIndicator+choiceGap+2,0,qMax(0,button.width()-choiceIndicator-choiceGap-4),button.height()));
        int flags=Qt::AlignVCenter|Qt::TextShowMnemonic|(button.layoutDirection()==Qt::RightToLeft?Qt::AlignRight:Qt::AlignLeft);
        if(!button.style()->styleHint(QStyle::SH_UnderlineShortcut,&option,&button))flags|=Qt::TextHideMnemonic;
        painter.setPen(text);painter.drawText(label,flags,button.text());
    }
    if(button.hasFocus()&&option.state.testFlag(QStyle::State_KeyboardFocusChange)){
        painter.setBrush(Qt::NoBrush);painter.setPen(QPen(accent,1,Qt::DashLine));painter.drawRoundedRect(button.rect().adjusted(0,0,-1,-1),7,7);
    }
}
}
QLabel *numberLabel(const QString &text,QWidget *parent){auto label=new QLabel(text,parent);label->setTextFormat(Qt::PlainText);label->setProperty("numberLabel",true);label->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);return label;}
CheckBox::CheckBox(const QString &text,QWidget *parent):QCheckBox(text,parent){
    setCursor(Qt::PointingHandCursor);setAttribute(Qt::WA_Hover);setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    m_motion.setDuration(120);m_motion.setEasingCurve(QEasingCurve::OutCubic);
    connect(this,&QCheckBox::toggled,this,[this](bool checked){m_motion.stop();if(!isVisible()){m_mark=checked?1:0;update();return;}m_motion.setStartValue(m_mark);m_motion.setEndValue(checked?1.:0.);m_motion.start();});
    connect(&m_motion,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){m_mark=value.toReal();update();});
}
QSize CheckBox::sizeHint() const{return choiceSize(*this,false);}
void CheckBox::paintEvent(QPaintEvent *){if(m_motion.state()!=QAbstractAnimation::Running)m_mark=isChecked()?1:0;QStyleOptionButton option;initStyleOption(&option);paintChoice(*this,option,false,checkState()==Qt::PartiallyChecked,m_mark);}
RadioButton::RadioButton(const QString &text,QWidget *parent):QRadioButton(text,parent){setCursor(Qt::PointingHandCursor);setAttribute(Qt::WA_Hover);setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);}
QSize RadioButton::sizeHint() const{return choiceSize(*this,true);}
void RadioButton::paintEvent(QPaintEvent *){QStyleOptionButton option;initStyleOption(&option);paintChoice(*this,option,true,false,isChecked()?1:0);}
NumberBox::NumberBox(QWidget *parent):QSpinBox(parent){
    setProperty("numberBox",true);setButtonSymbols(QAbstractSpinBox::NoButtons);setAlignment(Qt::AlignCenter);setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    lineEdit()->setProperty("numberEditor",true);
    m_field=new QFrame(this);m_field->setProperty("numberField",true);m_field->setAttribute(Qt::WA_TransparentForMouseEvents);m_field->lower();lineEdit()->installEventFilter(this);
    m_minus=new QToolButton(this);m_plus=new QToolButton(this);
    m_minus->setText(QStringLiteral("−"));m_plus->setText(QStringLiteral("+"));
    m_minus->setAccessibleName(QCoreApplication::translate("M2CL", "减少数值"));m_plus->setAccessibleName(QCoreApplication::translate("M2CL", "增加数值"));
    for(auto button:{m_minus,m_plus}){button->setProperty("numberStep",true);button->setFocusPolicy(Qt::NoFocus);button->setCursor(Qt::PointingHandCursor);button->setAutoRepeat(true);button->setAutoRepeatDelay(350);button->setAutoRepeatInterval(70);}
    connect(m_minus,&QToolButton::clicked,this,[this]{setFocus(Qt::MouseFocusReason);interpretText();stepDown();});
    connect(m_plus,&QToolButton::clicked,this,[this]{setFocus(Qt::MouseFocusReason);interpretText();stepUp();});
    connect(this,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{updateSteps();});updateSteps();
}
void NumberBox::setEmptyValuePlaceholder(const QString &text){
    m_emptyMinimum=!text.isEmpty();lineEdit()->setPlaceholderText(text);lineEdit()->setProperty("optionalNumber",m_emptyMinimum);
    lineEdit()->style()->unpolish(lineEdit());lineEdit()->style()->polish(lineEdit());lineEdit()->setText(textFromValue(value()));updateGeometry();
}
QString NumberBox::textFromValue(int value) const {return m_emptyMinimum&&value==minimum()?QString():QSpinBox::textFromValue(value);}
int NumberBox::valueFromText(const QString &text) const {return m_emptyMinimum&&text.trimmed().isEmpty()?minimum():QSpinBox::valueFromText(text);}
QValidator::State NumberBox::validate(QString &input,int &position) const {return m_emptyMinimum&&input.trimmed().isEmpty()?QValidator::Acceptable:QSpinBox::validate(input,position);}
void NumberBox::resizeEvent(QResizeEvent *event){
    QSpinBox::resizeEvent(event);layoutParts();
}
QSize NumberBox::sizeHint() const {const auto base=QSpinBox::sizeHint();return {qMax(160,base.width()+2*(numberStepWidth+numberGap)),numberHeight};}
QSize NumberBox::minimumSizeHint() const{return {160,numberHeight};}
void NumberBox::layoutParts(){
    if(!m_minus||!m_plus)return;const int button=numberStepWidth;
    m_minus->setGeometry(0,0,button,height());m_plus->setGeometry(width()-button,0,button,height());
    lineEdit()->setGeometry(button+numberGap,0,qMax(0,width()-2*(button+numberGap)),height());
    m_field->setGeometry(lineEdit()->geometry());m_field->lower();lineEdit()->raise();
    m_minus->raise();m_plus->raise();
}
void NumberBox::changeEvent(QEvent *event){QSpinBox::changeEvent(event);updateSteps();layoutParts();}
bool NumberBox::eventFilter(QObject *object,QEvent *event){
    if(object==lineEdit()&&m_field&&(event->type()==QEvent::FocusIn||event->type()==QEvent::FocusOut)){
        m_field->setProperty("editorFocused",event->type()==QEvent::FocusIn);m_field->style()->unpolish(m_field);m_field->style()->polish(m_field);m_field->update();
    }
    return QSpinBox::eventFilter(object,event);
}
void NumberBox::updateSteps(){
    if(!m_minus||!m_plus)return;const auto steps=stepEnabled();
    m_minus->setEnabled(isEnabled()&&!isReadOnly()&&steps.testFlag(QAbstractSpinBox::StepDownEnabled));
    m_plus->setEnabled(isEnabled()&&!isReadOnly()&&steps.testFlag(QAbstractSpinBox::StepUpEnabled));
}
RoundedMenu::RoundedMenu(QWidget *parent) : QMenu(parent) {
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
}
namespace {
class TableHeader final : public QHeaderView {
public:
    explicit TableHeader(QWidget *parent):QHeaderView(Qt::Horizontal,parent){setFixedHeight(44);setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);}
protected:
    void paintSection(QPainter *p,const QRect &rect,int column) const override{
        if(!rect.isValid())return;p->save();p->fillRect(rect,qApp->palette().color(QPalette::AlternateBase));
        auto line=qApp->palette().color(QPalette::WindowText);line.setAlpha(24);p->setPen(line);p->drawLine(rect.bottomLeft(),rect.bottomRight());
        auto font=this->font();font.setPixelSize(13);font.setWeight(QFont::DemiBold);p->setFont(font);p->setPen(qApp->palette().color(QPalette::Text));
        const auto value=model()->headerData(column,orientation(),Qt::TextAlignmentRole);const auto alignment=value.isValid()?Qt::Alignment(value.toInt()):defaultAlignment();
        const auto area=rect.adjusted(RoundedTable::CellInset,0,-RoundedTable::CellInset,0);const auto text=model()->headerData(column,orientation(),Qt::DisplayRole).toString();
        p->drawText(area,alignment|Qt::TextSingleLine,QFontMetrics(font).elidedText(text,Qt::ElideRight,area.width()));p->restore();
    }
};
}
void TableItemDelegate::paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const{
    QStyleOptionViewItem base(option);initStyleOption(&base,index);const auto text=base.text;const auto icon=base.icon;base.text.clear();base.icon={};
    const auto widget=base.widget;const auto style=widget?widget->style():QApplication::style();style->drawControl(QStyle::CE_ItemViewItem,&base,p,widget);
    p->save();auto area=option.rect.adjusted(RoundedTable::CellInset,0,-RoundedTable::CellInset,0);
    if(!icon.isNull()){const auto size=base.decorationSize.isValid()?base.decorationSize:QSize(20,20);icon.paint(p,QRect(area.left(),area.center().y()-size.height()/2,size.width(),size.height()));area.adjust(size.width()+8,0,0,0);}
    p->setFont(base.font);const auto foreground=index.data(Qt::ForegroundRole);p->setPen(foreground.isValid()?qvariant_cast<QBrush>(foreground).color():base.palette.color(QPalette::Text));
    const auto header=index.model()->headerData(index.column(),Qt::Horizontal,Qt::TextAlignmentRole);const auto alignment=header.isValid()?Qt::Alignment(header.toInt()):base.displayAlignment;
    p->drawText(area,alignment|Qt::TextSingleLine,QFontMetrics(base.font).elidedText(text,base.textElideMode,area.width()));p->restore();
}
ResourceToolbar::ResourceToolbar(const QString &kind,QWidget *parent):QWidget(parent){
 setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
 auto body=new QVBoxLayout(this);body->setContentsMargins(0,0,0,0);body->setSpacing(12);
 auto actions=new QHBoxLayout;actions->setSpacing(10);
 add=new QPushButton(QCoreApplication::translate("M2CL", "添加%1  ⌄").arg(kind));add->setProperty("primary",true);actions->addWidget(add);
 open=new QPushButton(QCoreApplication::translate("M2CL", "打开目录"));open->setIcon(lineIcon(Glyph::Folder));actions->addWidget(open);actions->addStretch();
 count=new QLabel;count->setProperty("muted",true);actions->addWidget(count);body->addLayout(actions);
 filters=new QHBoxLayout;filters->setSpacing(10);
 search=new QLineEdit;search->setPlaceholderText(QCoreApplication::translate("M2CL", "搜索名称或文件名…"));search->setClearButtonEnabled(true);filters->addWidget(search,1);
 refresh=new QPushButton(QCoreApplication::translate("M2CL", "刷新"));refresh->setIcon(lineIcon(Glyph::Refresh));filters->addWidget(refresh);body->addLayout(filters);
}
RoundedTable::RoundedTable(int rows,int columns,QWidget *parent):QTableWidget(rows,columns,parent){
    setProperty("dataTable",true);setHorizontalHeader(new TableHeader(this));setItemDelegate(new TableItemDelegate(this));setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    for(auto widget:QList<QWidget *>{viewport(),horizontalHeader(),verticalHeader()})widget->installEventFilter(this);
    connect(horizontalHeader(),&QHeaderView::geometriesChanged,this,[this]{if(m_columns.isEmpty()||m_layoutPending||m_layingOut)return;m_layoutPending=true;QTimer::singleShot(0,this,[this]{m_layoutPending=false;layoutColumns();});});
}
void RoundedTable::setColumnLayout(const QList<TableColumn> &columns){m_columns=columns;horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);horizontalHeader()->setMinimumSectionSize(24);for(const auto &column:columns)if(column.index<columnCount()&&horizontalHeaderItem(column.index))horizontalHeaderItem(column.index)->setTextAlignment(column.alignment);layoutColumns();}
void RoundedTable::layoutColumns(){
    if(m_layingOut||m_columns.isEmpty())return;m_layingOut=true;int available=viewport()->width();qreal totalWeight=0;QList<TableColumn> flexible;QHash<int,int> widths;
    auto font=horizontalHeader()->font();font.setPixelSize(13);font.setWeight(QFont::DemiBold);const QFontMetrics metrics(font);
    for(auto column:m_columns)if(column.index>=0&&column.index<columnCount()&&!isColumnHidden(column.index)){
        const auto caption=model()->headerData(column.index,Qt::Horizontal).toString();column.minimumWidth=qMax(column.minimumWidth,metrics.horizontalAdvance(caption)+2*CellInset+16);
        if(column.weight>0){flexible.append(column);totalWeight+=column.weight;}else{widths.insert(column.index,column.minimumWidth);available-=column.minimumWidth;}
    }
    bool clamped=true;while(clamped&&!flexible.isEmpty()){clamped=false;for(int i=0;i<flexible.size();++i)if(available*flexible.at(i).weight/totalWeight<flexible.at(i).minimumWidth){const auto column=flexible.takeAt(i);widths.insert(column.index,column.minimumWidth);available-=column.minimumWidth;totalWeight-=column.weight;clamped=true;break;}}
    for(const auto &column:flexible){const int width=qMax(column.minimumWidth,qRound(available*column.weight/totalWeight));widths.insert(column.index,width);available-=width;totalWeight-=column.weight;}
    for(auto it=widths.begin();it!=widths.end();++it)if(columnWidth(it.key())!=it.value())setColumnWidth(it.key(),it.value());m_layingOut=false;
}
void RoundedTable::clipContents(){
    QPainterPath path;path.addRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),7,7);const QRegion inside(path.toFillPolygon().toPolygon());
    for(auto widget:QList<QWidget *>{viewport(),horizontalHeader(),verticalHeader()}){
        if(widget->size().isEmpty())continue;const auto origin=widget->mapTo(this,QPoint());const auto mask=inside.translated(-origin).intersected(widget->rect());if(widget->mask()!=mask)widget->setMask(mask);
    }
}
void RoundedTable::resizeEvent(QResizeEvent *event){QTableWidget::resizeEvent(event);layoutColumns();clipContents();}
bool RoundedTable::eventFilter(QObject *object,QEvent *event){if(object==viewport()&&event->type()==QEvent::Resize)layoutColumns();if(event->type()==QEvent::Resize||event->type()==QEvent::Move||event->type()==QEvent::Show||event->type()==QEvent::StyleChange)clipContents();return QTableWidget::eventFilter(object,event);}
void SelectBox::paintEvent(QPaintEvent *event){QComboBox::paintEvent(event);QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(palette().color(isEnabled()?QPalette::Active:QPalette::Disabled,QPalette::Text),1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));const auto x=width()-15.;const auto y=height()/2.;QPolygonF chevron;chevron<<QPointF(x-4,y-2)<<QPointF(x,y+2)<<QPointF(x+4,y-2);painter.drawPolyline(chevron);}
ToggleSwitch::ToggleSwitch(const QString &text,QWidget *parent):QCheckBox(text,parent){
    setCursor(Qt::PointingHandCursor);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);m_motion.setDuration(120);m_motion.setEasingCurve(QEasingCurve::OutCubic);
    connect(this,&QCheckBox::toggled,this,[this](bool on){m_motion.stop();if(!isVisible()){m_position=on?1.:0.;update();return;}m_motion.setStartValue(m_position);m_motion.setEndValue(on?1.:0.);m_motion.start();});connect(&m_motion,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){m_position=v.toReal();update();});
}
QSize ToggleSwitch::sizeHint() const {return {fontMetrics().horizontalAdvance(text())+(text().isEmpty()?40:60),28};}
void ToggleSwitch::paintEvent(QPaintEvent*){
    if(m_motion.state()!=QAbstractAnimation::Running)m_position=isChecked()?1.:0.;
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const auto accent=palette().color(QPalette::Highlight);const QRectF track(width()-38,(height()-20)/2.,36,20);p.setPen(Qt::NoPen);p.setBrush(isChecked()?accent:palette().color(QPalette::Mid));if(!isEnabled())p.setOpacity(.5);p.drawRoundedRect(track,10,10);p.setBrush(QColor("#f4fff8"));p.drawEllipse(QRectF(track.left()+3+16*m_position,track.top()+3,14,14));if(!text().isEmpty()){p.setPen(palette().color(QPalette::WindowText));p.drawText(QRect(0,0,width()-48,height()),Qt::AlignLeft|Qt::AlignVCenter,text());}if(hasFocus()){p.setPen(QPen(accent,1,Qt::DashLine));p.setBrush(Qt::NoBrush);p.drawRoundedRect(rect().adjusted(1,1,-1,-1),4,4);}
}

namespace {
void glyph(QPainter &p, Glyph icon, const QRectF &rect, const QColor &color) {
    p.save(); p.translate(rect.topLeft()); p.scale(rect.width()/24., rect.height()/24.);
    p.setRenderHint(QPainter::Antialiasing); p.setBrush(Qt::NoBrush);
    p.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath path;
    switch (icon) {
    case Glyph::Home:
        p.drawPolyline(QPolygonF(QVector<QPointF>{{3,10},{12,3},{21,10}})); p.drawPolyline(QPolygonF(QVector<QPointF>{{5,9},{5,21},{10,21},{10,14},{14,14},{14,21},{19,21},{19,9}})); break;
    case Glyph::Grid:
        for(int y : {4,14}) for(int x : {4,14}) p.drawRoundedRect(QRectF(x,y,6,6),1,1); break;
    case Glyph::Mods:
        p.drawPolygon(QPolygonF(QVector<QPointF>{{12,3},{21,8},{21,17},{12,22},{3,17},{3,8}}));
        p.drawPolyline(QPolygonF(QVector<QPointF>{{3,8},{12,13},{21,8}})); p.drawLine(12,13,12,22); p.drawLine(7,6,16,11); break;
    case Glyph::Folder:
        p.drawPolyline(QPolygonF(QVector<QPointF>{{3,9},{3,5},{9,5},{11,8},{21,8},{21,20},{3,20},{3,9},{21,9}})); break;
    case Glyph::Document:
        p.drawPolyline(QPolygonF(QVector<QPointF>{{14,3},{5,3},{5,21},{19,21},{19,8},{14,3},{14,8},{19,8}}));p.drawLine(8,12,16,12);p.drawLine(8,16,14,16);break;
    case Glyph::Gamepad:
        path.moveTo(7,7);path.cubicTo(5,7,4,9,3,13);path.lineTo(2,18);path.cubicTo(1,21,4,22,6,19);path.lineTo(8,16);path.lineTo(16,16);path.lineTo(18,19);path.cubicTo(20,22,23,21,22,18);path.lineTo(21,13);path.cubicTo(20,9,19,7,17,7);path.closeSubpath();p.drawPath(path);p.drawLine(6,12,10,12);p.drawLine(8,10,8,14);p.drawEllipse(QPointF(16,11),.8,.8);p.drawEllipse(QPointF(18,14),.8,.8);break;
    case Glyph::Search:p.drawEllipse(QRectF(3,3,13,13));p.drawLine(14,14,21,21);break;
    case Glyph::Ban:p.drawEllipse(QRectF(3,3,18,18));p.drawLine(6,18,18,6);break;
    case Glyph::Trash:p.drawLine(3,6,21,6);p.drawPolyline(QPolygonF(QVector<QPointF>{{8,6},{8,3},{16,3},{16,6}}));p.drawPolyline(QPolygonF(QVector<QPointF>{{5,6},{6,21},{18,21},{19,6}}));p.drawLine(10,10,10,17);p.drawLine(14,10,14,17);break;
    case Glyph::Settings:
        for(int i=0;i<8;++i) { p.save(); p.translate(12,12); p.rotate(i*45); p.drawLine(0,-8,0,-10); p.restore(); }
        p.drawEllipse(QRectF(5,5,14,14)); p.drawEllipse(QRectF(9,9,6,6)); break;
    case Glyph::Download:
        p.drawLine(12,3,12,15); p.drawPolyline(QPolygonF(QVector<QPointF>{{7,10},{12,15},{17,10}}));
        p.drawPolyline(QPolygonF(QVector<QPointF>{{4,15},{4,21},{20,21},{20,15}})); break;
    case Glyph::Palette:
        path.moveTo(20,13); path.cubicTo(24,4,12,0,6,5); path.cubicTo(-1,12,4,23,12,21);
        path.cubicTo(17,20,10,15,15,14); path.cubicTo(17,14,19,15,20,13); p.drawPath(path);
        for(auto point : {QPoint(7,10),QPoint(10,6),QPoint(15,6),QPoint(18,9)}) p.drawEllipse(QPointF(point), .8, .8); break;
    case Glyph::Plus: p.drawLine(12,5,12,19); p.drawLine(5,12,19,12); break;
    case Glyph::More:
        p.setBrush(color); p.setPen(Qt::NoPen); for(int x : {5,12,19}) p.drawEllipse(QPointF(x,12),1.3,1.3); break;
    case Glyph::Star:
        for(int i=0;i<10;++i) { const qreal angle=(i*36-90)*3.14159265/180; const qreal radius=i%2?4.3:9;
            const QPointF point(12+radius*std::cos(angle),12+radius*std::sin(angle)); if(i==0)path.moveTo(point);else path.lineTo(point); }
        path.closeSubpath(); p.drawPath(path); break;
    case Glyph::Play: p.setBrush(color); p.setPen(Qt::NoPen); p.drawPolygon(QPolygonF(QVector<QPointF>{{7,4},{20,12},{7,20}})); break;
    case Glyph::Chevron: p.drawPolyline(QPolygonF(QVector<QPointF>{{6,9},{12,15},{18,9}})); break;
    case Glyph::Edit:
        p.drawPolygon(QPolygonF(QVector<QPointF>{{4,16},{16,4},{20,8},{8,20},{3,21}})); p.drawLine(13,7,17,11); break;
    case Glyph::Java:
        p.drawPolyline(QPolygonF(QVector<QPointF>{{5,11},{5,19},{16,19},{16,11}})); p.drawArc(QRectF(14,11,7,7),-90*16,180*16);
        path.moveTo(9,8); path.cubicTo(4,5,14,5,10,1); p.drawPath(path); p.drawLine(3,22,19,22); break;
    case Glyph::Check: p.drawPolyline(QPolygonF(QVector<QPointF>{{4,12},{9,17},{20,6}})); break;
    case Glyph::Refresh: p.drawArc(QRectF(4,4,16,16),30*16,290*16); p.drawPolyline(QPolygonF(QVector<QPointF>{{20,3},{20,9},{14,9}})); break;
    case Glyph::Close: p.drawLine(6,6,18,18); p.drawLine(18,6,6,18); break;
    case Glyph::Minimize: p.drawLine(5,12,19,12); break;
    case Glyph::Maximize: p.drawRoundedRect(QRectF(5,5,14,14),1,1); break;
    case Glyph::Leaf:
        path.moveTo(4,20);path.cubicTo(1,9,12,3,21,3);path.cubicTo(22,14,13,23,4,20);p.drawPath(path);p.drawLine(4,20,16,8);break;
    case Glyph::Image:
        p.drawRoundedRect(QRectF(3,4,18,16),2,2);p.drawEllipse(QRectF(6,7,3,3));p.drawPolyline(QPolygonF(QVector<QPointF>{{4,18},{10,12},{13,15},{17,10},{21,15}}));break;
    case Glyph::Grass:
        p.setPen(Qt::NoPen);p.setBrush(QColor("#936335"));p.drawPolygon(QPolygonF(QVector<QPointF>{{2,7},{12,12},{12,23},{2,18}}));
        p.setBrush(QColor("#6a472d"));p.drawPolygon(QPolygonF(QVector<QPointF>{{12,12},{22,7},{22,18},{12,23}}));
        p.setBrush(QColor("#4c763b"));p.drawPolygon(QPolygonF(QVector<QPointF>{{2,7},{12,12},{12,15},{2,10}}));
        p.setBrush(QColor("#3f622f"));p.drawPolygon(QPolygonF(QVector<QPointF>{{12,12},{22,7},{22,10},{12,15}}));
        p.setBrush(QColor("#7aa449"));p.drawPolygon(QPolygonF(QVector<QPointF>{{2,7},{12,1},{22,7},{12,13}}));
        p.setBrush(QColor("#a4bb5b"));p.drawPolygon(QPolygonF(QVector<QPointF>{{5,7},{8,5},{11,7},{8,9}}));p.drawPolygon(QPolygonF(QVector<QPointF>{{12,5},{15,3},{18,5},{15,7}}));
        p.setBrush(QColor("#5d8539"));p.drawPolygon(QPolygonF(QVector<QPointF>{{10,10},{13,8},{16,10},{13,12}}));
        p.setBrush(QColor("#b38b50"));p.drawRect(QRectF(4,12,2,2));p.drawRect(QRectF(8,17,2,3));
        p.setBrush(QColor("#4e3527"));p.drawRect(QRectF(15,17,2,2));p.drawRect(QRectF(19,12,2,3));break;
    }
    p.restore();
}
class LineIconEngine final : public QIconEngine {
public:
    LineIconEngine(Glyph icon, QColor color) : m_icon(icon), m_color(std::move(color)) {}
    QIconEngine *clone() const override { return new LineIconEngine(m_icon,m_color); }
    void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State state) override {
        auto color=m_color.isValid()?m_color:qApp->palette().color(QPalette::WindowText);
        if(!m_color.isValid()&&state==QIcon::On)color=qApp->palette().color(QPalette::Highlight);
        if(mode==QIcon::Disabled)color.setAlpha(95);glyph(*p,m_icon,rect,color);
    }
    QPixmap pixmap(const QSize &size,QIcon::Mode mode,QIcon::State state) override {
        QPixmap result(size);result.fill(Qt::transparent);QPainter p(&result);paint(&p,QRect(QPoint(),size),mode,state);return result;
    }
private: Glyph m_icon;QColor m_color;
};
QColor accent() { return qApp->palette().color(QPalette::Highlight); }
QColor muted() { auto color=qApp->palette().color(QPalette::WindowText);color.setAlpha(155);return color; }
QColor border() { auto color=qApp->palette().color(QPalette::WindowText);color.setAlpha(27);return color; }
bool hoverEvent(QAbstractButton *button,QEvent *event) {
    if(event->type()==QEvent::Enter||event->type()==QEvent::Leave||event->type()==QEvent::FocusIn||event->type()==QEvent::FocusOut)button->update();
    return false;
}
qreal pill(QPainter &p, const QPointF &point, const QString &text, Glyph icon, const QColor &background, const QColor &foreground) {
    QFont f=p.font();f.setPixelSize(ui::Caption);p.setFont(f);
    const qreal width=p.fontMetrics().horizontalAdvance(text)+32;
    p.setPen(Qt::NoPen);p.setBrush(background);p.drawRoundedRect(QRectF(point,QSizeF(width,23)),5,5);
    glyph(p,icon,QRectF(point.x()+6,point.y()+5,13,13),foreground);
    p.setPen(foreground);p.drawText(QRectF(point.x()+23,point.y(),width-25,23),Qt::AlignVCenter,text);
    return width;
}
QPoint globalPosition(QMouseEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}
}
QIcon lineIcon(Glyph glyph, const QColor &color) { return QIcon(new LineIconEngine(glyph,color)); }
QIcon applicationIcon() {
    static const QIcon icon=[] { initializeCoverResources();
#ifdef Q_OS_MACOS
        return QIcon(QStringLiteral(":/branding/m2cl-macos.png"));
#else
        return QIcon(QStringLiteral(":/branding/m2cl.png"));
#endif
    }();
    return icon;
}
BrandMark::BrandMark(int size,QWidget *parent):QWidget(parent){setFixedSize(size,size);setAccessibleName(QCoreApplication::translate("M2CL", "M²CL 图标"));}
void BrandMark::paintEvent(QPaintEvent *){QPainter painter(this);initializeCoverResources();QIcon(QStringLiteral(":/branding/m2cl.png")).paint(&painter,rect());}
namespace {
struct BrandLetterLayout {
    QFont superscript;
    qreal exponentX,exponentBaseline,tailX;
    QRectF bounds;
};
BrandLetterLayout brandLetterLayout(const QFont &font) {
    const QFontMetricsF body(font);
    auto superscript=font;superscript.setPixelSize(10);
    const QFontMetricsF small(superscript);
    const qreal exponentX=body.horizontalAdvance(QStringLiteral("M"))+.5;
    // Keep the numeral above the capitals without depending on U+00B2 metrics.
    const qreal exponentBaseline=-(body.capHeight()-small.capHeight()+3);
    const qreal tailX=exponentX+small.horizontalAdvance(QStringLiteral("2"))+1;
    const auto bounds=body.tightBoundingRect(QStringLiteral("M"))
        .united(small.tightBoundingRect(QStringLiteral("2")).translated(exponentX,exponentBaseline))
        .united(body.tightBoundingRect(QStringLiteral("CL")).translated(tailX,0));
    return {superscript,exponentX,exponentBaseline,tailX,bounds};
}
}
BrandWordmark::BrandWordmark(QWidget *parent):QWidget(parent){
    auto type=font();type.setPixelSize(18);type.setWeight(QFont::DemiBold);setFont(type);
    setFixedHeight(36);setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    setAccessibleName(QStringLiteral("M²CL · Mercury MC Launcher"));
    setToolTip(QStringLiteral("Mercury MC Launcher"));
}
QSize BrandWordmark::sizeHint() const{return {qCeil(brandLetterLayout(font()).bounds.width())+4,36};}
void BrandWordmark::paintEvent(QPaintEvent *){
    QPainter painter(this);painter.setRenderHint(QPainter::TextAntialiasing);
    const auto letters=brandLetterLayout(font());const auto bounds=letters.bounds;
    painter.translate(2-bounds.left(),(height()-bounds.height())/2-bounds.top());
    painter.setPen(palette().color(QPalette::WindowText));painter.setFont(font());
    painter.drawText(QPointF(0,0),QStringLiteral("M"));
    painter.drawText(QPointF(letters.tailX,0),QStringLiteral("CL"));
    painter.setFont(letters.superscript);
    painter.drawText(QPointF(letters.exponentX,letters.exponentBaseline),QStringLiteral("2"));
}
BusySpinner::BusySpinner(QWidget *parent):QWidget(parent){setFixedSize(22,22);setAccessibleName(QCoreApplication::translate("M2CL", "正在加载"));m_timer.setInterval(70);connect(&m_timer,&QTimer::timeout,this,[this]{m_angle=(m_angle+30)%360;update();});}
void BusySpinner::showEvent(QShowEvent *event){QWidget::showEvent(event);m_timer.start();}
void BusySpinner::hideEvent(QHideEvent *event){m_timer.stop();QWidget::hideEvent(event);}
void BusySpinner::paintEvent(QPaintEvent *){QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.translate(width()/2.,height()/2.);p.rotate(m_angle);for(int i=0;i<12;++i){auto c=palette().color(QPalette::Highlight);c.setAlpha(30+i*19);p.setPen(QPen(c,2,Qt::SolidLine,Qt::RoundCap));p.drawLine(QPointF(0,-6),QPointF(0,-9));p.rotate(30);}}
AdaptiveTags::AdaptiveTags(const QStringList &labels,QWidget *parent):QWidget(parent),m_labels(labels) {
    auto f=font();f.setPixelSize(ui::Caption);setFont(f);setFixedHeight(fontMetrics().height()+8);setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    setAccessibleName(labels.join(QCoreApplication::translate("M2CL", "、")));setToolTip("<div style='width:280px'>"+labels.join(QStringLiteral(" · ")).toHtmlEscaped()+"</div>");
}
bool AdaptiveTags::event(QEvent *event){bool result=QWidget::event(event);if(event->type()==QEvent::FontChange)setFixedHeight(fontMetrics().height()+8);return result;}
int AdaptiveTags::visibleCount() const {
    const auto fm=fontMetrics();int used=0;
    for(int i=0;i<m_labels.size();++i){int next=used+fm.horizontalAdvance(m_labels.at(i))+12;int remaining=m_labels.size()-i-1;int reserve=remaining?6+fm.horizontalAdvance("+"+QString::number(remaining))+12:0;
        if(next+reserve>width())return i;used=next+6;}
    return m_labels.size();
}
void AdaptiveTags::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setFont(font());int x=0;auto foreground=palette().color(QPalette::Highlight);auto background=foreground;background.setAlpha(28);
    auto draw=[&](const QString &text){int w=fontMetrics().horizontalAdvance(text)+12;p.setPen(Qt::NoPen);p.setBrush(background);p.drawRoundedRect(QRect(x,1,w,height()-2),4,4);p.setPen(foreground);p.drawText(QRect(x+6,0,w-12,height()),Qt::AlignVCenter,text);x+=w+6;};
    int count=visibleCount();for(int i=0;i<count;++i)draw(m_labels.at(i));if(count<m_labels.size())draw("+"+QString::number(m_labels.size()-count));
}
FittingVersionLabel::FittingVersionLabel(const QStringList &families,QWidget *parent):QLabel(parent),m_families(families){setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);fit();}
void FittingVersionLabel::fit(){QString prefix=QCoreApplication::translate("M2CL", "版本系列：");QString result=prefix+QCoreApplication::translate("M2CL", "未标注");for(int i=1;i<=m_families.size();++i){auto candidate=prefix+m_families.mid(0,i).join("、")+(i<m_families.size()?QCoreApplication::translate("M2CL", " 等 %1 个系列").arg(m_families.size()):QString());if(i>1 && fontMetrics().horizontalAdvance(candidate)>width())break;result=candidate;}setText(result);}
void FittingVersionLabel::resizeEvent(QResizeEvent *event){QLabel::resizeEvent(event);fit();}
bool FittingVersionLabel::event(QEvent *event){const bool result=QLabel::event(event);if(event->type()==QEvent::FontChange)fit();return result;}
SummaryLabel::SummaryLabel(const QString &value,QWidget *parent):QLabel(value,parent) {
    setTextFormat(Qt::PlainText);setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);setFixedHeight(fontMetrics().lineSpacing()*2);setToolTip(value);
}
bool SummaryLabel::event(QEvent *event) {
    const bool result=QLabel::event(event);if(event->type()==QEvent::FontChange){setFixedHeight(fontMetrics().lineSpacing()*2);updateGeometry();}return result;
}
void SummaryLabel::paintEvent(QPaintEvent *) {
    QPainter painter(this);painter.setPen(palette().color(foregroundRole()));QTextLayout layout(text().simplified(),font());QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);layout.beginLayout();
    qreal y=0;for(int i=0;i<2;++i){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(width());line.setPosition(QPointF(0,y));
        if(i==1 && line.textStart()+line.textLength()<layout.text().size())painter.drawText(QPointF(0,y+line.ascent()),fontMetrics().elidedText(layout.text().mid(line.textStart()),Qt::ElideRight,width()));else line.draw(&painter,QPointF());y+=line.height();}
    layout.endLayout();
}
GlyphView::GlyphView(Glyph icon,int size,QWidget *parent):QWidget(parent),m_icon(icon) { setFixedSize(size,size); }
void GlyphView::paintEvent(QPaintEvent *) {
    QPainter painter(this);glyph(painter,m_icon,rect(),palette().color(QPalette::WindowText));
}
IconButton::IconButton(Glyph glyph, const QString &description, QWidget *parent) : QAbstractButton(parent),m_glyph(glyph) {
    setToolTip(description);setAccessibleName(description);setFocusPolicy(Qt::StrongFocus);setCursor(Qt::PointingHandCursor);setFixedSize(sizeHint());
    m_hover.setDuration(130);m_hover.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_hover,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){m_hoverAmount=value.toReal();update();});
}
void IconButton::setBadge(int count) { m_badge=count;update(); }
bool IconButton::event(QEvent *e) {
    if(e->type()==QEvent::Enter||e->type()==QEvent::Leave) {
        m_hover.stop();m_hover.setStartValue(m_hoverAmount);m_hover.setEndValue(e->type()==QEvent::Enter?1.:0.);m_hover.start();
    }
    if(e->type()==QEvent::Hide){m_hover.stop();m_hoverAmount=0;}
    hoverEvent(this,e);return QAbstractButton::event(e);
}
void IconButton::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    if(m_hoverAmount>0||isDown()||hasFocus()) { auto color=accent();color.setAlpha(isDown()?35:hasFocus()?18:qRound(18*m_hoverAmount));p.setPen(hasFocus()?QPen(accent(),1):QPen(Qt::NoPen));p.setBrush(color);p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),6,6); }
    QColor color=property("gold").toBool()?QColor("#d8a633"):qApp->palette().color(QPalette::WindowText);
    if(!isEnabled())color.setAlpha(90);
    glyph(p,m_glyph,QRectF(width()/2.-9,height()/2.-9,18,18),color);
    if(m_badge>0) { p.setPen(qApp->palette().color(QPalette::Base));p.setBrush(accent());p.drawEllipse(QRectF(21,0,14,14));
        p.setPen(qApp->palette().color(QPalette::HighlightedText));QFont f=font();f.setPixelSize(9);f.setBold(true);p.setFont(f);p.drawText(QRectF(21,0,14,14),Qt::AlignCenter,QString::number(m_badge)); }
}
QString CoverStore::family(const QString &version) {
    const auto match=QRegularExpression("^(1\\.(?:[0-9]|1[0-9]|2[01])|26\\.[123])(?:\\.|-|$)").match(version);
    return match.hasMatch()?match.captured(1):QString();
}
bool CoverStore::readable(const QString &path) {
    if(QFileInfo(path).size()>32*1024*1024)return false;
    QImageReader reader(path);const auto size=reader.size();
    return reader.canRead()&&size.isValid()&&size.width()<=16000&&size.height()<=16000;
}
QPixmap CoverStore::image(const QString &version, const QSize &size, const QString &custom) {
    static const bool initialized=[] { initializeCoverResources();QPixmapCache::setCacheLimit(16*1024);return true; }();Q_UNUSED(initialized);
    QString file;
    if(!custom.isEmpty()&&readable(custom))file=custom;
    else if(!family(version).isEmpty())file=QStringLiteral(":/covers/%1.webp").arg(family(version));
    if(file.isEmpty())return {};
    const QSize bounded(qBound(32,((size.width()+63)/64)*64,1920),qBound(32,((size.height()+63)/64)*64,1080));
    const auto key=QStringLiteral("cover:%1:%2:%3:%4").arg(file).arg(bounded.width()).arg(bounded.height()).arg(QFileInfo(file).lastModified().toMSecsSinceEpoch());
    QPixmap result;if(QPixmapCache::find(key,&result))return result;
    QImageReader reader(file);reader.setAutoTransform(true);const auto original=reader.size();
    if(original.isValid())reader.setScaledSize(original.scaled(bounded,Qt::KeepAspectRatioByExpanding).boundedTo(original));
    result=QPixmap::fromImage(reader.read());if(!result.isNull())QPixmapCache::insert(key,result);return result;
}
void CoverStore::paint(QPainter &p,const QRectF &rectangle,const QString &version,const QString &custom,qreal radius) {
    p.save();p.setRenderHint(QPainter::Antialiasing);QPainterPath path;path.addRoundedRect(rectangle,radius,radius);p.setClipPath(path);
    const auto ratio=p.device()->devicePixelRatioF();const auto pixmap=image(version,(rectangle.size()*ratio).toSize(),custom);
    if(!pixmap.isNull()) {
        const qreal scale=qMax(rectangle.width()/pixmap.width(),rectangle.height()/pixmap.height());
        const QSizeF visible(rectangle.width()/scale,rectangle.height()/scale);
        const QRectF source(QPointF((pixmap.width()-visible.width())/2,(pixmap.height()-visible.height())/2),visible);
        p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawPixmap(rectangle,pixmap,source);
    } else {
        QLinearGradient sky(rectangle.topLeft(),rectangle.bottomLeft());sky.setColorAt(0,QColor("#bbd4ca"));sky.setColorAt(1,QColor("#799d77"));p.fillRect(rectangle,sky);
        p.translate(rectangle.topLeft());p.scale(rectangle.width()/360,rectangle.height()/200);
        p.setPen(Qt::NoPen);p.setBrush(QColor("#86a889"));p.drawPolygon(QPolygon(QVector<QPoint>{{0,120},{60,120},{60,80},{120,80},{120,105},{230,105},{230,65},{290,65},{290,110},{360,110},{360,200},{0,200}}));
        p.setBrush(QColor("#466f55"));p.drawPolygon(QPolygon(QVector<QPoint>{{0,170},{90,170},{90,140},{170,140},{170,160},{260,160},{260,135},{360,135},{360,200},{0,200}}));
    }
    p.restore();
}
HeroBadge::HeroBadge(Glyph icon,const QString &text,QWidget *parent):QLabel(text,parent),m_icon(icon) {
    setWordWrap(false);setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
    setFixedHeight(sizeHint().height());
}
QSize HeroBadge::sizeHint() const { return {fontMetrics().horizontalAdvance(text())+38,qMax(28,fontMetrics().height()+8)}; }
bool HeroBadge::event(QEvent *event) {
    const bool result=QLabel::event(event);
    if(event->type()==QEvent::FontChange){setFixedHeight(sizeHint().height());updateGeometry();}
    return result;
}
namespace {
void paintGlass(QPainter &painter,QWidget *widget,bool hovered=false,bool pressed=false,bool focused=false) {
    QPainterPath clip;clip.addRoundedRect(QRectF(widget->rect()),6,6);painter.setClipPath(clip);
    CoverView *cover=nullptr;
    for(auto ancestor=widget->parentWidget();ancestor;ancestor=ancestor->parentWidget())
        if((cover=qobject_cast<CoverView *>(ancestor)))break;
    if(cover)cover->paintFrost(painter,QRect(widget->mapTo(cover,QPoint()),widget->size()));
    painter.fillRect(widget->rect(),QColor(231,239,234,pressed?28:hovered?70:48));
    painter.setClipping(false);painter.setBrush(Qt::NoBrush);painter.setPen(QColor(255,255,255,focused?210:hovered?90:40));
    painter.drawRoundedRect(QRectF(widget->rect()).adjusted(.5,.5,-.5,-.5),6,6);
}
}
void HeroBadge::paintEvent(QPaintEvent *) {
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);paintGlass(painter,this);
    glyph(painter,m_icon,QRectF(7,(height()-13)/2.,13,13),Qt::white);
    painter.setFont(font());painter.setPen(Qt::white);
    painter.drawText(QRectF(25,0,width()-31,height()),Qt::AlignVCenter,text());
}
GlassButton::GlassButton(const QString &text,QWidget *parent):QPushButton(text,parent) {
    setCursor(Qt::PointingHandCursor);setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
}
QSize GlassButton::sizeHint() const { return {fontMetrics().horizontalAdvance(text())+46,qMax(36,fontMetrics().height()+16)}; }
bool GlassButton::event(QEvent *event) { hoverEvent(this,event);return QPushButton::event(event); }
void GlassButton::paintEvent(QPaintEvent *) {
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
    paintGlass(painter,this,isEnabled()&&underMouse(),isDown(),hasFocus());
    if(!isEnabled())painter.setOpacity(.45);
    const int contentWidth=fontMetrics().horizontalAdvance(text())+23;
    const int left=(width()-contentWidth)/2;
    icon().paint(&painter,QRect(left,(height()-16)/2,16,16));
    painter.setFont(font());painter.setPen(Qt::white);
    painter.drawText(QRect(left+23,0,contentWidth-23,height()),Qt::AlignVCenter,text());
}
CoverView::CoverView(QWidget *parent):QWidget(parent) { setMinimumHeight(240); }
void CoverView::resizeEvent(QResizeEvent *) {
    m_frost={};
    // Keep the artwork proportional without making wide windows consume the whole page.
    if(property("fixedPreview").toBool())return;setFixedHeight(qBound(250,qRound(width()*.306),340));
}
void CoverView::setCover(const QString &version,const QString &custom) { m_version=version;m_custom=custom;m_frost={};update(); }
void CoverView::paintEvent(QPaintEvent *) { QPainter p(this);paintBackground(p); }
void CoverView::paintBackground(QPainter &p) {
    CoverStore::paint(p,rect(),m_version,m_custom,10);
    if(m_shaded) { QPainterPath clip;clip.addRoundedRect(QRectF(rect()),10,10);p.setClipPath(clip);
        QLinearGradient shade(0,0,0,height());shade.setColorAt(0,QColor(10,23,19,0));shade.setColorAt(.35,QColor(10,23,19,12));shade.setColorAt(1,QColor(6,19,15,220));p.fillRect(rect(),shade); }
}
void CoverView::paintFrost(QPainter &painter,const QRect &source) {
    if(m_frost.isNull()) {
        // Blur a half-resolution backdrop once per cover/size change, never the text or icons.
        m_frost=QImage(qMax(1,width()/2),qMax(1,height()/2),QImage::Format_ARGB32_Premultiplied);
        m_frost.fill(Qt::transparent);
        { QPainter p(&m_frost);p.scale(m_frost.width()/qreal(width()),m_frost.height()/qreal(height()));paintBackground(p); }
        const int radius=5;
        for(int pass=0;pass<4;++pass) {
            QImage output(m_frost.size(),m_frost.format());
            const bool horizontal=pass%2==0;
            const int length=horizontal?m_frost.width():m_frost.height();
            const int lines=horizontal?m_frost.height():m_frost.width();
            for(int line=0;line<lines;++line) {
                auto pixel=[&](int i){return horizontal?m_frost.pixel(qBound(0,i,length-1),line):m_frost.pixel(line,qBound(0,i,length-1));};
                int r=0,g=0,b=0,a=0;
                auto add=[&](QRgb c,int sign){r+=sign*qRed(c);g+=sign*qGreen(c);b+=sign*qBlue(c);a+=sign*qAlpha(c);};
                for(int i=-radius;i<=radius;++i)add(pixel(i),1);
                for(int i=0;i<length;++i) {
                    const auto color=qRgba(r/11,g/11,b/11,a/11);
                    if(horizontal)output.setPixel(i,line,color);else output.setPixel(line,i,color);
                    add(pixel(i-radius),-1);add(pixel(i+radius+1),1);
                }
            }
            m_frost=output;
        }
    }
    const qreal sx=m_frost.width()/qreal(width()),sy=m_frost.height()/qreal(height());
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(QRectF(QPointF(),source.size()),m_frost,QRectF(source.x()*sx,source.y()*sy,source.width()*sx,source.height()*sy));
}
InstanceCard::InstanceCard(QWidget *parent):QAbstractButton(parent) {
    setFocusPolicy(Qt::StrongFocus);setCursor(Qt::PointingHandCursor);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setMinimumWidth(165);
    m_addMods=new QPushButton(QCoreApplication::translate("M2CL", "添加模组"),this);m_addMods->setObjectName("addInstanceMods");m_addMods->setProperty("compact",true);m_addMods->setFixedHeight(30);connect(m_addMods,&QPushButton::clicked,this,[this]{emit addModsRequested(m_instance.path,m_addMods->mapToGlobal(QPoint(0,m_addMods->height())));});
    m_more=new IconButton(Glyph::More,QCoreApplication::translate("M2CL", "实例操作"),this);m_more->setObjectName("instanceMenu");m_more->setFixedSize(28,29);
    m_star=new IconButton(Glyph::Star,QCoreApplication::translate("M2CL", "收藏实例"),this);m_star->setObjectName("favoriteInstance");m_star->setFixedSize(28,29);
    connect(m_more,&QAbstractButton::clicked,this,[this]{emit menuRequested(m_instance.path,m_more->mapToGlobal(QPoint(0,m_more->height())));});
    connect(m_star,&QAbstractButton::clicked,this,[this]{emit favoriteRequested(m_instance.path);});
}
void InstanceCard::setInstance(const Instance &instance,const QString &name,const QString &cover,bool favorite,bool selected) {
    m_instance=instance;m_name=name;m_cover=cover;m_favorite=favorite;m_selected=selected;
    setProperty("instancePath",instance.path);setAccessibleName(name+QCoreApplication::translate("M2CL", "，Minecraft ")+instance.version+QCoreApplication::translate("M2CL", "，")+instance.loaderName());
    setToolTip(name+"\n"+instance.gameDirectory);m_star->setProperty("gold",favorite);m_star->setToolTip(favorite?QCoreApplication::translate("M2CL", "取消收藏"):QCoreApplication::translate("M2CL", "收藏实例"));m_star->setAccessibleName(m_star->toolTip());
    m_star->setVisible(true);m_star->update();update();
}
bool InstanceCard::event(QEvent *e) {
    hoverEvent(this,e);
    return QAbstractButton::event(e);
}
void InstanceCard::resizeEvent(QResizeEvent *) {m_addMods->setGeometry(10,height()-39,qMin(112,width()-62),30); m_more->move(width()-39,height()-37);m_star->move(width()-38,8+qRound((width()-14)*.5625)); }
void InstanceCard::contextMenuEvent(QContextMenuEvent *e) { emit menuRequested(m_instance.path,e->globalPos());e->accept(); }
void InstanceCard::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const auto body=QRectF(rect()).adjusted(1,1,-1,-1);
    p.setBrush(qApp->palette().color(QPalette::Base));p.setPen(QPen(m_selected||hasFocus()?accent():border(),m_selected||hasFocus()?1.4:1));p.drawRoundedRect(body,9,9);
    const qreal coverHeight=(width()-14)*.5625;CoverStore::paint(p,QRectF(7,7,width()-14,coverHeight),m_instance.version,m_cover,5);
    QFont title=font();title.setPixelSize(ui::Body);title.setWeight(QFont::DemiBold);p.setFont(title);p.setPen(qApp->palette().color(QPalette::WindowText));
    p.drawText(QRectF(12,coverHeight+15,width()-47,20),Qt::AlignVCenter,p.fontMetrics().elidedText(m_name,Qt::ElideRight,width()-47));
    const QColor background=qApp->palette().color(QPalette::AlternateBase);const qreal first=pill(p,QPointF(12,coverHeight+42),m_instance.version,Glyph::Mods,background,muted());
    pill(p,QPointF(18+first,coverHeight+42),m_instance.loaderName(),m_instance.supportsMods()?Glyph::Mods:Glyph::Leaf,background,muted());
    p.setPen(muted());QFont foot=font();foot.setPixelSize(ui::Secondary);p.setFont(foot);

}
InstanceGrid::InstanceGrid(QWidget *parent):QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);m_layout=new QGridLayout(this);m_layout->setContentsMargins(0,0,0,0);m_layout->setSpacing(ui::Space);
    m_empty=new QLabel(QCoreApplication::translate("M2CL", "还没有游戏实例\n安装原版或添加已有游戏目录，开始你的冒险。"),this);m_empty->setAlignment(Qt::AlignCenter);m_empty->setProperty("muted",true);m_empty->setObjectName("emptyInstances");
}
void InstanceGrid::setInstances(const QList<Instance> &instances,const QString &selected,const QStringList &favorites,const QJsonObject &names,const QJsonObject &covers) {
    bool same=m_cards.size()==instances.size();for(int i=0;same&&i<m_cards.size();++i)same=m_cards.at(i)->instancePath()==instances.at(i).path;
    if(!same) {
        for(auto item:m_cards){m_layout->removeWidget(item);item->hide();item->deleteLater();}m_cards.clear();m_columns=0;
        for(const auto &instance:instances) {
            auto item=new InstanceCard(this);m_cards.append(item);
            connect(item,&QAbstractButton::clicked,this,[this,item]{emit this->selected(item->instancePath());});
            connect(item,&InstanceCard::addModsRequested,this,&InstanceGrid::addModsRequested);
            connect(item,&InstanceCard::favoriteRequested,this,&InstanceGrid::favoriteRequested);connect(item,&InstanceCard::menuRequested,this,&InstanceGrid::menuRequested);
            item->setInstance(instance,instance.id,{},false,false);
        }
    }
    for(int i=0;i<instances.size();++i){const auto &instance=instances.at(i);m_cards.at(i)->setInstance(instance,names.value(instance.path).toString(instance.id),covers.value(instance.path).toString(),favorites.contains(instance.path),instance.path==selected);}
    m_empty->setVisible(instances.isEmpty());arrange();
}
void InstanceGrid::resizeEvent(QResizeEvent *) { arrange(); }
void InstanceGrid::arrange() {
    const int columns=qBound(1,(width()+12)/202,4);const int cardWidth=(width()-(columns-1)*12)/columns;const int cardHeight=qRound((cardWidth-14)*.5625)+115;
    if(columns!=m_columns){while(auto item=m_layout->takeAt(0))delete item;for(int i=0;i<m_cards.size();++i)m_layout->addWidget(m_cards.at(i),i/columns,i%columns);m_columns=columns;}
    for(auto item:m_cards){item->setFixedSize(cardWidth,cardHeight);}
    m_layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    const int rows=(m_cards.size()+columns-1)/columns;setFixedHeight(m_cards.isEmpty()?140:rows*cardHeight+qMax(0,rows-1)*12);m_empty->setGeometry(rect());
}
AppearanceChoice::AppearanceChoice(const QString &mode,QWidget *parent):QAbstractButton(parent),m_mode(mode) {
    setCheckable(true);setFocusPolicy(Qt::StrongFocus);setCursor(Qt::PointingHandCursor);setMinimumSize(120,108);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    setAccessibleName(mode=="light"?QCoreApplication::translate("M2CL", "浅色"):mode=="dark"?QCoreApplication::translate("M2CL", "深色"):QCoreApplication::translate("M2CL", "跟随系统"));setObjectName("appearance-"+mode);
}
bool AppearanceChoice::event(QEvent *e) { hoverEvent(this,e);return QAbstractButton::event(e); }
void AppearanceChoice::setTheme(const QString &name){m_theme=name;update();}
void AppearanceChoice::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const QRectF preview(2,2,width()-4,76);
    auto paintPreview=[&](bool dark){
        const auto theme=makeTheme(m_theme,dark);p.setPen(Qt::NoPen);p.setBrush(theme.background);p.drawRoundedRect(preview,7,7);
        p.fillRect(QRectF(8,10,22,59),theme.sidebar);
        for(int i=0;i<3;++i)p.fillRect(QRectF(12,20+i*12,13,3),i==0?theme.accent:theme.muted);
        CoverStore::paint(p,QRectF(36,10,width()-46,33),"1.20",{},3);
        p.setPen(Qt::NoPen);p.setBrush(theme.accent);p.drawRoundedRect(QRectF(36,49,(width()-50)*.6,7),2,2);
        p.setBrush(theme.border);p.drawRoundedRect(QRectF(36,60,width()-46,7),2,2);
    };
    paintPreview(m_mode=="dark");
    if(m_mode=="system"){p.save();p.setClipRect(QRectF(width()/2.,0,width()/2.,80));paintPreview(true);p.restore();}
    p.setBrush(Qt::NoBrush);p.setPen(QPen(isChecked()||hasFocus()?accent():border(),isChecked()?1.6:1));p.drawRoundedRect(preview,7,7);
    p.setPen(isChecked()?accent():muted());QFont f=font();f.setPixelSize(ui::Secondary);p.setFont(f);p.drawText(QRectF(0,85,width(),22),Qt::AlignCenter,accessibleName());
}
DownloadPanel::DownloadPanel(QWidget *parent):QFrame(parent,Qt::Popup|Qt::FramelessWindowHint|Qt::NoDropShadowWindowHint) {
    setObjectName("downloadPanel");setAttribute(Qt::WA_TranslucentBackground);setFrameShape(QFrame::NoFrame);setFixedWidth(390);
    auto body=new QVBoxLayout(this);body->setContentsMargins(ui::CardMargin,ui::CardMargin,ui::CardMargin,ui::CardMargin);body->setSpacing(ui::Space);
    auto title=new QLabel(QCoreApplication::translate("M2CL", "下载任务"));title->setObjectName("sectionTitle");body->addWidget(title);
    m_state=new QLabel(QCoreApplication::translate("M2CL", "准备下一次冒险"));m_state->setProperty("muted",true);body->addWidget(m_state);
    auto task=new QHBoxLayout;auto icon=new GlyphView(Glyph::Download,25);task->addWidget(icon);task->addSpacing(6);
    m_name=new QLabel(QCoreApplication::translate("M2CL", "还没有下载任务"));m_name->setWordWrap(true);task->addWidget(m_name,1);body->addLayout(task);
    m_progress=new QProgressBar;m_progress->setRange(0,1000);m_progress->setTextVisible(false);m_progress->hide();body->addWidget(m_progress);
    m_message=new QLabel(QCoreApplication::translate("M2CL", "在“发现模组”里选择喜欢的模组，下载进度会显示在这里。"));m_message->setWordWrap(true);m_message->setProperty("muted",true);body->addWidget(m_message);
    m_taskNotice=new NetworkNotice;m_taskNotice->setObjectName("downloadTaskNetworkState");body->addWidget(m_taskNotice);
    auto row=new QHBoxLayout;m_cancel=new QPushButton(QCoreApplication::translate("M2CL", "取消下载"));m_cancel->hide();row->addWidget(m_cancel);
    auto all=new QPushButton(QCoreApplication::translate("M2CL", "查看模组下载"));row->addWidget(all,1);body->addLayout(row);
    connect(m_cancel,&QPushButton::clicked,this,&DownloadPanel::cancelRequested);connect(all,&QPushButton::clicked,this,&DownloadPanel::openRequested);
}
void DownloadPanel::setTask(const QString &name,const QString &message,int progress,bool active) {
    m_name->setText(name.isEmpty()?QCoreApplication::translate("M2CL", "还没有下载任务"):name);m_message->setText(message);
    m_state->setText(active?QCoreApplication::translate("M2CL", "1 个任务正在下载"):name.isEmpty()?QCoreApplication::translate("M2CL", "准备下一次冒险"):QCoreApplication::translate("M2CL", "最近的下载"));
    m_taskNotice->clear();const bool incomplete=!active&&!name.isEmpty()&&progress<0;
    m_message->setVisible(!incomplete);if(incomplete)m_taskNotice->failure(diagnosticContains(message, QT_TRANSLATE_NOOP("M2CL", "取消"))?QCoreApplication::translate("M2CL", "任务已取消"):QCoreApplication::translate("M2CL", "任务未完成"),message,false);
    m_progress->setVisible(!name.isEmpty()&&(active||progress>=0));m_progress->setRange(0,progress<0?0:1000);if(progress>=0)m_progress->setValue(progress);
    m_cancel->setVisible(active);adjustSize();
}
void DownloadPanel::paintEvent(QPaintEvent *){QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setBrush(palette().color(QPalette::Base));p.setPen(QPen(border(),1));p.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),10,10);}
void DownloadPanel::keyPressEvent(QKeyEvent *event) { if(event->key()==Qt::Key_Escape){hide();event->accept();}else QFrame::keyPressEvent(event); }
WindowBar::WindowBar(QWidget *window):QWidget(window) {
    setFixedHeight(48);setObjectName("windowBar");auto row=new QHBoxLayout(this);row->setContentsMargins(18,3,12,3);row->setSpacing(6);
    auto subtitle=new QLabel(QStringLiteral(""));subtitle->setProperty("muted",true);row->addWidget(subtitle);row->addStretch();
    downloads=new IconButton(Glyph::Download,QCoreApplication::translate("M2CL", "下载任务"),this);downloads->setObjectName("downloadButton");row->addWidget(downloads);
    appearance=new IconButton(Glyph::Palette,QCoreApplication::translate("M2CL", "外观与主题"),this);appearance->setObjectName("appearanceButton");row->addWidget(appearance);row->addSpacing(14);
#ifdef Q_OS_WIN
    auto minimize=new IconButton(Glyph::Minimize,QCoreApplication::translate("M2CL", "最小化"),this);auto maximize=new IconButton(Glyph::Maximize,QCoreApplication::translate("M2CL", "最大化或还原"),this);auto close=new IconButton(Glyph::Close,QCoreApplication::translate("M2CL", "关闭"),this);
    minimize->setObjectName("windowMinimize");maximize->setObjectName("windowMaximize");close->setObjectName("windowClose");
    row->addWidget(minimize);row->addWidget(maximize);row->addWidget(close);
    connect(minimize,&QAbstractButton::clicked,window,&QWidget::showMinimized);connect(maximize,&QAbstractButton::clicked,window,[window]{window->isMaximized()?window->showNormal():window->showMaximized();});connect(close,&QAbstractButton::clicked,window,&QWidget::close);
#endif
}
void WindowBar::mousePressEvent(QMouseEvent *event) {
    if(event->button()!=Qt::LeftButton){QWidget::mousePressEvent(event);return;}
    // The custom toolbar is also a drag surface with native macOS/Linux decorations.
    m_dragOffset=globalPosition(event)-window()->frameGeometry().topLeft();
    m_dragging=!(window()->windowHandle()&&window()->windowHandle()->startSystemMove());
}
void WindowBar::mouseMoveEvent(QMouseEvent *event) { if(m_dragging&&(event->buttons()&Qt::LeftButton)&&!window()->isMaximized())window()->move(globalPosition(event)-m_dragOffset);else QWidget::mouseMoveEvent(event); }
void WindowBar::mouseReleaseEvent(QMouseEvent *event) { if(event->button()==Qt::LeftButton)m_dragging=false;QWidget::mouseReleaseEvent(event); }
void WindowBar::mouseDoubleClickEvent(QMouseEvent *event) { if(window()->windowFlags().testFlag(Qt::FramelessWindowHint)&&event->button()==Qt::LeftButton){window()->isMaximized()?window()->showNormal():window()->showMaximized();m_dragging=false;}else QWidget::mouseDoubleClickEvent(event); }
}
