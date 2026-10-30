// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/catalog.h"
#include "core/java.h"
#include "core/settings.h"
#include "network/resourceclient.h"
#include "network/gameinstaller.h"
#include "network/modinstaller.h"
#include "theme.h"
#include "launcherwidgets.h"
#include "javamanager.h"
#include <QFutureWatcher>
#include <QMainWindow>
#include <QPointer>

class QProcess;

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QSplitter;
class QBoxLayout;

namespace mc {
class NetworkNotice;
class AccountManager;
class AccountsPanel;
class HomeAccountCard;
class AvatarCache;
class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(SettingsStore store, QWidget *parent = nullptr);
    ~MainWindow() override;
    void setAppearance(const QString &appearance, const QString &theme);
    void changePage(int index);
    void editInstance(const QString &path = {}, int section = 0);
    void showDownloads();
    void showJavaSettings(bool downloads = false);
    void showAccounts();
    void browseModsForInstance(const QString &path);
    void showInstanceMods(const QString &path);
    void showVersionComponents(const QString &path);
    bool ready() const { return m_catalogReady && !m_catalogPending; }
    void writeDiagnostics(const QString &path) const;
private:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    QWidget *homePage();
    QWidget *libraryPage();
    QWidget *modsPage();
    QWidget *settingsPage();
    void save();
    void chooseRoot();
    void refreshCatalog();
    void selectInstance(const QString &path);
    void refreshInstanceViews();
    bool configureJava(InstallRequest &request, const QString &instancePath, QString *error) const;
    void createInstance();
    void manageVersionComponents(const Instance &instance);
    void launchInstance();
    void startInstall(const InstallRequest &request);
    void runGame(const LaunchCommand &command);
    void setMarketType(const QString &type);
    void browseResourcesForInstance(const QString &path,const QString &type);
    void searchMods();
    void searchModPage(int offset);
    void syncModFilters();
    void invalidateModResults();
    void chooseModVersion();
    void addModsMenu(const QString &path,const QPoint &position);
    void importMods(const QString &path);
    void commitModPlan();
    const Instance *modTarget() const;
    void showModVersions(const QList<ResourceFile> &files);
    void downloadModFile(const ResourceFile &file,const QString &targetPath);
    void chooseModInstallTarget(const ResourceFile &file);
    void closeModDetails();
    void animateCloseModDetails();
    void setModDetails(QWidget *view);
    void updateModBrowseLayout(bool details);
    void updateAppearance();
    QString instanceName(const Instance &instance) const;
    void toggleFavorite(const QString &path);
    void instanceMenu(const QString &path, const QPoint &position);
    void openDirectory(const QString &path);
    void updateDownloadPanel();
    const Instance *selectedInstance() const;
    void status(const QString &message);
    SettingsStore m_store;
    Settings m_settings;
    AccountManager *m_accounts=nullptr;
    AccountsPanel *m_accountsPanel=nullptr;
    HomeAccountCard *m_accountCard=nullptr;
    AvatarCache *m_avatars=nullptr;
    QPalette m_systemPalette;
    QList<Instance> m_instances;
    QList<JavaRuntime> m_javas;
    QList<ResourceProject> m_projects;
    QString m_marketType="mod";
    QComboBox *m_marketTypeChoice=nullptr;
    QLabel *m_marketTitle=nullptr;
    QPushButton *m_marketReturn=nullptr;
    QString m_resourceReturnPath;
    QString m_versionTarget, m_versionProjectName;
    ResourceProject m_versionProject;
    QString m_versionProjectError;
    bool m_modProjectPending=false;
    QStackedWidget *m_modViews = nullptr;
    QWidget *m_modDetails = nullptr, *m_modFilters = nullptr;
    QList<QWidget *> m_modFilterFields;
    QWidget *m_modSearchState = nullptr;
    QLabel *m_modSearchMessage = nullptr;
    QPushButton *m_modSearchRetry = nullptr;
    QPushButton *m_modRefresh = nullptr;
    BusySpinner *m_modSearchSpinner = nullptr;
    QWidget *m_modCategoryState=nullptr;
    BusySpinner *m_modCategorySpinner=nullptr;
    bool m_modVersionPending = false;
    bool m_modSearchPending = false;
    bool m_modInstallActive = false;
    bool m_catalogPending = false;
    bool m_catalogReady = false;
    QList<QPushButton *> m_nav;
    QPushButton *m_shaderNav=nullptr,*m_packNav=nullptr;
    QList<AppearanceChoice *> m_appearanceChoices;
    CoverView *m_hero = nullptr;
    InstanceGrid *m_homeGrid = nullptr, *m_libraryGrid = nullptr;
    WindowBar *m_bar = nullptr;
    DownloadPanel *m_downloadPanel = nullptr;
    QString m_activeDownloadName, m_activeDownloadMessage;
    int m_activeDownloadProgress = -1;
    bool m_resizeCursor = false;
    bool m_downloadInProgress = false;
    Qt::Edges m_dragResizeEdges;
    QRect m_resizeStartGeometry;
    QPoint m_resizeStartMouse;
    QFutureWatcher<CatalogResult> m_catalogWatcher;
    JavaManager *m_javaManager = nullptr;
    QStackedWidget *m_settingsSections = nullptr;
    bool m_javaDownloadActive = false, m_lastDownloadJava = false;
    ResourceClient m_resources;
    QFutureWatcher<InstallResult> m_installWatcher;
    QFutureWatcher<ModPlan> m_modPlanWatcher;
    QFutureWatcher<ModInstallResult> m_modApplyWatcher;
    ModPlan m_pendingModPlan;
    CancelFlag m_modCancel;
    CancelFlag m_installCancelled;
    QPointer<QProcess> m_gameProcess;
    bool m_launchAfterInstall = false;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_status = nullptr, *m_heroTitle = nullptr, *m_instanceCount = nullptr;
    QLabel *m_libraryDetail = nullptr, *m_downloadStatus = nullptr;
    QLabel *m_heroPath = nullptr, *m_heroVersion = nullptr, *m_heroLoader = nullptr, *m_heroJava = nullptr;
    QLabel *m_heroPlayer=nullptr;
    QLabel *m_libraryCount = nullptr;
    QPushButton *m_heroLaunch = nullptr, *m_heroEdit = nullptr, *m_heroOpen = nullptr;
    QListWidget *m_roots = nullptr;
    QListWidget *m_modResults = nullptr, *m_modCategories = nullptr;
    QComboBox *m_modVersion = nullptr, *m_modLoader = nullptr, *m_modSort = nullptr;
    NetworkNotice *m_modTagStatus=nullptr;
    QLabel *m_modPageLabel=nullptr;
    QPushButton *m_modPrevious = nullptr, *m_modNext = nullptr;
    int m_modOffset = 0, m_modTotal = 0;
    ResourceTags m_modTags;
    QComboBox *m_theme = nullptr;
    QLineEdit *m_modQuery = nullptr;
    QPushButton *m_cancelDownload = nullptr;
    QProgressBar *m_downloadProgress = nullptr;
};
}
