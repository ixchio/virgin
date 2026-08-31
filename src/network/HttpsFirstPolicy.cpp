#include "HttpsFirstPolicy.hpp"
#include <QHostAddress>

namespace virgin::network {

bool HttpsFirstPolicy::shouldUpgrade(const QUrl& url) {
    if (url.scheme().toLower() != "http") return false;
    const QString host = url.host().toLower();
    if (host == "localhost" || host.endsWith(".localhost")) return false;
    QHostAddress address;
    if (address.setAddress(host) && address.isLoopback()) return false;
    return true;
}

QUrl HttpsFirstPolicy::upgradeIfNeeded(const QUrl& url) {
    // Sec 24: Try https:// first
    if (shouldUpgrade(url)) {
        QUrl upgraded = url;
        upgraded.setScheme("https");
        // Port 80 -> 443 default
        if (upgraded.port() == 80) upgraded.setPort(443);
        return upgraded;
    }
    return url;
}

} // namespace virgin::network
