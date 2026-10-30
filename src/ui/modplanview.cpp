// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "modplanview.h"
#include "network/resourceclient.h"
#include "launcherwidgets.h"
#include <QSignalBlocker>
#include <QApplication>
#include <QCheckBox>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>

namespace mc {
ModPlanView::ModPlanView(const ModPlan &input,QWidget *parent,const QString &customCover,const ResourceFile &additionalPack):InstallPlanDialog(parent),m_plan(expandedModPlan(input)){
    const auto &plan=m_plan;
    setObjectName("modInstallPlan");
    auto body=contentLayout();
    auto text=[](const QString &value,bool muted=false){auto label=new QLabel(value);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);label->setProperty("muted",muted);return label;};
    QMap<QString,ResourceFile> nodes;for(const auto &file:plan.resolved)nodes.insert(file.projectId,file);
    auto name=[&](const QString &id){auto title=plan.projectNames.value(id);if(!title.isEmpty())return title;auto file=nodes.value(id);if(!file.versionName.isEmpty())return file.versionName;return id;};

    addTargetInstance(plan.displayTargetName.isEmpty()?plan.target.id:plan.displayTargetName,
        QStringLiteral("Minecraft %1 · %2 %3").arg(plan.target.version,plan.target.loaderName(),plan.target.loaderVersion).trimmed(),
        CoverStore::image(plan.target.version,QSize(1200,320),customCover));
    auto listHost=new QWidget;auto listOuter=new QVBoxLayout(listHost);listOuter->setContentsMargins(0,0,0,0);auto list=section(listOuter,QCoreApplication::translate("M2CL", "将添加"),"add");
    if(!additionalPack.fileName.isEmpty()){auto row=planRow();auto card=qobject_cast<QFrame *>(row->parentWidget());auto mark=new CheckBox;mark->setChecked(true);mark->setEnabled(false);mark->setFixedWidth(22);row->addWidget(mark);auto icon=new QLabel;icon->setPixmap(lineIcon(Glyph::Folder).pixmap(24,24));row->addWidget(icon);auto kind=text(QCoreApplication::translate("M2CL", "光影包"));kind->setFixedWidth(126);row->addWidget(kind);row->addWidget(text(additionalPack.fileName,true),1);row->addWidget(text(additionalPack.versionNumber,true));list->addWidget(card);}
    auto icons=new ResourceClient(this);QMultiMap<QUrl,QLabel *> iconLabels;
    auto projectIcon=[&](const QString &id){
        auto icon=new QLabel;icon->setObjectName("installProjectIcon");icon->setProperty("projectId",id);icon->setFixedSize(30,30);icon->setAlignment(Qt::AlignCenter);
        QPixmap local;if(local.loadFromData(plan.localProjectIcons.value(id))){icon->setPixmap(local.scaled(30,30,Qt::KeepAspectRatio,Qt::SmoothTransformation));icon->setProperty("iconSource","archive");}
        else{icon->setPixmap(lineIcon(Glyph::Mods,palette().color(QPalette::WindowText)).pixmap(22,22));icon->setProperty("iconSource","fallback");if(!plan.projectIcons.value(id).isEmpty())iconLabels.insert(plan.projectIcons.value(id),icon);}
        return icon;
    };
    auto orderedFiles=plan.files;auto group=[&](const QString &id){return id==plan.rootProjectId?-1:plan.requiredProjects.contains(id)?0:plan.optionalProjects.contains(id)?1:2;};std::stable_sort(orderedFiles.begin(),orderedFiles.end(),[&](const ResourceFile &a,const ResourceFile &b){return group(a.projectId)<group(b.projectId);});bool optionalHeading=false;
    for(const auto &file:orderedFiles){
        if(group(file.projectId)>0&&!optionalHeading){list->addWidget(text(QCoreApplication::translate("M2CL", "可选组件及其前置"),true));optionalHeading=true;}
        auto row=planRow();auto card=qobject_cast<QFrame *>(row->parentWidget());
        if(file.projectId!=plan.rootProjectId){auto checked=new CheckBox;checked->setFixedWidth(22);checked->setObjectName("dependencyChoice");checked->setProperty("projectId",file.projectId);checked->setAccessibleName(QCoreApplication::translate("M2CL", "添加依赖 %1").arg(name(file.projectId)));checked->setChecked(true);m_dependencies.insert(file.projectId,checked);if(!plan.optionalProjects.contains(file.projectId))m_preferredDependencies.insert(file.projectId);row->addWidget(checked);}else{auto rootCheck=new CheckBox;rootCheck->setFixedWidth(22);rootCheck->setChecked(true);rootCheck->setEnabled(false);rootCheck->setToolTip(QCoreApplication::translate("M2CL", "本次安装的主模组"));row->addWidget(rootCheck);}
        const bool disabled=plan.disabledFiles.contains(file.projectId);QStringList oldVersions;for(auto it=plan.replacementProjects.begin();it!=plan.replacementProjects.end();++it)if(it.value()==file.projectId)oldVersions.append(plan.replacementVersions.value(it.key(),it.key()));oldVersions.removeDuplicates();
        row->addWidget(projectIcon(file.projectId));
        auto kind=text(file.projectId==plan.rootProjectId?QCoreApplication::translate("M2CL", "模组"):plan.optionalProjects.contains(file.projectId)?QCoreApplication::translate("M2CL", "可选组件"):QCoreApplication::translate("M2CL", "必需前置"));kind->setFixedWidth(126);row->addWidget(kind);
        auto title=text(name(file.projectId),true);title->setObjectName("dependencyDetails");title->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);row->addWidget(title,1);
        QStringList parents;for(const auto &edge:plan.edges)if(edge.to==file.projectId&&(edge.type=="required"||edge.type=="optional"))parents.append(name(edge.from));parents.removeDuplicates();
        QString details=parents.isEmpty()?name(file.projectId):name(file.projectId)+QCoreApplication::translate("M2CL", "\n依赖来源：")+parents.join(QCoreApplication::translate("M2CL", "、"));if(!oldVersions.isEmpty())details+=QCoreApplication::translate("M2CL", "\n替换：")+oldVersions.join(QCoreApplication::translate("M2CL", "、"));card->setToolTip(details);
        auto version=text(file.versionNumber,true);version->setFixedWidth(115);version->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);row->addWidget(version);
        if(disabled){auto action=new SelectBox;action->setObjectName("disabledDependencyAction");action->setProperty("projectId",file.projectId);action->addItem(QCoreApplication::translate("M2CL", "启用已有文件"),"enable");action->addItem(QCoreApplication::translate("M2CL", "重新下载"),"download");action->setMaximumWidth(160);m_enableActions.insert(file.projectId,action);list->addWidget(card);auto actionRow=new QHBoxLayout;actionRow->addStretch();actionRow->addWidget(action);list->addLayout(actionRow);card->setToolTip(plan.disabledFiles.value(file.projectId));continue;}list->addWidget(card);
    }
    if(!orderedFiles.isEmpty()||!additionalPack.fileName.isEmpty()){auto scroll=new QScrollArea;scroll->setObjectName("modDependencyChoices");scroll->setFrameShape(QFrame::NoFrame);scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setWidget(listHost);scroll->setFixedHeight(qMin(310,listHost->sizeHint().height()+2));body->addWidget(scroll);}else{delete listHost;body->addWidget(text(plan.reusedProjects.isEmpty()?QCoreApplication::translate("M2CL", "没有需要另行下载的前置。"):QCoreApplication::translate("M2CL", "检测到已安装的模组，可直接复用。"),true));}
    QStringList embedded;for(const auto &edge:plan.edges)if(edge.type=="embedded")embedded.append(edge.to);embedded.removeDuplicates();
    if(!plan.reusedProjects.isEmpty()||!embedded.isEmpty()){
        auto installed=new QFrame;auto installedOuter=new QVBoxLayout(installed);installedOuter->setContentsMargins(0,0,0,0);auto installedBody=section(installedOuter,QCoreApplication::translate("M2CL", "已有模组 / 内置依赖"),"reuse");
        for(const auto &id:plan.reusedProjects){
            auto row=planRow();auto card=qobject_cast<QFrame *>(row->parentWidget());
            auto mark=new QLabel;mark->setFixedSize(22,22);mark->setPixmap(lineIcon(Glyph::Check,palette().color(QPalette::Mid)).pixmap(20,20));row->addWidget(mark);
            row->addWidget(projectIcon(id));
            auto info=new QVBoxLayout;info->setSpacing(4);auto label=text(name(id));label->setToolTip(plan.reusedFiles.value(id));info->addWidget(label);
            info->addWidget(text(QCoreApplication::translate("M2CL", "%1 · 已安装").arg(nodes.value(id).versionNumber),true));row->addLayout(info,1);
            auto reuse=new RadioButton(QCoreApplication::translate("M2CL", "复用"));reuse->setObjectName("reuseChoice");reuse->setProperty("projectId",id);reuse->setChecked(true);
            auto again=new RadioButton(QCoreApplication::translate("M2CL", "重新下载"));again->setObjectName("redownloadChoice");again->setProperty("projectId",id);again->setToolTip(QCoreApplication::translate("M2CL", "校验成功后替换现有副本，旧文件保留备份"));
            reuse->setAccessibleName(QCoreApplication::translate("M2CL", "复用 %1").arg(name(id)));again->setAccessibleName(QCoreApplication::translate("M2CL", "重新下载 %1").arg(name(id)));
            auto choices=new QButtonGroup(card);choices->addButton(reuse);choices->addButton(again);m_reuseChoices.insert(id,reuse);m_redownloads.insert(id,again);row->addWidget(reuse);row->addWidget(again);installedBody->addWidget(card);

        }
        for(const auto &id:embedded){
            auto row=planRow();auto card=qobject_cast<QFrame *>(row->parentWidget());card->setObjectName("embeddedDependency");card->setProperty("projectId",id);
            auto mark=new QLabel;mark->setFixedSize(22,22);mark->setPixmap(lineIcon(Glyph::Check,palette().color(QPalette::Mid)).pixmap(20,20));row->addWidget(mark);
            row->addWidget(projectIcon(id));row->addWidget(text(name(id)),1);
            auto status=text(QCoreApplication::translate("M2CL", "随模组内置"),true);row->addWidget(status);
            QStringList parents;for(const auto &edge:plan.edges)if(edge.type=="embedded"&&edge.to==id)parents.append(name(edge.from));parents.removeDuplicates();
            card->setToolTip(QCoreApplication::translate("M2CL", "由 %1 随文件提供，无需单独下载。安装时仍会核对文件内的依赖。").arg(parents.join(QCoreApplication::translate("M2CL", "、"))));installedBody->addWidget(card);
        }
        auto scroll=new QScrollArea;scroll->setObjectName("modInstalledChoices");scroll->setFrameShape(QFrame::NoFrame);scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setWidget(installed);scroll->setFixedHeight(qMin(138,installed->sizeHint().height()+4));body->addWidget(scroll);
    }
    QStringList noteLines=plan.notes;for(auto it=plan.optionalErrors.begin();it!=plan.optionalErrors.end();++it)noteLines.append(QCoreApplication::translate("M2CL", "可选组件 %1 暂不可用：%2").arg(name(it.key()),it.value()));if(!noteLines.isEmpty()){auto details=new QPushButton(QCoreApplication::translate("M2CL", "查看检测说明"));details->setProperty("textLink",true);body->addWidget(details,0,Qt::AlignLeft);auto notes=new QScrollArea;notes->setFrameShape(QFrame::NoFrame);notes->setWidgetResizable(true);notes->setWidget(text(noteLines.join('\n'),true));notes->setFixedHeight(85);notes->hide();body->addWidget(notes);connect(details,&QPushButton::clicked,this,[notes]{notes->setVisible(!notes->isVisible());});}
    auto issues=new QWidget;auto issuesOuter=new QVBoxLayout(issues);issuesOuter->setContentsMargins(0,0,0,0);auto issuesBody=section(issuesOuter,QCoreApplication::translate("M2CL", "需要处理"),"warning");body->addWidget(issues);
    for(auto it=plan.disableCandidates.begin();it!=plan.disableCandidates.end();++it){auto choice=new CheckBox(QCoreApplication::translate("M2CL", "停用 %1").arg(it.key()));choice->setObjectName("disableConflictingComponent");choice->setToolTip(it.value());issuesBody->addWidget(choice);m_disableConflicts.insert(it.key(),choice);issuesBody->addWidget(text(it.value(),true));}
    if(!plan.error.isEmpty()){auto error=text(plan.error);error->setObjectName("modPlanError");issuesBody->addWidget(error);}
    auto warning=text(QCoreApplication::translate("M2CL", "未选择必需依赖，游戏可能无法启动。你可以稍后自行安装。"));warning->setObjectName("modDependencyWarning");warning->setProperty("planError",true);issuesBody->addWidget(warning);
    auto selectionError=text({});selectionError->setObjectName("modSelectionError");selectionError->setProperty("planError",true);issuesBody->addWidget(selectionError);
    auto replacement=text(QCoreApplication::translate("M2CL", "将替换现有副本，旧文件保留备份；不会留下两份同时启用的模组。"),true);replacement->setObjectName("modReplacementNotice");issuesBody->addWidget(replacement);
    auto footer=new QHBoxLayout;auto count=text({},true);count->setObjectName("modDependencyCount");footer->addWidget(count,1);auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->setStyleSheet("QDialogButtonBox { button-layout: 1; }");auto download=buttons->button(QDialogButtonBox::Ok);download->setProperty("primary",true);download->setEnabled(plan.error.isEmpty());buttons->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("M2CL", "取消"));footer->addWidget(buttons);body->addLayout(footer);
    auto update=[this,warning,replacement,count,download,selectionError,issues]{const auto selected=selectedPlan();selectionError->setText(selected.error);selectionError->setVisible(!selected.error.isEmpty()&&m_plan.error.isEmpty());download->setEnabled(selected.error.isEmpty());download->setToolTip(selected.error);int dependencies=0;for(auto it=m_dependencies.begin();it!=m_dependencies.end();++it){const QSignalBlocker blocker(it.value());const bool demanded=selected.demandedProjects.contains(it.key());const bool active=selected.activeProjects.contains(it.key());it.value()->setChecked(active);it.value()->setEnabled(demanded);it.value()->setToolTip(demanded?QString():QCoreApplication::translate("M2CL", "仅由未选择的上级前置引入，本次不添加"));if(active)++dependencies;}for(auto it=m_redownloads.begin();it!=m_redownloads.end();++it){it.value()->setEnabled(selected.activeProjects.contains(it.key()));m_reuseChoices.value(it.key())->setEnabled(selected.activeProjects.contains(it.key()));}for(auto it=m_enableActions.begin();it!=m_enableActions.end();++it)it.value()->setEnabled(selected.activeProjects.contains(it.key()));issues->setVisible(!m_disableConflicts.isEmpty()||!selected.error.isEmpty()||!selected.skippedProjects.isEmpty()||!selected.replacements.isEmpty());warning->setVisible(!selected.skippedProjects.isEmpty());replacement->setVisible(!selected.replacements.isEmpty());count->setText(QCoreApplication::translate("M2CL", "已选 %1 项依赖 · 下载 %2 项 · 启用 %3 项").arg(dependencies).arg(selected.files.size()).arg(selected.enableFiles.size()));download->setText(!selected.files.isEmpty()?QCoreApplication::translate("M2CL", "确认并安装"):!selected.enableFiles.isEmpty()?QCoreApplication::translate("M2CL", "确认启用"):QCoreApplication::translate("M2CL", "使用现有模组"));};
    for(auto check:m_disableConflicts)connect(check,&QCheckBox::toggled,this,update);
    for(auto it=m_dependencies.begin();it!=m_dependencies.end();++it){const auto id=it.key();connect(it.value(),&QCheckBox::toggled,this,[this,id,update](bool on){if(on)m_preferredDependencies.insert(id);else m_preferredDependencies.remove(id);update();});}for(auto check:m_redownloads)connect(check,&QRadioButton::toggled,this,update);for(auto action:m_enableActions)connect(action,QOverload<int>::of(&QComboBox::currentIndexChanged),this,update);update();
    connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(icons,&ResourceClient::iconReady,this,[iconLabels](const QUrl &url,const QByteArray &bytes){QPixmap image;if(image.loadFromData(bytes))for(auto label:iconLabels.values(url)){label->setText({});label->setPixmap(image.scaled(30,30,Qt::KeepAspectRatio,Qt::SmoothTransformation));label->setProperty("iconSource","project");}});
    if(!qApp->property("offlineUiTest").toBool()&&!qApp->arguments().contains("--smoke-test"))for(const auto &url:iconLabels.uniqueKeys())icons->icon(url);
    resize(700,sizeHint().height());
}
ModPlan ModPlanView::selectedPlan() const {
    QSet<QString> redownload,enable;for(auto it=m_redownloads.begin();it!=m_redownloads.end();++it)if(it.value()->isChecked())redownload.insert(it.key());for(auto it=m_enableActions.begin();it!=m_enableActions.end();++it)if(it.value()->currentData()=="enable")enable.insert(it.key());auto selected=selectModDependencies(m_plan,m_preferredDependencies,redownload,enable);for(auto it=m_disableConflicts.begin();it!=m_disableConflicts.end();++it){if(it.value()->isChecked())selected.disableFiles.append(it.key());else selected.error=QCoreApplication::translate("M2CL", "请确认停用冲突组件后再安装，或取消本次安装。");}return selected;
}
}
