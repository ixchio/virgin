#include "NavigationController.hpp"
#include "TabManager.hpp"
#include "BrowserTab.hpp"
#include "network/HttpsFirstPolicy.hpp"
#include "security/UrlSafety.hpp"
#include "storage/SettingsStore.hpp"

#include <QUrl>
#include <QUrlQuery>

namespace virgin::browser {

NavigationController::NavigationController(TabManager* tabs,
                                           storage::SettingsStore* settings,
                                           QObject* parent)
    : QObject(parent), tabs_(tabs), settings_(settings) {}

bool NavigationController::isProbablyUrl(const QString& input) {
    QString t = input.trimmed();
    if (t.isEmpty()) return false;
    if (t.contains("://")) return true;
    if (t.startsWith("virgin://")) return true;
    if (t == "localhost" || t.startsWith("localhost:") || t.startsWith("localhost/")) return true;
    // IP-like
    if (t.count('.') >= 1 && !t.contains(' ')) {
        // has dot and no spaces -> likely URL, unless it looks like a sentence
        // Also check TLD pattern: if contains dot and no spaces
        return true;
    }
    // Contains no spaces and has dot -> url
    if (!t.contains(' ') && t.contains('.')) return true;
    return false;
}

QUrl NavigationController::resolveInput(const QString& input) {
    return resolveInput(input, QStringLiteral("https://duckduckgo.com/?q=%s"), true);
}

QUrl NavigationController::resolveInput(const QString& input,
                                        const QString& searchTemplate,
                                        bool httpsFirst) {
    QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) return QUrl("virgin://newtab");

    // Already a valid URL with scheme
    QUrl url(trimmed);
    if (url.isValid() && !url.scheme().isEmpty() && trimmed.contains("://")) {
        // Apply URL safety canonicalization via QUrl (Sec 38)
        return url;
    }
    if (trimmed.startsWith("virgin://")) {
        return QUrl(trimmed);
    }

    if (isProbablyUrl(trimmed)) {
        // Sec 37: valid URL -> navigate, otherwise search
        // If user typed "example.com", add https://
        if (!trimmed.contains("://")) {
            QUrl withScheme("https://" + trimmed);
            if (withScheme.isValid()) {
                // HTTPS-first (Sec 24)
                return httpsFirst ? network::HttpsFirstPolicy::upgradeIfNeeded(withScheme) : withScheme;
            }
        }
        url = QUrl(trimmed);
        if (url.isValid()) return httpsFirst ? network::HttpsFirstPolicy::upgradeIfNeeded(url) : url;
    }

    // Otherwise -> search (user-configurable provider, default DuckDuckGo)
    // Sec 37: never send keystrokes to autocomplete by default — we only send on Enter.
    const QString query = QString::fromUtf8(QUrl::toPercentEncoding(trimmed));
    QString provider = searchTemplate;
    if (!provider.contains("%s")) provider = QStringLiteral("https://duckduckgo.com/?q=%s");
    provider.replace("%s", query);
    QUrl searchUrl(provider);
    if (!searchUrl.isValid() || searchUrl.scheme().toLower() != "https") {
        searchUrl = QUrl(QStringLiteral("https://duckduckgo.com/?q=") + query);
    }
    return searchUrl;
}

void NavigationController::navigateCurrent(const QString& input) {
    const QString searchTemplate = settings_
        ? settings_->searchEngineUrl()
        : QStringLiteral("https://duckduckgo.com/?q=%s");
    const bool httpsFirst = !settings_ || settings_->value("https_first", true).toBool();
    QUrl url = resolveInput(input, searchTemplate, httpsFirst);
    navigate(url);
}

void NavigationController::navigate(const QUrl& url) {
    if (!tabs_) return;
    auto* tab = tabs_->currentTab();
    if (!tab) {
        tab = tabs_->createTab(url, true);
        return;
    }
    // HTTPS-first policy (Sec 24)
    const bool httpsFirst = !settings_ || settings_->value("https_first", true).toBool();
    QUrl finalUrl = httpsFirst ? network::HttpsFirstPolicy::upgradeIfNeeded(url) : url;
    if (!security::UrlSafety::isUrlAllowedForNavigation(finalUrl, QWebEnginePage::NavigationTypeTyped, true)) {
        return;
    }
    tab->navigate(finalUrl);
}

} // namespace virgin::browser
