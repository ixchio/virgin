#include "CookiePolicy.hpp"

namespace virgin::privacy {

CookieMode CookiePolicy::defaultMode() { return CookieMode::Standard; }

bool CookiePolicy::shouldAllowThirdParty(CookieMode mode, bool isThirdParty, bool isTracker) {
    (void)isTracker;
    if (!isThirdParty) return true;
    return mode == CookieMode::Standard || mode == CookieMode::Ephemeral;
}

} // namespace virgin::privacy
