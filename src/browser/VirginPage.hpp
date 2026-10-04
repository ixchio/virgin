#pragma once

#include <QWebEnginePage>
#include <QWebEngineCertificateError>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineNewWindowRequest>
#include <QUrl>

#include "privacy/PermissionTypes.hpp"

#include <functional>

namespace virgin::browser {

class VirginPage final : public QWebEnginePage {
    Q_OBJECT
public:
    explicit VirginPage(QWebEngineProfile* profile, QObject* parent = nullptr);

protected:
    bool acceptNavigationRequest(const QUrl& url,
                                 NavigationType type,
                                 bool isMainFrame) override;

    void triggerAction(WebAction action, bool checked = false) override;

private slots:
    void handleCertificateError(QWebEngineCertificateError error);
    void handleNewWindowRequested(QWebEngineNewWindowRequest& request);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    void handlePermissionRequested(QWebEnginePermission permission);
#else
    void handlePermissionRequested(const QUrl& origin, QWebEnginePage::Feature feature);
#endif
    void handleRenderProcessTerminated(RenderProcessTerminationStatus status, int code);
    void handleFullScreenRequested(QWebEngineFullScreenRequest request);

signals:
    void createNewTabRequested(const QUrl& url);
    void externalSchemeBlocked(const QUrl& url);
    void externalSchemeRequested(const QUrl& url);
    void permissionRequested(const QUrl& origin, virgin::privacy::PermissionFeature feature);
    void certificateErrorIntercepted(const QUrl& url, const QString& errorString, bool overridable);
    void popupBlocked(const QUrl& url);

private:
    bool isExternalScheme(const QUrl& url) const;
    bool isFileUrlAllowed(const QUrl& url, NavigationType type, bool isMainFrame) const;
    void resolvePermissionRequest(const QUrl& origin,
                                  virgin::privacy::PermissionFeature feature,
                                  const std::function<void(bool granted)>& applyDecision);
};

} // namespace virgin::browser
