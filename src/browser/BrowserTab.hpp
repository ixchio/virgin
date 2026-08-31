#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QVBoxLayout>
#include <QUrl>
#include <QIcon>
#include <QWebEnginePage>
#include <QWebEngineFullScreenRequest>
#include <QWebEnginePermission>

namespace virgin::profiles { class VirginProfile; }

namespace virgin::browser {

class VirginPage;

class BrowserTab final : public QWidget {
    Q_OBJECT
public:
    explicit BrowserTab(virgin::profiles::VirginProfile* profileWrapper,
                        QWidget* parent = nullptr);
    ~BrowserTab() override;

    QWebEngineView* view() const { return view_; }
    VirginPage* page() const { return page_; }
    QUrl url() const;
    QString title() const;
    bool isLoading() const { return isLoading_; }
    bool isCrashed() const { return isCrashed_; }

    void navigate(const QUrl& url);
    void reload();
    void stop();
    void handleCrash();
    void injectCosmeticCss();

signals:
    void titleChanged(const QString& title);
    void urlChanged(const QUrl& url);
    void favIconChanged(const QIcon& icon);
    void loadingChanged(bool loading);
    void loadProgressChanged(int progress);
    void newTabRequested(const QUrl& url);
    void crashedChanged(bool crashed);
    void requestFullScreen(bool enable);
    void permissionRequested(const QUrl& origin, QWebEnginePermission::PermissionType feature);
    void certificateError(const QUrl& url, const QString& errorString, bool overridable);
    void externalSchemeRequested(const QUrl& url);
    void popupBlocked(const QUrl& url);

private slots:
    void onLoadStarted();
    void onLoadProgress(int progress);
    void onLoadFinished(bool ok);
    void onUrlChanged(const QUrl& url);
    void onTitleChanged(const QString& title);
    void onIconChanged(const QIcon& icon);
    void onRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus status, int code);
    void onFullScreenRequested(QWebEngineFullScreenRequest request);

private:
    QWebEngineView* view_ = nullptr;
    VirginPage* page_ = nullptr;
    QVBoxLayout* layout_ = nullptr;
    virgin::profiles::VirginProfile* profile_ = nullptr;
    bool isLoading_ = false;
    bool isCrashed_ = false;
    QWidget* fullScreenView_ = nullptr;
};

} // namespace virgin::browser
