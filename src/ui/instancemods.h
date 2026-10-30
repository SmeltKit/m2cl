// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/mods.h"
#include <QFutureWatcher>
#include <QWidget>
#include <QImage>
#include <QUrl>
#include <functional>
#include <QSet>
class QLabel;
class QTableWidget;
class QLineEdit;
class QComboBox;
class QPushButton;
class QFrame;
namespace mc {
class BusySpinner;
class ResourceClient;
class CheckBox;
class InstanceModsView final:public QWidget {
    Q_OBJECT
public:
    InstanceModsView(const Instance &instance,const QString &name,QWidget *parent=nullptr);
    ~InstanceModsView() override;
    void refresh();
    void updateInstance(const Instance &instance);
    void setMutationGuard(std::function<bool()> guard){m_mutationGuard=std::move(guard);}
    QString instancePath() const{return m_instance.path;}
signals:
    void importRequested(const QString &path);
    void marketRequested(const QString &path);
    void findCompatibleRequested(const QString &path,const QString &name);
    void backRequested();
    void resourceMarketRequested(const QString &path,const QString &type);
    void settingsRequested(const QString &path);
    void versionComponentsRequested(const QString &path);
private:
    Instance m_instance;
    QLabel *m_status;
    BusySpinner *m_spinner;
    QTableWidget *m_table;
    QLineEdit *m_search;
    QComboBox *m_filter;
    QLabel *m_count,*m_empty,*m_selectedText,*m_selectedTitle,*m_selectedIcon;
    QFrame *m_selection;
    QPushButton *m_disable,*m_find,*m_remove,*m_components;
    CheckBox *m_selectAll=nullptr;
    ResourceClient *m_icons;
    QHash<QString,QImage> m_localIcons;
    QHash<QUrl,QStringList> m_remoteIcons;
    QStringList m_problems;
    QString m_restoreSelection;
    QSet<QString> m_restoreChecked;
    bool m_rendering=false;
    QList<LocalMod> m_mods;
    struct ScanResult {QList<LocalMod> mods;QHash<QString,QImage> icons;QString error;};
    QFutureWatcher<ScanResult> m_scan;
    std::function<bool()> m_mutationGuard;
    void renderRows();
    void filterRows();
    void selectRow();
    void changeEnabled(int row,bool enabled);
    void changeSelectedEnabled();
    QList<int> selectedRows() const;
    void reload(const QHash<QString,QString> &renamed={});
    void resizeColumns();
    bool eventFilter(QObject *object,QEvent *event) override;
    void removeSelected();
    void showRowMenu(int row,const QPoint &position);
};
}
