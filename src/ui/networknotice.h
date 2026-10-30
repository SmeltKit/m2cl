// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QFrame>
class QLabel;
class QPushButton;
class QPlainTextEdit;
namespace mc {
class BusySpinner;
// A request owns its notice. Start/retry clears the previous error, success hides it.
class NetworkNotice final : public QFrame {
    Q_OBJECT
public:
    explicit NetworkNotice(QWidget *parent=nullptr);
    void loading();
    void failure(const QString &title,const QString &details,bool retry=true);
    void clear();
signals:
    void retryRequested();
private:
    QLabel *m_title,*m_summary;
    QWidget *m_icon;
    BusySpinner *m_spinner;
    QPushButton *m_retry,*m_detailsButton;
    QPlainTextEdit *m_details;
};
}
