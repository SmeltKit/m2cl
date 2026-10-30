// Copyright (C) 2026 ppm203; GPL-3.0-only with ADDITIONAL_TERMS.md.
#pragma once
#include "installplandialog.h"
#include "core/accounts.h"
#include "core/skins.h"
class QLabel;
class QPushButton;
namespace mc {
class AccountManager;
class NetworkNotice;
class SelectBox;
class SkinPreview;
class SkinEditor final:public InstallPlanDialog {
public:
    SkinEditor(AccountManager &manager,const Account &account,const QString &directory,QWidget *parent=nullptr);
    bool importFile(const QString &path);
private:
    void updatePreview();
    AccountManager &m_manager;
    Account m_account;
    SkinStore m_store;
    QJsonObject m_local;
    SelectBox *m_source,*m_model;
    SkinPreview *m_preview;
    QPushButton *m_browse,*m_save;
    QLabel *m_file,*m_hint;
    NetworkNotice *m_notice;
};
}
