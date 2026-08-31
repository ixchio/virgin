#include "BrowserTab.hpp"
#include "VirginPage.hpp"
#include "profiles/VirginProfile.hpp"
#include "adblock/AdBlockEngine.hpp"

#include <QWebEngineProfile>
#include <QWebEngineFullScreenRequest>
#include <QTimer>
#include <QIcon>

namespace virgin::browser {

BrowserTab::BrowserTab(virgin::profiles::VirginProfile* profileWrapper, QWidget* parent)
    : QWidget(parent), profile_(profileWrapper)
{
    QWebEngineProfile* qtProfile = profile_ ? profile_->qtProfile() : QWebEngineProfile::defaultProfile();

    page_ = new VirginPage(qtProfile, this);
    view_ = new QWebEngineView(this);
    view_->setPage(page_);

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0,0,0,0);
    layout_->setSpacing(0);
    layout_->addWidget(view_);
    setLayout(layout_);

    connect(view_, &QWebEngineView::loadStarted, this, &BrowserTab::onLoadStarted);
    connect(view_, &QWebEngineView::loadProgress, this, &BrowserTab::onLoadProgress);
    connect(view_, &QWebEngineView::loadFinished, this, &BrowserTab::onLoadFinished);
    connect(page_, &VirginPage::urlChanged, this, &BrowserTab::onUrlChanged);
    connect(page_, &VirginPage::titleChanged, this, &BrowserTab::onTitleChanged);
    connect(page_, &VirginPage::iconChanged, this, &BrowserTab::onIconChanged);
    connect(page_, &VirginPage::createNewTabRequested, this, &BrowserTab::newTabRequested);
    connect(page_, &QWebEnginePage::renderProcessTerminated,
            this, &BrowserTab::onRenderProcessTerminated);
    connect(page_, &QWebEnginePage::fullScreenRequested,
            this, &BrowserTab::onFullScreenRequested);
    connect(page_, &VirginPage::permissionRequested, this, &BrowserTab::permissionRequested);
    connect(page_, &VirginPage::certificateErrorIntercepted, this, &BrowserTab::certificateError);
    connect(page_, &VirginPage::externalSchemeRequested, this, &BrowserTab::externalSchemeRequested);
    connect(page_, &VirginPage::popupBlocked, this, &BrowserTab::popupBlocked);
}

BrowserTab::~BrowserTab() = default;

QUrl BrowserTab::url() const { return view_ ? view_->url() : QUrl(); }
QString BrowserTab::title() const { return view_ ? view_->title() : QString(); }

void BrowserTab::navigate(const QUrl& url) {
    isCrashed_ = false;
    emit crashedChanged(false);
    if (view_) view_->setUrl(url);
}

void BrowserTab::reload() {
    if (isCrashed_) { isCrashed_ = false; emit crashedChanged(false); }
    if (view_) view_->reload();
}

void BrowserTab::stop() { if (view_) view_->stop(); }

void BrowserTab::handleCrash() { isCrashed_ = true; emit crashedChanged(true); }

void BrowserTab::injectCosmeticCss() {
    if (!profile_ || !profile_->adblockEngine()) return;
    QUrl cur = url();
    if (cur.isEmpty() || cur.scheme() == "virgin" || cur.scheme() == "about" || cur.scheme() == "data") return;
    // Sec 16: domain-scoped minimal CSS, no global bloat
    QString css = profile_->adblockEngine()->cosmeticCssForUrl(cur);
    if (css.isEmpty()) return;
    // Sec 17: no scriptlets, only CSS hiding
    QString escaped = css;
    escaped.replace("\\", "\\\\");
    escaped.replace("'", "\\'");
    escaped.replace("\n", " ");
    QString js = QStringLiteral(
        "(function(){"
        "let id='virgin-cosmetic';"
        "let s=document.getElementById(id);"
        "if(!s){ s=document.createElement('style'); s.id=id; s.type='text/css'; (document.head||document.documentElement).appendChild(s); }"
        "s.textContent='%1';"
        "})();"
    ).arg(escaped);
    if (page_) page_->runJavaScript(js);
}

void BrowserTab::onLoadStarted() { isLoading_ = true; emit loadingChanged(true); }
void BrowserTab::onLoadProgress(int progress) { emit loadProgressChanged(progress); }
void BrowserTab::onLoadFinished(bool ok) {
    Q_UNUSED(ok)
    isLoading_ = false;
    isCrashed_ = false;
    emit crashedChanged(false);
    emit loadingChanged(false);
    injectCosmeticCss();
}
void BrowserTab::onUrlChanged(const QUrl& url) {
    emit urlChanged(url);
    // For SPA navigation via history API, inject again
    // Use singleShot to ensure DOM ready
    QTimer::singleShot(300, this, &BrowserTab::injectCosmeticCss);
}
void BrowserTab::onTitleChanged(const QString& t) { emit titleChanged(t); }
void BrowserTab::onIconChanged(const QIcon& i) { emit favIconChanged(i); }

void BrowserTab::onRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus status, int code) {
    Q_UNUSED(code)
    if (status == QWebEnginePage::AbnormalTerminationStatus ||
        status == QWebEnginePage::CrashedTerminationStatus ||
        status == QWebEnginePage::KilledTerminationStatus) {
        isCrashed_ = true;
        emit crashedChanged(true);
        emit titleChanged("[!] Crashed — " + url().host());
    }
}

void BrowserTab::onFullScreenRequested(QWebEngineFullScreenRequest request) {
    request.accept();
    emit requestFullScreen(request.toggleOn());
}

} // namespace virgin::browser
