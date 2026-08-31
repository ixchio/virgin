#pragma once

#include <QObject>
#include <atomic>

#include "CookiePolicy.hpp"

class QWebEngineProfile;
class QWebEngineCookieStore;

namespace virgin::privacy {

class CookieFilter final : public QObject {
    Q_OBJECT
public:
    explicit CookieFilter(QWebEngineProfile* profile, CookieMode mode, QObject* parent = nullptr);
    ~CookieFilter() override;

    void setMode(CookieMode mode) { mode_.store(mode, std::memory_order_relaxed); }
    CookieMode mode() const { return mode_.load(std::memory_order_relaxed); }

private:
    QWebEngineProfile* profile_ = nullptr;
    QWebEngineCookieStore* store_ = nullptr;
    std::atomic<CookieMode> mode_{CookieMode::Standard};
};

} // namespace virgin::privacy
