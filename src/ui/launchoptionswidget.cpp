// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "launchoptionswidget.h"
#include "launcherwidgets.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QSignalBlocker>

namespace mc {
LaunchOptionsWidget::LaunchOptionsWidget(const LaunchOptions &o,const MemoryContext &context,QWidget *parent,std::function<MemorySnapshot()> sampler)
    :QWidget(parent),m_context(context),m_sampler(sampler?std::move(sampler):systemMemory),m_manualMax(o.maxMemoryMiB){
    setObjectName("instanceLaunchOptions");auto body=new QVBoxLayout(this);body->setContentsMargins(0,0,0,0);body->setSpacing(14);
    auto heading=[&](const QString &text){auto label=new QLabel(text);label->setObjectName("sectionTitle");body->addWidget(label);};
    auto hint=[&](const QString &text){auto label=new QLabel(text);label->setWordWrap(true);label->setProperty("muted",true);body->addWidget(label);};
    auto memoryHeading=new QHBoxLayout;auto memoryTitle=new QLabel(QCoreApplication::translate("M2CL", "内存分配"));memoryTitle->setObjectName("sectionTitle");memoryHeading->addWidget(memoryTitle,1);
    m_recommend=new QPushButton(QCoreApplication::translate("M2CL", "使用推荐设置"));m_recommend->setObjectName("useRecommendedMemory");m_recommend->setProperty("compact",true);m_recommend->setIcon(lineIcon(Glyph::Refresh));memoryHeading->addWidget(m_recommend);body->addLayout(memoryHeading);
    auto modes=new QHBoxLayout;modes->setSpacing(18);m_auto=new RadioButton(QCoreApplication::translate("M2CL", "自动分配（推荐）"));m_auto->setObjectName("instanceMemoryAuto");m_manual=new RadioButton(QCoreApplication::translate("M2CL", "手动指定"));m_manual->setObjectName("instanceMemoryManual");m_auto->setChecked(o.autoMemory);m_manual->setChecked(!o.autoMemory);modes->addWidget(m_auto);modes->addWidget(m_manual);modes->addStretch();body->addLayout(modes);
    auto card=new QFrame;card->setProperty("card",true);auto cardBody=new QVBoxLayout(card);cardBody->setContentsMargins(16,16,16,16);cardBody->setSpacing(12);
    auto maximumLabel=new QLabel(QCoreApplication::translate("M2CL", "最大内存"));maximumLabel->setProperty("fieldLabel",true);cardBody->addWidget(maximumLabel);
    m_max=new NumberBox;m_max->setObjectName("instanceMaxMemory");m_max->setRange(512,32768);m_max->setSingleStep(512);m_max->setFixedWidth(176);m_max->setAccessibleName(QCoreApplication::translate("M2CL", "最大内存（MiB）"));m_max->setValue(o.maxMemoryMiB);
    auto maximum=new QWidget;auto row=new QHBoxLayout(maximum);row->setContentsMargins(0,0,0,0);row->setSpacing(10);m_slider=new QSlider(Qt::Horizontal);m_slider->setObjectName("instanceMemorySlider");m_slider->setRange(512,32768);m_slider->setSingleStep(512);m_slider->setValue(o.maxMemoryMiB);row->addWidget(m_slider,1);row->addWidget(m_max);row->addWidget(numberLabel("MiB"));
    m_modeBadge=new QLabel;m_modeBadge->setObjectName("instanceMemoryModeBadge");m_modeBadge->setProperty("resourceTag",true);row->addWidget(m_modeBadge);
    connect(m_slider,&QSlider::valueChanged,m_max,&QSpinBox::setValue);connect(m_max,QOverload<int>::of(&QSpinBox::valueChanged),m_slider,&QSlider::setValue);cardBody->addWidget(maximum);
    m_memoryHint=new QLabel;m_memoryHint->setObjectName("memoryRecommendationHint");m_memoryHint->setProperty("muted",true);m_memoryHint->setWordWrap(true);cardBody->addWidget(m_memoryHint);
    auto info=new QHBoxLayout;m_detailsButton=new QPushButton(QCoreApplication::translate("M2CL", "▸ 推荐依据"));m_detailsButton->setObjectName("memoryRecommendationDetails");m_detailsButton->setCheckable(true);m_detailsButton->setProperty("memoryDisclosure",true);info->addWidget(m_detailsButton);info->addStretch();auto refresh=new QPushButton;refresh->setIcon(lineIcon(Glyph::Refresh));refresh->setObjectName("refreshMemoryRecommendation");refresh->setProperty("iconOnly",true);refresh->setFixedSize(30,30);refresh->setToolTip(QCoreApplication::translate("M2CL", "重新检测可用内存"));refresh->setAccessibleName(QCoreApplication::translate("M2CL", "重新检测可用内存"));info->addWidget(refresh);cardBody->addLayout(info);
    m_basis=new QLabel;m_basis->setObjectName("memoryRecommendationBasis");m_basis->setWordWrap(true);m_basis->setProperty("muted",true);m_basis->hide();cardBody->addWidget(m_basis);
    auto divider=new QFrame;divider->setProperty("settingsDivider",true);cardBody->addWidget(divider);
    auto initial=new QHBoxLayout;auto initialLabel=numberLabel(QCoreApplication::translate("M2CL", "初始内存"));initialLabel->setProperty("fieldLabel",true);initial->addWidget(initialLabel);initial->addStretch();m_min=new NumberBox;m_min->setObjectName("instanceMinMemory");m_min->setRange(0,32768);m_min->setSingleStep(256);m_min->setEmptyValuePlaceholder(QCoreApplication::translate("M2CL", "由 Java 决定"));m_min->setFixedWidth(240);m_min->setAccessibleName(QCoreApplication::translate("M2CL", "初始内存（MiB）"));m_min->setAccessibleDescription(QCoreApplication::translate("M2CL", "可留空，由 Java 决定初始内存；也可填写 MiB 数值"));m_min->setValue(o.minMemoryMiB);initial->addWidget(m_min);initial->addWidget(numberLabel("MiB"));cardBody->addLayout(initial);body->addWidget(card);
    heading(QCoreApplication::translate("M2CL", "游戏窗口"));auto windowCard=new QFrame;windowCard->setProperty("card",true);auto windowBody=new QVBoxLayout(windowCard);windowBody->setContentsMargins(16,16,16,16);windowBody->setSpacing(14);m_resolution=new CheckBox(QCoreApplication::translate("M2CL", "自定义窗口大小"));m_resolution->setObjectName("instanceCustomResolution");m_resolution->setChecked(o.customResolution);windowBody->addWidget(m_resolution);
    auto resolution=new QWidget;auto dimensions=new QHBoxLayout(resolution);dimensions->setContentsMargins(0,0,0,0);dimensions->addWidget(numberLabel(QCoreApplication::translate("M2CL", "宽度")));
    m_width=new NumberBox;m_width->setObjectName("instanceWindowWidth");m_width->setRange(320,16384);m_width->setAccessibleName(QCoreApplication::translate("M2CL", "窗口宽度（px）"));m_width->setValue(o.width);dimensions->addWidget(m_width,1);dimensions->addWidget(numberLabel("px"));dimensions->addSpacing(12);dimensions->addWidget(numberLabel(QCoreApplication::translate("M2CL", "高度")));
    m_height=new NumberBox;m_height->setObjectName("instanceWindowHeight");m_height->setRange(240,16384);m_height->setAccessibleName(QCoreApplication::translate("M2CL", "窗口高度（px）"));m_height->setValue(o.height);dimensions->addWidget(m_height,1);dimensions->addWidget(numberLabel("px"));resolution->setEnabled(o.customResolution);windowBody->addWidget(resolution);connect(m_resolution,&QCheckBox::toggled,resolution,&QWidget::setEnabled);
    auto windowDivider=new QFrame;windowDivider->setProperty("settingsDivider",true);windowBody->addWidget(windowDivider);
    m_fullscreen=new CheckBox(QCoreApplication::translate("M2CL", "以全屏模式启动"));m_fullscreen->setObjectName("instanceFullscreen");m_fullscreen->setChecked(o.fullscreen);windowBody->addWidget(m_fullscreen);body->addWidget(windowCard);
    heading(QCoreApplication::translate("M2CL", "附加启动参数"));
    auto jvmLabel=new QLabel(QCoreApplication::translate("M2CL", "JVM 参数"));jvmLabel->setProperty("fieldLabel",true);body->addWidget(jvmLabel);
    m_jvm=new QPlainTextEdit(o.jvmArguments);m_jvm->setObjectName("instanceJvmArguments");m_jvm->setPlaceholderText("-XX:+UseG1GC\n-XX:MaxGCPauseMillis=100");m_jvm->setMinimumHeight(90);m_jvm->setMaximumHeight(130);m_jvm->setTabChangesFocus(true);body->addWidget(m_jvm);
    hint(QCoreApplication::translate("M2CL", "用于 Java 虚拟机。支持空格或换行分隔；带空格的值用英文引号括起。内存请用上面的控件设置。"));
    auto gameLabel=new QLabel(QCoreApplication::translate("M2CL", "游戏参数"));gameLabel->setProperty("fieldLabel",true);body->addWidget(gameLabel);
    m_game=new QPlainTextEdit(o.gameArguments);m_game->setObjectName("instanceGameArguments");m_game->setPlaceholderText(QCoreApplication::translate("M2CL", "填写 Minecraft 支持的附加参数"));m_game->setMinimumHeight(80);m_game->setMaximumHeight(120);m_game->setTabChangesFocus(true);body->addWidget(m_game);
    hint(QCoreApplication::translate("M2CL", "用于 Minecraft 客户端，是否可用取决于游戏版本。留空会保留版本自带的启动参数。"));
    m_error=new QLabel;m_error->setObjectName("instanceLaunchError");m_error->setProperty("planError",true);m_error->setWordWrap(true);body->addWidget(m_error);body->addStretch();
    for(auto spin:{m_max,m_min,m_width,m_height})connect(spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{validate();});
    connect(m_jvm,&QPlainTextEdit::textChanged,this,&LaunchOptionsWidget::validate);connect(m_game,&QPlainTextEdit::textChanged,this,&LaunchOptionsWidget::validate);
    connect(m_max,QOverload<int>::of(&QSpinBox::valueChanged),this,[this](int value){if(m_updating)return;m_manualMax=value;setAutomatic(false);});
    connect(m_auto,&QRadioButton::toggled,this,[this](bool enabled){if(!m_updating)setAutomatic(enabled);});
    connect(refresh,&QPushButton::clicked,this,&LaunchOptionsWidget::refreshRecommendation);
    connect(m_recommend,&QPushButton::clicked,this,[this]{refreshRecommendation();if(!m_recommendation.valid())return;m_min->setValue(0);setAutomatic(true);});
    connect(m_detailsButton,&QPushButton::toggled,this,[this](bool shown){m_basis->setVisible(shown);m_detailsButton->setText(shown?QCoreApplication::translate("M2CL", "▾ 推荐依据"):QCoreApplication::translate("M2CL", "▸ 推荐依据"));});
    refreshRecommendation();
}
LaunchOptions LaunchOptionsWidget::options() const {
    LaunchOptions o;o.autoMemory=m_auto->isChecked();o.maxMemoryMiB=o.autoMemory?m_manualMax:m_max->value();o.minMemoryMiB=m_min->value();o.customResolution=m_resolution->isChecked();o.width=m_width->value();o.height=m_height->value();o.fullscreen=m_fullscreen->isChecked();o.jvmArguments=m_jvm->toPlainText();o.gameArguments=m_game->toPlainText();return o;
}
QString LaunchOptionsWidget::selectionError() const{const auto o=options();const auto error=validateLaunchOptions(o);if(!error.isEmpty())return error;if(o.autoMemory&&m_recommendation.valid()&&o.minMemoryMiB>m_recommendation.maximumMiB)return QCoreApplication::translate("M2CL", "初始内存高于本次推荐上限，请调低初始内存或改为手动分配");return {};}
void LaunchOptionsWidget::validate(){const auto error=selectionError();m_error->setText(error);m_error->setVisible(!error.isEmpty());}
void LaunchOptionsWidget::setMemoryContext(const MemoryContext &context){m_context=context;refreshRecommendation();}
void LaunchOptionsWidget::refreshRecommendation(){
    m_recommendation=recommendMemory(m_sampler(),m_context);
    if(m_auto->isChecked()&&m_recommendation.valid()){m_updating=true;m_max->setValue(m_recommendation.maximumMiB);m_updating=false;}
    updateMemoryPresentation();validate();
}
void LaunchOptionsWidget::setAutomatic(bool enabled){
    m_updating=true;m_auto->setChecked(enabled);m_manual->setChecked(!enabled);
    m_max->setValue(enabled&&m_recommendation.valid()?m_recommendation.maximumMiB:m_manualMax);m_updating=false;
    updateMemoryPresentation();validate();
}
void LaunchOptionsWidget::updateMemoryPresentation(){
    const bool automatic=m_auto->isChecked();m_modeBadge->setText(automatic?(m_recommendation.valid()?QCoreApplication::translate("M2CL", "自动"):QCoreApplication::translate("M2CL", "待检测")):QCoreApplication::translate("M2CL", "手动"));
    auto message=m_recommendation.valid()?(automatic?QCoreApplication::translate("M2CL", "启动前根据可用内存重新计算"):QCoreApplication::translate("M2CL", "当前推荐 %1").arg(memorySize(m_recommendation.maximumMiB))):m_recommendation.error;
    if(!m_recommendation.warning.isEmpty())message+="\n"+m_recommendation.warning;
    m_memoryHint->setText(message);m_basis->setText(m_recommendation.basis);m_recommend->setEnabled(m_recommendation.valid());m_recommend->setToolTip(QCoreApplication::translate("M2CL", "仅恢复自动内存与默认初始内存，其他设置保持不变"));
}
}
