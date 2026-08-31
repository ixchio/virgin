#pragma once

#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineUrlRequestJob>
#include <QUrl>

namespace virgin::browser {

// Sec 39/40: virgin:// internal pages, isolated, static IDs only, no traversal, no exec
class VirginSchemeHandler final : public QWebEngineUrlSchemeHandler {
    Q_OBJECT
public:
    explicit VirginSchemeHandler(QObject* parent = nullptr) : QWebEngineUrlSchemeHandler(parent) {}
    void requestStarted(QWebEngineUrlRequestJob* job) override;
private:
    QByteArray pageFor(const QString& host, const QString& path) const;
    static QByteArray newTabHtml();
    static QByteArray versionHtml();
    static QByteArray nativeToolHtml(const QByteArray& title,
                                     const QByteArray& shortcut,
                                     const QByteArray& description);
};

} // namespace virgin::browser
