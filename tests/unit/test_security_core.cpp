#include "security/RuntimeSecurityCheck.hpp"
#include "security/UrlSafety.hpp"
#include "network/HttpsFirstPolicy.hpp"
#include "storage/SettingsStore.hpp"
#include "privacy/CookiePolicy.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char* label) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << label << '\n';
    if (!condition) ++failures;
}
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    char appName[] = "virgin";
    char safeFlag[] = "--safe";
    char noSandbox[] = "--no-sandbox";
    char* safeArgs[] = {appName, safeFlag};
    char* unsafeArgs[] = {appName, noSandbox};
    check(!virgin::security::RuntimeSecurityCheck::hasInsecureFlags(2, safeArgs),
          "safe command line accepted");
    check(virgin::security::RuntimeSecurityCheck::hasInsecureFlags(2, unsafeArgs),
          "sandbox-disable command line rejected");

    const QByteArray oldSandbox = qgetenv("QTWEBENGINE_DISABLE_SANDBOX");
    const bool hadSandbox = qEnvironmentVariableIsSet("QTWEBENGINE_DISABLE_SANDBOX");
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");
    check(!virgin::security::RuntimeSecurityCheck::verify(2, safeArgs),
          "sandbox-disable environment rejected");
    if (hadSandbox) qputenv("QTWEBENGINE_DISABLE_SANDBOX", oldSandbox);
    else qunsetenv("QTWEBENGINE_DISABLE_SANDBOX");

    const QByteArray oldFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    const bool hadFlags = qEnvironmentVariableIsSet("QTWEBENGINE_CHROMIUM_FLAGS");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu --no-sandbox");
    check(virgin::security::RuntimeSecurityCheck::hasInsecureFlags(2, safeArgs),
          "sandbox-disable Chromium environment flag rejected");
    if (hadFlags) qputenv("QTWEBENGINE_CHROMIUM_FLAGS", oldFlags);
    else qunsetenv("QTWEBENGINE_CHROMIUM_FLAGS");

    check(virgin::network::HttpsFirstPolicy::upgradeIfNeeded(QUrl("http://example.com/a")).scheme() == "https",
          "HTTP upgraded to HTTPS");
    check(virgin::network::HttpsFirstPolicy::upgradeIfNeeded(QUrl("http://localhost:8080/a")).scheme() == "http",
          "localhost remains compatible with HTTP development servers");
    check(virgin::security::UrlSafety::isVirginInternalUrlAllowed(QUrl("virgin://newtab")),
          "known internal page accepted");
    check(!virgin::security::UrlSafety::isVirginInternalUrlAllowed(QUrl("virgin://unknown")),
          "unknown internal page rejected");

    QTemporaryDir directory;
    const QString path = directory.filePath("settings.json");
    virgin::storage::SettingsStore settings(path);
    settings.setSearchEngineUrl("http://insecure.example/?q=%s");
    check(settings.searchEngineUrl().startsWith("https://duckduckgo.com/"),
          "insecure search provider rejected");
    check(settings.setSearchEngineUrl("https://www.google.com/search?q=%s") &&
          settings.searchEngineUrl() == "https://www.google.com/search?q=%s",
          "HTTPS search provider preset accepted");
    check(!virgin::storage::SettingsStore::isValidSearchEngineUrl("https://example.com/no-placeholder"),
          "search provider requires a query placeholder");
    settings.setValue("strict_mode", true);
    check(settings.save(), "settings save is atomic");
    virgin::storage::SettingsStore loaded(path);
    check(loaded.load() && loaded.strictMode(), "settings round trip");
    check(loaded.value("webrtc_public_only", true).toBool(), "missing defaults preserved");

    using virgin::privacy::CookieMode;
    using virgin::privacy::CookiePolicy;
    check(CookiePolicy::shouldAllowThirdParty(CookieMode::Standard, true, true),
          "standard cookie mode truthfully allows third-party cookies");
    check(!CookiePolicy::shouldAllowThirdParty(CookieMode::Strict, true, false),
          "strict cookie mode blocks third-party cookies");
    check(CookiePolicy::shouldAllowThirdParty(CookieMode::Ephemeral, true, false),
          "session-only cookie mode controls persistence, not acceptance");

    return failures == 0 ? 0 : 1;
}
