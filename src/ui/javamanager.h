// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/settings.h"
#include "network/javaprovider.h"
#include <QFutureWatcher>
#include <QWidget>
class QComboBox;
class QCheckBox;
class QRadioButton;
class QLabel;
class QListWidget;
class QPushButton;
class QProgressBar;
class QTableWidget;
class QFrame;
class QVBoxLayout;
namespace mc {
class BusySpinner;
class NetworkNotice;
class JavaChoiceWidget : public QWidget {
    Q_OBJECT
public:
    JavaChoiceWidget(const QJsonObject &choice, const QList<JavaRuntime> &runtimes, int requiredMajor = 0, QWidget *parent = nullptr);
    ~JavaChoiceWidget() override;
    QJsonObject choice() const;
    QString selectionError() const;
    void setChoice(const QJsonObject &choice);
    void setRuntimes(const QList<JavaRuntime> &runtimes);
signals:
    void changed();
private:
    void updateState();
    QComboBox *m_mode, *m_runtime;
    QLabel *m_path, *m_hint, *m_selectionLabel;
    QFrame *m_pathBox, *m_warning;
    QWidget *m_modesHost,*m_policyRow;
    QCheckBox *m_showAll,*m_forceLower;
    QRadioButton *m_auto,*m_fixed;
    QLabel *m_effective;
    QTableWidget *m_runtimeTable;
    QPushButton *m_details;
    int m_requiredMajor=0;
    QPushButton *m_browse;
    bool m_updating = false;
    QFutureWatcher<JavaRuntime> m_localProbe;
    JavaCancel m_probeCancel;
};
class JavaManager : public QWidget {
    Q_OBJECT
public:
    JavaManager(Settings &settings, SettingsStore store, QWidget *parent = nullptr);
    ~JavaManager() override;
    QList<JavaRuntime> runtimes() const { return m_runtimes; }
    void scan(bool deep = false);
    void refreshCatalog();
    void cancelInstall() { if (m_installCancel) m_installCancel->store(true); }
    bool installing() const { return m_install.isRunning(); }
    void setGameTaskBusy(bool busy);
signals:
    void changed();
    void taskProgress(const QString &name, const QString &message, int progress, bool active);
private:
    void resizeEvent(QResizeEvent *event) override;
    void persist();
    void refreshLocal();
    void populateBuilds();
    void merge(const QList<JavaRuntime> &runtimes);
    void query();
    void install();
    void detectPlatform();
    void refreshArchitectureOptions();
    void updateButtons();
    void layoutPlatformSelector();
    void showBuildDetails();
    void showRuntimeLicenses(const JavaRuntime &runtime);
    Settings &m_settings;
    SettingsStore m_store;
    QList<JavaRuntime> m_runtimes;
    QList<JavaPackage> m_packages;
    QFutureWatcher<JavaScanResult> m_scan;
    QFutureWatcher<JavaPackageList> m_query;
    QFutureWatcher<JavaInstallResult> m_install;
    JavaCancel m_scanCancel, m_queryCancel, m_installCancel;
    QTableWidget *m_local;
    QTableWidget *m_builds;
    QComboBox *m_major, *m_provider, *m_arch, *m_os;
    QPushButton *m_buildDetails=nullptr;
    BusySpinner *m_querySpinner;
    NetworkNotice *m_queryNotice,*m_installNotice;
    QVBoxLayout *m_buildBody;
    QLabel *m_buildHeading, *m_archiveNote;
    QWidget *m_buildEmpty, *m_buildEmptyIcon;
    QLabel *m_buildEmptyText;
    QPushButton *m_quick, *m_deep, *m_add, *m_stopScan, *m_refresh, *m_installButton, *m_stopInstall, *m_prev, *m_next, *m_providerSite;
    QLabel *m_scanStatus, *m_installStatus, *m_pageLabel;
    QString m_packageMessage;
    QProgressBar *m_progress;
    int m_page = 0;
    bool m_hasMore = false, m_gameBusy = false, m_platformDetected = false;
    QStringList m_availableArchitectures;
    bool m_architectureListComplete = false;
    QWidget *m_majorColumn=nullptr,*m_providerColumn=nullptr,*m_versionFilters=nullptr;
};
}
