// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "theme.h"
#include "core/i18n.h"
#include <QApplication>
#include <QPalette>
#include <QFontDatabase>

namespace mc {
QFont applicationFont() {
    auto font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    const bool traditional=currentLanguage()=="zh_TW";
#ifdef Q_OS_MACOS
    const auto family = font.family();
    const auto latin = family == "Sans Serif" || family == "Sans" ? QStringLiteral(".AppleSystemUIFont") : family;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    font.setFamilies({latin, traditional?QStringLiteral("PingFang TC"):QStringLiteral("PingFang SC")});
#else
    font.setFamily(latin);
#endif
#else
    // Resolve installed families explicitly so missing fonts do not select an
    // arbitrary replacement before the intended simplified-Chinese fallback.
#ifdef Q_OS_WIN
    const QStringList preferred=traditional?QStringList{QStringLiteral("Segoe UI"),QStringLiteral("Microsoft JhengHei UI"),QStringLiteral("Microsoft JhengHei")}:QStringList{QStringLiteral("Segoe UI"),
        QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei")};
#else
    const QStringList preferred=traditional?QStringList{QStringLiteral("Noto Sans"),QStringLiteral("Noto Sans CJK TC"),QStringLiteral("Noto Sans TC"),QStringLiteral("Source Han Sans TC"),QStringLiteral("Source Han Sans TW")}:QStringList{QStringLiteral("Noto Sans"),
        QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Noto Sans SC"),
        QStringLiteral("Source Han Sans SC"), QStringLiteral("Source Han Sans CN"),
        QStringLiteral("WenQuanYi Micro Hei")};
#endif
    const auto installed = QFontDatabase().families();
    QStringList families;
    for (const auto &candidate : preferred) {
        for (const auto &available : installed) {
            if (candidate.compare(available, Qt::CaseInsensitive) == 0) {
                families.append(available);
                break;
            }
        }
    }
    if (!families.contains(font.family())) families.append(font.family());
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    font.setFamilies(families);
#else
    font.setFamily(families.first());
#endif
#endif
    font.setPixelSize(ui::Body); font.setWeight(QFont::Normal); font.setKerning(true);
    return font;
}
Theme makeTheme(const QString &name, bool dark) {
    Theme t;
    t.dark = dark;
    if (name == "blue") {
        t.background = dark ? "#191d22" : "#f4f7fa"; t.surface = dark ? "#242b32" : "#ffffff";
        t.sidebar = dark ? "#1e262e" : "#eaf0f5"; t.text = dark ? "#e6edf4" : "#273847";
        t.muted = dark ? "#a4b5c4" : "#5f7180"; t.border = dark ? "#374653" : "#d9e3ed";
        t.accent = dark ? "#93bfdc" : "#356488";
    } else if (name == "sand") {
        t.background = dark ? "#211e19" : "#faf7f0"; t.surface = dark ? "#2e2921" : "#fffdf8";
        t.sidebar = dark ? "#29231b" : "#f1eadc"; t.text = dark ? "#f0e7d8" : "#463a29";
        t.muted = dark ? "#beb09b" : "#77664f"; t.border = dark ? "#4d4232" : "#e6dccb";
        t.accent = dark ? "#ddbd80" : "#805e31";
    } else if (name == "violet") {
        t.background = dark ? "#201c25" : "#f8f5fa"; t.surface = dark ? "#2d2635" : "#fffdff";
        t.sidebar = dark ? "#271f2e" : "#eee8f3"; t.text = dark ? "#eee5f5" : "#3d304a";
        t.muted = dark ? "#b5a5c0" : "#756580"; t.border = dark ? "#46394f" : "#e2d9ea";
        t.accent = dark ? "#c6a8e8" : "#72549a";
    } else {
        t.background = dark ? "#101a16" : "#f7f8f5"; t.surface = dark ? "#1b2a22" : "#ffffff";
        t.sidebar = dark ? "#14241c" : "#eef1eb"; t.text = dark ? "#edf4ef" : "#26332b";
        t.muted = dark ? "#abbdb1" : "#657268"; t.border = dark ? "#354b3e" : "#dfe5dc";
        t.accent = dark ? "#96dfb6" : "#357654";
    }
    t.onAccent = dark ? "#102a1d" : "#ffffff";
    t.soft = QColor::fromRgbF(t.surface.redF() * .89 + t.accent.redF() * .11,
        t.surface.greenF() * .89 + t.accent.greenF() * .11, t.surface.blueF() * .89 + t.accent.blueF() * .11);
    return t;
}
void applyTheme(const Theme &t) {
    QPalette palette;
    palette.setColor(QPalette::Window, t.background); palette.setColor(QPalette::WindowText, t.text);
    palette.setColor(QPalette::Base, t.surface); palette.setColor(QPalette::AlternateBase, t.sidebar);
    palette.setColor(QPalette::Text, t.text); palette.setColor(QPalette::Button, t.surface);
    palette.setColor(QPalette::ButtonText, t.text); palette.setColor(QPalette::Highlight, t.accent);
    palette.setColor(QPalette::HighlightedText, t.onAccent); palette.setColor(QPalette::ToolTipBase, t.surface);
    palette.setColor(QPalette::ToolTipText, t.text); palette.setColor(QPalette::Link, t.accent);
    palette.setColor(QPalette::Mid, t.muted);
    palette.setColor(QPalette::Disabled, QPalette::Base, t.sidebar);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, t.muted);
    palette.setColor(QPalette::Disabled, QPalette::Mid, t.border);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, t.soft);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, t.muted);
    palette.setColor(QPalette::Disabled, QPalette::Text, t.muted);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, t.muted);
    qApp->setPalette(palette);
    QString css = QString::fromUtf8(R"(
        QMainWindow, QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; color: @text@; }
        QWidget#appRoot { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 @depth@,stop:1 @bg@); color: @text@; }
        QFrame#sidebar { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 @side@,stop:1 @bg@); border-right: 1px solid @line@; }
        QFrame[card="true"] { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 @surface@,stop:1 @surfaceEnd@); border: 1px solid @line@; border-radius: 9px; }
        QTabWidget::pane { border: none; background: transparent; }
        QTextBrowser#modProjectReadme { background: @surface@; color: @text@; border: 1px solid @line@; border-radius: 8px; }
        QTabBar::tab { color: @muted@; background: transparent; padding: 10px 18px; border-bottom: 2px solid transparent; }
        QTabBar::tab:selected { color: @accent@; border-bottom-color: @accent@; font-weight: 600; }
        QTabBar#supportedGameVersionChannels { background: @surface@; border: 1px solid @line@; border-radius: 7px; padding: 2px; }
        QTabBar#supportedGameVersionChannels::tab { padding: 7px 14px; border: none; border-radius: 5px; }
        QTabBar#supportedGameVersionChannels::tab:selected { background: @accent@; color: @onAccent@; }
        QTabBar#modFileLoaders { background: transparent; border: none; }
        QTabBar#modFileLoaders::tab { background: @surface@; border: 1px solid @line@; border-radius: 6px; padding: 7px 14px; margin-right: 6px; color: @text@; }
        QTabBar#modFileLoaders::tab:hover { background: @soft@; }
        QTabBar#modFileLoaders::tab:selected { background: @accent@; color: @onAccent@; border-color: @accent@; }
        QTabBar#modReleaseChannel::tab { padding: 8px 10px; font-size: 13px; }
        QFrame#modPlanTarget, QFrame[dependencyCard="true"] { background: @surface@; border: 1px solid @line@; border-radius: 8px; }
        QFrame#installTarget { background: @bg@; border: none; border-radius: 8px; }
        QFrame#installTargetAccent { background: @accent@; border: none; border-radius: 1px; }
        QLabel#installTargetName { font-size: 17px; font-weight: 600; }
        QDialog#versionComponents QLabel[componentPageTitle="true"] { font-size: 24px; font-weight: 600; }
        QDialog#versionComponents QLabel#installTargetName { font-size: 20px; font-weight: 600; }
        QDialog#versionComponents QLabel[componentSectionTitle="true"] { font-size: 17px; font-weight: 600; }
        QDialog#versionComponents QLabel[componentName="true"] { font-size: 15px; font-weight: 600; }
        QDialog#versionComponents QLabel[componentCaption="true"] { font-size: 12px; color: @muted@; }
        QDialog#versionComponents QLabel[componentBadge="ok"] { background: @soft@; color: @accent@; border-radius: 5px; padding: 4px 8px; font-size: 12px; }
        QDialog#versionComponents QLabel[componentBadge="warning"] { background: @warningBg@; color: @warningText@; border-radius: 5px; padding: 4px 8px; font-size: 12px; }
        QFrame[networkNotice="true"] { background: @surface@; border: 1px solid @line@; border-radius: 8px; }
        QFrame[networkNotice="true"][networkLoading="true"] { background: transparent; border: none; }
        QFrame#installedComponentList { background: @surface@; border: 1px solid @line@; border-radius: 8px; }
        QFrame[componentRow="true"] { background: transparent; border: none; border-bottom: 1px solid @line@; }
        QListWidget#optifineBuildList { background: @surface@; border: 1px solid @line@; border-radius: 8px; padding: 4px; outline: none; }
        QListWidget#optifineBuildList::item { background: transparent; border: none; padding: 0; }
        QFrame#optifineBuildRow { background: transparent; border: none; border-radius: 5px; }
        QFrame#optifineBuildRow[selected="true"] { background: @soft@; }
        QTabBar#optifineChannels::tab { padding: 9px 15px; }
        QFrame[installSection="add"] { background: @installAdd@; border: 1px solid @line@; border-radius: 8px; }
        QFrame[installSection="reuse"] { background: @reuseBg@; border: 1px solid @reuseLine@; border-radius: 8px; }
        QFrame[installSection="warning"] { background: @warningBg@; border: 1px solid @warningBorder@; border-radius: 8px; }
        QFrame[installRow="true"] { background: transparent; border: none; border-bottom: 1px solid @installRule@; }
        QDialog[installationDialog="true"] { background: @surface@; }
        QDialog[installationDialog="true"] QLabel[muted="true"] { font-size: 14px; }
        QLabel[installSectionTitle="true"] { font-weight: 600; padding-bottom: 5px; }
        QFrame[installSection="add"] QLabel[installSectionTitle="true"] { color: @accent@; }
        QFrame[installSection="reuse"] QLabel[installSectionTitle="true"] { color: @reuseText@; }
        QFrame[installSection="warning"] QLabel[installSectionTitle="true"] { color: @warningText@; }
        QLabel#dependencyTitle { font-weight: 600; }
        QLabel[dependencyBadge="true"] { background: @soft@; color: @accent@; border-radius: 4px; padding: 2px 6px; }
        QScrollArea#modInstalledChoices { border: none; background: transparent; }
        QScrollArea#modDependencyChoices, QScrollArea#modDependencyChoices > QWidget > QWidget { background: transparent; border: none; }
        QTreeWidget#modDependencyTree { background: @surface@; color: @text@; border: 1px solid @line@; border-radius: 8px; padding: 6px; }
        QTreeWidget#modDependencyTree::item { padding: 6px 3px; }
        QLabel[planError="true"] { color: @warningText@; background: @warningBg@; padding: 8px; border-radius: 6px; }
        QFrame#supportedVersionRow { background: transparent; border: none; border-bottom: 1px solid @line@; border-radius: 0; }
        QLabel#supportedVersionSeries { font-size: 15px; font-weight: 600; }
        QLabel#supportedVersionValues { background: @surface@; border: 1px solid @line@; border-radius: 5px; padding: 3px 8px; font-size: 13px; }
        QLabel { color: @text@; background: transparent; }
        QLabel[muted="true"] { color: @muted@; font-size: @secondary@px; }
        QLabel[accent="true"] { color: @accent@; }
        QLabel#brandMark { background: transparent; }
        QLabel#eyebrow { color: @accent@; font-size: 12px; }
        QLabel#pageTitle { font-size: 22px; font-weight: 600; }
        QLabel#heroTitle { font-size: 23px; font-weight: 600; }
        QLabel#sectionTitle { font-size: 18px; font-weight: 600; }
        QLabel#instanceSettingsTitle { font-size: 20px; font-weight: 600; }
        QSlider#instanceMemorySlider { min-height: 32px; }
        QSlider#instanceMemorySlider::groove:horizontal { height: 4px; background: @line@; border-radius: 2px; }
        QSlider#instanceMemorySlider::sub-page:horizontal { background: @accent@; border-radius: 2px; }
        QSlider#instanceMemorySlider::handle:horizontal { width: 18px; margin: -9px 0; background: @surface@; border: 2px solid @accent@; border-radius: 11px; }
        QLabel#instanceMemoryModeBadge { background: @soft@; color: @accent@; border-radius: 12px; padding: 3px 10px; font-size: 12px; }
        QFrame[settingsDivider="true"] { background: @line@; border: none; min-height: 1px; max-height: 1px; }
        QPushButton[memoryDisclosure="true"] { background: transparent; border: none; padding: 0; text-align: left; color: @text@; font-size: 14px; }
        QPushButton[memoryDisclosure="true"]:hover { color: @accent@; }
        QPushButton { color: @text@; background: @surface@; border: 1px solid @line@; border-radius: 6px; padding: 8px 14px; min-height: 18px; }
        QPushButton:hover { background: @soft@; border-color: @accent@; }
        QPushButton:pressed { background: @side@; }
        QPushButton[primary="true"] { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 @accentTop@,stop:1 @accent@); color: @onAccent@; border-color: @accent@; font-weight: 600; padding: 8px 13px; }
        QPushButton:disabled { color: @muted@; background: @side@; border-color: @line@; }
        QPushButton[nav="true"] { text-align: left; border: none; background: transparent; padding: 12px 14px; color: @muted@; }
        QPushButton[nav="true"]:checked { background: @soft@; color: @accent@; font-weight: 600; }
        QPushButton[nav="true"]:hover { background: @soft@; }
        QPushButton[instanceSettingsNav="true"] { font-size: 16px; padding: 10px 12px; }
        QPushButton[mainNavigation="true"], QPushButton[instanceSettingsNav="true"] { color: @text@; font-size: 16px; }
        QPushButton[mainNavigation="true"] { font-size: 15px; }
        QPushButton[mainNavigation="true"]:checked, QPushButton[instanceSettingsNav="true"]:checked { color: @accent@; }
        QPushButton[flatAction="true"] { border: none; background: transparent; padding: 4px; }
        QPushButton[flatAction="true"]:hover { background: @soft@; }
        QPushButton[dangerAction="true"] { color: @dangerText@; }
        QPushButton[settingsTab="true"] { border: none; background: transparent; padding: 8px 16px; color: @muted@; }
        QPushButton[settingsTab="true"]:checked { background: @soft@; color: @accent@; font-weight: 600; }
        QPushButton[settingsTab="true"]:hover { background: @soft@; }
        QFrame[javaRuntimeCard="true"] { background: @surface@; border: 1px solid @line@; border-radius: 10px; }
        QFrame[javaRuntimeCard="true"][global="true"] { border-color: @accent@; }
        QFrame[javaSelectionPanel="true"] { background: @surface@; border: 1px solid @line@; border-radius: 8px; }
        QFrame[javaPanelHeader="true"] { background: transparent; border: none; border-bottom: 1px solid @line@; border-radius: 0; }
        QLabel[javaPanelTitle="true"] { font-size: 14px; font-weight: 600; }
        QListWidget[stepList="true"] { border: none; background: transparent; padding: 0; }
        QListWidget[stepList="true"]::item { padding: 10px 9px; margin: 0 0 5px 0; }
        QListWidget[modCategoryList="true"]::item { padding: 7px 5px; margin: 1px; }
        QListWidget[versionCards="true"] { background: transparent; border: none; padding: 0; }
        QListWidget[versionCards="true"]::item { border: none; margin: 0 0 8px 0; padding: 0; }
        QListWidget[versionCards="true"]::item:selected { background: transparent; }
        QFrame[modVersionCard="true"] { background: @surface@; border: 1px solid @line@; border-radius: 10px; }
        QFrame[modVersionCard="true"][selected="true"] { border: 1px solid @accent@; background: @soft@; }
        QLabel[releaseBadge="true"] { background: @warningBg@; color: @warningText@; border-radius: 4px; padding: 2px 6px; font-size: 12px; }
        QLabel[releaseBadge="true"][releaseType="release"] { background: @soft@; color: @accent@; }
        QListWidget[resourceCards="true"] { border: none; background: transparent; padding: 0; }
        QListWidget[resourceCards="true"]::item { padding: 0; margin: 0 0 12px 0; border: none; }
        QListWidget[javaCards="true"] { background: transparent; border: none; padding: 0; }
        QListWidget[javaCards="true"]::item { padding: 0; margin: 0 0 10px 0; border: none; }
        QLabel[resourceIcon="true"] { background: @side@; border-radius: 8px; }
        QLabel[resourceTag="true"] { background: @soft@; color: @accent@; border-radius: 4px; padding: 2px 6px; font-size: 12px; }
        QLabel[javaTitle="true"] { font-size: 14px; font-weight: 600; }
        QLabel[javaBadge="true"] { border-radius: 5px; padding: 2px 6px; background: @side@; color: @muted@; font-size: 12px; }
        QLabel[javaBadge="true"][kind="good"] { background: @soft@; color: @accent@; }
        QLabel[javaBadge="true"][kind="warning"] { background: @warningBg@; color: @warningText@; }
        QPushButton[textLink="true"] { background: transparent; color: @accent@; border: none; padding: 0 2px; min-height: 18px; font-size: 13px; }
        QPushButton[textLink="true"]:hover { text-decoration: underline; }
        QPushButton[iconOnly="true"] { padding: 4px; min-height: 18px; }
        QPushButton[compact="true"] { padding: 5px 9px; min-height: 18px; font-size: 13px; }
        QPushButton[choiceCard="true"] { min-height: 64px; text-align: left; padding: 0; border: 1px solid @line@; border-radius: 9px; }
        QPushButton[choiceCard="true"]:checked { background: @soft@; border-color: @accent@; color: @accent@; }
        QLabel[choiceHeading="true"] { font-size: 14px; font-weight: 600; }
        QLabel[choiceDetail="true"] { font-size: 13px; font-weight: 400; color: @muted@; }
        QLabel[choiceHeading="true"] { color: @text@; }
        QLabel[choiceHeading="true"][choiceSelected="true"] { color: @accent@; }
        QLabel[secondaryText="true"] { font-size: 13px; }
        QLabel[projectTitle="true"] { font-size: 16px; font-weight: 600; }
        QLabel[fieldLabel="true"] { font-size: 14px; font-weight: 600; }
        QLabel[numberLabel="true"] { font-size: 15px; color: @text@; }
        QLabel[javaEffective="true"] { color: @text@; background: @soft@; border: 1px solid @line@; border-radius: 8px; padding: 10px; }
        QFrame[javaPath="true"] { border: 1px solid @line@; border-radius: 8px; background: @side@; }
        QFrame[javaWarning="true"] { background: @warningBg@; border: 1px solid @warningBorder@; border-radius: 8px; }
        QFrame[javaWarning="true"] QLabel { color: @warningText@; }
        QTableWidget { background: @surface@; alternate-background-color: @bg@; color: @text@; border: 1px solid @line@; border-radius: 8px; selection-background-color: @soft@; selection-color: @text@; outline: none; }
        QTableWidget::item { border: none; border-bottom: 1px solid @line@; padding: 0 8px; }
        QHeaderView::section { background: @surface@; color: @muted@; border: none; border-bottom: 1px solid @line@; padding: 7px 6px; }
        QTableCornerButton::section { background: @surface@; border: none; }
        QTableWidget[dataTable="true"]::item { padding: 0; }
        QLineEdit, QComboBox, QListWidget, QPlainTextEdit, QSpinBox { color: @text@; background: @surface@; border: 1px solid @line@; border-radius: 8px; padding: 7px; selection-background-color: @accent@; selection-color: @onAccent@; }
        QLineEdit, QSpinBox { min-height: 20px; }
        QSpinBox[numberBox="true"] { background: transparent; border: none; padding: 0; min-height: 36px; }
        QFrame[numberField="true"] { background: @surface@; border: 1px solid @line@; border-radius: 5px; }
        QFrame[numberField="true"][editorFocused="true"] { border-color: @accent@; }
        QFrame[numberField="true"]:disabled { background: @side@; }
        QLineEdit[numberEditor="true"] { background: transparent; color: @text@; border: none; padding: 0 5px; min-height: 0; }
        QLineEdit[numberEditor="true"]:disabled { color: @muted@; }
        QLineEdit[numberEditor="true"][optionalNumber="true"] { font-size: 13px; placeholder-text-color: @placeholder@; }
        QToolButton[numberStep="true"] { background: @surface@; color: @text@; border: 1px solid @line@; border-radius: 5px; padding: 0; font-size: 19px; }
        QToolButton[numberStep="true"]:hover { background: @soft@; color: @accent@; }
        QToolButton[numberStep="true"]:pressed { background: @side@; color: @accent@; }
        QToolButton[numberStep="true"]:disabled { color: @muted@; background: @side@; }
        QComboBox { min-height: 20px; padding-right: 20px; }
        QComboBox QAbstractItemView { background: @surface@; color: @text@; selection-background-color: @accent@; selection-color: @onAccent@; }
        QListWidget { outline: none; }
        QListWidget::item { border: 1px solid transparent; border-radius: 8px; padding: 10px; margin: 2px; }
        QListWidget::item:selected { background: @soft@; color: @text@; border-color: @accent@; }
        QListWidget::item:hover { background: @soft@; }
        QListWidget[javaSelectorList="true"] { background: transparent; border: none; padding: 6px; }
        QListWidget[javaSelectorList="true"]::item { padding: 10px 8px; margin: 0 0 4px 0; border-radius: 6px; }
        QTableWidget[javaBuildGrid="true"] { background: transparent; border: none; border-radius: 0; }
        QCheckBox, QRadioButton { color: @text@; spacing: 8px; }
        QCheckBox:disabled, QRadioButton:disabled { color: @muted@; }
        QRadioButton::indicator { width: 16px; height: 16px; border: 1px solid @muted@; border-radius: 9px; background: @surface@; }
        QRadioButton::indicator:checked { width: 8px; height: 8px; background: @bg@; border: 5px solid @accent@; }
        QProgressBar { border: none; background: @side@; border-radius: 4px; min-height: 8px; max-height: 8px; }
        QProgressBar::chunk { background: @accent@; border-radius: 4px; }
        QToolTip { color: @text@; background: @surface@; border: 1px solid @line@; padding: 6px; }
        QMainWindow { background: transparent; }
        QWidget#appRoot { border: 1px solid @line@; border-radius: 12px; }
        QWidget#appRoot[nativeWindow="true"] { border: none; border-radius: 0; }
        QWidget#appRoot[nativeWindow="true"] QFrame#sidebar { border-top-left-radius: 0; border-bottom-left-radius: 0; }
        QFrame#sidebar { border-top-left-radius: 12px; border-bottom-left-radius: 12px; }
        QLabel#statusLabel { font-size: 12px; }
        QWidget#heroCover QLabel { color: white; }
        QWidget#heroCover QLabel#heroTitle { color: white; font-size: 27px; font-weight: 600; }
        QWidget#heroCover QLabel#heroPath { color: rgba(255,255,255,210); font-size: 13px; }
        QPushButton#heroAction { font-size: 14px; min-height: 24px; border-radius: 6px; }
        QDialog { background: @bg@; color: @text@; }
        QComboBox::down-arrow { image: none; width: 0; height: 0; }
        QComboBox::drop-down { border: none; subcontrol-origin: padding; subcontrol-position: top right; width: 25px; }
        QMenu { background: @surface@; color: @text@; border: 1px solid @line@; border-radius: 8px; padding: 6px; }
        QMenu::item { padding: 8px 26px 8px 12px; border-radius: 4px; }
        QMenu::item:selected { background: @soft@; color: @accent@; }
        QMenu::separator { height: 1px; background: @line@; margin: 5px 7px; }
        QFrame[choicePopup="true"] { background: @surface@; border: 1px solid @line@; border-radius: 8px; }
        QLabel[popupHeading="true"] { background: @side@; color: @text@; border-radius: 4px; padding: 6px 9px; font-weight: 600; }
        QListWidget[popupList="true"] { background: transparent; border: none; padding: 0; }
        QListWidget[popupList="true"]::item { padding: 5px 9px; margin: 1px 0; border-radius: 5px; border: none; }
        QListWidget[popupList="true"]::item:selected { background: @soft@; color: @accent@; }
        QPushButton[popupClear="true"] { background: transparent; border: none; text-align: left; }
        QPushButton[popupClear="true"]:checked { background: @soft@; color: @accent@; font-weight: 600; }
        QScrollBar:vertical { background: transparent; width: 7px; margin: 0; }
        QScrollBar::handle:vertical { background: @line@; min-height: 32px; border-radius: 3px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QScrollBar:horizontal { height: 7px; background: transparent; }
        QScrollBar::handle:horizontal { background: @line@; min-width: 32px; border-radius: 3px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
    )");
    const QList<QPair<QString, QColor>> colors{{"bg", t.background}, {"depth",t.dark?t.background.lighter(119):t.background}, {"surfaceEnd",t.dark?t.surface.darker(110):t.surface}, {"accentTop",t.accent.lighter(t.dark?105:100)}, {"surface", t.surface}, {"side", t.sidebar},
        {"text", t.text}, {"muted", t.muted}, {"line", t.border}, {"accent", t.accent}, {"onAccent", t.onAccent}, {"soft", t.soft}, {"warningBg", QColor(t.dark ? "#352b1b" : "#fff7e9")}, {"warningText", QColor(t.dark ? "#e6bb75" : "#956024")}, {"warningBorder", QColor(t.dark ? "#6a5030" : "#f0d4aa")}};
    for (const auto &color : colors) css.replace("@" + color.first + "@", color.second.name());
    css.replace("@installAdd@",t.dark?"#1b3025":"#f1faf5");
    css.replace("@installRule@",t.dark?"#2b4034":"#e7f0eb");
    css.replace("@reuseText@",t.dark?"#9bc5e3":"#356a87");
    css.replace("@reuseBg@",t.dark?"#1c2c38":"#edf5fa");
    css.replace("@reuseLine@",t.dark?"#354c60":"#d8e8f2");
    css.replace("@placeholder@",QColor::fromRgbF(t.muted.redF()*.68+t.surface.redF()*.32,t.muted.greenF()*.68+t.surface.greenF()*.32,t.muted.blueF()*.68+t.surface.blueF()*.32).name());
    css.replace("@dangerText@",t.dark?"#ef9da4":"#b33d4b");
    css.replace("@secondary@", QString::number(ui::Secondary));
    qApp->setStyleSheet(css);
}
}
