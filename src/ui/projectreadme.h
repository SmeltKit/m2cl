// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QTextBrowser>
#include <QHash>
#include <QImage>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QQueue>
#include <QVector>

namespace mc {
// A native, non-executing project description with explicitly fetched images.
class ProjectReadme final : public QTextBrowser {
    Q_OBJECT
public:
    explicit ProjectReadme(QWidget *parent=nullptr,QNetworkAccessManager *network=nullptr,int timeoutMs=15000);
    ~ProjectReadme() override;
    void setProjectMarkdown(const QString &markdown,const QUrl &base);
    void retryImages();
    int pendingImages() const { return m_queue.size()+m_replies.size(); }
    int failedImages() const;
    QString imageErrorDetails() const;
    int loadedImages() const;
    static QString descriptionHtml(const QString &markdown);
signals:
    void imageStatusChanged(int pending,int failed);
protected:
    QVariant loadResource(int type,const QUrl &name) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    struct Image { QString error;QUrl url;QImage pixels;bool failed=false;int redirects=0;qint64 startedMs=0; };
    struct Placement { int position;QString key; qreal width,height; };
    void pump();
    void layoutImages();
    void notify();
    void cancelImages();
    void styleDocument();
    QImage placeholder(bool failed) const;
    QNetworkAccessManager m_network;
    QNetworkAccessManager *m_manager;
    int m_timeoutMs;
    quint64 m_generation=0;
    QString m_markdown;
    QUrl m_base;
    QHash<QString,Image> m_images;
    QVector<Placement> m_placements;
    QVector<int> m_ruleBlocks;
    QQueue<QString> m_queue;
    QHash<QString,QPointer<QNetworkReply>> m_replies;
};
}
