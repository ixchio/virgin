#pragma once

#include <QUrl>
#include <QString>
#include <QWebEngineUrlRequestInfo>

namespace virgin::network {

enum class ResourceType {
    Document,
    SubDocument,
    Script,
    Image,
    Stylesheet,
    Font,
    Media,
    Xhr,
    Fetch,
    WebSocket,
    PingBeacon,
    Other
};

enum class HttpMethod { Get, Post, Put, Delete, Head, Options, Patch, Other };

struct RequestContext {
    QUrl requestUrl;
    QUrl firstPartyUrl;
    ResourceType resourceType = ResourceType::Other;
    bool thirdParty = false;
    bool topLevel = false;
    HttpMethod method = HttpMethod::Get;
};

class RequestClassifier {
public:
    static ResourceType classifyResourceType(const QString& resourceTypeStr);
    static ResourceType classifyFromQt(QWebEngineUrlRequestInfo::ResourceType qtType);
    static bool isThirdParty(const QUrl& requestUrl, const QUrl& firstPartyUrl);
    static RequestContext build(const QUrl& requestUrl,
                                const QUrl& firstPartyUrl,
                                const QString& resourceTypeStr,
                                bool isMainFrame);
    static RequestContext buildFromQt(const QUrl& requestUrl,
                                      const QUrl& firstPartyUrl,
                                      QWebEngineUrlRequestInfo::ResourceType qtType,
                                      const QByteArray& method);
};

} // namespace virgin::network
