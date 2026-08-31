#pragma once

#include <QUrl>

namespace virgin::network {

class HttpsFirstPolicy {
public:
    static QUrl upgradeIfNeeded(const QUrl& url);
    static bool shouldUpgrade(const QUrl& url);
};

} // namespace virgin::network
