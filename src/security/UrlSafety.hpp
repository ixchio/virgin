#pragma once

#include <QUrl>
#include <QWebEnginePage>

namespace virgin::security {

class UrlSafety {
public:
    static bool isScriptLikeScheme(const QUrl& url);
    static bool isUrlAllowedForNavigation(const QUrl& url,
                                          QWebEnginePage::NavigationType type,
                                          bool isMainFrame);
    static bool isVirginInternalUrlAllowed(const QUrl& url);
    static QString highlightRegistrableDomain(const QUrl& url);
    static bool isPunycodeSuspicious(const QUrl& url);
    static bool isSafeForExternalLaunch(const QUrl& url);
};

} // namespace virgin::security
