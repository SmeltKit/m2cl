// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "networknotice.h"
#include "javamanager.h"
#include "launcherwidgets.h"
#include "theme.h"
#include "core/i18n.h"
#include <QComboBox>
#include <QCheckBox>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QRadioButton>
#include <QResizeEvent>
#include <QTableWidget>
#include <QTimer>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QFile>
#include <algorithm>
#include <QtConcurrent/QtConcurrentRun>
namespace mc {
namespace {
QPushButton *action(const QString &text, const char *name, bool primary = false) {
    auto b = new QPushButton(text); b->setObjectName(name); b->setProperty("primary", primary); b->setCursor(Qt::PointingHandCursor); return b;
}
QLabel *note(const QString &text) { auto l = new QLabel(text); l->setProperty("muted", true); l->setProperty("secondaryText", true); l->setWordWrap(true); l->setTextFormat(Qt::PlainText); return l; }
class RuntimeStatusDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const auto foreground = index.data(Qt::ForegroundRole);
        if (!foreground.isValid()) { QStyledItemDelegate::paint(painter, option, index); return; }
        QStyleOptionViewItem cell(option);
        initStyleOption(&cell, index);
        const auto text = cell.text;
        cell.text.clear();
        auto style = cell.widget ? cell.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, cell.widget);
        painter->save();
        painter->setFont(cell.font);
        painter->setPen(QPen(qvariant_cast<QBrush>(foreground), 1));
        const auto area = style->subElementRect(QStyle::SE_ItemViewItemText, &cell, cell.widget);
        painter->drawText(area, cell.displayAlignment | Qt::TextSingleLine, text);
        painter->restore();
    }
};
QString distribution(const QString &vendor) {
    if (vendor.contains("Azul", Qt::CaseInsensitive)) return "Zulu";
    if (vendor.contains("Adoptium", Qt::CaseInsensitive) || vendor.contains("Temurin", Qt::CaseInsensitive)) return "Temurin";
    if (vendor.contains("Microsoft", Qt::CaseInsensitive)) return "Microsoft";
    if (vendor.contains("Oracle", Qt::CaseInsensitive)) return "Oracle";
    return vendor.isEmpty() ? QCoreApplication::translate("M2CL", "未知发行版") : vendor;
}
QString providerName(const QString &provider) {
    if (provider == "mojang") return QCoreApplication::translate("M2CL", "Minecraft 官方运行时");
    if (provider == "oracle-openjdk") return "Oracle OpenJDK";
    if (provider == "temurin") return "Temurin";
    if (provider == "zulu") return "Zulu";
    if (provider == "microsoft") return "Microsoft";
    return provider;
}
QString architectureName(const QString &architecture) {
    const QHash<QString, QString> names{
        {"x86_64", QCoreApplication::translate("M2CL", "x64（amd64，64 位）")}, {"aarch64", QCoreApplication::translate("M2CL", "ARM64（aarch64，64 位）")},
        {"x86", QCoreApplication::translate("M2CL", "x86（32 位）")}, {"arm", QCoreApplication::translate("M2CL", "ARM32（armv7，32 位）")},
        {"riscv64", QStringLiteral("RISC-V 64（riscv64）")}, {"ppc64le", QCoreApplication::translate("M2CL", "PPC64LE（64 位）")}, {"s390x", QCoreApplication::translate("M2CL", "S390x（64 位）")}
    };
    return names.value(architecture, architecture);
}
class PathLabel : public QLabel {
public:
    explicit PathLabel(QString path, QWidget *parent = nullptr) : QLabel(parent), m_path(std::move(path)) {
        setToolTip(m_path); setProperty("muted", true); setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); setMinimumWidth(0); setTextFormat(Qt::PlainText); setText(m_path);
    }
    void setPath(const QString &path) { m_path = path; setToolTip(path); setText(fontMetrics().elidedText(m_path, Qt::ElideMiddle, width())); }
    QSize minimumSizeHint() const override { return {0, QLabel::minimumSizeHint().height()}; }
protected:
    void resizeEvent(QResizeEvent *event) override { QLabel::resizeEvent(event); setText(fontMetrics().elidedText(m_path, Qt::ElideMiddle, width())); }
private:
    QString m_path;
};
QString pinLabel(const QJsonObject &pin) {
    auto version = pin.value("runtimeVersion").toString(); if (version.isEmpty()) return pin.value("path").toString();
    return QStringLiteral("Java %1 · %2 · %3").arg(version, pin.value("vendor").toString(), pin.value("arch").toString());
}
}
static int choiceMajor(const QJsonObject &pin){auto major=pin.value("major").toInt();if(!major)major=parseJavaOutput("java.version = "+pin.value("runtimeVersion").toString()).major;return major;}
JavaChoiceWidget::~JavaChoiceWidget(){if(m_probeCancel)m_probeCancel->store(true);}
JavaChoiceWidget::JavaChoiceWidget(const QJsonObject &initial, const QList<JavaRuntime> &runtimes, int requiredMajor, QWidget *parent) : QWidget(parent), m_requiredMajor(requiredMajor) {
    auto layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(12); layout->setSizeConstraint(QLayout::SetMinimumSize);
    m_mode = new SelectBox(this); m_mode->setObjectName("javaChoiceMode");
    m_mode->addItem(QCoreApplication::translate("M2CL", "自动匹配兼容版本"), "auto"); m_mode->addItem(QCoreApplication::translate("M2CL", "固定构建 / 本地 Java"), "fixed");
    m_mode->hide();m_modesHost=new QWidget;m_modesHost->setObjectName("javaModeCards");auto modes=new QVBoxLayout(m_modesHost);modes->setContentsMargins(0,0,0,0);modes->setSpacing(10);
    m_effective=note({});m_effective->setObjectName("effectiveJavaPolicy");m_effective->setProperty("javaEffective",true);modes->addWidget(m_effective);
    m_policyRow=new QWidget;auto policy=new QHBoxLayout(m_policyRow);policy->setContentsMargins(0,0,0,0);policy->setSpacing(20);m_auto=new RadioButton(QCoreApplication::translate("M2CL", "自动管理（推荐）"));m_auto->setObjectName("automaticJavaPolicy");m_fixed=new RadioButton(QCoreApplication::translate("M2CL", "固定使用某个 Java"));m_fixed->setObjectName("fixedJavaPolicy");policy->addWidget(m_auto);policy->addWidget(m_fixed);policy->addStretch();modes->addWidget(m_policyRow);layout->addWidget(m_modesHost);
    connect(m_auto,&QRadioButton::clicked,this,[this]{m_mode->setCurrentIndex(m_mode->findData("auto"));});connect(m_fixed,&QRadioButton::clicked,this,[this]{m_mode->setCurrentIndex(m_mode->findData("fixed"));});
    m_selectionLabel = new QLabel(QCoreApplication::translate("M2CL", "选择 Java")); m_selectionLabel->setObjectName("javaSelectionLabel"); m_selectionLabel->setProperty("fieldLabel", true); m_selectionLabel->setContentsMargins(0, 0, 0, 0);
    auto row = new QHBoxLayout; row->addWidget(m_selectionLabel);m_runtime = new SelectBox(this); m_runtime->setObjectName("javaRuntimeChoice"); m_runtime->setMinimumContentsLength(10); m_runtime->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); m_runtime->hide();row->addStretch();m_showAll=new CheckBox(QCoreApplication::translate("M2CL", "显示所有版本"));m_showAll->setObjectName("showAllJavaVersions");row->addWidget(m_showAll);connect(m_showAll,&QCheckBox::toggled,this,[this]{updateState();});
    m_browse = action(QCoreApplication::translate("M2CL", "浏览…"), "browseJava"); row->addWidget(m_browse); layout->addLayout(row);
    m_runtimeTable=new RoundedTable(0,4);m_runtimeTable->setObjectName("javaRuntimePicker");m_runtimeTable->setHorizontalHeaderLabels({QCoreApplication::translate("M2CL", "Java 版本"),QCoreApplication::translate("M2CL", "发行版"),QCoreApplication::translate("M2CL", "架构"),QCoreApplication::translate("M2CL", "状态")});m_runtimeTable->verticalHeader()->hide();m_runtimeTable->setShowGrid(false);m_runtimeTable->setSelectionBehavior(QAbstractItemView::SelectRows);m_runtimeTable->setSelectionMode(QAbstractItemView::SingleSelection);m_runtimeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);m_runtimeTable->setFixedHeight(124);static_cast<RoundedTable *>(m_runtimeTable)->setColumnLayout({{0,160,3},{1,100,1.5},{2,90,1},{3,110,1.5}});layout->addWidget(m_runtimeTable);m_forceLower=new CheckBox(QCoreApplication::translate("M2CL", "强制使用低于游戏默认版本的 Java（可能启动失败）"));m_forceLower->setObjectName("forceLowerJava");layout->addWidget(m_forceLower);connect(m_forceLower,&QCheckBox::toggled,this,[this]{updateState();if(!m_updating)emit changed();});connect(m_runtimeTable,&QTableWidget::currentCellChanged,this,[this](int r,int,int,int){if(!m_updating && r>=0)m_runtime->setCurrentIndex(r);});
    m_details=action(QCoreApplication::translate("M2CL", "运行时详情"),"javaRuntimeDetails");m_details->setProperty("textLink",true);m_details->setCheckable(true);layout->addWidget(m_details,0,Qt::AlignLeft);connect(m_details,&QPushButton::toggled,this,[this]{updateState();});
    m_pathBox = new QFrame; m_pathBox->setProperty("javaPath", true); auto pathLayout = new QHBoxLayout(m_pathBox); pathLayout->setContentsMargins(10, 8, 10, 8);
    m_path = new PathLabel({}); m_path->setTextInteractionFlags(Qt::TextSelectableByMouse); pathLayout->addWidget(m_path, 1);
    auto copy = action(QCoreApplication::translate("M2CL", "复制"), "copyJavaPath"); copy->setProperty("compact", true); copy->setToolTip(QCoreApplication::translate("M2CL", "复制完整 Java 路径")); pathLayout->addWidget(copy); layout->addWidget(m_pathBox);
    m_hint = note({}); layout->addWidget(m_hint);
    m_warning = new QFrame; m_warning->setProperty("javaWarning", true); auto warningLayout = new QHBoxLayout(m_warning); warningLayout->setContentsMargins(12, 10, 12, 10);
    warningLayout->addWidget(new QLabel(QStringLiteral("!"))); warningLayout->addWidget(note(QCoreApplication::translate("M2CL", "固定指定会保存完整版本与发行版；失效或不兼容时提示修改，不会自动替换。")), 1); layout->addWidget(m_warning);
    setRuntimes(runtimes); setChoice(initial);
    connect(m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { updateState(); if (!m_updating) emit changed(); });
    connect(m_runtime, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { updateState(); if (!m_updating) emit changed(); });
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(choice().value("path").toString()); });
    connect(m_browse, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, QCoreApplication::translate("M2CL", "选择 Java 可执行文件")); if (path.isEmpty()) return;
        if(m_probeCancel)m_probeCancel->store(true);m_probeCancel=std::make_shared<std::atomic_bool>(false);const auto flag=m_probeCancel;m_browse->setEnabled(false);m_hint->setText(QCoreApplication::translate("M2CL", "正在检测所选 Java…"));m_hint->show();m_localProbe.setFuture(QtConcurrent::run([path,flag]{return probeJava(path,flag);}));
    });
    connect(&m_localProbe,&QFutureWatcher<JavaRuntime>::finished,this,[this]{m_browse->setEnabled(true);auto runtime=m_localProbe.result();if(runtime.major<=0){m_hint->setText(QCoreApplication::translate("M2CL", "所选文件无法识别为可运行的 Java，请重新选择。"));m_hint->show();return;}setChoice(javaPin(runtime));emit changed();});
}
QJsonObject JavaChoiceWidget::choice() const {
    if (m_mode->currentData() == "fixed") { auto p = m_runtime->currentData().toJsonObject(); p.insert("mode", "fixed");p.remove("allowBelowRecommended");if(m_forceLower->isChecked())p.insert("allowBelowRecommended",true);return p; }
    return {{"mode", m_mode->currentData().toString()}};
}
QString JavaChoiceWidget::selectionError() const {const auto selected=choice();const auto major=choiceMajor(selected);if(selected.value("mode")=="fixed"&&m_requiredMajor>0&&major>0&&major<m_requiredMajor&&!selected.value("allowBelowRecommended").toBool())return QCoreApplication::translate("M2CL", "所选 Java %1 低于游戏默认 Java %2，请主动勾选强制使用或选择其他版本。").arg(major).arg(m_requiredMajor);return {};}
void JavaChoiceWidget::setChoice(const QJsonObject &pin) {
    m_updating = true;const QSignalBlocker forceBlock(m_forceLower),showBlock(m_showAll);m_forceLower->setChecked(pin.value("allowBelowRecommended").toBool());if(m_forceLower->isChecked())m_showAll->setChecked(true); const auto mode = pin.value("mode").toString("auto");
    m_mode->setCurrentIndex(qMax(0, m_mode->findData(mode)));
    if (mode == "fixed") {
        int index = -1;
        for (int i = 0; i < m_runtime->count(); ++i) if ([&]{const auto candidate=m_runtime->itemData(i).toJsonObject();for(const auto &key:{"path","runtimeVersion","vendor","vendorVersion","arch"})if(candidate.value(key)!=pin.value(key))return false;return true;}()) { index = i; break; }
        if (index < 0) { m_runtime->addItem(pinLabel(pin), pin); index = m_runtime->count() - 1; }
        m_runtime->setCurrentIndex(index);
    }
    m_updating = false; updateState();
}
void JavaChoiceWidget::setRuntimes(const QList<JavaRuntime> &runtimes) {
    const auto pin = choice(); m_updating = true; m_runtime->clear();
    for (const auto &j : runtimes) m_runtime->addItem(javaLabel(j), javaPin(j));
    setChoice(pin); m_updating = false;
}
void JavaChoiceWidget::updateState() {
    const auto mode=m_mode->currentData().toString();const bool fixed=mode=="fixed";const QSignalBlocker autoBlock(m_auto),fixedBlock(m_fixed),tableBlock(m_runtimeTable),forceBlock(m_forceLower),showAllBlock(m_showAll);
    m_auto->setChecked(mode=="auto");m_fixed->setChecked(fixed);
    auto selected=m_runtime->currentData().toJsonObject();auto policy=choice();const auto kind=policy.value("mode").toString("auto");
    QString description=kind=="fixed"?QCoreApplication::translate("M2CL", "固定 Java %1 · %2 · %3").arg(policy.value("runtimeVersion").toString(QCoreApplication::translate("M2CL", "待验证")),distribution(policy.value("vendor").toString()),policy.value("arch").toString()):QCoreApplication::translate("M2CL", "自动选择兼容运行时，缺少时安装");
    m_effective->setText(description+(m_requiredMajor?QCoreApplication::translate("M2CL", "\n游戏默认：Java %1；可手动选择更高版本").arg(m_requiredMajor):QString()));
    m_selectionLabel->setVisible(fixed);m_browse->setVisible(fixed);m_showAll->setVisible(fixed&&m_requiredMajor>0);m_runtimeTable->setVisible(fixed);m_details->setVisible(fixed);const auto major=choiceMajor(selected);const bool lower=m_requiredMajor>0&&major>0&&major<m_requiredMajor;m_forceLower->setVisible(fixed&&(lower||major<=0));if(lower&&m_forceLower->isChecked())m_showAll->setChecked(true);if(major>=m_requiredMajor)m_forceLower->setChecked(false);m_runtimeTable->setRowCount(m_runtime->count());
    for(int i=0;i<m_runtime->count();++i){auto pin=m_runtime->itemData(i).toJsonObject();auto version=pin.value("runtimeVersion").toString();if(version.endsWith("-LTS"))version.chop(4);if(version.isEmpty())version=QCoreApplication::translate("M2CL", "本地 Java（待验证）");QStringList values{version,distribution(pin.value("vendor").toString()),pin.value("arch").toString(),QFileInfo(pin.value("path").toString()).isExecutable()?QCoreApplication::translate("M2CL", "可用"):QCoreApplication::translate("M2CL", "路径失效")};for(int c=0;c<4;++c){auto item=new QTableWidgetItem(values.at(c));item->setToolTip(pin.value("path").toString());m_runtimeTable->setItem(i,c,item);}m_runtimeTable->setRowHeight(i,30);const auto rowMajor=choiceMajor(pin);const bool below=m_requiredMajor>0&&rowMajor>0&&rowMajor<m_requiredMajor;m_runtimeTable->setRowHidden(i,below&&!m_showAll->isChecked());if(m_requiredMajor>0&&rowMajor>0&&QFileInfo(pin.value("path").toString()).isExecutable())m_runtimeTable->item(i,3)->setText(rowMajor==m_requiredMajor?QCoreApplication::translate("M2CL", "默认版本"):below?QCoreApplication::translate("M2CL", "低于默认"):QCoreApplication::translate("M2CL", "高于默认"));}
    if(m_runtime->currentIndex()>=0)m_runtimeTable->setCurrentCell(m_runtime->currentIndex(),0);m_pathBox->setVisible(fixed&&m_details->isChecked());static_cast<PathLabel *>(m_path)->setPath(selected.value("path").toString());m_warning->hide();const bool invalid=fixed&&!QFileInfo(selected.value("path").toString()).isExecutable();m_hint->setVisible(invalid||lower||(fixed&&major>m_requiredMajor));m_hint->setText(invalid?QCoreApplication::translate("M2CL", "固定路径不可执行，请重新选择 Java；固定设置不会被自动替换。"):lower?m_forceLower->isChecked()?QCoreApplication::translate("M2CL", "已允许低版本强制启动；游戏或加载器仍可能拒绝此运行时。"):QCoreApplication::translate("M2CL", "此运行时低于游戏默认版本；需主动勾选强制使用，启动前仍会核对构建。"):QCoreApplication::translate("M2CL", "已选择高于默认的 Java；部分旧游戏或加载器可能不兼容。"));
    layout()->invalidate();layout()->activate();updateGeometry();
}
JavaManager::JavaManager(Settings &settings, SettingsStore store, QWidget *parent)
    : QWidget(parent), m_settings(settings), m_store(std::move(store)), m_runtimes(javaCatalogFromJson(settings.javaCatalog)) {
    m_query.setParent(this);m_query.setObjectName("javaPackageQuery");m_queryCancel=std::make_shared<std::atomic_bool>(false);
    setObjectName("javaManager");setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Ignored); auto layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(12);
    auto tabs = new QTabWidget; tabs->setObjectName("javaTabs"); layout->addWidget(tabs, 1);
    auto local = new QWidget; auto ll = new QVBoxLayout(local); ll->setContentsMargins(0, 14, 0, 0); ll->setSpacing(12);
    auto controls = new QHBoxLayout; m_quick = action(QCoreApplication::translate("M2CL", "检测本机 Java"), "quickScanJava"); m_deep = action(QCoreApplication::translate("M2CL", "全盘扫描"), "fullScanJava"); m_add = action(QCoreApplication::translate("M2CL", "添加本地 Java"), "addLocalJava"); m_stopScan = action(QCoreApplication::translate("M2CL", "停止扫描"), "stopJavaScan");
    m_deep->setParent(local);m_deep->hide();auto scanMenu=new RoundedMenu(m_quick);scanMenu->addAction(QCoreApplication::translate("M2CL", "快速检测常见目录"),this,[this]{scan(false);});scanMenu->addAction(QCoreApplication::translate("M2CL", "全盘扫描可访问的磁盘"),this,[this]{scan(true);});m_quick->setMenu(scanMenu);auto installEntry=action(QCoreApplication::translate("M2CL", "安装 Java"),"openJavaDownloads",true);connect(installEntry,&QPushButton::clicked,this,[tabs]{tabs->setCurrentIndex(1);});for(auto b:{m_quick,installEntry,m_add,m_stopScan})controls->addWidget(b);controls->addStretch();ll->addLayout(controls);
    m_scanStatus = note(QCoreApplication::translate("M2CL", "快速检测常见目录；全盘扫描可访问的本地磁盘，跳过虚拟目录和不可访问位置，可随时取消。")); m_scanStatus->setObjectName("javaScanStatus"); ll->addWidget(m_scanStatus);
    auto installedHeading=new QLabel(QCoreApplication::translate("M2CL", "已安装的 Java 运行环境"));installedHeading->setObjectName("sectionTitle");ll->insertWidget(0,installedHeading);
    ll->insertWidget(1,note(QCoreApplication::translate("M2CL", "在各游戏实例的「Java 设置」中选择要使用的运行环境。")));
    m_local=new RoundedTable(0,5);m_local->setObjectName("localJavaList");m_local->setHorizontalHeaderLabels({QCoreApplication::translate("M2CL", "版本 / 构建"),QCoreApplication::translate("M2CL", "发行版"),QCoreApplication::translate("M2CL", "架构"),QCoreApplication::translate("M2CL", "状态"),QCoreApplication::translate("M2CL", "操作")});m_local->verticalHeader()->hide();m_local->setShowGrid(false);m_local->setSelectionBehavior(QAbstractItemView::SelectRows);m_local->setSelectionMode(QAbstractItemView::SingleSelection);m_local->setEditTriggers(QAbstractItemView::NoEditTriggers);m_local->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);static_cast<RoundedTable *>(m_local)->setColumnLayout({{0,200,3},{1,130,1.4},{2,100,1},{3,112,1.1},{4,76,0,Qt::AlignCenter}});m_local->setMinimumHeight(150);m_local->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Ignored);ll->addWidget(m_local,1);auto localScroll=new QScrollArea;localScroll->setObjectName("localJavaScroll");localScroll->setWidgetResizable(true);localScroll->setFrameShape(QFrame::NoFrame);localScroll->setWidget(local);tabs->addTab(localScroll,QCoreApplication::translate("M2CL", "本地 Java"));ll->addStretch();
    m_local->setItemDelegateForColumn(3, new RuntimeStatusDelegate(m_local));
    auto remote = new QWidget; auto rl = new QVBoxLayout(remote); rl->setContentsMargins(0, 14, 0, 0); rl->setSpacing(12);auto steps=note(QCoreApplication::translate("M2CL", "① 选择主版本　→　② 选择发行版　→　③ 选择具体构建"));steps->setProperty("accent",true);rl->addWidget(steps);
    auto platformToolbar = new QWidget;
    platformToolbar->setObjectName("javaPlatformToolbar");
    auto filters = new QGridLayout(platformToolbar);
    filters->setObjectName("javaPlatformGrid");
    filters->setContentsMargins(0, 0, 0, 0);
    filters->setSpacing(8);
    m_major = new SelectBox; m_major->setObjectName("javaMajorVersion"); m_major->setEditable(true);
    for (int major : {8, 11, 16, 17, 21, 25}) m_major->addItem(QString::number(major)); m_major->setCurrentText("17"); m_major->lineEdit()->setValidator(new QIntValidator(6, 99, m_major));
    m_provider = new SelectBox; m_provider->setObjectName("javaDistribution"); m_provider->addItem(QCoreApplication::translate("M2CL", "全部可下载发行版"), "all"); m_provider->addItem("Eclipse Temurin", "temurin"); m_provider->addItem("Azul Zulu", "zulu"); m_provider->addItem("Microsoft OpenJDK", "microsoft"); m_provider->addItem("Oracle OpenJDK", "oracle-openjdk");
    m_os = new SelectBox;
    m_os->setObjectName("javaOperatingSystem");
    m_os->addItem("Windows", "windows"); m_os->addItem("macOS", "osx"); m_os->addItem("Linux", "linux");
    m_os->setCurrentIndex(m_os->findData(GamePlatform::current().os));
    m_arch = new SelectBox;
    m_arch->setObjectName("javaArchitecture");
    m_arch->addItem(QCoreApplication::translate("M2CL", "x64（amd64，64 位）"), "x86_64");
    m_arch->addItem(QCoreApplication::translate("M2CL", "ARM64（aarch64，64 位）"), "aarch64");
    m_arch->addItem(QCoreApplication::translate("M2CL", "x86（32 位）"), "x86");
    m_arch->setCurrentIndex(m_arch->findData(GamePlatform::current().arch));
    for(auto combo:{m_major,m_provider,m_arch,m_os}){combo->setMinimumContentsLength(combo==m_provider?8:4);combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);combo->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);combo->setMinimumWidth(0);}
    m_os->setFixedWidth(100);
    m_os->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto fieldFont = font(); fieldFont.setPixelSize(ui::Body);
    m_arch->setFixedWidth(qMax(216, QFontMetrics(fieldFont).horizontalAdvance(QCoreApplication::translate("M2CL", "ARM64（aarch64，64 位）")) + 68));
    m_arch->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_arch->setToolTip(QCoreApplication::translate("M2CL", "x64 / amd64 / x86_64 为 64 位 x86；ARM64 / aarch64 为 64 位 ARM；x86 为 32 位。"));
    m_provider->setCurrentIndex(m_provider->findData("temurin"));
    m_versionFilters = new QWidget;
    auto versionFilters = new QHBoxLayout(m_versionFilters);
    versionFilters->setContentsMargins(0, 0, 0, 0);
    versionFilters->setSpacing(12);
    for (const auto &pair : QList<QPair<QString, QComboBox *>>{{QCoreApplication::translate("M2CL", "主版本"), m_major}, {QCoreApplication::translate("M2CL", "发行版"), m_provider}}) {
        auto host = new QWidget;
        auto box = new QVBoxLayout(host);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(5);
        box->addWidget(note(pair.first));
        box->addWidget(pair.second);
        versionFilters->addWidget(host, pair.second == m_provider ? 2 : 1);
    }
    rl->addWidget(m_versionFilters);
    for (const auto &pair : QList<QPair<QString, QComboBox *>>{{QCoreApplication::translate("M2CL", "系统"), m_os}, {QCoreApplication::translate("M2CL", "架构"), m_arch}}) {
        auto caption = note(pair.first);
        caption->setWordWrap(false);
        caption->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        caption->setObjectName(pair.second == m_arch ? "javaArchitectureLabel" : "javaOperatingSystemLabel");
        filters->addWidget(caption);
        filters->addWidget(pair.second);
    }
    auto detect = action(QCoreApplication::translate("M2CL", "自动检测"), "detectJavaPlatform");
    detect->setToolTip(QCoreApplication::translate("M2CL", "识别本机系统与 CPU 架构，重新填入这两个选项。"));
    filters->addWidget(detect);
    m_refresh = action(QCoreApplication::translate("M2CL", "查询构建"), "queryJavaBuilds", true); m_refresh->setFixedWidth(100); filters->addWidget(m_refresh);filters->setColumnStretch(6,1); rl->addWidget(platformToolbar);
    QTimer::singleShot(0,this,[this]{layoutPlatformSelector();});
    m_refresh->setToolTip(QCoreApplication::translate("M2CL", "查询所选系统与架构的正式构建；列表大小为压缩包下载大小。"));
    connect(detect, &QPushButton::clicked, this, &JavaManager::detectPlatform);
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) { if (index == 1 && !m_platformDetected) detectPlatform(); });
    m_querySpinner = new BusySpinner(m_refresh);
    m_querySpinner->setObjectName("javaQuerySpinner");
    m_querySpinner->move(7, 7);
    m_querySpinner->hide();
    m_providerSite = action(QCoreApplication::translate("M2CL", "发行版官网 ↗"), "javaProviderSite");m_providerSite->setProperty("textLink",true);m_providerSite->hide();
    connect(m_providerSite, &QPushButton::clicked, this, [this] {
        const auto url = m_providerSite->property("targetUrl").toUrl();
        if (url.scheme() == "https" && !url.host().isEmpty()) QDesktopServices::openUrl(url);
    });
    m_builds = new RoundedTable(0, 7); m_builds->setObjectName("javaBuildList"); m_builds->setHorizontalHeaderLabels({QString(), QCoreApplication::translate("M2CL", "版本 / 构建"), QCoreApplication::translate("M2CL", "发行版"), QCoreApplication::translate("M2CL", "平台"), QCoreApplication::translate("M2CL", "架构"), QCoreApplication::translate("M2CL", "类型"), QCoreApplication::translate("M2CL", "下载大小")});
    m_builds->verticalHeader()->hide(); m_builds->setShowGrid(false); m_builds->setAlternatingRowColors(true); m_builds->setSelectionBehavior(QAbstractItemView::SelectRows); m_builds->setSelectionMode(QAbstractItemView::SingleSelection); m_builds->setEditTriggers(QAbstractItemView::NoEditTriggers); m_builds->setTextElideMode(Qt::ElideMiddle);
    m_builds->setMinimumHeight(80);m_builds->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Ignored);m_builds->setColumnHidden(3,true);
    static_cast<RoundedTable *>(m_builds)->setColumnLayout({{0,44,0,Qt::AlignCenter},{1,180,4},{2,140,1.5},{3,100,1},{4,100,1.4},{5,80,1.1},{6,120,1.7}});
    auto viewportLayout = new QVBoxLayout(m_builds->viewport());
    viewportLayout->setContentsMargins(16, 16, 16, 16);
    m_buildEmpty = new QWidget;
    m_buildEmpty->setObjectName("javaBuildEmptyState");
    auto emptyLayout = new QVBoxLayout(m_buildEmpty);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setSpacing(12);
    emptyLayout->addStretch();
    m_buildEmptyIcon = new GlyphView(Glyph::Java, 36);
    auto faint = new QGraphicsOpacityEffect(m_buildEmptyIcon); faint->setOpacity(.35); m_buildEmptyIcon->setGraphicsEffect(faint);
    emptyLayout->addWidget(m_buildEmptyIcon, 0, Qt::AlignHCenter);
    emptyLayout->addWidget(m_querySpinner, 0, Qt::AlignHCenter);
    m_buildEmptyText = note(QCoreApplication::translate("M2CL", "点击上方「查询构建」查看可用版本"));
    m_buildEmptyText->setObjectName("javaBuildEmptyText");
    m_buildEmptyText->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_buildEmptyText);
    emptyLayout->addStretch();
    viewportLayout->addWidget(m_buildEmpty);
    auto columns = new QHBoxLayout;
    columns->setSpacing(12);
    auto panel = [](const QString &title, const char *name, QVBoxLayout **contents, QLabel **heading = nullptr) {
        auto frame = new QFrame;
        frame->setObjectName(name);
        frame->setProperty("javaSelectionPanel", true);
        auto body = new QVBoxLayout(frame);
        body->setContentsMargins(1, 1, 1, 1);
        body->setSpacing(0);
        auto header = new QFrame;
        header->setProperty("javaPanelHeader", true);
        header->setFixedHeight(40);
        auto headerRow = new QHBoxLayout(header);
        headerRow->setContentsMargins(12, 0, 12, 0);
        headerRow->setSpacing(8);
        auto caption = new QLabel(title);
        caption->setProperty("javaPanelTitle", true);
        caption->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        headerRow->addWidget(caption, 1);
        body->addWidget(header);
        *contents = body;
        if (heading) *heading = caption;
        return frame;
    };
    auto majorList = new QListWidget;
    majorList->setObjectName("javaMajorSteps");
    majorList->setProperty("javaSelectorList", true);
    QVBoxLayout *majors;
    m_majorColumn = panel(QCoreApplication::translate("M2CL", "主版本"), "javaMajorPanel", &majors);
    majors->addWidget(majorList, 1);
    m_majorColumn->setFixedWidth(currentLanguage()=="en_US"?136:116);
    for(const auto &value:QStringList{"21","17","11","8","16","25"}){auto item=new QListWidgetItem(value,majorList);item->setData(Qt::UserRole,value);}majorList->setCurrentRow(1);
    auto providers = new QListWidget;
    providers->setObjectName("javaProviderSteps");
    providers->setProperty("javaSelectorList", true);
    QVBoxLayout *distributions;
    m_providerColumn = panel(QCoreApplication::translate("M2CL", "发行版"), "javaProviderPanel", &distributions);
    distributions->addWidget(providers, 1);
    m_providerColumn->setFixedWidth(172);
    for (auto list : {majorList, providers}) {
        list->setCursor(Qt::PointingHandCursor);
        list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    }
    for(const auto &entry:QList<QPair<QString,QString>>{{"Zulu","zulu"},{"Temurin","temurin"},{"Microsoft","microsoft"},{"Oracle OpenJDK","oracle-openjdk"},{QCoreApplication::translate("M2CL", "全部发行版"),"all"}}){auto item=new QListWidgetItem(entry.first,providers);item->setData(Qt::UserRole,entry.second);}providers->setCurrentRow(1);
    columns->addWidget(m_majorColumn);
    columns->addWidget(m_providerColumn);
    QVBoxLayout *builds;
    auto buildPanel = panel(QCoreApplication::translate("M2CL", "具体构建"), "javaBuildPanel", &builds, &m_buildHeading);m_buildBody=builds;
    m_builds->setProperty("javaBuildGrid", true);
    m_queryNotice=new NetworkNotice;m_queryNotice->setObjectName("javaQueryNetworkState");builds->addWidget(m_queryNotice);connect(m_queryNotice,&NetworkNotice::retryRequested,this,&JavaManager::query);
    builds->addWidget(m_builds, 1);
    m_archiveNote = note(QCoreApplication::translate("M2CL", "官方归档构建不再更新"));
    m_archiveNote->setWordWrap(false);
    m_archiveNote->hide();
    m_buildHeading->parentWidget()->layout()->addWidget(m_archiveNote);
    columns->addWidget(buildPanel, 1);
    rl->addLayout(columns, 1);
    connect(majorList,&QListWidget::currentRowChanged,this,[this,majorList](int row){if(row>=0)m_major->setCurrentText(majorList->item(row)->data(Qt::UserRole).toString());});connect(providers,&QListWidget::currentRowChanged,this,[this,providers](int row){if(row>=0)m_provider->setCurrentIndex(m_provider->findData(providers->item(row)->data(Qt::UserRole)));});
    connect(m_provider,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,providers](int){const QSignalBlocker blocker(providers);for(int row=0;row<providers->count();++row)if(providers->item(row)->data(Qt::UserRole)==m_provider->currentData())providers->setCurrentRow(row);});connect(m_major,&QComboBox::currentTextChanged,this,[majorList](const QString &value){const QSignalBlocker blocker(majorList);majorList->setCurrentRow(-1);for(int row=0;row<majorList->count();++row)if(majorList->item(row)->data(Qt::UserRole)==value)majorList->setCurrentRow(row);});
    auto paging = new QHBoxLayout; m_prev = action(QCoreApplication::translate("M2CL", "上一页"), "previousJavaPage"); m_next = action(QCoreApplication::translate("M2CL", "下一页"), "nextJavaPage"); m_pageLabel = note(QCoreApplication::translate("M2CL", "第 1 页")); paging->addWidget(m_prev); paging->addWidget(m_pageLabel); paging->addWidget(m_next); paging->addStretch(); rl->addLayout(paging);
    m_installStatus = note(QCoreApplication::translate("M2CL", "SHA-256 / 官方文件清单校验通过后安装，并复核实际构建。")); m_installStatus->setObjectName("javaInstallStatus"); m_installStatus->hide(); rl->addWidget(m_installStatus);
    m_progress = new QProgressBar; m_progress->setRange(0, 1000); m_progress->setValue(0); m_progress->hide(); rl->addWidget(m_progress);
    m_installNotice=new NetworkNotice;m_installNotice->setObjectName("javaInstallNetworkState");rl->addWidget(m_installNotice);connect(m_installNotice,&NetworkNotice::retryRequested,this,&JavaManager::install);
    auto remotePage=new QWidget;auto remoteRoot=new QVBoxLayout(remotePage);remoteRoot->setContentsMargins(0,0,0,0);remoteRoot->setSpacing(8);auto remoteScroll=new QScrollArea;remoteScroll->setObjectName("downloadJavaScroll");remoteScroll->setWidgetResizable(true);remoteScroll->setFrameShape(QFrame::NoFrame);remoteScroll->setWidget(remote);remoteRoot->addWidget(remoteScroll,1);
    auto remoteActions = new QHBoxLayout;
    m_installButton = action(QCoreApplication::translate("M2CL", "下载并安装所选构建"), "installJavaBuild", true);
    m_stopInstall = action(QCoreApplication::translate("M2CL", "取消下载"), "cancelJavaInstall");
    m_buildDetails = action(QCoreApplication::translate("M2CL", "构建详情 / 许可证"), "javaBuildDetails");
    remoteActions->addWidget(m_providerSite);
    remoteActions->addStretch();
    remoteActions->addWidget(m_buildDetails);
    remoteActions->addWidget(m_stopInstall);
    remoteActions->addWidget(m_installButton);
    remoteRoot->addLayout(remoteActions);
    tabs->addTab(remotePage, QCoreApplication::translate("M2CL", "下载 Java"));
    connect(m_buildDetails, &QPushButton::clicked, this, &JavaManager::showBuildDetails);
    connect(m_quick, &QPushButton::clicked, this, [this] { scan(false); }); connect(m_deep, &QPushButton::clicked, this, [this] { scan(true); });
    connect(m_stopScan, &QPushButton::clicked, this, [this] { if (m_scanCancel) m_scanCancel->store(true); m_scanStatus->setText(QCoreApplication::translate("M2CL", "正在停止，已找到的结果会保留…")); });
    connect(m_add, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, QCoreApplication::translate("M2CL", "选择 java / java.exe 可执行文件")); if (file.isEmpty()) return;
        if (!m_settings.javaFolders.contains(file)) m_settings.javaFolders.append(file); persist(); scan(false);
    });
    connect(&m_scan, &QFutureWatcher<JavaScanResult>::finished, this, [this] {
        const auto result = m_scan.result(); merge(result.runtimes); persist();
        m_scanStatus->setText(QCoreApplication::translate("M2CL", "%1 · 扫描 %2 个目录，候选 %3，验证通过 %4，跳过 %5%6").arg(result.cancelled ? QCoreApplication::translate("M2CL", "已取消，保留已找到结果") : QCoreApplication::translate("M2CL", "检测完成")).arg(result.progress.directories).arg(result.progress.candidates).arg(result.runtimes.size()).arg(result.progress.skipped).arg(result.limited ? QCoreApplication::translate("M2CL", "；快速检测已到范围上限，可全盘扫描") : QString())); updateButtons();
    });
    connect(m_refresh, &QPushButton::clicked, this, [this] { m_page = 0; query(); });
    connect(m_prev, &QPushButton::clicked, this, [this] { if (m_page > 0) { --m_page; query(); } }); connect(m_next, &QPushButton::clicked, this, [this] { if (m_hasMore) { ++m_page; query(); } });
    auto invalidate = [this] { if (m_queryCancel) m_queryCancel->store(true); m_packages.clear(); m_builds->setRowCount(0); m_page = 0; m_hasMore = false; m_pageLabel->setText(QCoreApplication::translate("M2CL", "第 1 页")); m_builds->setVisible(true);m_builds->setColumnHidden(2,m_provider->currentData()!="all");
        m_packageMessage.clear(); m_queryNotice->clear(); updateButtons(); };
    auto resetArchitectures = [this, invalidate] {
        m_availableArchitectures.clear(); m_architectureListComplete = false;
        refreshArchitectureOptions(); invalidate();
    };
    connect(m_major, &QComboBox::currentTextChanged, this, resetArchitectures);
    connect(m_provider, qOverload<int>(&QComboBox::currentIndexChanged), this, resetArchitectures);
    connect(m_os, qOverload<int>(&QComboBox::currentIndexChanged), this, resetArchitectures);
    connect(m_arch, qOverload<int>(&QComboBox::currentIndexChanged), this, invalidate);
    connect(&m_query, &QFutureWatcher<JavaPackageList>::finished, this, [this] {
        if (m_queryCancel->load()) { updateButtons(); return; } const auto result = m_query.result(); m_packages = result.packages; m_hasMore = result.hasMore; m_builds->setRowCount(0);
        for (const auto &arch : result.availableArchitectures) if (!m_availableArchitectures.contains(arch)) m_availableArchitectures.append(arch);
        m_architectureListComplete |= result.architectureListComplete;
        refreshArchitectureOptions();
        populateBuilds();
        m_pageLabel->setText(QCoreApplication::translate("M2CL", "第 %1 页").arg(m_page + 1));
        m_queryNotice->clear();
        m_packageMessage=m_packages.isEmpty()?QCoreApplication::translate("M2CL", "没有找到符合条件的构建，请调整版本、发行版或架构后重试。"):QString();
        if(!result.error.isEmpty())m_queryNotice->failure(m_packages.isEmpty()?QCoreApplication::translate("M2CL", "Java 构建查询失败"):QCoreApplication::translate("M2CL", "部分发行版未能加载，已保留可用构建"),result.error);
        if (!m_packages.isEmpty()) m_builds->setCurrentCell(0, 1); updateButtons();
    });
    connect(m_builds, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) { for (int i = 0; i < m_builds->rowCount(); ++i) if (auto cell = m_builds->cellWidget(i, 0)) if (auto radio = cell->findChild<QRadioButton *>()) radio->setChecked(i == row); updateButtons(); }); connect(m_installButton, &QPushButton::clicked, this, &JavaManager::install);
    connect(m_stopInstall, &QPushButton::clicked, this, [this] { if (m_installCancel) m_installCancel->store(true); m_installStatus->setText(QCoreApplication::translate("M2CL", "正在取消，已安装构建保持可用…")); });
    connect(&m_install, &QFutureWatcher<JavaInstallResult>::finished, this, [this] {
        const auto result = m_install.result(); const auto message = result.error.isEmpty() ? QCoreApplication::translate("M2CL", "安装完成：%1").arg(javaLabel(result.runtime)) + (result.warning.isEmpty() ? QString() : "\n" + result.warning) : result.error;
        m_installNotice->clear();m_installStatus->setText(message);m_installStatus->setVisible(result.error.isEmpty());
        if(!result.error.isEmpty()){m_installNotice->failure(QCoreApplication::translate("M2CL", "Java 安装未完成"),result.error);m_progress->hide();}
        if (result.error.isEmpty()) { merge({result.runtime}); persist(); m_progress->setRange(0, 1000); m_progress->setValue(1000); }
        emit taskProgress(QCoreApplication::translate("M2CL", "Java 安装"), message, result.error.isEmpty() ? 1000 : -1, false); updateButtons();
    });
    refreshArchitectureOptions(); refreshLocal(); updateButtons();
}
void JavaManager::layoutPlatformSelector(){
    auto grid=findChild<QGridLayout *>("javaPlatformGrid");if(!grid)return;
    auto osLabel=findChild<QWidget *>("javaOperatingSystemLabel");
    auto archLabel=findChild<QWidget *>("javaArchitectureLabel");
    auto detect=findChild<QWidget *>("detectJavaPlatform");
    if(!osLabel||!archLabel||!detect||!m_refresh)return;
    QList<QWidget *> fields{osLabel,m_os,archLabel,m_arch,detect,m_refresh};
    const bool compact=width()<(currentLanguage()=="en_US"?940:690);
    if(grid->property("arranged").toBool()&&grid->property("compact").toBool()==compact)return;
    grid->setProperty("arranged",true);grid->setProperty("compact",compact);
    while(auto item=grid->takeAt(0))delete item;
    for(int i=0;i<7;++i)grid->setColumnStretch(i,0);
    if(compact){
        grid->addWidget(osLabel,0,0);grid->addWidget(m_os,0,1,Qt::AlignLeft);grid->addWidget(detect,0,2);
        grid->addWidget(archLabel,1,0);grid->addWidget(m_arch,1,1,Qt::AlignLeft);grid->addWidget(m_refresh,1,2);grid->setColumnStretch(3,1);
    }else{for(int i=0;i<fields.size();++i)grid->addWidget(fields.at(i),0,i);grid->setColumnStretch(6,1);}
}
void JavaManager::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);layoutPlatformSelector();const bool wide=width()>=690;if(m_majorColumn){m_majorColumn->setVisible(wide);m_providerColumn->setVisible(wide);m_versionFilters->setVisible(!wide);m_builds->setColumnHidden(2,m_provider->currentData()!="all");updateButtons();}}
JavaManager::~JavaManager() {
    for (const auto &flag : {m_scanCancel, m_queryCancel, m_installCancel}) if (flag) flag->store(true);
    m_scan.waitForFinished(); m_query.waitForFinished(); m_install.waitForFinished();
}
void JavaManager::persist() { m_settings.javaCatalog = javaCatalogJson(m_runtimes); QString error; if (!m_store.save(m_settings, &error)) m_scanStatus->setText(QCoreApplication::translate("M2CL", "设置保存失败：%1").arg(error)); emit changed(); }
void JavaManager::merge(const QList<JavaRuntime> &found) {
    for (const auto &j : found) { bool replaced = false; for (auto &old : m_runtimes) if (old.executable == j.executable) { old = j; replaced = true; break; } if (!replaced) m_runtimes.append(j); } refreshLocal();
}
void JavaManager::refreshCatalog() { m_runtimes = javaCatalogFromJson(m_settings.javaCatalog); refreshLocal(); }
void JavaManager::refreshLocal() {
    m_local->setRowCount(m_runtimes.size());m_local->setFixedHeight(qMin(424,40+48*qMax(1,m_runtimes.size())));
    for(int index=0;index<m_runtimes.size();++index){const auto j=m_runtimes.at(index);const bool available=QFileInfo(j.executable).isExecutable();auto version=j.runtimeVersion.isEmpty()?j.version:j.runtimeVersion;if(version.endsWith("-LTS"))version.chop(4);QStringList values{version,distribution(j.vendor),j.architecture,available?QCoreApplication::translate("M2CL", "● 可用"):QCoreApplication::translate("M2CL", "路径失效")};
        for(int c=0;c<4;++c){auto item=new QTableWidgetItem(values.at(c));item->setToolTip(javaLabel(j)+"\n"+j.vmName+"\n"+j.executable);if(c==3&&available)item->setForeground(makeTheme(m_settings.theme,m_settings.appearance=="dark").accent);m_local->setItem(index,c,item);}m_local->setRowHeight(index,48);
        auto host=new QWidget;auto actions=new QHBoxLayout(host);actions->setContentsMargins(5,4,6,4);auto more=action({},"javaRuntimeMore");more->setIcon(lineIcon(Glyph::More));more->setProperty("iconOnly",true);more->setFixedSize(30,30);actions->addWidget(more);m_local->setCellWidget(index,4,host);
        connect(more,&QPushButton::clicked,this,[this,more,j]{RoundedMenu menu;auto details=menu.addAction(javaLabel(j)+"\n"+j.vmName);details->setEnabled(false);menu.addSeparator();menu.addAction(QCoreApplication::translate("M2CL", "打开位置"),&menu,[j]{QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(j.executable).path()));});const auto legal=javaLegalPaths(j.executable);auto license=menu.addAction(QCoreApplication::translate("M2CL", "查看包内许可证"),this,[this,j]{showRuntimeLicenses(j);});license->setObjectName("viewRuntimeLicenses");license->setEnabled(!legal.licenses.isEmpty());auto legalFolder=menu.addAction(QCoreApplication::translate("M2CL", "打开 legal 声明目录"),this,[legal]{QDesktopServices::openUrl(QUrl::fromLocalFile(legal.legalDirectory));});legalFolder->setEnabled(!legal.legalDirectory.isEmpty());menu.addAction(QCoreApplication::translate("M2CL", "复制完整路径"),&menu,[j]{QApplication::clipboard()->setText(j.executable);});menu.addAction(QCoreApplication::translate("M2CL", "移除列表记录"),&menu,[this,j]{QTimer::singleShot(0,this,[this,j]{for(int i=m_runtimes.size()-1;i>=0;--i)if(m_runtimes.at(i).executable==j.executable)m_runtimes.removeAt(i);refreshLocal();persist();});});menu.exec(more->mapToGlobal(QPoint(0,more->height())));});
    }
    updateButtons();
}
void JavaManager::populateBuilds() {
    m_builds->setRowCount(0);
    for (int i = 0; i < m_packages.size(); ++i) {
        const auto &p = m_packages.at(i); m_builds->insertRow(i); m_builds->setRowHeight(i, 36);
        auto cell = new QWidget; auto layout = new QHBoxLayout(cell); layout->setContentsMargins(0, 0, 0, 0);layout->setAlignment(Qt::AlignCenter); auto radio = new RadioButton; radio->setAutoExclusive(false); radio->setAccessibleName(QCoreApplication::translate("M2CL", "选择 %1 %2").arg(providerName(p.provider), p.label)); layout->addWidget(radio); m_builds->setCellWidget(i, 0, cell); connect(radio, &QRadioButton::clicked, this, [this, i, radio] { radio->setChecked(true); m_builds->setCurrentCell(i, 1); });
        const auto os = p.os == "osx" ? "macOS" : p.os == "windows" ? "Windows" : "Linux";
        const QStringList values{p.label, providerName(p.provider), os, p.arch, p.packageType, p.size > 0 ? QStringLiteral("%1 MB").arg(p.size / 1000000., 0, 'f', 1) : QStringLiteral("—")};
        for (int col = 0; col < values.size(); ++col) { auto item = new QTableWidgetItem(values.at(col)); item->setToolTip(p.label + "\n" + p.archiveName + "\n" + p.url.toString()); m_builds->setItem(i, col + 1, item); }
    }
}
void JavaManager::showBuildDetails() {
    const auto row = m_builds->currentRow(); if (row < 0 || row >= m_packages.size()) return;
    const auto p = m_packages.at(row); const auto info = javaPackageLicense(p);
    auto dialog = new QDialog(this); dialog->setObjectName("javaBuildDetailsDialog"); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle(QCoreApplication::translate("M2CL", "Java 构建详情")); dialog->resize(650, 380);
    auto body = new QVBoxLayout(dialog); body->setContentsMargins(20, 18, 20, 18); body->setSpacing(12);
    auto heading = new QLabel(providerName(p.provider) + " · " + p.label); heading->setTextFormat(Qt::PlainText); heading->setWordWrap(true); heading->setObjectName("sectionTitle"); body->addWidget(heading);
    auto environment = new QLabel(p.os + " · " + p.arch + " · " + p.packageType); environment->setTextFormat(Qt::PlainText); body->addWidget(environment);
    auto license = new QLabel(QCoreApplication::translate("M2CL", "主许可证：%1").arg(info.name)); license->setObjectName("javaBuildLicenseName"); license->setWordWrap(true); body->addWidget(license);
    auto scope = note(info.scope.isEmpty() ? QCoreApplication::translate("M2CL", "未获取到该构建的在线许可信息，请查看发行包内声明。") : info.scope); scope->setObjectName("javaBuildLicenseScope"); body->addWidget(scope);
    auto file = note(p.archiveName); file->setTextFormat(Qt::PlainText); file->setTextInteractionFlags(Qt::TextSelectableByMouse); body->addWidget(file);
    auto links = new QHBoxLayout;
    for (const auto &entry : QList<QPair<QString,QUrl>>{{QCoreApplication::translate("M2CL", "查看许可证 ↗"),info.licenseUrl},{QCoreApplication::translate("M2CL", "官方构建来源 ↗"),info.buildUrl},{QCoreApplication::translate("M2CL", "官方网站 ↗"),info.website}}) {
        auto button = new QPushButton(entry.first); button->setProperty("textLink",true); button->setProperty("targetUrl",entry.second); button->setEnabled(entry.second.scheme()=="https"&&!entry.second.host().isEmpty()); links->addWidget(button); const auto url=entry.second; connect(button,&QPushButton::clicked,dialog,[url]{QDesktopServices::openUrl(url);});
    }
    links->addStretch(); body->addLayout(links); body->addStretch();
    auto actions = new QDialogButtonBox(QDialogButtonBox::Close); actions->button(QDialogButtonBox::Close)->setText(QCoreApplication::translate("M2CL", "关闭")); body->addWidget(actions); connect(actions,&QDialogButtonBox::rejected,dialog,&QDialog::close); dialog->show();
}
void JavaManager::showRuntimeLicenses(const JavaRuntime &runtime) {
    const auto paths=javaLegalPaths(runtime.executable); if(paths.licenses.isEmpty())return;
    auto dialog=new QDialog(this);dialog->setObjectName("javaRuntimeLicensesDialog");dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(QCoreApplication::translate("M2CL", "包内许可证 · %1").arg(runtime.runtimeVersion));dialog->resize(760,540);
    auto body=new QVBoxLayout(dialog);auto files=new SelectBox;files->setObjectName("runtimeLicenseFiles");for(const auto &path:paths.licenses)files->addItem(QDir(paths.home).relativeFilePath(path),path);body->addWidget(files);
    auto content=new QPlainTextEdit;content->setObjectName("runtimeLicenseText");content->setReadOnly(true);body->addWidget(content,1);
    auto read=[files,content]{QFile file(files->currentData().toString());if(!file.open(QIODevice::ReadOnly)){content->setPlainText(QCoreApplication::translate("M2CL", "无法读取：%1").arg(file.errorString()));return;}if(file.size()>2*1024*1024){content->setPlainText(QCoreApplication::translate("M2CL", "文件超过预览大小，可从安装目录打开查看。"));return;}content->setPlainText(QString::fromUtf8(file.readAll()));};connect(files,qOverload<int>(&QComboBox::currentIndexChanged),dialog,[read]{read();});read();
    auto actions=new QDialogButtonBox(QDialogButtonBox::Close);actions->button(QDialogButtonBox::Close)->setText(QCoreApplication::translate("M2CL", "关闭"));auto folder=actions->addButton(QCoreApplication::translate("M2CL", "打开声明目录"),QDialogButtonBox::ActionRole);const auto directory=paths.legalDirectory.isEmpty()?paths.home:paths.legalDirectory;connect(folder,&QPushButton::clicked,dialog,[directory]{QDesktopServices::openUrl(QUrl::fromLocalFile(directory));});connect(actions,&QDialogButtonBox::rejected,dialog,&QDialog::close);body->addWidget(actions);dialog->show();
}
void JavaManager::scan(bool deep) {
    if (m_scan.isRunning() || m_install.isRunning()) return;
    auto roots = deep ? fullJavaScanRoots() : defaultJavaRoots(m_settings.gameRoots); roots.append(m_settings.javaFolders); roots.append(QDir(m_store.directory()).filePath("java"));
    for (const auto &j : m_runtimes) roots.append(j.executable);
    m_scanCancel = std::make_shared<std::atomic_bool>(false); const auto flag = m_scanCancel;
    m_scanStatus->setText(deep ? QCoreApplication::translate("M2CL", "正在全盘扫描…可以随时停止") : QCoreApplication::translate("M2CL", "正在快速检测…"));
    m_scan.setFuture(QtConcurrent::run([this, roots, flag, deep] {
        JavaScanOptions options; options.deep = deep;
        return scanJavaRuntimes(roots, flag, options, [this](const JavaScanProgress &p) { QMetaObject::invokeMethod(this, [this, p] { m_scanStatus->setText(QCoreApplication::translate("M2CL", "已扫描 %1 个目录 · 找到 %2 个可用 Java · 跳过 %3\n%4").arg(p.directories).arg(p.found).arg(p.skipped).arg(p.path)); }, Qt::QueuedConnection); });
    })); updateButtons();
}
void JavaManager::query() {
    if (m_query.isRunning()) return; const int major = m_major->currentText().toInt(); if (major < 6 || major > 99) { m_queryNotice->clear();m_packageMessage=QCoreApplication::translate("M2CL", "请输入 6–99 的 Java 主版本"); updateButtons(); return; }
    auto platform = GamePlatform::current(); platform.os = m_os->currentData().toString(); platform.arch = m_arch->currentData().toString(); const auto provider = m_provider->currentData().toString(); const int page = m_page;
    m_packages.clear(); m_builds->setRowCount(0); m_queryCancel = std::make_shared<std::atomic_bool>(false); const auto flag = m_queryCancel; m_packageMessage.clear(); m_queryNotice->clear();
    m_query.setFuture(QtConcurrent::run([provider, major, platform, flag, page] { return fetchJavaPackages(provider, major, platform, flag, page, nullptr, true); })); updateButtons();
}
void JavaManager::install() {
    const int row = m_builds->currentRow(); if (row < 0 || row >= m_packages.size() || m_install.isRunning() || m_scan.isRunning() || m_gameBusy) return;
    if (m_packages.at(row).os != GamePlatform::current().os) return;
    const auto package = m_packages.at(row); const auto root = QDir(m_store.directory()).filePath("java");
    m_installCancel = std::make_shared<std::atomic_bool>(false); const auto flag = m_installCancel;
    m_installNotice->clear();m_installStatus->show(); m_progress->show(); m_installStatus->setText(QCoreApplication::translate("M2CL", "准备安装 %1…").arg(package.label)); m_progress->setRange(0, 0);
    m_install.setFuture(QtConcurrent::run([this, package, root, flag] {
        return installJavaPackage(package, root, flag, [this, package](const QString &message, int progress) { QMetaObject::invokeMethod(this, [this, package, message, progress] { m_installStatus->setText(message); m_progress->setRange(0, progress < 0 ? 0 : 1000); if (progress >= 0) m_progress->setValue(progress); emit taskProgress(package.label, message, progress, true); }, Qt::QueuedConnection); });
    })); emit taskProgress(package.label, m_installStatus->text(), -1, true); updateButtons();
}
void JavaManager::setGameTaskBusy(bool busy) { m_gameBusy = busy; updateButtons(); }
void JavaManager::detectPlatform() {
    const auto platform = GamePlatform::current();
    m_platformDetected = true;
    if (m_arch->findData(platform.arch) < 0) m_arch->addItem(platform.arch, platform.arch);
    m_os->setCurrentIndex(m_os->findData(platform.os));
    m_arch->setCurrentIndex(m_arch->findData(platform.arch));
    const auto detected = QCoreApplication::translate("M2CL", "本机：%1 · %2").arg(m_os->currentText(), m_arch->currentText());
    if (auto button = findChild<QPushButton *>("detectJavaPlatform")) { button->setToolTip(detected); button->setAccessibleDescription(detected); }
}
void JavaManager::refreshArchitectureOptions() {
    const auto selected = m_arch->currentData().toString();
    const auto system = m_os->currentData().toString();
    const auto native = GamePlatform::current();
    QStringList architectures{"x86_64", "aarch64"};
    if (system != "osx") architectures.append("x86");
    if (system == "linux") architectures.append({"arm", "riscv64", "ppc64le", "s390x"});
    for (const auto &arch : m_availableArchitectures) if (!architectures.contains(arch)) architectures.append(arch);
    if (system == native.os && !architectures.contains(native.arch)) architectures.append(native.arch);
    const QSignalBlocker blocker(m_arch);
    m_arch->clear();
    for (const auto &arch : architectures) {
        m_arch->addItem(architectureName(arch), arch);
        const auto row = m_arch->count() - 1;
        const bool verified = m_availableArchitectures.contains(arch);
        m_arch->setItemData(row, verified ? QCoreApplication::translate("M2CL", "发行版已返回此架构的正式构建。") : m_architectureListComplete ? QCoreApplication::translate("M2CL", "所选发行版与 Java 主版本没有提供此系统 / 架构的构建。") : QCoreApplication::translate("M2CL", "可查询的架构；是否提供构建，以发行版实际返回为准。"), Qt::ToolTipRole);
        if (m_architectureListComplete && !verified) if (auto model = qobject_cast<QStandardItemModel *>(m_arch->model())) model->item(row)->setEnabled(false);
    }
    auto index = m_arch->findData(selected);
    if (index < 0) index = m_arch->findData(system == native.os ? native.arch : QString("x86_64"));
    m_arch->setCurrentIndex(index);
}
void JavaManager::updateButtons() {
    const bool scanning = m_scan.isRunning(), downloading = m_install.isRunning(), querying = m_query.isRunning();
    m_quick->setEnabled(!scanning && !downloading); m_deep->setEnabled(!scanning && !downloading); m_add->setEnabled(!scanning && !downloading); m_stopScan->setEnabled(scanning);m_stopScan->setVisible(scanning);
    m_refresh->setEnabled(!querying && !downloading);m_buildDetails->setEnabled(!querying&&m_builds->currentRow()>=0&&m_builds->currentRow()<m_packages.size()); m_installButton->setEnabled(!scanning && !downloading && !m_gameBusy && !querying && m_builds->currentRow() >= 0 && m_builds->currentRow() < m_packages.size()); m_stopInstall->setEnabled(downloading);m_stopInstall->setVisible(downloading);
    const bool localSystem = m_os->currentData() == GamePlatform::current().os;
    if (!localSystem) m_installButton->setEnabled(false);
    m_installButton->setToolTip(localSystem ? QString() : QCoreApplication::translate("M2CL", "所选构建用于 %1，不能作为本机 Java 运行环境安装。可浏览构建详情或前往发行版官网。").arg(m_os->currentText()));
    m_querySpinner->setVisible(querying);
    m_refresh->setText(querying ? QCoreApplication::translate("M2CL", "查询中") : QCoreApplication::translate("M2CL", "查询构建"));
    const bool empty = m_builds->rowCount() == 0;
    const bool noticeShown=!m_queryNotice->isHidden();
    if(empty){auto body=static_cast<QVBoxLayout *>(m_buildEmpty->layout());body->removeWidget(m_queryNotice);body->insertWidget(body->count()-1,m_queryNotice);}
    else m_buildBody->insertWidget(1,m_queryNotice);
    m_queryNotice->setVisible(noticeShown);m_buildEmpty->setVisible(empty);
    m_buildEmptyIcon->setVisible(!querying&&m_queryNotice->isHidden());
    m_buildEmptyText->setVisible(!querying&&m_queryNotice->isHidden());
    m_buildEmptyText->setText(querying ? QCoreApplication::translate("M2CL", "正在查询可用构建…") : m_packageMessage.isEmpty() ? QCoreApplication::translate("M2CL", "点击上方「查询构建」查看可用版本") : m_packageMessage);

    JavaPackage provider;
    provider.provider = m_provider->currentData().toString();
    if (provider.provider == "all" && m_builds->currentRow() >= 0 && m_builds->currentRow() < m_packages.size()) provider = m_packages.at(m_builds->currentRow());
    const auto website = javaPackageLicense(provider).website;
    m_providerSite->setProperty("targetUrl", website);
    m_providerSite->setToolTip(website.toDisplayString());
    m_providerSite->setVisible(!website.isEmpty() && !downloading);
    const auto caption = QCoreApplication::translate("M2CL", "具体构建（%1 · %2 · %3）").arg(m_provider->currentData() == "all" ? QCoreApplication::translate("M2CL", "全部发行版") : providerName(m_provider->currentData().toString()), m_major->currentText(), m_arch->currentData().toString());
    m_buildHeading->setText(caption);
    m_buildHeading->setToolTip(caption);
    m_archiveNote->setVisible(width() >= 860 && std::any_of(m_packages.begin(), m_packages.end(), [](const JavaPackage &p) { return p.archived; }));
    const bool paging=m_hasMore||m_page>0;for(auto widget:QList<QWidget *>{m_prev,m_next,m_pageLabel})widget->setVisible(paging);
    m_prev->setEnabled(!querying && !downloading && m_page > 0); m_next->setEnabled(!querying && !downloading && m_hasMore);

}
}
