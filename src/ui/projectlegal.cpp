// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "projectlegal.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

// Explicit reference keeps this resource object when launcher_ui is statically linked.
static void initializeLegalResources() { Q_INIT_RESOURCE(licenses); }

namespace mc {
ProjectLegalDialog::ProjectLegalDialog(QWidget *parent)
    : InstallPlanDialog(QCoreApplication::translate("M2CL", "许可证与署名"), parent) {
    initializeLegalResources();
    setObjectName("projectLegalDialog");
    resize(760, 600);
    auto content = contentLayout();
    auto attribution = new QLabel(QCoreApplication::translate("M2CL", "原项目：Mercury MC Launcher（M²CL）\n原项目作者：ppm203 · Copyright © 2026 ppm203"));
    attribution->setObjectName("projectAttribution");
    attribution->setTextFormat(Qt::PlainText);
    attribution->setWordWrap(true);
    attribution->setTextInteractionFlags(Qt::TextSelectableByMouse);
    content->addWidget(attribution);
    auto summary = new QLabel(QCoreApplication::translate("M2CL", "GNU GPL 第 3 版 + 第 7 节附加条款。修改版须保留合理署名、明确区分来源，不得暗示原作者背书。软件不提供担保；第三方组件保留原许可。"));
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);
    summary->setProperty("muted", true);
    content->addWidget(summary);
    auto tabs = new QTabWidget;
    tabs->setObjectName("projectLegalTabs");
    struct Document { const char *path; const char *objectName; const char *title; };
    const Document documents[] = {
        {":/legal/NOTICE", "projectNoticeText", QT_TRANSLATE_NOOP("M2CL", "声明与署名")},
        {":/legal/GPL-3.0.txt", "projectGplText", QT_TRANSLATE_NOOP("M2CL", "GPLv3 全文")},
        {":/legal/ADDITIONAL_TERMS.md", "projectTermsText", QT_TRANSLATE_NOOP("M2CL", "附加条款")},
        {":/legal/md4c-LICENSE.md", "projectMd4cText", QT_TRANSLATE_NOOP("M2CL", "第三方 · MD4C")}
    };
    for (const auto &document : documents) {
        auto text = new QPlainTextEdit;
        text->setObjectName(document.objectName);
        text->setReadOnly(true);
        text->setFont(QApplication::font());
        text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        QFile file(QString::fromLatin1(document.path));
        text->setPlainText(file.open(QIODevice::ReadOnly)
                              ? QString::fromUtf8(file.readAll())
                              : QCoreApplication::translate("M2CL", "随附许可证文件读取失败，请查看发行包的 licenses 目录。"));
        tabs->addTab(text, QCoreApplication::translate("M2CL", document.title));
    }
    content->addWidget(tabs, 1);
    auto buttons = new QDialogButtonBox;
    auto close = buttons->addButton(QCoreApplication::translate("M2CL", "关闭"), QDialogButtonBox::RejectRole);
    close->setDefault(true);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    content->addWidget(buttons);
}
}
