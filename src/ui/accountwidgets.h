// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/accounts.h"
#include <QFrame>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QSet>

class QLabel;
class QPushButton;
class QBoxLayout;
class QNetworkAccessManager;
class QMouseEvent;
class QKeyEvent;

namespace mc {
class AccountManager;
class NetworkNotice;

// Public texture cache and managed local skins; this class never receives tokens.
class AvatarCache final : public QObject {
    Q_OBJECT
public:
    explicit AvatarCache(const QString &directory, QObject *parent=nullptr,
                         QNetworkAccessManager *network=nullptr);
    QPixmap avatar(const Account &account);
    static QPixmap defaultAvatar();
    static QPixmap skinFace(const QByteArray &png);
signals:
    void avatarChanged();
private:
    QString m_directory,m_skinDirectory;
    QNetworkAccessManager *m_network;
    QHash<QString,QPixmap> m_faces;
    QSet<QString> m_attempted;
};

class AccountAvatar final : public QWidget {
    Q_OBJECT
public:
    explicit AccountAvatar(AvatarCache &cache, int size=56, QWidget *parent=nullptr);
    void setAccount(const Account &account);
    void setEditable(bool editable);
signals:
    void activated();
protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
private:
    AvatarCache &m_cache;
    Account m_account;
    QPixmap m_face;
    bool m_editable=false;
};

class HomeAccountCard final : public QFrame {
    Q_OBJECT
public:
    HomeAccountCard(AccountManager &manager, AvatarCache &cache, QWidget *parent=nullptr);
    QSize minimumSizeHint() const override { return QSize(0, sizeHint().height()); }
signals:
    void microsoftLoginRequested(const QString &accountId);
    void offlineCreationRequested();
    void managementRequested();
    void skinEditRequested(const QString &accountId);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void refresh();
    void chooseType(bool microsoft);
    void select(const QString &id);
    void showAccounts();
    AccountManager &m_manager;
    AvatarCache &m_cache;
    AccountAvatar *m_avatar;
    QLabel *m_name, *m_state;
    QPushButton *m_microsoft, *m_offline, *m_switch, *m_manage, *m_relogin;
    QWidget *m_identity, *m_actions;
    QBoxLayout *m_row;
    NetworkNotice *m_notice;
    QString m_lastMicrosoft, m_lastOffline;
    QPointer<QFrame> m_picker;
};
}
