// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "network/networkerror.h"
#include "network/resourceclient.h"
#include <QFile>
#include <QJsonDocument>
#include <QHash>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <cstring>

class FakeReply final : public QNetworkReply {
public:
    FakeReply(const QNetworkRequest &request, QByteArray body, int delay, QObject *parent, int status=200)
        : QNetworkReply(parent), m_body(std::move(body)) {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute,status);setRequest(request); setUrl(request.url()); setOperation(QNetworkAccessManager::GetOperation);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(delay, this, [this] {
            if (isFinished()) return;
            m_available = true; emit readyRead();
            if (isFinished()) return;
            setFinished(true); emit finished();
        });
    }
    void abort() override {
        if (isFinished()) return;
        setError(QNetworkReply::OperationCanceledError, "Cancelled"); setFinished(true); emit finished();
    }
    qint64 bytesAvailable() const override { return (m_available ? m_body.size() - m_position : 0) + QIODevice::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 maximum) override {
        if (!m_available || m_position == m_body.size()) return -1;
        const auto amount = qMin(maximum, static_cast<qint64>(m_body.size() - m_position));
        std::memcpy(data, m_body.constData() + m_position, static_cast<size_t>(amount)); m_position += amount;
        return amount;
    }
private:
    QByteArray m_body;
    qint64 m_position = 0;
    bool m_available = false;
};
class FakeNetwork final : public QNetworkAccessManager {
public:
    QByteArray body;
    QHash<QString,QByteArray> routeBodies;
    QHash<QString,int> routeDelays,routeStatuses;
    int delay = 0;
    int requests = 0;
    QNetworkRequest lastRequest;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        ++requests; lastRequest = request;
        return new FakeReply(request, routeBodies.value(request.url().path(),body), routeDelays.value(request.url().path(),delay), this,routeStatuses.value(request.url().path(),200));
    }
};
class NetworkTests : public QObject {
    Q_OBJECT
    static mc::ResourceFile descriptor(const QByteArray &body, bool sha512 = true) {
        mc::ResourceFile file; file.fileName = "test.jar"; file.url = QUrl("https://cdn.modrinth.com/test.jar"); file.size = body.size();
        const auto hash = QString::fromLatin1(QCryptographicHash::hash(body, sha512 ? QCryptographicHash::Sha512 : QCryptographicHash::Sha1).toHex());
        if (sha512) file.sha512 = hash; else file.sha1 = hash;
        return file;
    }
private slots:
    void installedProjectIconsAreBatchedAndRestrictedToRequestedProjects(){
        FakeNetwork network;network.body=R"([{"id":"known","icon_url":"https://cdn.modrinth.com/icon.png"},{"id":"unrelated","icon_url":"https://cdn.modrinth.com/other.png"}])";
        mc::ResourceClient client(nullptr,&network);QSignalSpy ready(&client,&mc::ResourceClient::projectIconReady);
        client.projectIcons({"known","known",""});QTRY_COMPARE(ready.size(),1);QCOMPARE(network.requests,1);QCOMPARE(ready.first().first().toString(),QString("known"));
        const int count=network.requests;client.icon(QUrl("https://example.com/icon.png"));QCOMPARE(network.requests,count);
        network.body="not json";client.projectIcons({"known"});QTest::qWait(30);QCOMPARE(ready.size(),1);
    }
    void malformedApiResponseReportsFailure() {
        FakeNetwork network; network.body = "{not json";
        mc::ResourceClient client(nullptr, &network); QSignalSpy failed(&client, &mc::ResourceClient::failed);
        client.search("test", "1.20.1", "fabric"); QTRY_COMPARE(failed.size(), 1);
    }
    void latestSearchWins() {
        qRegisterMetaType<QList<mc::ResourceProject>>();
        FakeNetwork network; network.body = "{\"hits\":[{\"project_id\":\"old\",\"title\":\"Old\"}]}"; network.delay = 80;
        mc::ResourceClient client(nullptr, &network); QSignalSpy ready(&client, &mc::ResourceClient::projectsReady);
        client.search("old", "1.20.1", "fabric");
        network.body = "{\"hits\":[{\"project_id\":\"new\",\"title\":\"New\"}]}"; network.delay = 0;
        client.search("new", "1.20.1", "fabric"); QTRY_COMPARE(ready.size(), 1);
        QCOMPARE(qvariant_cast<QList<mc::ResourceProject>>(ready.first().first()).first().id, QString("new"));
        QTest::qWait(100); QCOMPARE(ready.size(), 1);
    }
    void httpStatusCodesRemainDistinctFromConnectionTimeouts() {
        FakeNetwork network;network.body="Denied";network.routeStatuses.insert("/v2/search",403);mc::ResourceClient client(nullptr,&network,20);QSignalSpy failure(&client,&mc::ResourceClient::searchFailed);
        client.search("private-search-text",{},{});QTRY_COMPARE(failure.size(),1);const auto forbidden=failure.first().first().toString();QVERIFY(forbidden.contains("HTTP 403"));QVERIFY(forbidden.contains("api.modrinth.com/v2/search"));QVERIFY(!forbidden.contains("private-search-text"));QVERIFY(mc::networkErrorSummary(forbidden).contains(QStringLiteral("访问被拒绝")));
        network.routeStatuses["/v2/search"]=503;client.search({}, {}, {});QTRY_COMPARE(failure.size(),2);QVERIFY(mc::networkErrorSummary(failure.last().first().toString()).contains("HTTP 503"));
        network.routeStatuses.clear();network.delay=60000;client.search({}, {}, {});QTRY_COMPARE(failure.size(),3);const auto timeout=failure.last().first().toString();QVERIFY(timeout.contains("Timeout"));QVERIFY(!timeout.contains("HTTP"));
        QTemporaryDir dir;network.delay=0;network.body="test bytes";network.routeStatuses["/test.jar"]=503;QSignalSpy download(&client,&mc::ResourceClient::downloadFailed);client.download(descriptor(network.body),dir.path());QTRY_COMPARE(download.size(),1);QVERIFY(download.first().first().toString().contains("HTTP 503"));QVERIFY(!QFile::exists(dir.filePath("test.jar")));
    }
    void failedRequestsAreScopedAndDoNotRetryAutomatically() {
        FakeNetwork network;network.delay=60000;mc::ResourceClient client(nullptr,&network,25);
        QSignalSpy searches(&client,&mc::ResourceClient::searchFailed),versions(&client,&mc::ResourceClient::versionsFailed),downloads(&client,&mc::ResourceClient::downloadFailed),projects(&client,&mc::ResourceClient::projectFailed);
        client.search({}, {}, {});client.versions("sodium",{},{});client.project("sodium");
        QTRY_COMPARE(searches.size(),1);QTRY_COMPARE(versions.size(),1);QTRY_COMPARE(projects.size(),1);QCOMPARE(downloads.size(),0);QCOMPARE(network.requests,3);
        QTest::qWait(75);QCOMPARE(network.requests,3);QCOMPARE(searches.size(),1);QCOMPARE(versions.size(),1);
        QTemporaryDir dir;client.download({},dir.path());QCOMPARE(downloads.size(),1);QCOMPARE(searches.size(),1);QCOMPARE(versions.size(),1);
        // A superseded timed-out search cannot overwrite a successful replacement.
        QSignalSpy ready(&client,&mc::ResourceClient::projectsReady);client.search("old",{},{});network.delay=0;network.body=R"({"hits":[]})";client.search("new",{},{});QTRY_COMPARE(ready.size(),1);QTest::qWait(50);QCOMPARE(searches.size(),1);
    }
    void tagsAreFilteredCachedAndRetryable() {
        qRegisterMetaType<mc::ResourceTags>(); FakeNetwork network;
        network.routeBodies.insert("/v2/tag/category",R"([{"name":"optimization","project_type":"mod"},{"name":"shadows","project_type":"shader"}])");
        network.routeBodies.insert("/v2/tag/loader",R"([{"name":"fabric","supported_project_types":["mod"]},{"name":"iris","supported_project_types":["shader"]}])");
        network.routeBodies.insert("/v2/tag/game_version",R"([{"version":"1.20.1","version_type":"release"},{"version":"24w14a","version_type":"snapshot"}])");
        mc::ResourceClient client(nullptr,&network);QSignalSpy ready(&client,&mc::ResourceClient::tagsReady),failed(&client,&mc::ResourceClient::tagsFailed);
        client.tags();QTRY_COMPARE(ready.size(),1);QCOMPARE(network.requests,3); auto tags=qvariant_cast<mc::ResourceTags>(ready.first().first());
        QCOMPARE(tags.categories.size(),1);QCOMPARE(tags.loaders.size(),1);QCOMPARE(tags.gameVersions.size(),2);
        QCOMPARE(tags.gameVersionTypes.value("1.20.1"),QString("release"));QCOMPARE(tags.gameVersionTypes.value("24w14a"),QString("snapshot"));
        client.tags();QCOMPARE(ready.size(),2);QCOMPARE(network.requests,3);
        network.routeBodies.insert("/v2/tag/category","bad json");client.tags(true);QTRY_COMPARE(failed.size(),1);QCOMPARE(ready.size(),3);QVERIFY(failed.first().first().toString().contains(QStringLiteral("分类")));
        network.routeBodies.insert("/v2/tag/category","[]");client.tags();QTRY_COMPARE(ready.size(),4);
    }
    void metadataDeadlineStopsSpinnersAndKeepsSuccessfulFilters() {
        qRegisterMetaType<mc::ResourceTags>();FakeNetwork network;
        network.routeBodies.insert("/v2/tag/category","[]");network.routeDelays.insert("/v2/tag/category",60000);
        network.routeBodies.insert("/v2/tag/loader",R"([{"name":"fabric","supported_project_types":["mod"]}])");
        network.routeBodies.insert("/v2/tag/game_version",R"([{"version":"1.20.1","version_type":"release"}])");
        mc::ResourceClient client(nullptr,&network,30);QSignalSpy ready(&client,&mc::ResourceClient::tagsReady),failed(&client,&mc::ResourceClient::tagsFailed),searchFailed(&client,&mc::ResourceClient::failed);
        client.tags();QTRY_COMPARE(failed.size(),1);QCOMPARE(ready.size(),1);const auto tags=qvariant_cast<mc::ResourceTags>(ready.first().first());
        QCOMPARE(tags.gameVersions,QStringList{"1.20.1"});QCOMPARE(tags.loaders.size(),1);QVERIFY(failed.first().first().toString().contains(QStringLiteral("超时")));
        network.routeDelays.clear();network.delay=60000;client.search({}, {}, {});QTRY_COMPARE(searchFailed.size(),1);QVERIFY(searchFailed.first().first().toString().contains(QStringLiteral("超时")));
        network.delay=0;network.body=R"({"hits":[]})";QSignalSpy projects(&client,&mc::ResourceClient::projectsReady);client.search({}, {}, {});QTRY_COMPARE(projects.size(),1);
        client.tags(true);QTRY_COMPARE(ready.size(),2);QCOMPARE(failed.size(),1);
    }
    void versionResponseMustMatchBothRequestedFields() {
        FakeNetwork network;
        const QJsonObject jar{{"filename","test.jar"},{"url","https://cdn.modrinth.com/test.jar"},{"size",20},{"hashes",QJsonObject{{"sha512","aabb"}}}};
        QJsonArray versions;
        for(const auto &pair:QList<QPair<QString,QString>>{{"1.20.1","forge"},{"1.21.1","fabric"},{"1.20.1","fabric"}})
            versions.append(QJsonObject{{"id",pair.first+pair.second},{"files",QJsonArray{jar}},{"game_versions",QJsonArray{pair.first}},{"loaders",QJsonArray{pair.second}}});
        network.body=QJsonDocument(versions).toJson();mc::ResourceClient client(nullptr,&network);QSignalSpy ready(&client,&mc::ResourceClient::versionsReady);
        client.versions("test","1.20.1","fabric");QTRY_COMPARE(ready.size(),1);
        const auto files=qvariant_cast<QList<mc::ResourceFile>>(ready.first().first());QCOMPARE(files.size(),1);QCOMPARE(files.first().versionId,QString("1.20.1fabric"));
    }
    void changedFiltersInvalidateInflightResponses() {
        FakeNetwork network;network.body=R"({"hits":[{"project_id":"old"}]})";network.delay=50;
        mc::ResourceClient client(nullptr,&network);QSignalSpy ready(&client,&mc::ResourceClient::projectsReady);client.search("test","1.20.1","forge");client.invalidateSearch();QTest::qWait(100);QCOMPARE(ready.size(),0);
    }
    void projectIconsAreCachedAndBounded() {
        FakeNetwork network;network.body="small image bytes";mc::ResourceClient client(nullptr,&network);QSignalSpy ready(&client,&mc::ResourceClient::iconReady);
        client.icon(QUrl("http://cdn.modrinth.com/icon.png"));client.icon(QUrl("https://other.example/icon.png"));QCOMPARE(network.requests,0);
        const QUrl url("https://cdn.modrinth.com/icon.png");client.icon(url);client.icon(url);QTRY_COMPARE(ready.size(),1);QCOMPARE(network.requests,1);
        client.icon(url);QCOMPARE(ready.size(),2);QCOMPARE(network.requests,1);
        network.body=QByteArray(2*1024*1024+1,'x');client.icon(QUrl("https://cdn.modrinth.com/huge.png"));QTest::qWait(50);QCOMPARE(ready.size(),2);
    }
    void closedVersionDetailsDiscardLateReply() {
        FakeNetwork network;network.body="[]";network.delay=50;mc::ResourceClient client(nullptr,&network);QSignalSpy ready(&client,&mc::ResourceClient::versionsReady);client.versions("example","1.20.1","fabric");client.invalidateVersions();QTest::qWait(80);QCOMPARE(ready.size(),0);
    }
    void projectDescriptionUsesOfficialBodyAndDiscardsStalePages() {
        qRegisterMetaType<mc::ResourceProject>();
        FakeNetwork network;network.body=R"({"id":"first","title":"First","body":"# Full introduction","description":"Short","game_versions":["1.20.1"],"loaders":["fabric"]})";
        mc::ResourceClient client(nullptr,&network);QSignalSpy ready(&client,&mc::ResourceClient::projectReady);
        client.project("first");QTRY_COMPARE(ready.size(),1);
        const auto project=qvariant_cast<mc::ResourceProject>(ready.first().first());QCOMPARE(project.body,QString("# Full introduction"));QCOMPARE(project.gameVersions,QStringList{"1.20.1"});
        network.delay=50;client.project("first");client.invalidateVersions();QTest::qWait(80);QCOMPARE(ready.size(),1);
        network.body=R"({"id":"old","title":"Old","body":"Wrong page"})";client.project("old");
        network.body=R"({"id":"new","title":"New","body":"New page"})";network.delay=0;client.project("new");QTRY_COMPARE(ready.size(),2);QTest::qWait(80);
        QCOMPARE(ready.size(),2);QCOMPARE(qvariant_cast<mc::ResourceProject>(ready.last().first()).id,QString("new"));
    }
    void verifiedDownload_data() { QTest::addColumn<bool>("sha512"); QTest::newRow("sha512") << true; QTest::newRow("sha1") << false; }
    void verifiedDownload() {
        QFETCH(bool, sha512); QTemporaryDir dir; FakeNetwork network; network.body = "verified jar content";
        mc::ResourceClient client(nullptr, &network); QSignalSpy done(&client, &mc::ResourceClient::downloadFinished); QSignalSpy error(&client, &mc::ResourceClient::failed);
        client.download(descriptor(network.body, sha512), dir.path()); QTRY_COMPARE(done.size(), 1); QCOMPARE(error.size(), 0);
        QFile output(dir.filePath("test.jar")); QVERIFY(output.open(QIODevice::ReadOnly)); QCOMPARE(output.readAll(), network.body);
        QVERIFY(!client.downloading()); QVERIFY(!network.lastRequest.rawHeader("User-Agent").isEmpty());
    }
    void badHashPreservesExistingFile() {
        QTemporaryDir dir; FakeNetwork network; network.body = "corrupt download";
        QFile old(dir.filePath("test.jar")); QVERIFY(old.open(QIODevice::WriteOnly)); old.write("existing mod"); old.close();
        mc::ResourceClient client(nullptr, &network); QSignalSpy failed(&client, &mc::ResourceClient::failed);
        auto file = descriptor(network.body); file.sha512 = QString(128, '0'); client.download(file, dir.path());
        QTRY_COMPARE(failed.size(), 1); QVERIFY(old.open(QIODevice::ReadOnly)); QCOMPARE(old.readAll(), QByteArray("existing mod"));
    }
    void sizeMismatchDoesNotInstall() {
        QTemporaryDir dir; FakeNetwork network; network.body = "truncated";
        mc::ResourceClient client(nullptr, &network); QSignalSpy failed(&client, &mc::ResourceClient::failed);
        auto file = descriptor(network.body); ++file.size; client.download(file, dir.path());
        QTRY_COMPARE(failed.size(), 1); QVERIFY(!QFile::exists(dir.filePath("test.jar")));
    }
    void oversizedDownloadDoesNotInstall() {
        QTemporaryDir dir; FakeNetwork network; network.body = "unexpected extra content";
        mc::ResourceClient client(nullptr, &network); QSignalSpy failed(&client, &mc::ResourceClient::failed);
        auto file = descriptor(network.body); file.size = 1; client.download(file, dir.path());
        QTRY_COMPARE(failed.size(), 1); QVERIFY(!QFile::exists(dir.filePath("test.jar")));
    }
    void cancelledDownloadLeavesNoFile() {
        QTemporaryDir dir; FakeNetwork network; network.body = "pending content"; network.delay = 100;
        mc::ResourceClient client(nullptr, &network); QSignalSpy failed(&client, &mc::ResourceClient::failed);
        client.download(descriptor(network.body), dir.path()); QVERIFY(client.downloading()); client.cancelDownload();
        QTRY_COMPARE(failed.size(), 1); QVERIFY(!client.downloading()); QVERIFY(!QFile::exists(dir.filePath("test.jar")));
    }
    void invalidDestinationNameIsRejectedBeforeRequest() {
        QTemporaryDir dir; FakeNetwork network; network.body = "jar";
        mc::ResourceClient client(nullptr, &network); QSignalSpy failed(&client, &mc::ResourceClient::failed);
        auto file = descriptor(network.body); file.fileName = "../escape.jar"; client.download(file, dir.path());
        QCOMPARE(failed.size(), 1); QCOMPARE(network.requests, 0); QVERIFY(!client.downloading());
    }
};
QTEST_GUILESS_MAIN(NetworkTests)
#include "network_tests.moc"
