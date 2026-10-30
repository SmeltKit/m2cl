// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/catalog.h"
#include <QAbstractButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>
#include <QTableWidget>
#include <QStyledItemDelegate>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPixmap>
#include <QPushButton>
#include <QVariantAnimation>
#include <QTimer>
#include <QSpinBox>

class QLineEdit;
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QGridLayout;
class QToolButton;
class QListWidget;

namespace mc {
enum class Glyph { Home, Grid, Mods, Folder, Settings, Download, Palette, Plus, More, Star, Play,
    Chevron, Edit, Java, Check, Refresh, Close, Minimize, Maximize, Leaf, Image, Grass, Document, Gamepad, Search, Ban, Trash };
QIcon lineIcon(Glyph glyph, const QColor &color = {});
QIcon applicationIcon();
class BrandMark final : public QWidget {
public:
    explicit BrandMark(int size, QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};
class BrandWordmark final : public QWidget {
public:
    explicit BrandWordmark(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }
protected:
    void paintEvent(QPaintEvent *) override;
};
QLabel *numberLabel(const QString &text,QWidget *parent=nullptr);
// Shared choice controls retain Qt's keyboard, accessibility and grouping behavior.
class CheckBox final : public QCheckBox {
public:
    explicit CheckBox(const QString &text={},QWidget *parent=nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override {return sizeHint();}
protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &point) const override {return rect().contains(point);}
private:
    QVariantAnimation m_motion;
    qreal m_mark=0;
};
class RadioButton final : public QRadioButton {
public:
    explicit RadioButton(const QString &text={},QWidget *parent=nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override {return sizeHint();}
protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &point) const override {return rect().contains(point);}
};
class NumberBox final:public QSpinBox {
public:
    explicit NumberBox(QWidget *parent=nullptr);
    // Empty input maps to the minimum sentinel; the hint is never editable text.
    void setEmptyValuePlaceholder(const QString &text);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *object,QEvent *event) override;
    QString textFromValue(int value) const override;
    int valueFromText(const QString &text) const override;
    QValidator::State validate(QString &input,int &position) const override;
private:
    bool m_emptyMinimum=false;
    QFrame *m_field=nullptr;
    QToolButton *m_minus=nullptr,*m_plus=nullptr;
    void updateSteps();
    void layoutParts();
};
class RoundedMenu final : public QMenu {
public:
    explicit RoundedMenu(QWidget *parent = nullptr);
};
class RoundedPopup final : public QFrame {
public:
    explicit RoundedPopup(QWidget *parent=nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
};
struct TableColumn {
    int index;
    int minimumWidth;
    qreal weight=0;
    Qt::Alignment alignment=Qt::AlignLeft|Qt::AlignVCenter;
};
class TableItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter,const QStyleOptionViewItem &option,const QModelIndex &index) const override;
};
class ResourceToolbar final:public QWidget {
public:
 explicit ResourceToolbar(const QString &kind,QWidget *parent=nullptr);
 QPushButton *add,*open,*refresh;
 QLineEdit *search;
 QLabel *count;
 QHBoxLayout *filters;
};
class RoundedTable final : public QTableWidget {
public:
    explicit RoundedTable(int rows, int columns, QWidget *parent = nullptr);
    void setColumnLayout(const QList<TableColumn> &columns);
    static constexpr int CellInset=12;
protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    void clipContents();
    void layoutColumns();
    QList<TableColumn> m_columns;
    bool m_layingOut=false,m_layoutPending=false;
};
class SelectBox final : public QComboBox {
    Q_OBJECT
public:
    using QComboBox::QComboBox;
    static constexpr int PopupGroupRole = Qt::UserRole + 37;
    void showPopup() override;
    void hidePopup() override;
    void setOptionsState(bool loading, const QString &error = {});
signals:
    void retryOptionsRequested();
protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    QPointer<QFrame> m_popup;
    QList<QListWidget *> m_popupLists;
};
class ToggleSwitch final : public QCheckBox {
public:
    explicit ToggleSwitch(const QString &text = {}, QWidget *parent = nullptr);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &point) const override {return rect().contains(point);}
private:
    QVariantAnimation m_motion;
    qreal m_position=0;
};

class BusySpinner final : public QWidget {
    Q_OBJECT
public:
    explicit BusySpinner(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    QTimer m_timer;
    int m_angle = 0;
};
class AdaptiveTags final : public QWidget {
    Q_OBJECT
public:
    explicit AdaptiveTags(const QStringList &labels,QWidget *parent = nullptr);
    int visibleCount() const;
    QStringList labels() const { return m_labels; }
protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *) override;
private:
    QStringList m_labels;
};
class FittingVersionLabel final : public QLabel {
    Q_OBJECT
public:
    explicit FittingVersionLabel(const QStringList &families,QWidget *parent = nullptr);
protected:
    void resizeEvent(QResizeEvent *) override;
    bool event(QEvent *) override;
private:
    void fit();
    QStringList m_families;
};
class SummaryLabel final : public QLabel {
public:
    explicit SummaryLabel(const QString &text, QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *) override;
};

class GlyphView final : public QWidget {
public:
    GlyphView(Glyph icon, int size, QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    Glyph m_icon;
};

class IconButton final : public QAbstractButton {
    Q_OBJECT
public:
    IconButton(Glyph glyph, const QString &description, QWidget *parent = nullptr);
    void setBadge(int count);
    QSize sizeHint() const override { return {36, 34}; }
protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *event) override;
private:
    Glyph m_glyph;
    int m_badge = 0;
    QVariantAnimation m_hover;
    qreal m_hoverAmount = 0;
};

class CoverStore {
public:
    static QString family(const QString &version);
    static QPixmap image(const QString &version, const QSize &size, const QString &custom = {});
    static bool readable(const QString &path);
    static void paint(QPainter &painter, const QRectF &rectangle, const QString &version,
        const QString &custom = {}, qreal radius = 8);
};

class HeroBadge final : public QLabel {
    Q_OBJECT
public:
    HeroBadge(Glyph icon, const QString &text, QWidget *parent = nullptr);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    Glyph m_icon;
    bool event(QEvent *event) override;
};

class GlassButton final : public QPushButton {
public:
    explicit GlassButton(const QString &text, QWidget *parent = nullptr);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *event) override;
};

class CoverView final : public QWidget {
    Q_OBJECT
public:
    explicit CoverView(QWidget *parent = nullptr);
    void setCover(const QString &version, const QString &custom = {});
    void setShaded(bool shaded) { m_shaded = shaded; m_frost = {}; update(); }
    void paintFrost(QPainter &painter, const QRect &source);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
private:
    void paintBackground(QPainter &painter);
    QImage m_frost;
    QString m_version = "1.20", m_custom;
    bool m_shaded = true;
};

class InstanceCard final : public QAbstractButton {
    Q_OBJECT
public:
    explicit InstanceCard(QWidget *parent = nullptr);
    void setInstance(const Instance &instance, const QString &name, const QString &cover, bool favorite, bool selected);
    QString instancePath() const { return m_instance.path; }
signals:
    void addModsRequested(const QString &path,const QPoint &position);
    void favoriteRequested(const QString &path);
    void menuRequested(const QString &path, const QPoint &position);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool event(QEvent *event) override;
private:
    Instance m_instance;
    QString m_name, m_cover;
    bool m_selected = false, m_favorite = false;
    IconButton *m_more, *m_star;
    QPushButton *m_addMods;
};

class InstanceGrid final : public QWidget {
    Q_OBJECT
public:
    explicit InstanceGrid(QWidget *parent = nullptr);
    void setInstances(const QList<Instance> &instances, const QString &selected, const QStringList &favorites,
        const QJsonObject &names, const QJsonObject &covers);
signals:
    void selected(const QString &path);
    void addModsRequested(const QString &path,const QPoint &position);
    void favoriteRequested(const QString &path);
    void menuRequested(const QString &path, const QPoint &position);
protected:
    void resizeEvent(QResizeEvent *) override;
private:
    void arrange();
    QList<InstanceCard *> m_cards;
    QGridLayout *m_layout;
    QLabel *m_empty;
    int m_columns = 0;
};

class AppearanceChoice final : public QAbstractButton {
    Q_OBJECT
public:
    AppearanceChoice(const QString &mode, QWidget *parent = nullptr);
    QString mode() const { return m_mode; }
    void setTheme(const QString &name);
    QSize sizeHint() const override { return {170, 108}; }
protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *event) override;
private:
    QString m_mode,m_theme="forest";
};

class NetworkNotice;
class DownloadPanel final : public QFrame {
    Q_OBJECT
public:
    explicit DownloadPanel(QWidget *parent = nullptr);
    void setTask(const QString &name, const QString &message, int progress, bool active);
protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
signals:
    void cancelRequested();
    void openRequested();
private:
    QLabel *m_name, *m_message, *m_state;
    NetworkNotice *m_taskNotice;
    QProgressBar *m_progress;
    QPushButton *m_cancel;
};

class WindowBar final : public QWidget {
public:
    explicit WindowBar(QWidget *window);
    IconButton *downloads, *appearance;
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
private:
    QPoint m_dragOffset;
    bool m_dragging = false;
};
}
