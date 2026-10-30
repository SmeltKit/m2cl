// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QCoreApplication>
#include <QDialog>
class QVBoxLayout;
class QHBoxLayout;
class QPixmap;
namespace mc {
// Shared presentation shell; dependency solving and authorization stay in callers.
class InstallPlanDialog : public QDialog {
public:
    explicit InstallPlanDialog(QWidget *parent=nullptr);
    InstallPlanDialog(const QString &title, QWidget *parent);
    QVBoxLayout *contentLayout() const { return m_content; }
    static QVBoxLayout *section(QVBoxLayout *parent,const QString &title,const char *tone);
    void addTargetInstance(const QString &name, const QString &configuration, const QPixmap &cover,
                           const QString &caption = QCoreApplication::translate("M2CL", "安装到"));
protected:
    static QHBoxLayout *planRow();
    void showEvent(QShowEvent *event) override;
private:
    QVBoxLayout *m_content;
};
}
