#include "CookieFilter.hpp"
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QUrl>

namespace virgin::privacy {

CookieFilter::CookieFilter(QWebEngineProfile* profile, CookieMode mode, QObject* parent)
    : QObject(parent), profile_(profile), mode_(mode)
{
    if (!profile_) return;
    store_ = profile_->cookieStore();
    store_->setCookieFilter([this](const QWebEngineCookieStore::FilterRequest& request) {
        const CookieMode current = mode_.load(std::memory_order_relaxed);
        // Persistence is configured on QWebEngineProfile. The filter only decides
        // whether a cookie may be accepted at all.
        return CookiePolicy::shouldAllowThirdParty(current, request.thirdParty, false);
    });
}

CookieFilter::~CookieFilter() {
    if (store_) store_->setCookieFilter(nullptr);
}

} // namespace virgin::privacy
