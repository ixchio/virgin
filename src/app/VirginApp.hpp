#pragma once

#include <QApplication>
#include <QList>
#include <memory>

namespace virgin::profiles { class ProfileManager; class VirginProfile; }
namespace virgin::storage { class SettingsStore; class HistoryStore; class BookmarkStore; class SessionStore; }
namespace virgin::browser { class BrowserWindow; }

namespace virgin::app {

class VirginApp final : public QObject {
    Q_OBJECT
public:
    explicit VirginApp(int& argc, char** argv);
    ~VirginApp() override;

    [[nodiscard]] bool initialize();
    int run();
    void shutdown();

    profiles::ProfileManager* profileManager() const { return profiles_.get(); }
    storage::SettingsStore* settings() const { return settings_.get(); }
    storage::HistoryStore* history() const { return history_.get(); }
    storage::BookmarkStore* bookmarks() const { return bookmarks_.get(); }
    storage::SessionStore* sessions() const { return sessions_.get(); }

    browser::BrowserWindow* createWindow(profiles::VirginProfile* profile = nullptr,
                                         bool ownsSession = false);
    browser::BrowserWindow* createPrivateWindow();
    browser::BrowserWindow* createContainerWindow(const QString& id);
    bool removeContainer(const QString& id);
    void panic(); // Sec 30
    QList<browser::BrowserWindow*> windows() const { return windows_; }
    static VirginApp* instance() { return s_instance; }

private slots:
    void onWindowClosed(QObject* obj);

private:
    bool verifyRuntimeSecurity() const;
    void installCrashHandlers();
    void createFirstWindow();
    void registerVirginScheme();
    void applyProfileSettings(profiles::VirginProfile* profile);

    std::unique_ptr<QApplication> qtApp_;
    std::unique_ptr<profiles::ProfileManager> profiles_;
    std::unique_ptr<storage::SettingsStore> settings_;
    std::unique_ptr<storage::HistoryStore> history_;
    std::unique_ptr<storage::BookmarkStore> bookmarks_;
    std::unique_ptr<storage::SessionStore> sessions_;

    QList<browser::BrowserWindow*> windows_;
    browser::BrowserWindow* mainWindow_ = nullptr;
    int& argc_;
    char** argv_;
    static VirginApp* s_instance;
};

} // namespace virgin::app
