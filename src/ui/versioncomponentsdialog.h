// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "installplandialog.h"
#include "core/catalog.h"
#include "network/gameinstaller.h"
#include <QFutureWatcher>

class QAction;
class QLabel;
class QListWidget;
class QPushButton;
class QTabBar;
namespace mc {
class BusySpinner;
class NetworkNotice;
class RadioButton;
class SelectBox;

class VersionComponentsDialog final : public InstallPlanDialog {
    Q_OBJECT
public:
    VersionComponentsDialog(const Instance &instance,const QString &name,const QString &source,
                            const QString &customCover={},QWidget *parent=nullptr);
    ~VersionComponentsDialog() override;
    void queryBuilds();
    void setBuilds(const VersionList &result);
    QString action() const { return m_action; }
    QJsonObject selectedBuild() const;
    QString localFile() const { return m_localFile; }
signals:
    void downloadSourceChanged(const QString &source);
protected:
    void changeEvent(QEvent *event) override;
private:
    Instance m_instance;
    QString m_source,m_action="install",m_localFile;
    bool m_reinstall=false,m_loading=false,m_requery=false,m_closing=false,m_everQueried=false;
    QJsonArray m_builds;
    QFutureWatcher<VersionList> m_watcher;
    CancelFlag m_cancel;
    SelectBox *m_sourceChoice;
    QTabBar *m_channels;
    QListWidget *m_list;
    QLabel *m_empty,*m_message;
    BusySpinner *m_spinner;
    NetworkNotice *m_networkNotice;
    QPushButton *m_refresh,*m_import,*m_primary;
    QAction *m_reinstallAction;
    QList<RadioButton *> m_radios;
    void populate();
    void updateSelection();
    void importLocal();
    void chooseBuild(const QJsonObject &build);
};
}
