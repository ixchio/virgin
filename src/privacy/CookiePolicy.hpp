#pragma once

namespace virgin::privacy {

enum class CookieMode { Standard, Strict, Private, Ephemeral };

class CookiePolicy {
public:
    static CookieMode defaultMode();
    static bool shouldAllowThirdParty(CookieMode mode, bool isThirdParty, bool isTracker);
};

} // namespace virgin::privacy
