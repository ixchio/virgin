#pragma once

#include <QMainWindow>
#include <QToolBar>
#include <QPushButton>
#include <QProgressBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QKeySequence>
#include <QLabel>
#include <QWebEnginePage>
#include <QWebEnginePermission>

namespace virgin::profiles { class VirginProfile; }
namespace virgin::storage { class SettingsStore; class HistoryStore; class BookmarkStore; class SessionStore; }
namespace virgin::downloads { class DownloadShelf; }
namespace virgin::adblock { struct BlockResult; }

namespace virgin::browser { class TabManager; class NavigationController; class BrowserTab; }
namespace virgin::ui { class Omnibox; class PrivacyPanel; }

namespace virgin::browser {

class BrowserWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit BrowserWindow(virgin::profiles::VirginProfile* profile,
                           virgin::storage::SettingsStore* settings,
                           virgin::storage::HistoryStore* history,
                           virgin::storage::BookmarkStore* bookmarks,
                           virgin::storage::SessionStore* sessions,
                           QWidget* parent = nullptr);
    ~BrowserWindow() override;

    void openInitialPage();
    TabManager* tabManager() const { return tabs_; }
    virgin::profiles::VirginProfile* profile() const { return profile_; }

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onBack();
    void onForward();
    void onReload();
    void onStop();
    void onHome();
    void onShieldClicked();
    void onBookmarkToggle();
    void onNewTab();
    void onCloseTab();
    void onReopenTab();
    void onDuplicateTab();
    void onFocusOmnibox();
    void onPrivateWindow();
    void onContainerWindow();
    void onManageContainers();
    void onPanic();
    void onShowHistory();
    void onShowBookmarks();
    void onShowDownloads();
    void onShowSettings();
    void onUpdateFilters();
    void onClearSiteData(const QUrl& site);
    void onForgetSite(const QUrl& site);
    void onActiveTabChanged(BrowserTab* tab);
    void onTabUrlChanged(const QUrl& url);
    void onTabLoadingChanged(bool loading);
    void onTabLoadProgress(int progress);
    void onTabCrashedChanged(bool crashed);
    void onFullScreenRequested(bool enable);
    void onOmniboxReturnPressed(const QString& text);
    void onPermissionRequested(const QUrl& origin, QWebEnginePermission::PermissionType feature);
    void onCertificateError(const QUrl& url, const QString& errorString, bool overridable);
    void onExternalSchemeRequested(const QUrl& url);
    void onRequestBlocked(const virgin::adblock::BlockResult& result, const QUrl& requestUrl, const QUrl& firstParty);
    void onPopupBlocked(const QUrl& url);
    void onCurrentTabTitleChanged(const QString& title);
    void onFilterUpdateProgress(int percent);
    void onFilterUpdateFinished(bool success, const QString& message);

private:
    void setupToolbar();
    void setupMenus();
    void setupShortcuts();
    void updateNavButtons();
    void updateBookmarkButton();
    void updatePrivateIndicator();
    void updatePrivacyPanel();
    void updateShieldBadge();
    void saveSession();
    bool restoreSession();

    virgin::profiles::VirginProfile* profile_ = nullptr;
    virgin::storage::SettingsStore* settings_ = nullptr;
    virgin::storage::HistoryStore* history_ = nullptr;
    virgin::storage::BookmarkStore* bookmarks_ = nullptr;
    virgin::storage::SessionStore* sessions_ = nullptr;

    TabManager* tabs_ = nullptr;
    NavigationController* nav_ = nullptr;

    QToolBar* toolbar_ = nullptr;
    virgin::ui::Omnibox* omnibox_ = nullptr;
    QPushButton* backBtn_ = nullptr;
    QPushButton* forwardBtn_ = nullptr;
    QPushButton* reloadBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QPushButton* homeBtn_ = nullptr;
    QPushButton* bookmarkBtn_ = nullptr;
    QPushButton* shieldBtn_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QLabel* privateBadge_ = nullptr;

    virgin::ui::PrivacyPanel* privacyPanel_ = nullptr;
    virgin::downloads::DownloadShelf* downloadShelf_ = nullptr;

    QUrl homeUrl_ = QUrl("virgin://newtab");
    bool isFullScreen_ = false;
    bool sessionRestored_ = false;
};

} // namespace virgin::browser
