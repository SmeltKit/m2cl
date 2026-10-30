// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "installplandialog.h"
#include "network/accountmanager.h"
#include "core/settings.h"
class QLabel;
class QPushButton;
class QCheckBox;
class QLineEdit;
class QTableWidget;
namespace mc {
class NetworkNotice;
class BusySpinner;
class AvatarCache;
class AccountAvatar;
class MicrosoftLoginDialog final:public InstallPlanDialog {
    Q_OBJECT
public:
    MicrosoftLoginDialog(AccountManager &manager,const QString &clientId,const QString &accountId={},bool refresh=false,QWidget *parent=nullptr);
    AuthSession session() const{return m_session;}
    void reject() override;
signals:
    void configurationRequested();
private:
    void start();
    AccountManager &m_manager;
    QString m_clientId,m_accountId;
    bool m_refresh=false;
    QUrl m_browserUrl;
    AuthSession m_session;
    QLabel *m_stage;
    BusySpinner *m_spinner;
    NetworkNotice *m_notice;
    QCheckBox *m_remember;
    QPushButton *m_start,*m_cancel,*m_browser,*m_once,*m_config,*m_serviceHelp;
};
class AccountsPanel final:public QWidget {
    Q_OBJECT
public:
    AccountsPanel(AccountManager &manager,AvatarCache &avatars,Settings &settings,SettingsStore store,QWidget *parent=nullptr);
    void showConfiguration();
    static QString configuredClientId(const Settings &settings);
    void login(const QString &id={});
    void addOffline();
    void editSkin(const QString &id);
signals:
    void configurationRequested();
private:
    void refresh();
    AccountManager &m_manager;
    AvatarCache &m_avatars;
    Settings &m_settings;
    SettingsStore m_store;
    QLabel *m_current,*m_currentState;
    AccountAvatar *m_currentAvatar;
    QTableWidget *m_table;
    QPushButton *m_addMicrosoft,*m_addOffline;
    NetworkNotice *m_notice;
    QWidget *m_configuration;
    QPushButton *m_configurationToggle;
    QLineEdit *m_clientId;
};
}
