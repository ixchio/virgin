#pragma once

#include <QString>

namespace virgin::security {

class RuntimeSecurityCheck {
public:
    // INV-01: sandbox cannot be disabled, INV-07 no telemetry, plus env checks
    static bool verify(int argc, char* argv[]);
    static bool isSandboxDisabled();
    static bool hasInsecureFlags(int argc, char* argv[]);
    static QString lastError();
};

} // namespace virgin::security
