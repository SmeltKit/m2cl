// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/catalog.h"
#include "core/resources.h"
#include <QWidget>
#include <QSet>
#include <functional>
class QTableWidget;
class QLabel;
class QTextBrowser;
class QTabBar;
class QStackedWidget;
class QLineEdit;
class QComboBox;
namespace mc {
class ProjectReadme;
class NetworkNotice;
class ModVersionView final : public QWidget {
    Q_OBJECT
public:
    ModVersionView(const QString &projectName,const QString &targetName,const Instance *target,const QList<ResourceFile> &files,QWidget *parent = nullptr);
    int selectedIndex() const;
    void setProjectInfo(const ResourceProject &project);
    void setProjectError(const QString &message);
    void setProjectLoading();
    void setSupportedVersionMetadata(const ResourceTags &tags);
    void setDownloadFilter(const QString &gameVersion,const QString &loader);
    void setInstalledVersions(const QSet<QString> &versions){m_installedVersions=versions;if(m_update)m_update();}
signals:
    void backRequested();
    void retryProjectRequested();
    void downloadRequested(int fileIndex);
    void manageRequested();
private:
    void rebuildSupportedVersions();
    QTableWidget *m_list = nullptr;
    QLabel *m_description;
    QLabel *m_projectIcon,*m_author;
    ProjectReadme *m_readme;
    NetworkNotice *m_projectNotice;
    QTabBar *m_supportedChannels;
    QStackedWidget *m_supportedPages;
    QLineEdit *m_supportedSearch;
    QTabBar *m_fileLoaders;
    QComboBox *m_fileGameVersions;
    std::function<void()> m_fileFilter;
    ResourceTags m_versionTags;
    ResourceProject m_project;
    QSet<QString> m_installedVersions;
    std::function<void()> m_update;
};
}
