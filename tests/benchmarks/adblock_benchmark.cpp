#include "../../src/adblock/AdBlockEngine.hpp"
#include "../../src/adblock/FilterCompiler.hpp"
#include "../../src/network/RequestClassifier.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QUrl>

#include <algorithm>
#include <iostream>
#include <vector>

using namespace virgin::adblock;
using namespace virgin::network;

namespace {

QString realisticSyntheticRules(int count) {
    QString raw;
    raw.reserve(count * 45);
    for (int i = 0; i < count; ++i) {
        const QString id = QStringLiteral("%1").arg(i, 6, 10, QLatin1Char('0'));
        switch (i % 4) {
        case 0: raw += "||ad" + id + ".bench.example^$third-party\n"; break;
        case 1: raw += "/ads/generated-" + id + ".js$script\n"; break;
        case 2: raw += "||media" + id + ".bench.example/tracker/*/pixel^$image,third-party\n"; break;
        default: raw += "tracker-" + id + "$third-party\n"; break;
        }
    }
    return raw;
}

bool readLists(const QStringList& paths, QString* rules) {
    for (const QString& path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            std::cerr << "Cannot read filter list: " << path.toStdString() << '\n';
            return false;
        }
        rules->append(QString::fromUtf8(file.readAll()));
        rules->append('\n');
    }
    return true;
}

std::vector<RequestContext> benchmarkRequests() {
    std::vector<RequestContext> requests;
    requests.reserve(2048);
    const QUrl firstParty("https://daily.example/home");
    for (int i = 0; i < 2048; ++i) {
        const int rule = (i * 37) % 100000;
        const QString id = QStringLiteral("%1").arg(rule, 6, 10, QLatin1Char('0'));
        QUrl url;
        ResourceType type = ResourceType::Script;
        switch (rule % 4) {
        case 0: url = QUrl("https://ad" + id + ".bench.example/banner"); break;
        case 1: url = QUrl("https://cdn.example/ads/generated-" + id + ".js"); break;
        case 2:
            url = QUrl("https://media" + id + ".bench.example/tracker/a/pixel.gif");
            type = ResourceType::Image;
            break;
        default: url = QUrl("https://metrics.example/tracker-" + id); break;
        }
        if ((i % 5) == 0) url = QUrl("https://daily.example/content/" + QString::number(i));
        RequestContext context;
        context.requestUrl = url;
        context.firstPartyUrl = firstParty;
        context.resourceType = type;
        context.thirdParty = RequestClassifier::isThirdParty(url, firstParty);
        requests.push_back(std::move(context));
    }
    return requests;
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QString rules;
    const QStringList listPaths = app.arguments().mid(1);
    const bool realLists = !listPaths.isEmpty();
    if (realLists) {
        if (!readLists(listPaths, &rules)) return 2;
    } else {
        rules = realisticSyntheticRules(100000);
    }

    QElapsedTimer compileTimer;
    compileTimer.start();
    auto compiled = FilterCompiler::compile(rules);
    const double compileMs = static_cast<double>(compileTimer.nsecsElapsed()) / 1'000'000.0;
    AdBlockEngine engine;
    engine.updateRules(compiled);
    const auto stats = engine.compiledStats();
    auto requests = benchmarkRequests();

    for (int pass = 0; pass < 20; ++pass) {
        for (const auto& request : requests) (void)engine.shouldBlock(request);
    }

    constexpr int operations = 50000;
    std::vector<qint64> latencies;
    latencies.reserve(operations);
    for (int i = 0; i < operations; ++i) {
        QElapsedTimer timer;
        timer.start();
        volatile auto result = engine.shouldBlock(requests[static_cast<size_t>(i) % requests.size()]);
        Q_UNUSED(result)
        latencies.push_back(timer.nsecsElapsed());
    }
    std::sort(latencies.begin(), latencies.end());
    const auto percentileUs = [&latencies](size_t percentile) {
        const size_t index = (latencies.size() - 1U) * percentile / 100U;
        return static_cast<double>(latencies[index]) / 1000.0;
    };

    const double p50 = percentileUs(50);
    const double p95 = percentileUs(95);
    const double p99 = percentileUs(99);
    std::cout << (realLists ? "Real filter lists" : "100k unique EasyList-like rules") << '\n'
              << "rules: " << stats.blockRules << " block + " << stats.allowRules << " allow\n"
              << "compile: " << compileMs << " ms\n"
              << "blocker latency: p50 " << p50 << " us, p95 " << p95
              << " us, p99 " << p99 << " us (" << operations << " requests)\n";

    const bool withinBudget = p50 < 10.0 && p95 < 50.0 && p99 < 150.0;
    std::cout << "budget p50<10us p95<50us p99<150us: "
              << (withinBudget ? "PASS" : "FAIL") << '\n';
    return withinBudget ? 0 : 1;
}
