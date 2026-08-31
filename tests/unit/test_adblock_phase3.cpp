#include "../../src/adblock/AdBlockEngine.hpp"
#include "../../src/adblock/FilterParser.hpp"
#include "../../src/adblock/FilterCompiler.hpp"
#include "../../src/network/RequestClassifier.hpp"
#include <QUrl>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

using namespace virgin::adblock;
using namespace virgin::network;

static RequestContext ctxFor(const QString& url, const QString& firstParty, const QString& resource) {
    QUrl u(url);
    QUrl fp(firstParty);
    // resource string like "script", "image"
    auto rt = RequestClassifier::classifyResourceType(resource);
    bool third = RequestClassifier::isThirdParty(u, fp);
    RequestContext c;
    c.requestUrl = u;
    c.firstPartyUrl = fp;
    c.resourceType = rt;
    c.thirdParty = third;
    c.topLevel = false;
    return c;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // Build the engine with representative network filters.
    QString raw = R"(
||doubleclick.net^
||googlesyndication.com^
||facebook.net^$third-party
||ads.example^$image,script
||tracker.example^
||domainonly.example^$domain=example.com
||negated.example^$~image
||cdn.example/ads/*^
||important.example^$important
@@||important.example^
||unsupported.example^$script,redirect=noop
*_ad_*$media,domain=youtube.com,third-party
@@||allow.example^$document
@@||tracker.example^$domain=allow.example
)";

    auto compiled = FilterCompiler::compile(raw);

    AdBlockEngine engine;
    engine.updateRules(compiled);

    struct TestCase {
        QString name;
        QString url;
        QString firstParty;
        QString resource;
        bool expectBlocked;
    };

    QVector<TestCase> tests = {
        {"known-ad -> block", "https://doubleclick.net/ads?id=1", "https://example.com", "script", true},
        {"normal CDN -> allow", "https://cdn.example/js/app.js", "https://example.com", "script", false},
        {"exception -> allow", "https://allow.example/page", "https://allow.example", "document", false},
        {"third-party constrained -> block third", "https://facebook.net/pixel", "https://example.com", "image", true},
        {"third-party constrained -> allow first", "https://facebook.net/pixel", "https://facebook.net/page", "image", false},
        {"resource-type constrained -> block image", "https://ads.example/banner.jpg", "https://example.com", "image", true},
        {"resource-type constrained -> block script", "https://ads.example/app.js", "https://example.com", "script", true},
        {"resource-type constrained -> allow document", "https://ads.example/page", "https://example.com", "document", false},
        {"domain-specific -> block when domain matches", "https://domainonly.example/resource", "https://example.com", "script", true},
        {"domain-specific -> allow when domain not matches", "https://domainonly.example/resource", "https://other.com", "script", false},
        {"tracker generic still blocks other.com", "https://tracker.example/collect", "https://other.com", "script", true},
        {"exception domain overrides", "https://tracker.example/collect", "https://allow.example", "script", false},
        {"negated type blocks script", "https://negated.example/app.js", "https://example.com", "script", true},
        {"negated type allows image", "https://negated.example/image.png", "https://example.com", "image", false},
        {"suffix boundary rejects lookalike host", "https://notdoubleclick.net/ads", "https://example.com", "script", false},
        {"host path wildcard blocks matching path", "https://cdn.example/ads/banner.js?x=1", "https://example.com", "script", true},
        {"host path wildcard preserves nonmatching path", "https://cdn.example/assets/banner.js", "https://example.com", "script", false},
        {"important overrides exception", "https://important.example/a.js", "https://example.com", "script", true},
        {"unsupported modifier is skipped", "https://unsupported.example/a.js", "https://example.com", "script", false},
        {"YouTube identifiable ad media blocked", "https://rr1.googlevideo.com/videoplayback?id=clip_ad_123", "https://www.youtube.com/watch?v=abc", "media", true},
        {"YouTube normal media preserved", "https://rr1.googlevideo.com/videoplayback?id=clip_123", "https://www.youtube.com/watch?v=abc", "media", false},
        {"YouTube rule remains site scoped", "https://rr1.googlevideo.com/videoplayback?id=clip_ad_123", "https://example.com", "media", false},
    };

    int passed = 0, failed = 0;
    for (auto& tc : tests) {
        auto ctx = ctxFor(tc.url, tc.firstParty, tc.resource);
        auto res = engine.shouldBlock(ctx);
        bool blocked = res.blocked;
        bool ok = (blocked == tc.expectBlocked);
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << tc.name.toStdString()
                  << " | url=" << tc.url.toStdString()
                  << " first=" << tc.firstParty.toStdString()
                  << " res=" << tc.resource.toStdString()
                  << " -> blocked=" << blocked << " expected=" << tc.expectBlocked
                  << " filter=" << res.filter.toStdString() << "\n";
        if (ok) passed++; else failed++;
    }

    if (RequestClassifier::isThirdParty(QUrl("https://cdn.example.co.uk/a"),
                                        QUrl("https://www.example.co.uk/"))) {
        std::cerr << "[FAIL] multi-label public suffix first-party classification\n";
        ++failed;
    } else {
        ++passed;
    }
    if (!RequestClassifier::isThirdParty(QUrl("https://evil.co.uk/a"),
                                         QUrl("https://www.example.co.uk/"))) {
        std::cerr << "[FAIL] multi-label public suffix third-party classification\n";
        ++failed;
    } else {
        ++passed;
    }

    QTemporaryDir cacheDirectory;
    const QString cachePath = cacheDirectory.filePath("rules.bin");
    if (!FilterCompiler::writeCache(*compiled, cachePath) ||
        !FilterCompiler::readCache(cachePath)) {
        std::cerr << "[FAIL] compiled cache round trip\n";
        ++failed;
    } else {
        ++passed;
    }
    QFile corrupt(cachePath);
    if (corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        corrupt.write("not-a-filter-cache");
        corrupt.close();
    }
    if (FilterCompiler::readCache(cachePath)) {
        std::cerr << "[FAIL] corrupt cache accepted\n";
        ++failed;
    } else {
        ++passed;
    }

    const auto sharedStore = std::make_shared<RuleStore>();
    AdBlockEngine first(sharedStore, nullptr);
    AdBlockEngine second(sharedStore, nullptr);
    first.updateRules(FilterCompiler::compile("||shared.example^\n"));
    if (!second.shouldBlock(ctxFor("https://shared.example/a", "https://site.example", "script")).blocked) {
        std::cerr << "[FAIL] shared rules did not propagate across profiles\n";
        ++failed;
    } else {
        ++passed;
    }

    const auto siteStats = engine.siteStatsForHost("example.com");
    if (siteStats.requests == 0 || siteStats.blocked == 0 ||
        siteStats.blocked != siteStats.trackers + siteStats.ads) {
        std::cerr << "[FAIL] per-site request statistics are inconsistent\n";
        ++failed;
    } else {
        ++passed;
    }

    std::cout << "\nAdBlock functional tests: " << passed << " passed, " << failed << " failed\n";
    std::cout << "AdBlockEngine stats: total=" << engine.totalRequests() << " blocked=" << engine.totalBlocked()
              << " trackers=" << engine.trackersBlocked() << " ads=" << engine.adsBlocked() << "\n";

    return failed == 0 ? 0 : 1;
}
