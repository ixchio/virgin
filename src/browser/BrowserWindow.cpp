#include "BrowserWindow.hpp"
#include "TabManager.hpp"
#include "BrowserTab.hpp"
#include "NavigationController.hpp"
#include "profiles/VirginProfile.hpp"
#include "profiles/ProfileManager.hpp"
#include "profiles/ContainerManager.hpp"
#include "storage/SettingsStore.hpp"
#include "storage/HistoryStore.hpp"
#include "storage/BookmarkStore.hpp"
#include "storage/SessionStore.hpp"
#include "ui/Omnibox.hpp"
#include "ui/PrivacyPanel.hpp"
#include "ui/HistoryDialog.hpp"
#include "ui/BookmarkDialog.hpp"
#include "ui/SettingsDialog.hpp"
#include "ui/PixelIcons.hpp"
#include "downloads/DownloadShelf.hpp"
#include "app/VirginApp.hpp"
#include "app/Version.hpp"
#include "privacy/CookieFilter.hpp"
#include "privacy/PermissionManager.hpp"
#include "adblock/AdBlockEngine.hpp"
#include "adblock/FilterUpdater.hpp"
#include "security/UrlSafety.hpp"

#include <QVBoxLayout>
#include <QAction>
#include <QLabel>
#include <QToolButton>
#include <QShortcut>
#include <QCloseEvent>
#include <QMessageBox>
#include <QWebEngineHistory>
#include <QDesktopServices>
#include <QWebEnginePage>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QInputDialog>
#include <QColor>

namespace virgin::browser {

BrowserWindow::BrowserWindow(virgin::profiles::VirginProfile* profile,
                             virgin::storage::SettingsStore* settings,
                             virgin::storage::HistoryStore* history,
                             virgin::storage::BookmarkStore* bookmarks,
                             virgin::storage::SessionStore* sessions,
                             QWidget* parent)
    : QMainWindow(parent), profile_(profile), settings_(settings), history_(history), bookmarks_(bookmarks), sessions_(sessions)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    const bool isPrivate = profile_ && profile_->isOffTheRecord();
    if (isPrivate) {
        setWindowTitle("Virgin — Private Window (Off-the-Record)");
    } else {
        const bool isContainer = profile_ && profile_->type() == profiles::VirginProfile::Type::Container;
        setWindowTitle(isContainer ? "Virgin — Container: " + profile_->id()
                                   : "Virgin — Privacy-First Browser");
    }
    resize(1280, 800);
    setMinimumSize(960, 600);

    tabs_ = new TabManager(profile_, history_, this);
    nav_ = new NavigationController(tabs_, settings_, this);
    refreshHomeUrl();

    auto* central = new QWidget(this);
    auto* vbox = new QVBoxLayout(central);
    vbox->setContentsMargins(0,0,0,0);
    vbox->setSpacing(0);

    if (isPrivate) {
        auto* badge = new QLabel("Private browsing — history and cookies from this window are cleared when it closes", central);
        badge->setObjectName("privateBanner");
        badge->setAlignment(Qt::AlignCenter);
        vbox->addWidget(badge);
        privateBadge_ = badge;
    }

    vbox->addWidget(tabs_);

    if (profile_ && profile_->downloadManager()) {
        downloadShelf_ = new virgin::downloads::DownloadShelf(profile_->downloadManager(), central);
        vbox->addWidget(downloadShelf_);
    }
    setCentralWidget(central);

    setupToolbar();
    setupMenus();
    setupShortcuts();

    connect(tabs_, &TabManager::activeTabChanged, this, &BrowserWindow::onActiveTabChanged);

    progress_ = new QProgressBar(this);
    progress_->setMaximumWidth(120);
    progress_->setTextVisible(false);
    progress_->setRange(0,100);
    progress_->hide();
    statusBar()->addPermanentWidget(progress_);
    statusBar()->setSizeGripEnabled(false);
    if (isPrivate) statusBar()->showMessage("Private browsing", 3000);

    if (profile_ && profile_->adblockEngine()) {
        connect(profile_->adblockEngine(), &virgin::adblock::AdBlockEngine::blocked,
                this, &BrowserWindow::onRequestBlocked, Qt::QueuedConnection);
    }
    if (profile_ && profile_->filterUpdater()) {
        connect(profile_->filterUpdater(), &virgin::adblock::FilterUpdater::updateProgress,
                this, &BrowserWindow::onFilterUpdateProgress);
        connect(profile_->filterUpdater(), &virgin::adblock::FilterUpdater::updateFinished,
                this, &BrowserWindow::onFilterUpdateFinished);
    }

    updatePrivateIndicator();
    updateShieldBadge();

    if (!isPrivate && sessions_ && profile_ &&
        profile_->type() == profiles::VirginProfile::Type::Normal) {
        sessionSaveTimer_ = new QTimer(this);
        sessionSaveTimer_->setInterval(15'000);
        connect(sessionSaveTimer_, &QTimer::timeout, this, &BrowserWindow::saveSession);
        sessionSaveTimer_->start();
    }
}

BrowserWindow::~BrowserWindow() = default;

void BrowserWindow::setupToolbar() {
    toolbar_ = addToolBar("Navigation");
    toolbar_->setMovable(false);
    toolbar_->setIconSize(QSize(18,18));

    backBtn_ = new QPushButton(this);
    forwardBtn_ = new QPushButton(this);
    reloadBtn_ = new QPushButton(this);
    stopBtn_ = new QPushButton(this);
    homeBtn_ = new QPushButton(this);
    bookmarkBtn_ = new QPushButton(this);
    shieldBtn_ = new QPushButton(this);

    backBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Back));
    forwardBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Forward));
    reloadBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Reload));
    stopBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Stop));
    homeBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Home));
    bookmarkBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Bookmark));
    shieldBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Shield, QColor("#397147")));

    for (auto* b : {backBtn_, forwardBtn_, reloadBtn_, stopBtn_, homeBtn_, bookmarkBtn_, shieldBtn_}) {
        b->setFixedSize(30,30);
        b->setIconSize(QSize(18,18));
    }
    stopBtn_->hide();
    if (profile_ && profile_->isOffTheRecord()) {
        shieldBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Shield, QColor("#5e5368")));
        shieldBtn_->setToolTip("Privacy Shield — Private window: strict, no persistence");
    } else {
        shieldBtn_->setToolTip("Privacy Shield — blocked requests, permissions, site controls");
    }
    bookmarkBtn_->setToolTip("Bookmark this page (Ctrl+D)");
    bookmarkBtn_->setCheckable(true);

    toolbar_->addWidget(backBtn_);
    toolbar_->addWidget(forwardBtn_);
    toolbar_->addWidget(reloadBtn_);
    toolbar_->addWidget(stopBtn_);
    toolbar_->addWidget(homeBtn_);

    const bool privateMode = profile_ && profile_->isOffTheRecord();
    omnibox_ = new virgin::ui::Omnibox(privateMode ? nullptr : history_,
                                       privateMode ? nullptr : bookmarks_, this);
    if (profile_ && profile_->isOffTheRecord()) {
        omnibox_->setPlaceholderText("Search or enter address (private)");
    } else {
        omnibox_->setPlaceholderText("Search or enter address");
    }
    toolbar_->addWidget(omnibox_);
    toolbar_->widgetForAction(toolbar_->actions().last())->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    toolbar_->addWidget(bookmarkBtn_);
    toolbar_->addWidget(shieldBtn_);

    connect(backBtn_, &QPushButton::clicked, this, &BrowserWindow::onBack);
    connect(forwardBtn_, &QPushButton::clicked, this, &BrowserWindow::onForward);
    connect(reloadBtn_, &QPushButton::clicked, this, &BrowserWindow::onReload);
    connect(stopBtn_, &QPushButton::clicked, this, &BrowserWindow::onStop);
    connect(homeBtn_, &QPushButton::clicked, this, &BrowserWindow::onHome);
    connect(bookmarkBtn_, &QPushButton::clicked, this, &BrowserWindow::onBookmarkToggle);
    connect(shieldBtn_, &QPushButton::clicked, this, &BrowserWindow::onShieldClicked);
    connect(omnibox_, &virgin::ui::Omnibox::returnPressedWithText, this, &BrowserWindow::onOmniboxReturnPressed);

    privacyPanel_ = new virgin::ui::PrivacyPanel(this);
    privacyPanel_->hide();
    connect(privacyPanel_, &virgin::ui::PrivacyPanel::siteControlsRequested,
            this, [this](const QUrl&) { privacyPanel_->hide(); onShowSettings(); });
    connect(privacyPanel_, &virgin::ui::PrivacyPanel::clearDataRequested,
            this, &BrowserWindow::onClearSiteData);
    connect(privacyPanel_, &virgin::ui::PrivacyPanel::forgetSiteRequested,
            this, &BrowserWindow::onForgetSite);
}

void BrowserWindow::setupMenus() {
    const auto addShortcutAction = [this](QMenu* menu, const QString& label,
                                          const QKeySequence& shortcut, auto handler) {
        auto* action = menu->addAction(label);
        action->setShortcut(shortcut);
        connect(action, &QAction::triggered, this, handler);
    };

    auto* fileMenu = menuBar()->addMenu("&File");
    addShortcutAction(fileMenu, "New Tab", QKeySequence("Ctrl+T"), &BrowserWindow::onNewTab);
    addShortcutAction(fileMenu, "Duplicate Tab", QKeySequence("Ctrl+Shift+K"), &BrowserWindow::onDuplicateTab);
    addShortcutAction(fileMenu, "Close Tab", QKeySequence("Ctrl+W"), &BrowserWindow::onCloseTab);
    addShortcutAction(fileMenu, "Reopen Closed Tab", QKeySequence("Ctrl+Shift+T"), &BrowserWindow::onReopenTab);
    fileMenu->addSeparator();
    addShortcutAction(fileMenu, "New Private Window", QKeySequence("Ctrl+Shift+P"), &BrowserWindow::onPrivateWindow);
    fileMenu->addAction("New Container Window…", this, &BrowserWindow::onContainerWindow);
    fileMenu->addAction("Manage Containers…", this, &BrowserWindow::onManageContainers);
    fileMenu->addSeparator();
    addShortcutAction(fileMenu, "Bookmark This Page", QKeySequence("Ctrl+D"), &BrowserWindow::onBookmarkToggle);
    fileMenu->addSeparator();
    addShortcutAction(fileMenu, "Panic (close private)", QKeySequence("Ctrl+Shift+X"), &BrowserWindow::onPanic);

    auto* editMenu = menuBar()->addMenu("&Edit");
    addShortcutAction(editMenu, "Focus Address Bar", QKeySequence("Ctrl+L"), &BrowserWindow::onFocusOmnibox);

    auto* viewMenu = menuBar()->addMenu("&View");
    addShortcutAction(viewMenu, "Reload", QKeySequence("Ctrl+R"), &BrowserWindow::onReload);
    addShortcutAction(viewMenu, "Full Screen", QKeySequence("F11"), [this]{ onFullScreenRequested(!isFullScreen_); });
    viewMenu->addSeparator();
    addShortcutAction(viewMenu, "History", QKeySequence("Ctrl+H"), &BrowserWindow::onShowHistory);
    addShortcutAction(viewMenu, "Bookmarks", QKeySequence("Ctrl+Shift+O"), &BrowserWindow::onShowBookmarks);
    addShortcutAction(viewMenu, "Downloads", QKeySequence("Ctrl+J"), &BrowserWindow::onShowDownloads);
    viewMenu->addSeparator();
    viewMenu->addAction("Settings", this, &BrowserWindow::onShowSettings);
    viewMenu->addAction("Update Filter Lists", this, &BrowserWindow::onUpdateFilters);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("About Virgin", [this]{
        bool isPrivate = profile_ && profile_->isOffTheRecord();
        QMessageBox::about(this, "About Virgin",
            QString("<h2>Virgin %1</h2>"
            "<p>Security profile — %2</p>"
            "<p>A small, local-first browser built with Qt WebEngine.</p>"
            "<p>No Virgin account, cloud service, telemetry, or crash uploader.</p>"
            "<p>Private windows keep browsing state in memory. Strict mode adds stronger cookie and canvas restrictions.</p>"
            "<p><a href='virgin://version'>virgin://version</a> · <a href='virgin://newtab'>virgin://newtab</a></p>")
            .arg(QString::fromLatin1(app::kVersion),
                 isPrivate ? QStringLiteral("Private Window") : QStringLiteral("Normal Window")));
    });
}

void BrowserWindow::setupShortcuts() {
    new QShortcut(QKeySequence("Ctrl+Tab"), this, [this]{
        if (tabs_->count() > 0) tabs_->setCurrentIndex((tabs_->currentIndex() + 1) % tabs_->count());
    });
    new QShortcut(QKeySequence("Ctrl+Shift+Tab"), this, [this]{
        if (tabs_->count() > 0) tabs_->setCurrentIndex((tabs_->currentIndex() - 1 + tabs_->count()) % tabs_->count());
    });
    new QShortcut(QKeySequence::Quit, this, [this]{ close(); });
}

void BrowserWindow::openInitialPage() {
    if (!sessionRestored_) {
        // Private windows never restore
        if (profile_ && profile_->isOffTheRecord()) {
            tabs_->createTab(homeUrl_, true);
        } else if (!restoreSession()) {
            tabs_->createTab(homeUrl_, true);
        }
        sessionRestored_ = true;
    }
    updateNavButtons();
    updateBookmarkButton();
    updatePrivateIndicator();
}

bool BrowserWindow::restoreSession() {
    if (!sessions_ || !tabs_) return false;
    if (profile_ && profile_->isOffTheRecord()) return false; // INV-02 never restore private
    if (profile_ && profile_->type() != profiles::VirginProfile::Type::Normal) return false;
    virgin::storage::SessionWindow win;
    if (!sessions_->restoreWindow(win)) return false;
    if (win.tabs.isEmpty()) return false;
    if (!win.geometry.isEmpty()) restoreGeometry(win.geometry);
    QList<QUrl> urls;
    for (auto& t : win.tabs) urls << t.url;
    tabs_->restoreTabs(urls, win.activeIndex);
    statusBar()->showMessage(QString("Restored %1 tabs").arg(urls.size()), 3000);
    return true;
}

void BrowserWindow::saveSession() {
    if (!sessions_ || !tabs_ || !profile_) return;
    if (profile_->isOffTheRecord()) return; // INV-02: private URLs never written
    if (profile_->type() != profiles::VirginProfile::Type::Normal) return;
    virgin::storage::SessionWindow win;
    win.geometry = saveGeometry();
    win.activeIndex = tabs_->currentIndexSafe();
    for (auto* t : tabs_->allTabs()) {
        virgin::storage::SessionTab st;
        st.url = t->url();
        st.title = t->title();
        st.pinned = false;
        st.containerId = profile_->id();
        if (st.url.toString()=="virgin://newtab" && tabs_->count()>1) continue;
        if (st.url.isEmpty()) continue;
        // INV-02 double guard: skip private-like URLs
        if (st.url.host() == "private" && st.url.scheme()=="virgin") continue;
        win.tabs.append(st);
    }
    if (win.tabs.isEmpty()) return;
    if (!sessions_->saveWindow(win)) {
        statusBar()->showMessage("Could not save the browser session", 8000);
    }
}

void BrowserWindow::onBack() { if (auto* t = tabs_->currentTab()) t->view()->back(); }
void BrowserWindow::onForward() { if (auto* t = tabs_->currentTab()) t->view()->forward(); }
void BrowserWindow::onReload() { if (auto* t = tabs_->currentTab()) t->reload(); }
void BrowserWindow::onStop() { if (auto* t = tabs_->currentTab()) t->stop(); }
void BrowserWindow::onHome() { nav_->navigate(homeUrl_); }

void BrowserWindow::refreshHomeUrl() {
    const QString configured = settings_
        ? settings_->value("homepage", "virgin://newtab").toString().trimmed()
        : QStringLiteral("virgin://newtab");
    const QUrl candidate = QUrl::fromUserInput(configured);
    const QString scheme = candidate.scheme().toLower();
    if (!candidate.isValid() || candidate.isEmpty() ||
        (scheme != QStringLiteral("http") && scheme != QStringLiteral("https") &&
         (scheme != QStringLiteral("virgin") || !security::UrlSafety::isVirginInternalUrlAllowed(candidate)))) {
        homeUrl_ = QUrl(QStringLiteral("virgin://newtab"));
        return;
    }
    homeUrl_ = candidate;
}
void BrowserWindow::onBookmarkToggle() {
    if (profile_ && profile_->isOffTheRecord()) {
        QMessageBox::information(this, "Bookmarks", "Bookmarks are not saved from private windows.");
        return;
    }
    auto* tab = tabs_->currentTab();
    if (!tab || !bookmarks_) return;
    QUrl url = tab->url();
    if (url.isEmpty() || url.scheme()=="virgin") return;
    if (bookmarks_->contains(url)) {
        bookmarks_->remove(url);
        bookmarkBtn_->setChecked(false);
        bookmarkBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Bookmark));
        statusBar()->showMessage("Removed bookmark", 2000);
    } else {
        bookmarks_->add(url, tab->title());
        bookmarkBtn_->setChecked(true);
        bookmarkBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::BookmarkFilled, QColor("#8a6928")));
        statusBar()->showMessage("Bookmarked", 2000);
    }
}
void BrowserWindow::onShieldClicked() {
    if (privacyPanel_->isVisible()) privacyPanel_->hide();
    else {
        updatePrivacyPanel();
        QPoint p = shieldBtn_->mapToGlobal(QPoint(0, shieldBtn_->height()));
        QPoint local = mapFromGlobal(p);
        privacyPanel_->move(local.x() - privacyPanel_->width() + shieldBtn_->width(), toolbar_->height());
        privacyPanel_->show();
        privacyPanel_->raise();
        bool isPrivate = profile_ && profile_->isOffTheRecord();
        bool isStrict = profile_ && profile_->isStrict();
        privacyPanel_->setProtectionLevel(isPrivate ? "PRIVATE" : (isStrict ? "STRICT" : "STANDARD"));
        // Ensure shield tooltip reflects live counts
        updateShieldBadge();
    }
}
void BrowserWindow::onNewTab() { tabs_->createTab(QUrl(), true); }
void BrowserWindow::onCloseTab() { tabs_->closeCurrentTab(); if (tabs_->count()==0) close(); }
void BrowserWindow::onReopenTab() {
    if (tabs_->canReopen()) {
        tabs_->reopenLastClosedTab();
        statusBar()->showMessage("Reopened tab", 2000);
    }
}
void BrowserWindow::onDuplicateTab() {
    auto* t = tabs_->currentTab();
    if (t) tabs_->createTab(t->url(), true);
}
void BrowserWindow::onFocusOmnibox() { omnibox_->setFocus(); omnibox_->selectAll(); }
void BrowserWindow::onPrivateWindow() {
    if (auto* app = virgin::app::VirginApp::instance()) {
        auto* win = app->createPrivateWindow();
        win->openInitialPage();
    } else {
        QMessageBox::information(this, "Private Window", "Failed to create private window — app not found.");
    }
}
void BrowserWindow::onContainerWindow() {
    bool accepted = false;
    const QString id = QInputDialog::getText(
        this, "Open Container", "Container ID (letters, numbers, _ or -):",
        QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted) return;
    if (!profiles::ContainerManager::isValidId(id)) {
        QMessageBox::warning(this, "Invalid Container ID",
                             "Use 1–64 ASCII letters, numbers, underscores, or hyphens.");
        return;
    }
    auto* app = virgin::app::VirginApp::instance();
    if (!app || !app->createContainerWindow(id)) {
        QMessageBox::critical(this, "Container", "Could not create the container profile.");
    }
}
void BrowserWindow::onManageContainers() {
    auto* app = virgin::app::VirginApp::instance();
    if (!app || !app->profileManager()) return;
    const QStringList ids = app->profileManager()->containerIds();
    if (ids.isEmpty()) {
        QMessageBox::information(this, "Containers", "No containers exist yet.");
        return;
    }
    bool accepted = false;
    const QString id = QInputDialog::getItem(this, "Delete Container", "Container:", ids, 0, false, &accepted);
    if (!accepted || id.isEmpty()) return;
    const auto choice = QMessageBox::warning(
        this, "Delete Container",
        QString("Permanently delete container '%1' and its cookies, storage, cache, and permissions?\n\n"
                "Close every window using it first.").arg(id),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice != QMessageBox::Yes) return;
    if (!app->removeContainer(id)) {
        QMessageBox::warning(this, "Container Not Deleted",
                             "The container is open or its storage could not be removed.");
    } else {
        statusBar()->showMessage("Deleted container " + id, 5000);
    }
}
void BrowserWindow::onPanic() {
    // Sec 30: destroy private sessions
    if (auto* app = virgin::app::VirginApp::instance()) {
        app->panic();
        statusBar()->showMessage("Panic — private sessions destroyed, clipboard cleared", 4000);
    }
}
void BrowserWindow::onShowHistory() {
    if (profile_ && profile_->type() != profiles::VirginProfile::Type::Normal) {
        QMessageBox::information(this, "History", "Virgin history is disabled for private and container windows.");
        return;
    }
    virgin::ui::HistoryDialog dlg(history_, tabs_, this);
    dlg.exec();
}
void BrowserWindow::onShowBookmarks() {
    if (profile_ && profile_->isOffTheRecord()) {
        QMessageBox::information(this, "Bookmarks", "Bookmarks not available in Private (ephemeral). Use a normal window.");
        return;
    }
    virgin::ui::BookmarkDialog dlg(bookmarks_, tabs_, this);
    dlg.exec();
    updateBookmarkButton();
}
void BrowserWindow::onShowDownloads() {
    if (downloadShelf_) {
        downloadShelf_->setVisible(!downloadShelf_->isVisible());
        if (downloadShelf_->isVisible()) downloadShelf_->raise();
    } else {
        QMessageBox::information(this, "Downloads", "No downloads yet. Files go to ~/Downloads");
    }
}
void BrowserWindow::onShowSettings() {
    virgin::ui::SettingsDialog dlg(settings_, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshHomeUrl();
        bool strict = settings_->strictMode();
        if (profile_ && !profile_->isOffTheRecord()) {
            profile_->setStrictMode(strict);
            profile_->setWebRtcPublicOnly(settings_->value("webrtc_public_only", true).toBool());
        }
        QString cm = settings_->value("cookie_mode", "standard").toString();
        if (profile_) {
            if (profile_->isOffTheRecord()) profile_->setCookieMode(virgin::privacy::CookieMode::Private);
            else if (cm == "strict") profile_->setCookieMode(virgin::privacy::CookieMode::Strict);
            else if (cm == "ephemeral") profile_->setCookieMode(virgin::privacy::CookieMode::Ephemeral);
            else profile_->setCookieMode(virgin::privacy::CookieMode::Standard);
        }
        bool adblockOn = settings_->value("adblock_enabled", true).toBool();
        if (profile_) profile_->setAdblockEnabled(adblockOn);
        int keepDays = settings_->historyRetentionDays();
        if (keepDays >=0 && keepDays != -1 && history_) {
            history_->deleteExpired(keepDays);
        } else if (keepDays == 0 && history_) {
            history_->clearAll();
        }
        updatePrivateIndicator();
        updateShieldBadge();
        updatePrivacyPanel();
    }
}

void BrowserWindow::onUpdateFilters() {
    if (!profile_ || !profile_->filterUpdater()) return;
    profile_->filterUpdater()->updateFromNetwork();
}

void BrowserWindow::onFilterUpdateProgress(int percent) {
    statusBar()->showMessage(QString("Updating protection lists… %1%").arg(percent));
}

void BrowserWindow::onFilterUpdateFinished(bool success, const QString& message) {
    statusBar()->showMessage(success ? "Protection lists are up to date"
                                     : "Protection list update failed: " + message,
                             success ? 3500 : 8000);
    updateShieldBadge();
}

void BrowserWindow::onClearSiteData(const QUrl& site) {
    if (!profile_ || !profile_->qtProfile() || site.host().isEmpty()) return;
    privacyPanel_->hide();
    const auto choice = QMessageBox::warning(
        this, "Clear Browser Data",
        QString("Qt WebEngine cannot reliably clear every storage type for only %1.\n\n"
                "Clear all cookies and HTTP cache in the current profile?").arg(site.host()),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice != QMessageBox::Yes) return;
    profile_->qtProfile()->cookieStore()->deleteAllCookies();
    profile_->qtProfile()->clearHttpCache();
    statusBar()->showMessage("Profile cookies and cache cleared", 5000);
}

void BrowserWindow::onForgetSite(const QUrl& site) {
    if (!profile_ || !profile_->qtProfile() || site.host().isEmpty()) return;
    privacyPanel_->hide();
    const QString host = site.host().toLower();
    const auto choice = QMessageBox::warning(
        this, "Forget This Site",
        QString("Remove Virgin history, bookmarks, and permission choices for %1?\n\n"
                "Because Qt WebEngine lacks complete per-origin deletion, this also clears all cookies and HTTP cache in this profile.").arg(host),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice != QMessageBox::Yes) return;
    if (history_ && !profile_->isOffTheRecord()) history_->removeHost(host);
    if (bookmarks_ && !profile_->isOffTheRecord()) bookmarks_->removeHost(host);
    if (profile_->permissionManager()) profile_->permissionManager()->resetPermissions(site);
    profile_->qtProfile()->cookieStore()->deleteAllCookies();
    profile_->qtProfile()->clearHttpCache();
    updateBookmarkButton();
    statusBar()->showMessage("Forgot " + host, 5000);
}
void BrowserWindow::onActiveTabChanged(BrowserTab* tab) {
    if (!tab) return;
    connect(tab, &BrowserTab::urlChanged, this, &BrowserWindow::onTabUrlChanged, Qt::UniqueConnection);
    connect(tab, &BrowserTab::loadingChanged, this, &BrowserWindow::onTabLoadingChanged, Qt::UniqueConnection);
    connect(tab, &BrowserTab::loadProgressChanged, this, &BrowserWindow::onTabLoadProgress, Qt::UniqueConnection);
    connect(tab, &BrowserTab::crashedChanged, this, &BrowserWindow::onTabCrashedChanged, Qt::UniqueConnection);
    connect(tab, &BrowserTab::requestFullScreen, this, &BrowserWindow::onFullScreenRequested, Qt::UniqueConnection);
    connect(tab, &BrowserTab::permissionRequested, this, &BrowserWindow::onPermissionRequested, Qt::UniqueConnection);
    connect(tab, &BrowserTab::certificateError, this, &BrowserWindow::onCertificateError, Qt::UniqueConnection);
    connect(tab, &BrowserTab::externalSchemeRequested, this, &BrowserWindow::onExternalSchemeRequested, Qt::UniqueConnection);
    connect(tab, &BrowserTab::popupBlocked, this, &BrowserWindow::onPopupBlocked, Qt::UniqueConnection);
    omnibox_->setUrl(tab->url());
    updateNavButtons();
    updateBookmarkButton();
    connect(tab, &BrowserTab::titleChanged, this, &BrowserWindow::onCurrentTabTitleChanged, Qt::UniqueConnection);
    if (omnibox_) {
        const bool privateMode = profile_ && profile_->isOffTheRecord();
        omnibox_->setHistoryStore(privateMode ? nullptr : history_);
        omnibox_->setBookmarkStore(privateMode ? nullptr : bookmarks_);
    }
    updatePrivateIndicator();
}

void BrowserWindow::onPopupBlocked(const QUrl& url) {
    statusBar()->showMessage("Popup blocked: " + url.toString(), 3000);
}

void BrowserWindow::onCurrentTabTitleChanged(const QString& title) {
    auto* tab = qobject_cast<BrowserTab*>(sender());
    if (!tab || tab != tabs_->currentTab()) return;
    QString suffix = " — Virgin";
    if (profile_ && profile_->isOffTheRecord()) suffix += " (Private)";
    else if (profile_ && profile_->type() == profiles::VirginProfile::Type::Container) {
        suffix += " [" + profile_->id() + "]";
    }
    setWindowTitle((title.isEmpty() ? "New Tab" : title) + suffix);
}

void BrowserWindow::onTabUrlChanged(const QUrl& url) {
    auto* tab = qobject_cast<BrowserTab*>(sender());
    if (tab && tab == tabs_->currentTab()) {
        omnibox_->setUrl(url);
        if (privacyPanel_) privacyPanel_->setUrl(url);
        updateBookmarkButton();
        updatePrivateIndicator();
    }
    updateNavButtons();
}

void BrowserWindow::onTabLoadingChanged(bool loading) {
    auto* tab = qobject_cast<BrowserTab*>(sender());
    bool isCurrent = !tab || tab == tabs_->currentTab();
    if (!isCurrent) return;
    reloadBtn_->setVisible(!loading);
    stopBtn_->setVisible(loading);
    if (loading) { progress_->show(); progress_->setValue(0); }
    else { progress_->hide(); progress_->setValue(100); }
    updateNavButtons();
    updateBookmarkButton();
}

void BrowserWindow::onTabLoadProgress(int progress) {
    auto* tab = qobject_cast<BrowserTab*>(sender());
    if (!tab || tab != tabs_->currentTab() || !progress_) return;
    progress_->setValue(progress);
}

void BrowserWindow::onTabCrashedChanged(bool crashed) {
    auto* tab = qobject_cast<BrowserTab*>(sender());
    if (!tab) return;
    int idx = tabs_->indexOf(tab);
    if (idx==-1) return;
    if (crashed) {
        tabs_->setTabText(idx, "[!] Crashed");
        tabs_->setTabToolTip(idx, "Renderer crashed — click Reload");
        statusBar()->showMessage("Tab crashed — isolated, other tabs safe. Press Reload.", 5000);
    }
}

void BrowserWindow::onFullScreenRequested(bool enable) {
    isFullScreen_ = enable;
    if (enable) {
        toolbar_->hide();
        menuBar()->hide();
        statusBar()->hide();
        if (downloadShelf_) downloadShelf_->hide();
        showFullScreen();
    } else {
        toolbar_->show();
        menuBar()->show();
        statusBar()->show();
        showNormal();
    }
}

void BrowserWindow::onOmniboxReturnPressed(const QString& text) {
    nav_->navigateCurrent(text);
}

void BrowserWindow::onPermissionRequested(const QUrl& origin, virgin::privacy::PermissionFeature feature) {
    Q_UNUSED(origin) Q_UNUSED(feature)
    // VirginPage already shows PermissionDialog synchronously; we just update shield
    if (privacyPanel_) privacyPanel_->setUrl(tabs_->currentTab() ? tabs_->currentTab()->url() : QUrl());
    statusBar()->showMessage("Permission requested: " + origin.host(), 3000);
}

void BrowserWindow::onCertificateError(const QUrl& url, const QString& errorString, bool overridable) {
    Q_UNUSED(overridable)
    statusBar()->showMessage("Certificate error for " + url.host() + ": " + errorString, 5000);
    if (privacyPanel_) privacyPanel_->setUrl(url);
}

void BrowserWindow::onExternalSchemeRequested(const QUrl& url) {
    statusBar()->showMessage("A site asked to open another application (" + url.scheme() + ")", 3000);
}

void BrowserWindow::updateNavButtons() {
    auto* tab = tabs_->currentTab();
    if (!tab) return;
    backBtn_->setEnabled(tab->view()->history()->canGoBack());
    forwardBtn_->setEnabled(tab->view()->history()->canGoForward());
}

void BrowserWindow::updateBookmarkButton() {
    auto* tab = tabs_->currentTab();
    if (!tab || !bookmarks_ || (profile_ && profile_->isOffTheRecord())) {
        bookmarkBtn_->setChecked(false);
        bookmarkBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Bookmark));
        bookmarkBtn_->setEnabled(profile_ ? !profile_->isOffTheRecord() : true);
        return;
    }
    QUrl url = tab->url();
    bool isBookmarked = bookmarks_->contains(url);
    bookmarkBtn_->setChecked(isBookmarked);
    bookmarkBtn_->setIcon(virgin::ui::pixelIcon(
        isBookmarked ? virgin::ui::PixelIcon::BookmarkFilled : virgin::ui::PixelIcon::Bookmark,
        isBookmarked ? QColor("#8a6928") : QColor("#3f4347")));
    bookmarkBtn_->setToolTip(isBookmarked ? "Remove bookmark" : "Bookmark this page");
}

void BrowserWindow::updatePrivateIndicator() {
    bool isPrivate = profile_ && profile_->isOffTheRecord();
    bool isStrict = profile_ && profile_->isStrict();
    if (isPrivate) {
        shieldBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Shield, QColor("#5e5368")));
    } else if (isStrict) {
        shieldBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Shield, QColor("#745c2f")));
    } else {
        shieldBtn_->setIcon(virgin::ui::pixelIcon(virgin::ui::PixelIcon::Shield, QColor("#397147")));
    }
    updateShieldBadge();
    updatePrivacyPanel();
}

void BrowserWindow::updateShieldBadge() {
    if (!profile_ || !profile_->adblockEngine() || !shieldBtn_) return;
    auto* engine = profile_->adblockEngine();
    const uint64_t total = engine->totalBlocked();
    QString tip;
    if (profile_->isOffTheRecord()) {
        tip = "Private — strict protections, no persistence";
    } else if (profile_->isStrict()) {
        tip = "Strict — public-only WebRTC, canvas reads denied, third-party cookies blocked";
    } else {
        tip = "Standard — request blocking and permissions denied by default";
    }
    tip += QString("\nBlocked: %1 (trackers %2, ads %3)")
               .arg(total).arg(engine->trackersBlocked()).arg(engine->adsBlocked());
    shieldBtn_->setToolTip(tip);
}

void BrowserWindow::updatePrivacyPanel() {
    if (!privacyPanel_ || !profile_ || !profile_->adblockEngine()) return;
    auto* engine = profile_->adblockEngine();
    auto* tab = tabs_->currentTab();
    QUrl cur = tab ? tab->url() : QUrl();
    QString host = cur.host().toLower();
    const auto site = host.isEmpty() ? virgin::adblock::SiteStats{} : engine->siteStatsForHost(host);
    privacyPanel_->setStats(static_cast<int>(site.requests), static_cast<int>(site.blocked),
                            static_cast<int>(site.trackers), static_cast<int>(site.ads));
    privacyPanel_->setUrl(cur);

    const bool isPrivate = profile_->isOffTheRecord();
    const bool isStrict = profile_->isStrict();
    privacyPanel_->setProtectionLevel(isPrivate ? "PRIVATE" : (isStrict ? "STRICT" : "STANDARD"));

    QString cookieState;
    if (profile_->blocksThirdPartyCookies()) cookieState = "BLOCK";
    else if (profile_->cookieMode() == virgin::privacy::CookieMode::Ephemeral) cookieState = "ALLOW / SESSION";
    else cookieState = "ALLOW";
    const QString canvasState = profile_->canvasReadsBlocked() ? "BLOCK" : "ALLOW";
    const QString webrtcState = profile_->webRtcPublicOnly() ? "PUBLIC ONLY" : "DEFAULT";

    auto permissionState = [this, &cur](virgin::privacy::PermissionFeature feature) {
        auto* manager = profile_->permissionManager();
        if (!manager || cur.host().isEmpty() || !manager->hasEntry(cur, feature)) return QString("ASK");
        const auto decision = manager->storedDecision(cur, feature);
        return decision == virgin::privacy::PermissionManager::Decision::Granted
            ? QString("ALLOW") : QString("BLOCK");
    };
    const QString permissions = QString("Location    %1\nCamera      %2\nMicrophone  %3")
        .arg(permissionState(virgin::privacy::PermissionGeolocation))
        .arg(permissionState(virgin::privacy::PermissionMediaVideoCapture))
        .arg(permissionState(virgin::privacy::PermissionMediaAudioCapture));
    privacyPanel_->setProtectionDetails(cookieState, canvasState, webrtcState, permissions);

    if (site.blocked > 0) {
        statusBar()->showMessage(QString("Blocked %1 requests for %2").arg(site.blocked).arg(host), 3000);
    }
}

void BrowserWindow::onRequestBlocked(const virgin::adblock::BlockResult& result, const QUrl& requestUrl, const QUrl& firstParty) {
    Q_UNUSED(result)
    Q_UNUSED(requestUrl)
    updateShieldBadge();
    auto* cur = tabs_->currentTab();
    if (cur && cur->url().host().toLower() == firstParty.host().toLower()) {
        updatePrivacyPanel();
    }
}

void BrowserWindow::closeEvent(QCloseEvent* event) {
    saveSession();
    event->accept();
}

void BrowserWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && isFullScreen_) {
        onFullScreenRequested(false);
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

} // namespace virgin::browser
