// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "ui/projectreadme.h"
#include <QBuffer>
#include <QFile>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextTable>
#include <QScrollBar>
#include <QTimer>
#include <cstring>

struct Route { QByteArray bytes;int delay=0;QString redirect;int status=200; };
class ImageReply final:public QNetworkReply {
public:
    ImageReply(const QNetworkRequest &request,const Route &route,QObject *parent):QNetworkReply(parent),m_bytes(route.bytes){
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute,route.status);setRequest(request);setUrl(request.url());setOperation(QNetworkAccessManager::GetOperation);open(QIODevice::ReadOnly|QIODevice::Unbuffered);
        if(!route.redirect.isEmpty())setAttribute(QNetworkRequest::RedirectionTargetAttribute,QUrl(route.redirect));
        QTimer::singleShot(route.delay,this,[this]{if(isFinished())return;m_ready=true;emit readyRead();if(isFinished())return;setFinished(true);emit finished();});
    }
    void abort() override {if(!isFinished()){setError(OperationCanceledError,"cancelled");setFinished(true);emit finished();}}
    qint64 bytesAvailable() const override{return (m_ready?m_bytes.size()-m_offset:0)+QIODevice::bytesAvailable();}
protected:
    qint64 readData(char *target,qint64 maximum) override {if(!m_ready||m_offset>=m_bytes.size())return -1;auto n=qMin(maximum,qint64(m_bytes.size()-m_offset));std::memcpy(target,m_bytes.constData()+m_offset,size_t(n));m_offset+=n;return n;}
private:QByteArray m_bytes;qint64 m_offset=0;bool m_ready=false;
};
class ImageNetwork final:public QNetworkAccessManager {
public:QHash<QString,Route> routes;QStringList requested;
protected:QNetworkReply *createRequest(Operation,const QNetworkRequest &request,QIODevice *)override{requested.append(request.url().path());return new ImageReply(request,routes.value(request.url().path()),this);}
};
class ReadmeTests:public QObject {
    Q_OBJECT
    static QByteArray png(){QImage image(1200,600,QImage::Format_RGB32);image.fill(Qt::red);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");return bytes;}
    static QList<QTextImageFormat> images(mc::ProjectReadme &view){QList<QTextImageFormat> result;for(auto block=view.document()->begin();block.isValid();block=block.next())for(auto it=block.begin();!it.atEnd();++it){auto part=it.fragment();if(part.isValid()&&part.charFormat().isImageFormat())result.append(part.charFormat().toImageFormat());}return result;}
private slots:
    void detailsPreserveQuestionsAnswersAndLiteralCode(){
        ImageNetwork network;mc::ProjectReadme view(nullptr,&network);
        view.setProjectMarkdown("<details><summary>Frequently asked questions</summary>\n\n1. **Can I upgrade?**\nYes, upgrading is safe.\n\n2. **Which loader?**\nFabric and Forge.\n</details>\n\n`<summary>literal</summary>`\n\n```html\n<details>example</details>\n```",QUrl("https://modrinth.com/mod/example"));
        const auto text=view.toPlainText();QVERIFY(text.contains("Frequently asked questions"));QVERIFY(text.contains("Can I upgrade?"));QVERIFY(text.contains("upgrading is safe"));QVERIFY(text.contains("Fabric and Forge"));QVERIFY(text.contains("<summary>literal</summary>"));QVERIFY(text.contains("<details>example</details>"));QCOMPARE(network.requested.size(),0);
    }
    void realDescriptionRegression() {
        const auto review=qEnvironmentVariable("M2CL_README_REVIEW");if(review.isEmpty())QSKIP("Opt-in actual project body check");
        for(const auto &name:QStringList{"sodium","ct-overhaul-village","xaeros-minimap"}){
            QFile file(review+"/"+name+".md");QVERIFY(file.open(QIODevice::ReadOnly));const auto markdown=QString::fromUtf8(file.readAll());ImageNetwork network;mc::ProjectReadme view(nullptr,&network);
            view.setProjectMarkdown(markdown,QUrl("https://modrinth.com/mod/"+name));qInfo()<<name<<"raw"<<markdown.size()<<"text"<<view.toPlainText().size();
            QVERIFY(view.toPlainText().size()>markdown.size()/2);
            QFile rendered(review+"/"+name+".rendered.txt");QVERIFY(rendered.open(QIODevice::WriteOnly));rendered.write(view.toPlainText().toUtf8());
            if(name=="xaeros-minimap"){QVERIFY(view.toPlainText().contains("Multiple minimap size options"));QVERIFY(view.toPlainText().contains("Modpacks and Other"));QVERIFY(view.toPlainText().contains("Only monetization"));QVERIFY(view.toPlainText().contains("<minecraft version>"));
                const auto expectedPath=qEnvironmentVariable("M2CL_README_XAERO_EXPECTED");
                if(!expectedPath.isEmpty()){
                    QFile expected(expectedPath);QVERIFY(expected.open(QIODevice::ReadOnly));
                    const auto normalized=[](QString value){value.remove(QChar::ObjectReplacementCharacter);value.replace(QRegularExpression("\\s+")," ");return value.trimmed();};
                    QCOMPARE(normalized(view.toPlainText()),normalized(QString::fromUtf8(expected.readAll())));
                }
            }
        }
    }
    void mixedHtmlListsFormattingCodeAndTailDoNotDisappear(){
        ImageNetwork network;mc::ProjectReadme view(nullptr,&network);
        view.setProjectMarkdown("### Heading <a href=\"https://example.com\">Author</a>\n\n<p>Paragraph <strong>bold</strong> and more.</p>\n\n<ul><li>First <strong>feature</strong>.</li><li>Second with <a href=\"https://example.com\">link</a>.</li></ul>\n\n## Developers\n\nReplace <minecraft version> and <mod version>.\n\n```groovy\nimplementation \"library-<minecraft version>:<mod version>\"\n```\n\n## Conditions\n\n<p><span style=\"color: purple\"><em><strong>Required conditions</strong></em></span></p>\n<ul><li>Keep this first condition.</li><li>Keep this final condition.</li></ul>",QUrl("https://modrinth.com/mod/example"));
        const auto rendered=view.toPlainText();for(const auto &value:QStringList{"Heading Author","Replace <minecraft version> and <mod version>.","Paragraph bold and more.","First feature.","Second with link.","<minecraft version>","Required conditions","Keep this final condition."})QVERIFY2(rendered.contains(value),qPrintable(value));
    }
    void commonMarkdownStylesKeepContentAndAdaptToTheme(){
        ImageNetwork network;mc::ProjectReadme view(nullptr,&network);view.resize(700,900);view.show();
        view.setProjectMarkdown("# Main heading\n\n> Quoted **bold** and [linked](https://example.com).\n>\n> Second paragraph.\n>\n> > Nested quotation.\n\n## Subheading\n\nNormal `inline_code` and ~~deleted words~~.\n\n- [ ] Unfinished task\n- [x] Finished task\n\n```java\nfirst_line();\nsecond_line();\n```\n\n| Name | Value |\n| --- | --- |\n| Alpha | One |\n\n---\n\nFinal paragraph.",QUrl("https://modrinth.com/mod/example"));
        const auto plain=view.toPlainText();for(const auto &value:QStringList{"Quoted bold and linked.","Second paragraph.","Nested quotation.","☐ Unfinished task","☑ Finished task","first_line();","second_line();","Final paragraph."})QVERIFY2(plain.contains(value),qPrintable(value));
        int quotes=0,dataTables=0,codeFrames=0;QList<QTextFrame *> frames{view.document()->rootFrame()};QColor oldRule;
        while(!frames.isEmpty()){auto frame=frames.takeLast();frames.append(frame->childFrames());if(auto table=qobject_cast<QTextTable *>(frame)){
            const auto cell=table->cellAt(0,0).format().toTableCellFormat();if(cell.leftBorder()==3){++quotes;QCOMPARE(cell.rightBorder(),0.);oldRule=cell.leftBorderBrush().color();}
            else if(cell.leftBorder()==0){++codeFrames;QVERIFY(cell.background().style()!=Qt::NoBrush);}
            else{++dataTables;QCOMPARE(cell.leftBorder(),1.);QVERIFY(cell.background().style()!=Qt::NoBrush);}
        }}QCOMPARE(quotes,2);QCOMPARE(dataTables,1);QCOMPARE(codeFrames,1);
        bool deleted=false,inlineCode=false,fenced=false,link=false;
        for(auto block=view.document()->begin();block.isValid();block=block.next()){
            if(block.text().startsWith(QChar(0x2610))||block.text().startsWith(QChar(0x2611)))QVERIFY(!block.textList());
            if(block.text()=="first_line();"){fenced=true;QVERIFY(QTextCursor(block).currentTable());}
            for(auto it=block.begin();!it.atEnd();++it){const auto part=it.fragment();if(part.text().contains("deleted words"))deleted=part.charFormat().fontStrikeOut();if(part.text()=="inline_code")inlineCode=part.charFormat().fontFixedPitch()&&part.charFormat().background().style()!=Qt::NoBrush;if(part.text()=="linked")link=part.charFormat().anchorHref()=="https://example.com";}
        }QVERIFY(deleted&&inlineCode&&fenced&&link);
        QPalette dark=view.palette();dark.setColor(QPalette::Text,QColor("#edf4ef"));dark.setColor(QPalette::Base,QColor("#1b2a22"));dark.setColor(QPalette::AlternateBase,QColor("#14241c"));dark.setColor(QPalette::Link,QColor("#96dfb6"));view.setPalette(dark);QCOMPARE(view.toPlainText(),plain);
        auto table=qobject_cast<QTextTable *>(view.document()->rootFrame()->childFrames().first());QVERIFY(table);QVERIFY(table->cellAt(0,0).format().toTableCellFormat().leftBorderBrush().color()!=oldRule);
        view.resize(300,900);QTest::qWait(20);QCOMPARE(view.horizontalScrollBar()->maximum(),0);
        const auto review=qEnvironmentVariable("M2CL_README_REVIEW");if(!review.isEmpty()){view.resize(700,900);QTest::qWait(20);view.grab().save(review+"/styles-dark.png");}
    }
    void imagesLoadOnceAndFitTheViewportIncludingSvg(){
        ImageNetwork network;network.routes.insert("/wide.png",{png()});network.routes.insert("/badge.svg",{"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"90\" height=\"22\"><rect width=\"90\" height=\"22\" fill=\"green\"/></svg>"});
        mc::ProjectReadme view(nullptr,&network);view.resize(420,320);view.show();
        view.setProjectMarkdown("![wide](https://images.example/wide.png)\n\n<img src=\"https://images.example/badge.svg\" height=\"22\">\n\n![repeat](https://images.example/wide.png)\n\n> > ![nested quote](https://images.example/wide.png)",QUrl("https://modrinth.com/mod/example"));
        QTRY_COMPARE(view.pendingImages(),0);QCOMPARE(view.loadedImages(),2);QCOMPARE(view.failedImages(),0);QCOMPARE(network.requested.count("/wide.png"),1);QCOMPARE(images(view).size(),4);QVERIFY(images(view).last().width()<images(view).first().width());
        for(const auto &image:images(view))QVERIFY(image.width()<=view.viewport()->width());const auto previous=images(view).first().width();view.resize(250,320);QTest::qWait(30);QVERIFY(images(view).first().width()<previous);QVERIFY(images(view).at(1).height()<=22);
    }
    void imageErrorsExposeHttpStatusAndClearAfterRetry(){
        ImageNetwork network;network.routes["/denied.png"]={"denied",0,{},403};mc::ProjectReadme view(nullptr,&network,30);view.setProjectMarkdown("![image](https://example.test/denied.png)",QUrl("https://example.test/"));QTRY_COMPARE(view.failedImages(),1);QVERIFY(view.imageErrorDetails().contains("HTTP 403"));
        network.routes["/denied.png"]={png()};view.retryImages();QTRY_COMPARE(view.loadedImages(),1);QVERIFY(view.imageErrorDetails().isEmpty());
    }
    void timeoutRetryAndLateRepliesDoNotReplaceAnotherProject(){
        ImageNetwork network;network.routes.insert("/retry.png",{"",60000});mc::ProjectReadme view(nullptr,&network,30);view.setProjectMarkdown("![retry](https://images.example/retry.png)",QUrl("https://modrinth.com/mod/example"));
        QTRY_COMPARE(view.failedImages(),1);QVERIFY(view.imageErrorDetails().contains("Timeout"));QCOMPARE(view.pendingImages(),0);QVERIFY(images(view).first().height()<=30);network.routes["/retry.png"]={png()};view.retryImages();QTRY_COMPARE(view.loadedImages(),1);QCOMPARE(view.failedImages(),0);
        network.routes.insert("/old.png",{png(),80});view.setProjectMarkdown("![old](https://images.example/old.png)",QUrl("https://modrinth.com/mod/old"));QTest::qWait(1);view.setProjectMarkdown("New project",QUrl("https://modrinth.com/mod/new"));QTest::qWait(100);QCOMPARE(view.toPlainText(),QString("New project"));QCOMPARE(view.loadedImages(),0);QCOMPARE(view.pendingImages(),0);
    }
    void imageRequestsCannotReadLocalFilesOrFollowLocalRedirects(){
        ImageNetwork network;network.routes.insert("/redirect",{"",0,"file:///tmp/private-image.png"});network.routes.insert("/unsafe.svg",{"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10\" height=\"10\"><image href=\"file:///tmp/private.png\"/></svg>"});
        mc::ProjectReadme view(nullptr,&network);view.setProjectMarkdown("![local](file:///tmp/private-image.png)\n\n![loop](https://127.0.0.1/private.png)\n\n![redirect](https://images.example/redirect)\n\n![svg](https://images.example/unsafe.svg)",QUrl("https://modrinth.com/mod/example"));
        QTRY_COMPARE(view.failedImages(),4);QCOMPARE(network.requested.size(),2);QCOMPARE(view.loadedImages(),0);
    }
};
QTEST_MAIN(ReadmeTests)
#include "readme_tests.moc"
