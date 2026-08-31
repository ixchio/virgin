#pragma once
#include <QWebEngineCertificateError>

namespace virgin::security {

class CertificatePolicy {
public:
    enum class Decision { Reject, AllowSubresource, Interstitial };
    static Decision evaluate(const QWebEngineCertificateError& error);
    static bool isOverridable(const QWebEngineCertificateError& error);
};

} // namespace virgin::security
