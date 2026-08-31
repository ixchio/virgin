#include "CertificatePolicy.hpp"

namespace virgin::security {

CertificatePolicy::Decision CertificatePolicy::evaluate(const QWebEngineCertificateError& error) {
    // Sec 25: subresource -> REJECT, main-frame -> blocking interstitial, never accept globally
    if (!error.isOverridable() || !error.isMainFrame()) return Decision::Reject;
    if (error.url().isEmpty()) return Decision::Reject;
    // Main frame -> interstitial (still reject by default; user must explicitly override narrowly)
    return Decision::Interstitial;
}

bool CertificatePolicy::isOverridable(const QWebEngineCertificateError& error) {
    return error.isOverridable();
}

} // namespace virgin::security
