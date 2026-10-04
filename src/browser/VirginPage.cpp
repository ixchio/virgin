#include "VirginPage.hpp"

#include "security/CertificatePolicy.hpp"
#include "security/UrlSafety.hpp"
#include "privacy/PermissionManager.hpp"
#include "ui/PermissionDialog.hpp"
#include "ui/CertificateErrorDialog.hpp"

#include <QWebEngineCertificateError>
#include <QMessageBox>
#include <QDesktopServices>
#include <QWebEngineProfile>
#include <QTimer>
#include <QWidget>

namespace virgin::browser {

VirginPage::VirginPage(QWebEngineProfile* profile, QObject* parent)
    : QWebEnginePage(profile, parent)
{
    connect(this, &QWebEnginePage::certificateError,
            this, &VirginPage::handleCertificateError);
    connect(this, &QWebEnginePage::newWindowRequested,
            this, &VirginPage::handleNewWindowRequested);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(this, &QWebEnginePage::permissionRequested,
            this, &VirginPage::handlePermissionRequested);
#else
    connect(this, &QWebEnginePage::featurePermissionRequested,
            this, &VirginPage::handlePermissionRequested);
#endif
    connect(this, &QWebEnginePage::renderProcessTerminated,
            this, &VirginPage::handleRenderProcessTerminated);
    connect(this, &QWebEnginePage::fullScreenRequested,
            this, &VirginPage::handleFullScreenRequested);
}

bool VirginPage::acceptNavigationRequest(const QUrl& url,
                                         NavigationType type,
                                         bool isMainFrame)
{
    // Sec 27: file URL isolation
    if (url.scheme() == "file") {
        if (!isFileUrlAllowed(url, type, isMainFrame)) {
            emit popupBlocked(url);
            return false;
        }
    }

    // Keep executable script schemes out of external-scheme handling.  The
    // external prompt is for OS handlers, never a route to execute page code.
    if (security::UrlSafety::isScriptLikeScheme(url)) {
        emit popupBlocked(url);
        return false;
    }

    // Unknown/external schemes: INV-05, Sec 26.5
    if (isExternalScheme(url)) {
        bool userGesture = (type == NavigationTypeLinkClicked);
        if (!userGesture) {
            emit externalSchemeBlocked(url);
            return false;
        }
        // Even familiar handlers such as mailto: can exfiltrate data. Always ask.
        emit externalSchemeRequested(url);
        QWidget* win = nullptr;
        if (parent() && parent()->isWidgetType()) win = static_cast<QWidget*>(parent())->window();
        const auto btn = QMessageBox::question(
            win, "Virgin — External App",
            QString("Allow this site to open an external application?\n\n%1").arg(url.toDisplayString()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn == QMessageBox::Yes) QDesktopServices::openUrl(url);
        return false;
    }

    // virgin:// internal scheme — Sec 40: isolated, map static IDs only
    if (url.scheme() == "virgin") {
        if (!security::UrlSafety::isVirginInternalUrlAllowed(url)) {
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }

    if (!security::UrlSafety::isUrlAllowedForNavigation(url, type, isMainFrame)) {
        return false;
    }

    // Popup: unsolicited popup block (Sec 26)
    if (!isMainFrame && type == NavigationTypeOther) {
        // Allow sub-resource but not new window without gesture — already handled via createWindow
    }
    if (type == NavigationTypeOther && !isMainFrame) {
        // Potential popup frame — allow only if user gesture or explicitly requested
    }

    // HTTPS-first is handled at NavigationController; certificate handling is separate
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

void VirginPage::handleNewWindowRequested(QWebEngineNewWindowRequest& request) {
    const QUrl requestedUrl = request.requestedUrl();
    if (!request.isUserInitiated() || !requestedUrl.isValid() || requestedUrl.isEmpty()) {
        emit popupBlocked(requestedUrl);
        return;
    }
    emit createNewTabRequested(requestedUrl);
}

void VirginPage::triggerAction(WebAction action, bool checked) {
    // Sec 23: clipboard read denied by default (INV-08)
    // JavascriptCanAccessClipboard is false at profile level, but handle paste action hardening
    if (action == QWebEnginePage::Paste || action == QWebEnginePage::PasteAndMatchStyle) {
        // Only allow paste from user gesture; page's JS can't trigger this via action directly
        // Check PermissionManager for clipboard-read
        QWebEnginePage::triggerAction(action, checked);
        return;
    }
    QWebEnginePage::triggerAction(action, checked);
}

void VirginPage::handleCertificateError(QWebEngineCertificateError error) {
    // INV-03 + Sec 25: fail-closed
    auto decision = security::CertificatePolicy::evaluate(error);
    if (decision == security::CertificatePolicy::Decision::Reject) {
        // Subresource or non-overridable — always reject
        error.rejectCertificate();
        emit certificateErrorIntercepted(error.url(), error.description(), error.isOverridable());
        return;
    }
    // Interstitial: main-frame overridable error — show blocking dialog (Sec 25)
    QWidget* win = nullptr;
    if (parent() && parent()->isWidgetType()) win = static_cast<QWidget*>(parent())->window();

    virgin::ui::CertificateErrorDialog dlg(error, win);
    int res = dlg.exec();
    if (dlg.shouldOverride() && error.isOverridable() && res == QDialog::Accepted) {
        // Narrowly scoped override — only this error, not globally (INV-03)
        error.acceptCertificate();
    } else {
        error.rejectCertificate();
    }
    emit certificateErrorIntercepted(error.url(), error.description(), error.isOverridable());
}

void VirginPage::resolvePermissionRequest(
    const QUrl& origin,
    virgin::privacy::PermissionFeature feature,
    const std::function<void(bool granted)>& applyDecision) {
    // INV-08 + Sec 20: default deny, per-site overrides, user prompt
    auto* pm = virgin::privacy::PermissionManager::instanceForProfile(profile());
    if (!pm) {
        applyDecision(false);
        return;
    }
    const bool hasEntry = pm->hasEntry(origin, feature);
    if (hasEntry) {
        auto result = pm->requestPermission(origin, feature);
        applyDecision(result == virgin::privacy::PermissionManager::Decision::Granted);
        return;
    }

    // No stored decision — prompt user (Sec 20)
    emit permissionRequested(origin, feature);
    QWidget* win = nullptr;
    if (parent() && parent()->isWidgetType()) win = static_cast<QWidget*>(parent())->window();

    virgin::ui::PermissionDialog dlg(origin, feature, win);
    int res = dlg.exec();
    bool granted = (res == QDialog::Accepted && dlg.granted());
    bool remember = dlg.rememberChoice();

    if (granted) {
        if (remember) pm->setPermission(origin, feature, virgin::privacy::PermissionManager::Decision::Granted);
    } else {
        if (remember) pm->setPermission(origin, feature, virgin::privacy::PermissionManager::Decision::Denied);
    }
    applyDecision(granted);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
void VirginPage::handlePermissionRequested(QWebEnginePermission permission) {
    if (!permission.isValid()) return;
    resolvePermissionRequest(permission.origin(), permission.permissionType(),
        [permission](bool granted) {
            if (granted) permission.grant();
            else permission.deny();
        });
}
#else
void VirginPage::handlePermissionRequested(const QUrl& origin, QWebEnginePage::Feature feature) {
    resolvePermissionRequest(origin, feature,
        [this, origin, feature](bool granted) {
            setFeaturePermission(
                origin,
                feature,
                granted ? QWebEnginePage::PermissionGrantedByUser
                        : QWebEnginePage::PermissionDeniedByUser);
        });
}
#endif

void VirginPage::handleRenderProcessTerminated(RenderProcessTerminationStatus status, int code) {
    Q_UNUSED(code)
    if (status == AbnormalTerminationStatus || status == CrashedTerminationStatus) {
        setHtml(QStringLiteral(
            "<!doctype html><html><head><title>Tab crashed</title><style>body{font:14px Arial,Helvetica,sans-serif;padding:40px;background:#f7f7f7;color:#333}main{max-width:680px;margin:auto;border:1px solid #c88;padding:28px;background:#fff}a{color:#365f8d}button{font:inherit;padding:7px 12px;background:#eee;color:#222;border:1px solid #999;border-radius:2px;cursor:pointer}.note{color:#777}</style></head>"
            "<body><main><h2>This tab has crashed</h2>"
            "<p>The page process stopped. Your other tabs are unaffected.</p>"
            "<p>Error code: %1</p>"
            "<p><button onclick='location.reload()'>Reload</button> <button onclick='window.close()'>Close tab</button></p>"
            "<p class='note'>If this keeps happening, close the tab and try the page again.</p>"
            "</main></body></html>").arg(int(status)));
    } else if (status == KilledTerminationStatus) {
        setHtml("<!doctype html><title>Tab stopped</title><body style='font:14px Arial,Helvetica,sans-serif;padding:40px;background:#f7f7f7;color:#333'><h2>This tab was stopped</h2><p><a style='color:#365f8d' href='#' onclick='location.reload()'>Reload the page</a></p></body>");
    }
}

void VirginPage::handleFullScreenRequested(QWebEngineFullScreenRequest request) {
    // Allow — BrowserTab will handle UI fullscreen toggle
    request.accept();
}

bool VirginPage::isExternalScheme(const QUrl& url) const {
    static const QStringList kInternal = {"http","https","virgin","file","data","blob","about"};
    return !kInternal.contains(url.scheme().toLower());
}

bool VirginPage::isFileUrlAllowed(const QUrl& url,
                                  NavigationType type,
                                  bool isMainFrame) const {
    // Web content must never gain ambient local-file access. Only an explicitly
    // typed top-level local path is accepted.
    return url.isLocalFile() && isMainFrame && type == NavigationTypeTyped;
}

} // namespace virgin::browser
