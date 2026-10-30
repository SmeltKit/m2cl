// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "network/modinstaller.h"
#include "installplandialog.h"
class QCheckBox;
class QComboBox;
class QRadioButton;
namespace mc {
class ModPlanView final:public InstallPlanDialog {
public:
    explicit ModPlanView(const ModPlan &plan,QWidget *parent=nullptr,const QString &customCover={},const ResourceFile &additionalPack={});
    ModPlan selectedPlan() const;
private:
    ModPlan m_plan;
    QSet<QString> m_preferredDependencies;
    QMap<QString,QCheckBox *> m_dependencies,m_disableConflicts;
    QMap<QString,QRadioButton *> m_redownloads,m_reuseChoices;
    QMap<QString,QComboBox *> m_enableActions;
};
}
