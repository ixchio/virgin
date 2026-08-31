#include "RequestInterceptor.hpp"
#include "RequestClassifier.hpp"
#include "adblock/AdBlockEngine.hpp"

namespace virgin::network {

RequestInterceptor::RequestInterceptor(virgin::adblock::AdBlockEngine* engine, QObject* parent)
    : QWebEngineUrlRequestInterceptor(parent), engine_(engine) {}

void RequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info) {
    // Sec 42: hot path — no disk I/O, no network I/O, no DB writes, no expensive logging (INV-09)

    QUrl url = info.requestUrl();
    QString scheme = url.scheme().toLower();

    // Stage 0 bust: internal schemes bypass (Sec 14 Stage 0)
    if (scheme == "virgin" || scheme == "data" || scheme == "blob" || scheme == "file" || scheme == "about") {
        return;
    }
    QUrl firstParty = info.firstPartyUrl();

    // Build RequestContext (Sec 15) — accurate resource + third-party
    auto ctx = RequestClassifier::buildFromQt(url, firstParty, info.resourceType(), info.requestMethod());

    if (engine_ && engine_->isEnabled()) {
        auto result = engine_->shouldBlock(ctx);
        if (result.blocked) {
            info.block(true);
            // Stats are updated atomically inside shouldBlock (INV-09 safe, no DB)
            return;
        }
    }
    // Not blocked -> Chromium network stack proceeds
}

} // namespace virgin::network
