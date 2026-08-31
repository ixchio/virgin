#pragma once

#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>

namespace virgin::adblock { class AdBlockEngine; }

namespace virgin::network {

class RequestInterceptor final : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    explicit RequestInterceptor(virgin::adblock::AdBlockEngine* engine, QObject* parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo& info) override;

private:
    virgin::adblock::AdBlockEngine* engine_ = nullptr;
};

} // namespace virgin::network
