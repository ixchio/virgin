#include "UrlSafety.hpp"
#include <QRegularExpression>

namespace virgin::security {

bool UrlSafety::isScriptLikeScheme(const QUrl& url) {
    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("javascript") || scheme == QStringLiteral("vbscript");
}

bool UrlSafety::isVirginInternalUrlAllowed(const QUrl& url) {
    if (url.scheme() != "virgin") return false;
    // Sec 40: map static IDs, reject traversal, reject unrecognized hosts
    static const QStringList kAllowed = {
        "newtab", "settings", "history", "downloads", "privacy", "version"
    };
    QString host = url.host().toLower();
    if (host.isEmpty()) {
        // virgin://newtab without host is slash-based: path
        QString path = url.path().mid(1).toLower();
        return kAllowed.contains(path);
    }
    if (kAllowed.contains(host)) return true;
    // Reject traversal like virgin://../etc
    if (url.path().contains("..")) return false;
    return false;
}

bool UrlSafety::isUrlAllowedForNavigation(const QUrl& url,
                                          QWebEnginePage::NavigationType type,
                                          bool isMainFrame) {
    Q_UNUSED(type)
    Q_UNUSED(isMainFrame)
    if (!url.isValid()) return false;
    // Block file:// from remote? Handled at VirginPage; allow file:// only if user explicitly typed
    if (isScriptLikeScheme(url)) {
        // Script URLs execute in a document context and must never be treated
        // as navigable or externally launchable URLs.
        return false;
    }
    // Deceptive URL checks (Sec 38)
    if (url.host().contains("--")) {
        // punycode/homograph awareness — log but allow with warning
    }
    return true;
}

QString UrlSafety::highlightRegistrableDomain(const QUrl& url) {
    QString host = url.host();
    auto parts = host.split('.');
    if (parts.size() >= 2) return parts.mid(parts.size()-2).join('.');
    return host;
}

bool UrlSafety::isPunycodeSuspicious(const QUrl& url) {
    return url.host().startsWith("xn--");
}

bool UrlSafety::isSafeForExternalLaunch(const QUrl& url) {
    // Sec 26/23: unknown schemes only from user interaction
    static const QStringList safe = {"http","https","virgin","mailto"};
    return safe.contains(url.scheme().toLower());
}

} // namespace virgin::security
