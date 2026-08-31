#include "VirginSchemeHandler.hpp"
#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

namespace virgin::browser {

void VirginSchemeHandler::requestStarted(QWebEngineUrlRequestJob* job) {
    QUrl url = job->requestUrl();
    if (url.scheme() != "virgin") {
        job->fail(QWebEngineUrlRequestJob::UrlInvalid);
        return;
    }
    QString host = url.host().toLower();
    QString path = url.path().toLower();
    if (host.isEmpty() && !path.isEmpty()) {
        if (path.startsWith('/')) path = path.mid(1);
        host = path;
        path.clear();
    }
    if (host.contains("..") || path.contains("..")) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    QByteArray html = pageFor(host, path);
    if (html.isEmpty()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    auto* buffer = new QBuffer(job);
    buffer->setData(html);
    buffer->open(QIODevice::ReadOnly);
    job->reply("text/html", buffer);
}

QByteArray VirginSchemeHandler::pageFor(const QString& host, const QString& path) const {
    Q_UNUSED(path)
    if (host == "newtab" || host == "newtab/") return newTabHtml();
    if (host == "version") return versionHtml();
    if (host == "history") return nativeToolHtml("History", "Ctrl+H", "Search, open, or remove your local browsing history.");
    if (host == "settings") return nativeToolHtml("Settings", "View → Settings", "Change privacy, search, history, and blocking preferences.");
    if (host == "privacy") return nativeToolHtml("Privacy", "Shield button", "See protection details and clear site data.");
    if (host == "downloads") return nativeToolHtml("Downloads", "Ctrl+J", "See active and completed downloads.");
    if (host == "blank") return QByteArray("<!doctype html><meta name='color-scheme' content='light'><style>html{background:#fff}</style>");
    return {};
}

QByteArray VirginSchemeHandler::newTabHtml() {
    QFile logo(QStringLiteral(":/virgin/logo-192.png"));
    const QByteArray logoUrl = logo.open(QIODevice::ReadOnly)
        ? QByteArray("data:image/png;base64,") + logo.readAll().toBase64()
        : QByteArray();
    QByteArray html(R"(
<!doctype html><html><head><meta charset="utf-8"><title>New Tab</title><meta name="color-scheme" content="light">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; img-src data:">
<style>
*{box-sizing:border-box}body{background:#f7f7f7;color:#252525;font:14px Arial,Helvetica,sans-serif;margin:0}
main{width:min(680px,calc(100vw - 48px));margin:15vh auto 0;text-align:center}
.logo{display:block;width:112px;height:112px;object-fit:contain;margin:0 auto 16px}
h1{font-size:30px;font-weight:400;letter-spacing:3px;margin:0;color:#303030}.tagline{color:#777;margin:7px 0 32px}
.hint{background:#fff;border:1px solid #bbb;border-radius:2px;color:#777;padding:14px;text-align:left;box-shadow:inset 0 1px 2px rgba(0,0,0,.06)}
.note{color:#888;font-size:12px;margin-top:28px}
</style></head><body><main><img class="logo" src="%LOGO%" alt="Virgin logo">
<h1>Virgin</h1><p class="tagline">A small, private browser.</p>
<div class="hint">Search or type a web address in the address bar above</div>
<p class="note">Press Ctrl+L to focus the address bar</p></main></body></html>
)");
    return html.replace("%LOGO%", logoUrl);
}

QByteArray VirginSchemeHandler::versionHtml() {
    QByteArray html = QByteArray(R"(
<!doctype html><html><head><meta charset="utf-8"><title>About Virgin</title><meta name="color-scheme" content="light"><meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'">
<style>body{font:14px Arial,Helvetica,sans-serif;background:#f7f7f7;color:#333;padding:36px}main{max-width:720px;margin:auto;background:#fff;border:1px solid #ccc;padding:28px}h1{font-size:24px;font-weight:400;margin-top:0}code{background:#f1f1f1;border:1px solid #d5d5d5;padding:2px 5px}a{color:#365f8d}</style></head>
<body><main><h1>Virgin 0.1.0</h1>
<p>Qt <code>%1</code> &nbsp; Chromium <code>%2</code></p>
<p>Normal profile: persistent &nbsp; Private: off-the-record (memory only)</p>
<p>Permissions: default deny &nbsp; WebRTC: public-interfaces-only &nbsp; Canvas: strict blocks reads</p>
<p>Certificate: fail-closed &nbsp; Popup: user-gesture only &nbsp; File URL: isolated</p>
<p>No Virgin account, cloud service, analytics, or crash uploader.</p>
<p><a href="virgin://newtab">newtab</a> · <a href="virgin://history">history</a> · <a href="virgin://settings">settings</a></p>
</main></body></html>
)");
    return html.replace("%1", qVersion()).replace("%2", qWebEngineChromiumVersion());
}

QByteArray VirginSchemeHandler::nativeToolHtml(const QByteArray& title,
                                               const QByteArray& shortcut,
                                               const QByteArray& description) {
    QByteArray html("<!doctype html><html><head><meta charset='utf-8'>"
        "<meta http-equiv='Content-Security-Policy' content=\"default-src 'none'; style-src 'unsafe-inline'\">"
        "<title>");
    html += title + QByteArray("</title><meta name='color-scheme' content='light'>"
        "<style>body{font:14px Arial,Helvetica,sans-serif;background:#f7f7f7;color:#333;display:grid;place-items:center;height:100vh;margin:0}"
        "main{width:min(560px,calc(100vw - 40px));border:1px solid #ccc;padding:28px;background:#fff}"
        "h1{font-size:24px;font-weight:400;margin-top:0}.key{display:inline-block;color:#333;background:#eee;border:1px solid #bbb;border-radius:2px;padding:5px 8px}.note{color:#777}</style>"
        "</head><body><main><h1>");
    html += title + "</h1><p>" + description + "</p><p class='key'>" + shortcut +
            "</p><p class='note'>Open this feature from the browser controls.</p></main></body></html>";
    return html;
}

} // namespace virgin::browser
