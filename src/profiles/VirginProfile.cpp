#include "VirginProfile.hpp"
#include "network/RequestInterceptor.hpp"
#include "privacy/PermissionManager.hpp"
#include "privacy/CookieFilter.hpp"
#include "adblock/AdBlockEngine.hpp"
#include "adblock/FilterUpdater.hpp"
#include "downloads/DownloadManager.hpp"
#include "browser/VirginSchemeHandler.hpp"

#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QWebEngineDownloadRequest>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QFile>

namespace virgin::profiles {

VirginProfile::VirginProfile(QWebEngineProfile* profile, Type type, const QString& id,
                             std::shared_ptr<adblock::RuleStore> ruleStore, QObject* parent)
    : QObject(parent), qtProfile_(profile), type_(type), id_(id)
{
    // The wrapper owns the native profile. This is essential for private mode:
    // deleting the wrapper must actually destroy Chromium's in-memory store.
    qtProfile_->setParent(this);

    // Private profiles are always strict (Sec 48/P1 strict)
    if (isOffTheRecord()) strict_ = true;

    adblock_ = new adblock::AdBlockEngine(std::move(ruleStore), this);
    filterUpdater_ = new adblock::FilterUpdater(adblock_, this);
    interceptor_ = new network::RequestInterceptor(adblock_, this);
    qtProfile_->setUrlRequestInterceptor(interceptor_);

    permissionManager_ = new privacy::PermissionManager(qtProfile_, this);
    privacy::PermissionManager::registerProfile(qtProfile_, permissionManager_);

    schemeHandler_ = new browser::VirginSchemeHandler(this);
    qtProfile_->installUrlSchemeHandler("virgin", schemeHandler_);

    // Cookie filtering (Sec 19)
    if (type == Type::Private || type == Type::Ephemeral) {
        cookieMode_ = privacy::CookieMode::Private;
    }
    cookieFilter_ = new privacy::CookieFilter(qtProfile_, cookieMode(), this);

    downloads_ = new downloads::DownloadManager(this);
    connect(qtProfile_, &QWebEngineProfile::downloadRequested,
            this, [this](QWebEngineDownloadRequest* r){ downloads_->handleDownload(r); });

    applyPrivacySettings();
}

VirginProfile::~VirginProfile() {
    if (qtProfile_) {
        privacy::PermissionManager::unregisterProfile(qtProfile_);
    }
}

VirginProfile* VirginProfile::createNormal(const QString& dataPath,
                                            std::shared_ptr<adblock::RuleStore> ruleStore,
                                            QObject* parent) {
    auto* p = new QWebEngineProfile("virgin-default", parent);
    p->setPersistentStoragePath(dataPath);
    p->setCachePath(dataPath + "/cache");
    p->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    return new VirginProfile(p, Type::Normal, "default", std::move(ruleStore), parent);
}

VirginProfile* VirginProfile::createPrivate(std::shared_ptr<adblock::RuleStore> ruleStore,
                                             QObject* parent) {
    auto* p = new QWebEngineProfile(parent);
    return new VirginProfile(p, Type::Private, "private", std::move(ruleStore), parent);
}

VirginProfile* VirginProfile::createContainer(const QString& containerId,
                                               const QString& dataPath,
                                               std::shared_ptr<adblock::RuleStore> ruleStore,
                                               QObject* parent) {
    auto* p = new QWebEngineProfile("virgin-" + containerId, parent);
    p->setPersistentStoragePath(dataPath);
    p->setCachePath(dataPath + "/cache");
    p->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    return new VirginProfile(p, Type::Container, containerId, std::move(ruleStore), parent);
}

bool VirginProfile::isOffTheRecord() const {
    return qtProfile_ ? qtProfile_->isOffTheRecord() : (type_ == Type::Private || type_ == Type::Ephemeral);
}

void VirginProfile::applyPrivacySettings() {
    if (!qtProfile_) return;
    auto* s = qtProfile_->settings();
    // Sec 67 + Sec 48 hardening
    s->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false); // INV-08, Sec 23
    s->setAttribute(QWebEngineSettings::JavascriptCanPaste, false);
    s->setAttribute(QWebEngineSettings::WebRTCPublicInterfacesOnly,
                    isOffTheRecord() || webRtcPublicOnly_); // Sec 21 privacy
    s->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, false);
    s->setAttribute(QWebEngineSettings::NavigateOnDropEnabled, false);
    s->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false); // Sec 27
    s->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    s->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true); // gated via VirginPage
    s->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);
    s->setAttribute(QWebEngineSettings::AutoLoadIconsForPage, true);
    s->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, false); // no ping
    s->setAttribute(QWebEngineSettings::ReadingFromCanvasEnabled, !strict_); // Sec 22
    s->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    s->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);
    s->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    s->setAttribute(QWebEngineSettings::AllowGeolocationOnInsecureOrigins, false);
    s->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);

    // Cookie handling updated via CookieFilter
    if (cookieFilter_) {
        cookieFilter_->setMode(cookieMode());
    }

    if (isOffTheRecord()) {
        qtProfile_->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        qtProfile_->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    } else {
        qtProfile_->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
        qtProfile_->setPersistentCookiesPolicy(
            cookieMode_ == privacy::CookieMode::Ephemeral
                ? QWebEngineProfile::NoPersistentCookies
                : QWebEngineProfile::ForcePersistentCookies);
    }
}

void VirginProfile::setStrictMode(bool strict) {
    // Private stays strict regardless
    if (isOffTheRecord()) strict = true;
    strict_ = strict;
    applyPrivacySettings();
}

void VirginProfile::setWebRtcPublicOnly(bool enabled) {
    webRtcPublicOnly_ = isOffTheRecord() ? true : enabled;
    applyPrivacySettings();
}

void VirginProfile::setCookieMode(privacy::CookieMode mode) {
    cookieMode_ = isOffTheRecord() ? privacy::CookieMode::Private : mode;
    applyPrivacySettings();
}

privacy::CookieMode VirginProfile::cookieMode() const {
    if (isOffTheRecord()) return privacy::CookieMode::Private;
    if (strict_) return privacy::CookieMode::Strict;
    return cookieMode_;
}

bool VirginProfile::blocksThirdPartyCookies() const {
    const auto mode = cookieMode();
    return mode == privacy::CookieMode::Strict || mode == privacy::CookieMode::Private;
}

void VirginProfile::setAdblockEnabled(bool enabled) {
    if (adblock_) adblock_->setEnabled(enabled);
    syncYouTubeProtectionScript(enabled);
}

void VirginProfile::syncYouTubeProtectionScript(bool enabled) {
    if (!qtProfile_ || !qtProfile_->scripts()) return;

    constexpr auto scriptName = "Virgin YouTube ad protection";
    auto* scripts = qtProfile_->scripts();
    const auto existing = scripts->find(QString::fromLatin1(scriptName));
    for (const auto& script : existing) scripts->remove(script);
    if (!enabled) return;

    QFile source(QStringLiteral(":/virgin/scripts/youtube-adblock.js"));
    if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QWebEngineScript script;
    script.setName(QString::fromLatin1(scriptName));
    script.setSourceCode(QString::fromUtf8(source.readAll()));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    script.setRunsOnSubFrames(false);
    scripts->insert(script);
}

} // namespace virgin::profiles
