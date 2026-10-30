// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "network/networkerror.h"
#include "projectreadme.h"
#include "md4c-html.h"
#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QCache>
#include <QCoreApplication>
#include <QDateTime>
#include <QHostAddress>
#include <QFontDatabase>
#include <QImageReader>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPaintEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSvgRenderer>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextFragment>
#include <QTextTable>
#include <QTextList>
#include <QEvent>
#include <QTimer>
#include <QXmlStreamReader>
#include <cmath>
#include <functional>

namespace mc {
namespace {
QCache<QString,QImage> imageCache(32768); // KiB, shared across descriptions.
constexpr qint64 MaxBytes=8*1024*1024;
constexpr int QuoteFrame=QTextFormat::UserProperty+101;
constexpr int CodeBlock=QTextFormat::UserProperty+102;
constexpr int InlineCode=QTextFormat::UserProperty+103;
constexpr int CodeFrame=QTextFormat::UserProperty+104;
constexpr int RuleWidth=QTextFormat::UserProperty+105;
QFont codeFont(){
    auto font=QFontDatabase::systemFont(QFontDatabase::FixedFont);
#ifdef Q_OS_MACOS
    font.setFamily(QStringLiteral("Menlo"));
#endif
    font.setStyleHint(QFont::Monospace);font.setFixedPitch(true);return font;
}
QColor blend(const QColor &foreground,const QColor &background,qreal amount){
    return QColor::fromRgbF(foreground.redF()*amount+background.redF()*(1-amount),foreground.greenF()*amount+background.greenF()*(1-amount),foreground.blueF()*amount+background.blueF()*(1-amount));
}
QUrl imageUrl(const QUrl &base,const QString &name) {
    auto url=base.resolved(QUrl(name));
    if(url.scheme()=="http")url.setScheme("https");
    const auto host=url.host().toLower();QHostAddress ip;
    if(url.scheme()!="https" || host.isEmpty() || !url.userInfo().isEmpty() || url.port(443)!=443 || host=="localhost" || host.endsWith(".localhost") || host.endsWith(".local") || ip.setAddress(host))return {};
    url.setFragment({});return url;
}
QHash<QString,QString> htmlAttributes(const QString &tag) {
    QHash<QString,QString> attributes;
    const QRegularExpression pattern(QStringLiteral(R"ATTR(([\w:-]+)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))ATTR"));
    auto matches=pattern.globalMatch(tag);
    while(matches.hasNext()){const auto match=matches.next();const auto value=!match.captured(2).isNull()?match.captured(2):!match.captured(3).isNull()?match.captured(3):match.captured(4);auto decoded=value;decoded.replace('<',"&lt;");decoded.replace('>',"&gt;");attributes.insert(match.captured(1).toLower(),QTextDocumentFragment::fromHtml(decoded).toPlainText());}
    return attributes;
}
QImage decodeImage(const QByteArray &bytes) {
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);reader.setAutoTransform(true);
    if(reader.format()=="svg" || bytes.trimmed().startsWith("<svg") || bytes.trimmed().startsWith("<?xml")) {
        QXmlStreamReader xml(bytes);
        while(!xml.atEnd()){
            xml.readNext();if(xml.isDTD())return {};
            if(xml.isStartElement())for(const auto &attribute:xml.attributes()){
                const auto name=attribute.name().toString(),value=attribute.value().toString().trimmed();
                if((name=="href"||name=="src")&&!value.startsWith('#'))return {};
                if(value.contains("url(",Qt::CaseInsensitive)&&!value.contains("url(#"))return {};
            }
        }
        if(xml.hasError())return {};
        QSvgRenderer svg(bytes);
#if QT_VERSION >= QT_VERSION_CHECK(6,7,0)
        svg.setAnimationEnabled(false);
#endif
        if(!svg.isValid())return {};const auto natural=svg.defaultSize();if(natural.isEmpty())return {};
        const auto size=natural.width()>1600||natural.height()>1600?natural.scaled(1600,1600,Qt::KeepAspectRatio):natural;QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);svg.render(&painter);return image;
    }
    auto size=reader.size();if(!size.isValid() || size.width()>16384 || size.height()>16384 || qint64(size.width())*size.height()>32*1024*1024)return {};
    if(size.width()>1600 || size.height()>2400)reader.setScaledSize(size.scaled(1600,2400,Qt::KeepAspectRatio));
    return reader.read(); // Animated formats deliberately use their first frame.
}
}
QString ProjectReadme::descriptionHtml(const QString &markdown) {
    const auto input=markdown.toUtf8();QByteArray output;
    const auto append=[](const MD_CHAR *text,MD_SIZE size,void *context){static_cast<QByteArray *>(context)->append(text,int(size));};
    if(md_html(input.constData(),MD_SIZE(input.size()),append,&output,MD_DIALECT_GITHUB,0)!=0)return "<p>"+markdown.toHtmlEscaped()+"</p>";
    auto html=QString::fromUtf8(output);
    html.remove(QRegularExpression("<!--[\\s\\S]*?-->"));
    html.remove(QRegularExpression("<(script|style|iframe|object)\\b[^>]*>[\\s\\S]*?</\\1\\s*>",QRegularExpression::CaseInsensitiveOption));
    const QSet<QString> allowed{"p","div","center","span","a","b","strong","i","em","u","s","del","sup","sub","blockquote","pre","code","kbd","br","hr","img","ul","ol","li","dl","dt","dd","table","thead","tbody","tfoot","tr","th","td","h1","h2","h3","h4","h5","h6"};
    const QRegularExpression tags(QStringLiteral(R"TAG(<\s*(/?)\s*([A-Za-z][A-Za-z0-9:-]*)(\b(?:"[^"]*"|'[^']*'|[^'">])*)>)TAG"));
    auto matches=tags.globalMatch(html);QVector<QRegularExpressionMatch> tokens;while(matches.hasNext())tokens.append(matches.next());
    for(int i=tokens.size()-1;i>=0;--i){const auto token=tokens.at(i);auto name=token.captured(2).toLower();const bool closing=!token.captured(1).isEmpty();
        if(name=="details")name="div";else if(name=="summary")name="h3";else if(name=="font")name="span";else if(name=="del"||name=="strike")name="s";
        QString replacement;
        // Unknown pseudo-tags are often placeholders in author prose, e.g. <minecraft version>.
        // Preserve them literally instead of silently deleting their text.
        const QSet<QString> omitted{"html","head","body","title","meta","link","section","article","header","footer","main","aside","nav","figure","figcaption","picture","source","video","audio","input","button","form","select","option","textarea","wbr"};
        if(!allowed.contains(name)&&!omitted.contains(name))replacement=token.captured().toHtmlEscaped();
        if(allowed.contains(name)){
            if(closing)replacement="</"+name+">";
            else {
                const auto attributes=htmlAttributes(token.captured(3));QStringList keep;
                if(name=="img")keep=QStringList{"src","alt","title","width","height"};
                else if(name=="a")keep=QStringList{"href","name","title"};
                else if(name=="ol")keep=QStringList{"start","type"};
                else if(name=="ul")keep=QStringList{"type"};
                else if(name=="table")keep=QStringList{"cellspacing","cellpadding","border","width"};
                else if(name=="th"||name=="td")keep=QStringList{"colspan","rowspan","align","valign"};
                else if(name=="p"||name=="div")keep=QStringList{"align"};
                replacement="<"+name;
                for(const auto &key:keep)if(attributes.contains(key))replacement+=" "+key+"=\""+attributes.value(key).toHtmlEscaped()+"\"";
                replacement+=">";
                if(attributes.contains("id"))replacement="<a name=\""+attributes.value("id").toHtmlEscaped()+"\"></a>"+replacement;
            }
        }
        // Qt only indents blockquote; a native one-cell table provides its missing left rule.
        if(name=="blockquote")replacement=closing?QStringLiteral("</td></tr></table>"):QStringLiteral("<table cellspacing=\"0\" cellpadding=\"0\" width=\"100%\"><tr><td style=\"border-left:3px solid #808080;padding-left:12px;padding-top:2px;padding-bottom:2px\">");
        if(name=="pre")replacement=closing?QStringLiteral("</pre></td></tr></table>"):QStringLiteral("<table cellspacing=\"0\" cellpadding=\"0\" width=\"100%\"><tr><td style=\"padding-left:12px;padding-right:12px;padding-top:10px;padding-bottom:10px\"><pre>");
        if(name=="input"&&!closing&&htmlAttributes(token.captured(3)).value("type").compare("checkbox",Qt::CaseInsensitive)==0){
            const bool checked=QRegularExpression("(?:^|\\s)checked(?:\\s|=|$)",QRegularExpression::CaseInsensitiveOption).match(token.captured(3)).hasMatch();
            replacement=checked?QStringLiteral("☑ "):QStringLiteral("☐ ");
        }
        html.replace(token.capturedStart(),token.capturedLength(),replacement);
    }
    return html.trimmed();
}
ProjectReadme::ProjectReadme(QWidget *parent,QNetworkAccessManager *network,int timeoutMs)
    :QTextBrowser(parent),m_network(this),m_manager(network?network:&m_network),m_timeoutMs(qMax(1,timeoutMs)) {
    setOpenLinks(false);setOpenExternalLinks(false);setFrameShape(QFrame::NoFrame);document()->setDocumentMargin(14);
    document()->setDefaultStyleSheet("pre, code, kbd { font-family: '"+codeFont().family()+"'; }");
}
ProjectReadme::~ProjectReadme(){cancelImages();}
void ProjectReadme::cancelImages(){
    ++m_generation;const auto replies=m_replies;m_replies.clear();m_queue.clear();for(const auto &reply:replies)if(reply)reply->abort();
}
QImage ProjectReadme::placeholder(bool failed) const {
    QImage image(144,26,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);painter.setFont(font());painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(image.rect(),Qt::AlignVCenter|Qt::AlignLeft,failed?QCoreApplication::translate("M2CL", "[图片未能加载]"):QCoreApplication::translate("M2CL", "[图片加载中…]"));return image;
}
QVariant ProjectReadme::loadResource(int type,const QUrl &name) {
    if(type!=QTextDocument::ImageResource)return {};
    const auto key=imageUrl(m_base,name.toString()).toString();const auto it=m_images.constFind(key);
    return it!=m_images.cend()&&!it->pixels.isNull()?it->pixels:placeholder(it==m_images.cend()||it->failed);
}
int ProjectReadme::failedImages() const {int count=0;for(const auto &image:m_images)if(image.failed)++count;return count;}
int ProjectReadme::loadedImages() const {int count=0;for(const auto &image:m_images)if(!image.pixels.isNull())++count;return count;}
QString ProjectReadme::imageErrorDetails() const {QStringList errors;for(const auto &image:m_images)if(image.failed&&!image.error.isEmpty())errors.append(image.error);return errors.join(QStringLiteral("\n\n"));}
void ProjectReadme::notify(){emit imageStatusChanged(pendingImages(),failedImages());}
void ProjectReadme::setProjectMarkdown(const QString &markdown,const QUrl &base){
    if(m_markdown==markdown && m_base==base)return;
    cancelImages();m_images.clear();m_placements.clear();m_markdown=markdown;m_base=base;
    QHash<QString,QSizeF> declaredSizes;const QRegularExpression imageTags("<img\\b[^>]*>",QRegularExpression::CaseInsensitiveOption);auto tags=imageTags.globalMatch(markdown);
    while(tags.hasNext()){const auto attributes=htmlAttributes(tags.next().captured());declaredSizes.insert(imageUrl(base,attributes.value("src")).toString(),QSizeF(attributes.value("width").toDouble(),attributes.value("height").toDouble()));}
    document()->setBaseUrl(base);setHtml(descriptionHtml(markdown));
    for(auto block=document()->begin();block.isValid();block=block.next()){
        QTextCursor cursor(block);auto format=block.blockFormat();format.setLineHeight(135,QTextBlockFormat::ProportionalHeight);format.setBottomMargin(8);cursor.setBlockFormat(format);
        for(auto it=block.begin();!it.atEnd();++it){const auto fragment=it.fragment();if(!fragment.isValid()||!fragment.charFormat().isImageFormat())continue;
            auto format=fragment.charFormat().toImageFormat();auto url=imageUrl(base,format.name());const auto key=url.isEmpty()?QString("blocked:")+format.name():url.toString();
            const auto declared=declaredSizes.value(key);m_placements.append({fragment.position(),key,declared.width()>0?declared.width():format.width(),declared.height()>0?declared.height():format.height()});
            if(!m_images.contains(key)){
                Image image;image.url=url;if(m_images.size()>=64)image.url=QUrl{};image.failed=image.url.isEmpty()||(m_manager==&m_network&&(QCoreApplication::instance()->property("offlineUiTest").toBool()||QCoreApplication::arguments().contains("--smoke-test")));
                if(!image.failed){if(auto cached=imageCache.object(key))image.pixels=*cached;else m_queue.enqueue(key);}
                m_images.insert(key,image);
            }
        }
    }
    styleDocument();layoutImages();notify();QTimer::singleShot(0,this,&ProjectReadme::pump);
}
void ProjectReadme::layoutImages(){
    const int oldScroll=verticalScrollBar()->value();const auto top=cursorForPosition(QPoint(1,1));const int oldY=cursorRect(top).top();
    for(const auto &placement:m_placements){const auto image=m_images.value(placement.key);auto pixels=image.pixels;QSizeF size;
        qreal available=viewport()->width()-2*document()->documentMargin()-16;
        QTextCursor position(document());position.setPosition(placement.position);
        for(auto frame=position.currentFrame();frame;frame=frame->parentFrame())if(frame->frameFormat().boolProperty(QuoteFrame))available-=15;
        available=qMax(40.,available);
        if(pixels.isNull()){pixels=placeholder(image.failed);size=QSizeF(pixels.size());}
        else {
            size=QSizeF(pixels.size());
            if(placement.width>0){size.setWidth(placement.width);size.setHeight(placement.height>0?placement.height:pixels.height()*placement.width/pixels.width());}
            else if(placement.height>0){size.setHeight(placement.height);size.setWidth(pixels.width()*placement.height/pixels.height());}
        }
        if(size.width()>available)size.scale(available,size.height(),Qt::KeepAspectRatio);
        QTextCursor cursor(document());cursor.setPosition(placement.position);cursor.setPosition(placement.position+1,QTextCursor::KeepAnchor);auto format=cursor.charFormat().toImageFormat();format.setWidth(size.width());format.setHeight(size.height());cursor.setCharFormat(format);
        document()->addResource(QTextDocument::ImageResource,QUrl(format.name()),pixels);document()->addResource(QTextDocument::ImageResource,m_base.resolved(QUrl(format.name())),pixels);
    }
    document()->markContentsDirty(0,document()->characterCount());document()->documentLayout()->documentSize();
    if(oldScroll>0)verticalScrollBar()->setValue(oldScroll+cursorRect(top).top()-oldY);viewport()->update();
}
void ProjectReadme::resizeEvent(QResizeEvent *event){QTextBrowser::resizeEvent(event);layoutImages();}
void ProjectReadme::styleDocument(){
    m_ruleBlocks.clear();
    const auto foreground=palette().color(QPalette::Text),background=palette().color(QPalette::Base);
    const auto rule=blend(foreground,background,.25),muted=blend(foreground,background,.72),codeBackground=palette().color(QPalette::AlternateBase);
    std::function<void(QTextFrame *)> styleFrames=[&](QTextFrame *frame){
        for(auto child:frame->childFrames()){
            if(auto table=qobject_cast<QTextTable *>(child)){
                auto format=table->format();const auto first=table->cellAt(0,0).format().toTableCellFormat();
                const bool quote=format.boolProperty(QuoteFrame)||(table->rows()==1&&table->columns()==1&&first.leftBorder()==3&&first.rightBorder()==0);
                const bool code=format.boolProperty(CodeFrame)||(table->rows()==1&&table->columns()==1&&first.leftPadding()==12&&first.topPadding()==10&&first.bottomPadding()==10);
                format.setProperty(QuoteFrame,quote);format.setProperty(CodeFrame,code);format.setBorder(0);format.setCellSpacing(0);format.setCellPadding(quote||code?0:6);format.setWidth(QTextLength(QTextLength::PercentageLength,100));format.setBorderCollapse(true);table->setFormat(format);
                for(int row=0;row<table->rows();++row)for(int column=0;column<table->columns();++column){auto cell=table->cellAt(row,column);auto style=cell.format().toTableCellFormat();style.setBorder(quote||code?0:1);style.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);style.setBorderBrush(rule);
                    if(quote){style.setLeftBorder(3);style.setLeftPadding(12);style.setRightPadding(0);style.setTopPadding(2);style.setBottomPadding(2);}
                    else if(code){style.setBackground(codeBackground);style.setLeftPadding(12);style.setRightPadding(12);style.setTopPadding(10);style.setBottomPadding(10);}
                    else if(row<format.headerRowCount())style.setBackground(codeBackground);
                    cell.setFormat(style);
                }
            }
            styleFrames(child);
        }
    };styleFrames(document()->rootFrame());
    auto fixed=codeFont();fixed.setPixelSize(font().pixelSize()>0?font().pixelSize():14);
    for(auto block=document()->begin();block.isValid();block=block.next()){
        QTextCursor cursor(block);auto format=block.blockFormat();bool quote=false,codeContainer=false;
        if((block.text().startsWith(QChar(0x2610))||block.text().startsWith(QChar(0x2611)))&&block.textList()){
            const int indent=block.textList()->format().indent();block.textList()->remove(block);format.clearProperty(QTextFormat::ObjectIndex);format.setIndent(indent);cursor.setBlockFormat(format);
        }
        for(auto frame=cursor.currentFrame();frame;frame=frame->parentFrame()){if(frame->frameFormat().boolProperty(QuoteFrame))quote=true;if(frame->frameFormat().boolProperty(CodeFrame))codeContainer=true;}
        const bool code=codeContainer||format.boolProperty(CodeBlock)||format.nonBreakableLines();
        if(code){
            format.setProperty(CodeBlock,true);format.setNonBreakableLines(false);format.clearBackground();format.setLeftMargin(0);format.setRightMargin(0);format.setTopMargin(0);format.setBottomMargin(0);cursor.setBlockFormat(format);
        }
        else if(format.headingLevel()==1||format.headingLevel()==2){format.setBottomMargin(16);cursor.setBlockFormat(format);}
        if(format.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)){
            format.setProperty(RuleWidth,format.property(QTextFormat::BlockTrailingHorizontalRulerWidth));format.clearProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);format.setTopMargin(6);format.setBottomMargin(6);cursor.setBlockFormat(format);
        }
        if(format.headingLevel()==1||format.headingLevel()==2||format.hasProperty(RuleWidth))m_ruleBlocks.append(block.position());
        struct Span {int position,length;QTextCharFormat format;};QList<Span> spans;
        for(auto it=block.begin();!it.atEnd();++it){const auto fragment=it.fragment();if(fragment.isValid()&&!fragment.charFormat().isImageFormat())spans.append({fragment.position(),fragment.length(),fragment.charFormat()});}
        for(const auto &span:spans){
            auto character=span.format;const bool inlineCode=character.boolProperty(InlineCode)||character.font().family().compare("monospace",Qt::CaseInsensitive)==0||character.font().family()==fixed.family();
            if(code||inlineCode){
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
                character.setFontFamilies({fixed.family()});
#else
                character.setFontFamily(fixed.family());
#endif
                character.setFontFixedPitch(true);character.setProperty(InlineCode,true);if(!code)character.setBackground(codeBackground);
            }
            if(quote&&!character.isAnchor())character.setForeground(muted);
            if(character.isAnchor()&&!character.anchorHref().isEmpty())character.setForeground(palette().color(QPalette::Link));
            QTextCursor selection(document());selection.setPosition(span.position);selection.setPosition(span.position+span.length,QTextCursor::KeepAnchor);selection.setCharFormat(character);
        }
    }
    viewport()->update();
}
void ProjectReadme::paintEvent(QPaintEvent *event){
    QTextBrowser::paintEvent(event);QPainter painter(viewport());painter.setClipRect(event->rect());
    painter.setPen(QPen(blend(palette().color(QPalette::Text),palette().color(QPalette::Base),.25),1));
    const auto offset=QPointF(-horizontalScrollBar()->value(),-verticalScrollBar()->value());
    for(const auto position:m_ruleBlocks){const auto block=document()->findBlock(position);if(!block.isValid())continue;
        const auto format=block.blockFormat();const bool heading=format.headingLevel()==1||format.headingLevel()==2,horizontalRule=format.hasProperty(RuleWidth);if(!heading&&!horizontalRule)continue;
        const auto bounds=document()->documentLayout()->blockBoundingRect(block).translated(offset);const qreal y=heading?bounds.bottom()+6:bounds.center().y();qreal width=bounds.width();
        if(horizontalRule){const auto length=format.property(RuleWidth).value<QTextLength>();if(length.type()==QTextLength::FixedLength)width=qMin(width,length.rawValue());else if(length.type()==QTextLength::PercentageLength)width*=qBound(0.,length.rawValue()/100.,1.);}
        const auto left=bounds.center().x()-width/2.;if(y>=event->rect().top()&&y<=event->rect().bottom())painter.drawLine(QPointF(left,y),QPointF(left+width,y));
    }
}
void ProjectReadme::changeEvent(QEvent *event){
    QTextBrowser::changeEvent(event);if(event->type()==QEvent::PaletteChange||event->type()==QEvent::FontChange)styleDocument();
}
void ProjectReadme::retryImages(){
    for(auto it=m_images.begin();it!=m_images.end();++it)if(it->failed&&!it->url.isEmpty()){it->failed=false;it->error.clear();it->redirects=0;it->startedMs=0;m_queue.enqueue(it.key());}
    layoutImages();notify();pump();
}
void ProjectReadme::pump(){
    while(m_replies.size()<4 && !m_queue.isEmpty()){
        const auto key=m_queue.dequeue();const auto generation=m_generation;const auto url=m_images.value(key).url;
        auto &image=m_images[key];if(!image.startedMs)image.startedMs=QDateTime::currentMSecsSinceEpoch();const auto remaining=m_timeoutMs-(QDateTime::currentMSecsSinceEpoch()-image.startedMs);
        if(remaining<=0){image.failed=true;image.error=QCoreApplication::translate("M2CL", "图片连接超时（Timeout）");layoutImages();continue;}
        QNetworkRequest request(url);request.setRawHeader("User-Agent","m2cl/0.1.0 (project description images)");request.setRawHeader("Accept","image/*");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setTransferTimeout(0);
        auto reply=m_manager->get(request);m_replies.insert(key,reply);auto bytes=std::make_shared<QByteArray>();
        QTimer::singleShot(int(remaining),reply,[reply]{if(!reply->isFinished()){reply->setProperty("networkTimedOut",true);reply->abort();}});
        connect(reply,&QNetworkReply::readyRead,this,[reply,bytes]{if(reply->bytesAvailable()>MaxBytes-bytes->size()){reply->abort();return;}bytes->append(reply->readAll());});
        connect(reply,&QNetworkReply::finished,this,[this,reply,key,generation,bytes]{
            reply->deleteLater();if(generation!=m_generation)return;m_replies.remove(key);auto &image=m_images[key];
            const auto redirect=reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            if(reply->error()==QNetworkReply::NoError&&!redirect.isEmpty()&&image.redirects<3){
                const auto target=imageUrl(reply->url(),redirect.toString());if(!target.isEmpty()){image.url=target;++image.redirects;m_queue.enqueue(key);pump();return;}
            }
            if(networkReplyError(reply).isEmpty()&&reply->bytesAvailable()<=MaxBytes-bytes->size()){
                bytes->append(reply->readAll());image.pixels=decodeImage(*bytes);
                if(!image.pixels.isNull())imageCache.insert(key,new QImage(image.pixels),qMax(1,int(image.pixels.sizeInBytes()/1024)));
            }
            image.failed=image.pixels.isNull();image.error=networkReplyError(reply);if(image.failed&&image.error.isEmpty())image.error=QCoreApplication::translate("M2CL", "图片格式无法读取或跳转无效\n请求：")+reply->url().adjusted(QUrl::RemoveUserInfo|QUrl::RemoveQuery|QUrl::RemoveFragment).toDisplayString();layoutImages();notify();pump();
        });
    }
    notify();
}
}
