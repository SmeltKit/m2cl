// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "network/networkerror.h"
#include "networknotice.h"
#include "launcherwidgets.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
namespace mc {
NetworkNotice::NetworkNotice(QWidget *parent):QFrame(parent){
    setProperty("networkNotice",true);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    auto body=new QVBoxLayout(this);body->setContentsMargins(14,12,14,12);body->setSpacing(8);
    auto row=new QHBoxLayout;row->setSpacing(10);m_spinner=new BusySpinner;m_spinner->setFixedSize(24,24);row->addWidget(m_spinner);
    m_icon=new GlyphView(Glyph::Ban,22);row->addWidget(m_icon);
    m_title=new QLabel;m_title->setTextFormat(Qt::PlainText);m_title->setWordWrap(true);m_title->setObjectName("networkNoticeTitle");auto caption=new QVBoxLayout;caption->setSpacing(4);caption->addWidget(m_title);m_summary=new QLabel;m_summary->setObjectName("networkErrorSummary");m_summary->setTextFormat(Qt::PlainText);m_summary->setWordWrap(true);m_summary->setProperty("muted",true);caption->addWidget(m_summary);row->addLayout(caption,1);
    m_detailsButton=new QPushButton(QCoreApplication::translate("M2CL", "详情"));m_detailsButton->setObjectName("networkShowDetails");m_detailsButton->setCheckable(true);m_detailsButton->setProperty("textLink",true);row->addWidget(m_detailsButton);
    m_retry=new QPushButton(QCoreApplication::translate("M2CL", "重试"));m_retry->setIcon(lineIcon(Glyph::Refresh));m_retry->setObjectName("networkRetry");row->addWidget(m_retry);body->addLayout(row);
    m_details=new QPlainTextEdit;m_details->setObjectName("networkErrorDetails");m_details->setReadOnly(true);m_details->setMaximumHeight(110);m_details->setMinimumHeight(64);body->addWidget(m_details);
    connect(m_detailsButton,&QPushButton::toggled,m_details,&QWidget::setVisible);connect(m_retry,&QPushButton::clicked,this,&NetworkNotice::retryRequested);clear();
}
void NetworkNotice::clear(){hide();m_title->clear();m_summary->clear();m_summary->hide();m_details->clear();m_detailsButton->setChecked(false);m_details->hide();m_spinner->hide();}
void NetworkNotice::loading(){clear();setProperty("networkLoading",true);style()->unpolish(this);style()->polish(this);m_icon->hide();m_detailsButton->hide();m_retry->hide();m_title->hide();m_spinner->show();show();}
void NetworkNotice::failure(const QString &title,const QString &details,bool retry){
    clear();setProperty("networkLoading",false);style()->unpolish(this);style()->polish(this);m_title->setText(title);m_title->show();const auto summary=networkErrorSummary(details);m_summary->setText(summary);m_summary->setVisible(!summary.isEmpty()&&summary!=title);m_icon->show();m_retry->setVisible(retry);m_details->setPlainText(details.left(16384));m_detailsButton->setVisible(!details.isEmpty());show();
}
}
