#include "RuntimeSecurityCheck.hpp"
#include <QRegularExpression>

namespace virgin::security {

static QString g_lastError;

bool RuntimeSecurityCheck::isSandboxDisabled() {
    // Fail closed whenever the escape hatch exists; Chromium treats this as a
    // process-wide security decision and a surprising value must not weaken it.
    if (qEnvironmentVariableIsSet("QTWEBENGINE_DISABLE_SANDBOX")) return true;
    return false;
}

bool RuntimeSecurityCheck::hasInsecureFlags(int argc, char* argv[]) {
    for (int i=0;i<argc;++i) {
        const QString arg = QString::fromUtf8(argv[i]);
        if (arg == "--no-sandbox" || arg == "--disable-sandbox") return true;
    }
    const QString chromiumFlags = QString::fromUtf8(qgetenv("QTWEBENGINE_CHROMIUM_FLAGS"));
    const QStringList tokens = chromiumFlags.split(QRegularExpression(QStringLiteral("\\s+")),
                                                   Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        if (token == "--no-sandbox" || token == "--disable-sandbox") return true;
    }
    return false;
}

QString RuntimeSecurityCheck::lastError() { return g_lastError; }

bool RuntimeSecurityCheck::verify(int argc, char* argv[]) {
    g_lastError.clear();
    if (isSandboxDisabled()) {
        g_lastError = "Sandbox disabled via QTWEBENGINE_DISABLE_SANDBOX — refusing to start (INV-01)";
        return false;
    }
    if (hasInsecureFlags(argc, argv)) {
        g_lastError = "Chromium sandbox-disable flag detected — refusing to start (INV-01)";
        return false;
    }
    return true;
}

} // namespace virgin::security
