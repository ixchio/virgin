#include "RequestClassifier.hpp"
#include <QSet>

namespace virgin::network {

ResourceType RequestClassifier::classifyResourceType(const QString& s) {
    if (s == "mainFrame" || s == "document") return ResourceType::Document;
    if (s == "subFrame" || s == "subdocument") return ResourceType::SubDocument;
    if (s == "script") return ResourceType::Script;
    if (s == "image") return ResourceType::Image;
    if (s == "stylesheet") return ResourceType::Stylesheet;
    if (s == "fontResource" || s == "font") return ResourceType::Font;
    if (s == "media") return ResourceType::Media;
    if (s == "xmlhttprequest" || s == "xhr") return ResourceType::Xhr;
    if (s == "fetch") return ResourceType::Fetch;
    if (s == "websocket") return ResourceType::WebSocket;
    if (s == "ping" || s == "beacon" || s == "pingBeacon") return ResourceType::PingBeacon;
    return ResourceType::Other;
}

ResourceType RequestClassifier::classifyFromQt(QWebEngineUrlRequestInfo::ResourceType qtType) {
    switch (qtType) {
        case QWebEngineUrlRequestInfo::ResourceTypeMainFrame: return ResourceType::Document;
        case QWebEngineUrlRequestInfo::ResourceTypeSubFrame: return ResourceType::SubDocument;
        case QWebEngineUrlRequestInfo::ResourceTypeStylesheet: return ResourceType::Stylesheet;
        case QWebEngineUrlRequestInfo::ResourceTypeScript: return ResourceType::Script;
        case QWebEngineUrlRequestInfo::ResourceTypeImage: return ResourceType::Image;
        case QWebEngineUrlRequestInfo::ResourceTypeFontResource: return ResourceType::Font;
        case QWebEngineUrlRequestInfo::ResourceTypeMedia: return ResourceType::Media;
        case QWebEngineUrlRequestInfo::ResourceTypeXhr: return ResourceType::Xhr;
        case QWebEngineUrlRequestInfo::ResourceTypePing: return ResourceType::PingBeacon;
        case QWebEngineUrlRequestInfo::ResourceTypePrefetch: return ResourceType::Other;
        case QWebEngineUrlRequestInfo::ResourceTypeFavicon: return ResourceType::Image;
        case QWebEngineUrlRequestInfo::ResourceTypeSubResource: return ResourceType::Other;
        case QWebEngineUrlRequestInfo::ResourceTypeObject: return ResourceType::Other;
        case QWebEngineUrlRequestInfo::ResourceTypeWorker: return ResourceType::Script;
        case QWebEngineUrlRequestInfo::ResourceTypeSharedWorker: return ResourceType::Script;
        case QWebEngineUrlRequestInfo::ResourceTypeServiceWorker: return ResourceType::Script;
        case QWebEngineUrlRequestInfo::ResourceTypeCspReport: return ResourceType::Other;
        case QWebEngineUrlRequestInfo::ResourceTypePluginResource: return ResourceType::Other;
        default: return ResourceType::Other;
    }
}

bool RequestClassifier::isThirdParty(const QUrl& requestUrl, const QUrl& firstPartyUrl) {
    if (!requestUrl.isValid() || !firstPartyUrl.isValid()) return false;
    QString reqHost = requestUrl.host().toLower();
    QString fpHost = firstPartyUrl.host().toLower();
    if (reqHost.isEmpty() || fpHost.isEmpty()) return false;
    if (reqHost == fpHost) return false;
    auto registrable = [](const QUrl& url) -> QString {
        const QString host = url.host().toLower();
        const QStringList labels = host.split('.', Qt::SkipEmptyParts);
        if (labels.size() < 2) return host;
        // Cover common multi-label public suffixes. Cookie enforcement uses
        // Chromium's own third-party classification; this is the blocker hint.
        static const QSet<QString> multiLabelSuffixes = {
            "co.uk", "org.uk", "ac.uk", "gov.uk",
            "com.au", "net.au", "org.au", "edu.au",
            "co.jp", "ne.jp", "or.jp", "co.in", "firm.in", "net.in", "org.in",
            "com.br", "com.cn", "com.mx", "co.nz", "co.za", "com.sg", "com.tr"
        };
        const QString lastTwo = labels.sliced(labels.size() - 2).join('.');
        if (labels.size() >= 3 && multiLabelSuffixes.contains(lastTwo)) {
            return labels.sliced(labels.size() - 3).join('.');
        }
        return lastTwo;
    };
    return registrable(requestUrl) != registrable(firstPartyUrl);
}

RequestContext RequestClassifier::build(const QUrl& requestUrl,
                                        const QUrl& firstPartyUrl,
                                        const QString& resourceTypeStr,
                                        bool isMainFrame) {
    RequestContext ctx;
    ctx.requestUrl = requestUrl;
    ctx.firstPartyUrl = firstPartyUrl;
    ctx.resourceType = classifyResourceType(resourceTypeStr);
    ctx.thirdParty = isThirdParty(requestUrl, firstPartyUrl);
    ctx.topLevel = isMainFrame;
    ctx.method = HttpMethod::Get;
    return ctx;
}

RequestContext RequestClassifier::buildFromQt(const QUrl& requestUrl,
                                              const QUrl& firstPartyUrl,
                                              QWebEngineUrlRequestInfo::ResourceType qtType,
                                              const QByteArray& method) {
    RequestContext ctx;
    ctx.requestUrl = requestUrl;
    ctx.firstPartyUrl = firstPartyUrl;
    ctx.resourceType = classifyFromQt(qtType);
    ctx.thirdParty = isThirdParty(requestUrl, firstPartyUrl);
    ctx.topLevel = (qtType == QWebEngineUrlRequestInfo::ResourceTypeMainFrame);
    // Method
    if (method == "POST") ctx.method = HttpMethod::Post;
    else if (method == "PUT") ctx.method = HttpMethod::Put;
    else if (method == "DELETE") ctx.method = HttpMethod::Delete;
    else if (method == "HEAD") ctx.method = HttpMethod::Head;
    else ctx.method = HttpMethod::Get;
    return ctx;
}

} // namespace virgin::network
