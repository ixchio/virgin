#include "CertificatePolicy.hpp"

namespace virgin::security {

CertificatePolicy::Decision CertificatePolicy::evaluate(const QWebEngineCertificateError& error) {
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    Q_UNUSED(error)
    // Qt < 6.8 cannot identify a main-frame certificate error. Failing closed
    // avoids accidentally presenting an override for a subresource.
    return Decision::Reject;
#else
    // Sec 25: subresource -> REJECT, main-frame -> blocking interstitial, never accept globally
    if (!error.isOverridable() || !error.isMainFrame()) return Decision::Reject;
    if (error.url().isEmpty()) return Decision::Reject;
    // Main frame -> interstitial (still reject by default; user must explicitly override narrowly)
    return Decision::Interstitial;
#endif
}

bool CertificatePolicy::isOverridable(const QWebEngineCertificateError& error) {
    return error.isOverridable();
}

} // namespace virgin::security
