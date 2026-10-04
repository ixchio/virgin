#include "VirginApp.hpp"
#include "Paths.hpp"
#include "app/Version.hpp"

#include "profiles/ProfileManager.hpp"
#include "profiles/VirginProfile.hpp"
#include "adblock/AdBlockEngine.hpp"
#include "adblock/FilterUpdater.hpp"
#include "storage/SettingsStore.hpp"
#include "storage/HistoryStore.hpp"
#include "storage/BookmarkStore.hpp"
#include "storage/SessionStore.hpp"
#include "browser/BrowserWindow.hpp"
#include "security/RuntimeSecurityCheck.hpp"
#include "privacy/CookieFilter.hpp"
#include "ui/PixelTheme.hpp"

#include <QMessageBox>
#include <QWebEngineProfile>
#include <QWebEngineUrlScheme>
#include <QWebEngineView>
#include <QClipboard>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QDir>
#include <QIcon>
#include <QTimer>
#include <csignal>
#include <cstdio>

namespace virgin::app {

VirginApp* VirginApp::s_instance = nullptr;

VirginApp::VirginApp(int& argc, char** argv)
    : argc_(argc), argv_(argv)
{
    s_instance = this;
    QCoreApplication::setApplicationName("Virgin");
    QCoreApplication::setApplicationVersion(QString::fromLatin1(kVersion));
    QCoreApplication::setOrganizationName("Virgin");
    QCoreApplication::setOrganizationDomain("virgin.local");
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
}

VirginApp::~VirginApp() = default;

bool VirginApp::verifyRuntimeSecurity() const {
    return virgin::security::RuntimeSecurityCheck::verify(argc_, argv_);
}

void VirginApp::installCrashHandlers() {
    std::signal(SIGSEGV, [](int) { _exit(139); });
}

void VirginApp::registerVirginScheme() {
    // Sec 39/40: virgin:// scheme — secure, local, no network, CORS safe
    QWebEngineUrlScheme virginScheme("virgin");
    virginScheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                          QWebEngineUrlScheme::LocalScheme |
                          QWebEngineUrlScheme::NoAccessAllowed); // CSP: no website can fetch virgin://
    virginScheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    QWebEngineUrlScheme::registerScheme(virginScheme);
}

bool VirginApp::initialize() {
    if (!verifyRuntimeSecurity()) {
        const QByteArray error = security::RuntimeSecurityCheck::lastError().toUtf8();
        std::fprintf(stderr, "Virgin security check failed: %s\n", error.constData());
        return false;
    }

    // Custom schemes must be registered before QApplication/WebEngine starts.
    registerVirginScheme();
    qtApp_ = std::make_unique<QApplication>(argc_, argv_);
    qtApp_->setWindowIcon(QIcon(QStringLiteral(":/virgin/logo-192.png")));
    virgin::ui::applyPixelTheme(*qtApp_);

    if (!Paths::ensureDirectories()) {
        QMessageBox::critical(nullptr, "Virgin — Fatal",
            "Failed to create data directories at:\n" + Paths::virginRoot());
        return false;
    }

    installCrashHandlers();

    settings_ = std::make_unique<storage::SettingsStore>(Paths::settingsPath());
    settings_->load();

    QString dbPath = Paths::databasePath();
    history_ = std::make_unique<storage::HistoryStore>(dbPath);
    bookmarks_ = std::make_unique<storage::BookmarkStore>(dbPath);
    sessions_ = std::make_unique<storage::SessionStore>(dbPath);

    if (!history_->isReady() || !bookmarks_->isReady() || !sessions_->isReady()) {
        QMessageBox::critical(nullptr, "Virgin — Fatal",
            "Could not open or migrate the local browser database:\n" + dbPath +
            "\n\nVirgin stopped to avoid silently losing or corrupting browser data.");
        return false;
    }

    int keepDays = settings_->historyRetentionDays();
    if (keepDays >= 0 && keepDays != -1) {
        history_->deleteExpired(keepDays);
    }

    profiles_ = std::make_unique<profiles::ProfileManager>(this);
    if (!profiles_->initialize()) {
        QMessageBox::critical(nullptr, "Virgin — Fatal", "Failed to initialize profiles");
        return false;
    }

    applyProfileSettings(profiles_->defaultProfile());

    createFirstWindow();
    if (settings_->value("filter_auto_update", true).toBool()) {
        auto* updater = profiles_->defaultProfile()->filterUpdater();
        if (updater && updater->needsNetworkUpdate()) {
            QTimer::singleShot(0, updater, &adblock::FilterUpdater::updateFromNetwork);
        }
    }
    return true;
}

void VirginApp::applyProfileSettings(profiles::VirginProfile* profile) {
    if (settings_ && profile) {
        const bool strict = settings_->strictMode();
        profile->setStrictMode(strict);
        profile->setWebRtcPublicOnly(
            settings_->value("webrtc_public_only", true).toBool());
        const QString cookieMode = settings_->value("cookie_mode", "standard").toString();
        if (profile->isOffTheRecord()) profile->setCookieMode(privacy::CookieMode::Private);
        else if (cookieMode == "strict") profile->setCookieMode(privacy::CookieMode::Strict);
        else if (cookieMode == "ephemeral") profile->setCookieMode(privacy::CookieMode::Ephemeral);
        else profile->setCookieMode(privacy::CookieMode::Standard);
        const bool adblockEnabled = settings_->value("adblock_enabled", true).toBool();
        profile->setAdblockEnabled(adblockEnabled);
    }
}

void VirginApp::createFirstWindow() {
    auto* win = createWindow(profiles_->defaultProfile(), true);
    mainWindow_ = win;
    win->openInitialPage();
}

browser::BrowserWindow* VirginApp::createWindow(profiles::VirginProfile* profile, bool ownsSession) {
    if (!profile) profile = profiles_->defaultProfile();
    applyProfileSettings(profile);
    auto* win = new browser::BrowserWindow(profile, settings_.get(), history_.get(), bookmarks_.get(),
                                           ownsSession ? sessions_.get() : nullptr);
    windows_.append(win);
    connect(win, &QObject::destroyed, this, &VirginApp::onWindowClosed);
    win->show();
    return win;
}

browser::BrowserWindow* VirginApp::createPrivateWindow() {
    auto* privProfile = profiles_->privateProfile();
    applyProfileSettings(privProfile);
    auto* win = new browser::BrowserWindow(privProfile, settings_.get(), history_.get(), bookmarks_.get(), sessions_.get());

    windows_.append(win);
    connect(win, &QObject::destroyed, this, &VirginApp::onWindowClosed);
    win->setWindowTitle("Virgin — Private Window (Off-the-Record)");
    win->show();
    win->setProperty("virginPrivate", true);
    return win;
}

browser::BrowserWindow* VirginApp::createContainerWindow(const QString& id) {
    if (!profiles_) return nullptr;
    auto* profile = profiles_->containerProfile(id);
    if (!profile) return nullptr;
    auto* window = createWindow(profile);
    window->openInitialPage();
    return window;
}

bool VirginApp::removeContainer(const QString& id) {
    if (!profiles_) return false;
    for (auto* window : windows_) {
        if (window && window->profile() &&
            window->profile()->type() == profiles::VirginProfile::Type::Container &&
            window->profile()->id() == id) {
            return false;
        }
    }
    return profiles_->removeContainer(id);
}

void VirginApp::panic() {
    // Sec 30: sequence — close private tabs, destroy OTR profile, clear clipboard etc.
    // 1. Close private windows
    QList<browser::BrowserWindow*> toClose;
    for (auto* w : windows_) {
        if (w->profile() && w->profile()->isOffTheRecord()) {
            toClose.append(w);
        }
    }
    for (auto* w : toClose) {
        w->close();
        // destroyed signal will remove from list
    }
    // The last private window's destroyed callback deletes the OTR profile after
    // every page/view has released it.
    // 2. Clear clipboard optionally (privacy)
    if (QGuiApplication::clipboard()) {
        QGuiApplication::clipboard()->clear();
    }
    // 3. Clear transient HistoryStore queue (not yet flushed) — already guarded INV-02
    // 4. No private URLs in sessions — ensure cleared
    if (sessions_) sessions_->clearPrivateFromSession();
    // 5. Optionally close all Virgin if setting? For now keep normal windows open
    if (windows_.isEmpty() && mainWindow_) {
        // If all closed, create new normal window
        createFirstWindow();
    }
}

void VirginApp::onWindowClosed(QObject* obj) {
    auto* win = static_cast<browser::BrowserWindow*>(obj);
    windows_.removeOne(win);
    if (win == mainWindow_) mainWindow_ = nullptr;
    // If last normal window closed, quit — but private windows keep app alive
    if (windows_.isEmpty() && qtApp_) {
        qtApp_->quit();
    }
    // If private profile has no windows, destroy it (Sec 9.2: destroyed when last private tab closes)
    bool hasPrivateWindow = false;
    for (auto* w : windows_) if (w->profile() && w->profile()->isOffTheRecord()) hasPrivateWindow = true;
    if (!hasPrivateWindow && profiles_) {
        profiles_->destroyPrivateProfiles();
    }
}

int VirginApp::run() {
    if (!qtApp_) return 1;
    return qtApp_->exec();
}

void VirginApp::shutdown() {
    for (auto* w : windows_) if (w) w->close();
    if (profiles_) profiles_->shutdown();
    if (history_) history_->flush();
    if (settings_) settings_->save();
}

} // namespace virgin::app
