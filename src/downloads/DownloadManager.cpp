#include "DownloadManager.hpp"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QDesktopServices>
#include <QWebEnginePage>
#include <QUuid>

namespace virgin::downloads {

DownloadManager::DownloadManager(QObject* parent) : QObject(parent) {}

QString DownloadManager::downloadDirectory() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + "/Downloads";
    QDir().mkpath(dir);
    return dir;
}

QString DownloadManager::sanitizeFileName(const QString& name) const {
    QString n = QFileInfo(name).fileName();
    n.remove('/');
    n.remove('\\');
    n.replace("..", "_");
    for (qsizetype index = n.size(); index > 0; --index) {
        if (n[index - 1].isNull() || n[index - 1].category() == QChar::Other_Control) {
            n.remove(index - 1, 1);
        }
    }
    if (n.startsWith('.')) n.prepend('_');
    if (n.isEmpty()) n = "download";
    if (n.size() > 200) n = n.left(200);
    return n;
}

bool DownloadManager::isDangerousExtension(const QString& name) const {
    QString lower = name.toLower();
    if (lower.endsWith(".exe") || lower.endsWith(".desktop") || lower.endsWith(".sh")
        || lower.endsWith(".bat") || lower.endsWith(".appimage") || lower.endsWith(".run")
        || lower.endsWith(".cmd") || lower.endsWith(".com") || lower.endsWith(".scr")
        || lower.endsWith(".msi") || lower.endsWith(".ps1") || lower.endsWith(".jar")) return true;
    if (lower.count('.') >= 2) {
        QStringList parts = lower.split('.');
        QString secondLast = parts[parts.size()-2];
        if ((secondLast == "pdf" || secondLast == "txt" || secondLast == "jpg" || secondLast == "png")
            && (parts.last() == "exe" || parts.last() == "sh" || parts.last() == "bat")) return true;
    }
    return false;
}

void DownloadManager::handleDownload(QWebEngineDownloadRequest* download) {
    if (!download) return;

    QString rawName = download->downloadFileName();
    QString safeName = sanitizeFileName(rawName);
    QString dir = downloadDirectory();
    const QFileInfo original(safeName);
    const QString baseName = original.completeBaseName().isEmpty() ? QStringLiteral("download")
                                                                   : original.completeBaseName();
    const QString suffix = original.suffix();
    QString fullPath = dir + "/" + safeName;
    int counter = 1;
    while (QFileInfo::exists(fullPath) && counter < 10'000) {
        const QString numbered = suffix.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(baseName).arg(counter)
            : QStringLiteral("%1 (%2).%3").arg(baseName).arg(counter).arg(suffix);
        fullPath = dir + "/" + numbered;
        ++counter;
    }
    if (QFileInfo::exists(fullPath)) fullPath = dir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + "-" + safeName;

    bool dangerous = isDangerousExtension(safeName);

    DownloadInfo info;
    info.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    info.url = download->url();
    info.pageUrl = download->page() ? download->page()->url() : QUrl();
    info.fileName = QFileInfo(fullPath).fileName();
    info.directory = dir;
    info.fullPath = fullPath;
    info.state = QWebEngineDownloadRequest::DownloadRequested;
    info.isDangerous = dangerous;

    if (dangerous) {
        auto btn = QMessageBox::warning(nullptr, "Virgin — Dangerous Download",
            QString("This file looks dangerous:\n%1\n\nSource: %2\nSource page: %3\n\nDo you want to keep it?").arg(safeName, download->url().toString(), info.pageUrl.toString()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes) {
            download->cancel();
            emit downloadCancelled(info.id);
            return;
        }
    }

    download->setDownloadDirectory(dir);
    download->setDownloadFileName(QFileInfo(fullPath).fileName());

    // Track
    active_.insert(download, info);
    idMap_.insert(download, info.id);
    downloads_.append(info);
    emit downloadStarted(info);

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged,
            this, &DownloadManager::onReceivedBytesChanged);
    connect(download, &QWebEngineDownloadRequest::totalBytesChanged,
            this, &DownloadManager::onTotalBytesChanged);
    connect(download, &QWebEngineDownloadRequest::stateChanged,
            this, &DownloadManager::onStateChanged);
    connect(download, &QObject::destroyed, this, [this, download] {
        active_.remove(download);
        idMap_.remove(download);
    });

    download->accept();
}

void DownloadManager::onReceivedBytesChanged() {
    auto* d = qobject_cast<QWebEngineDownloadRequest*>(sender());
    if (!d || !active_.contains(d)) return;
    QString id = idMap_.value(d);
    qint64 rec = d->receivedBytes();
    qint64 total = d->totalBytes();
    auto& info = active_[d];
    info.receivedBytes = rec;
    info.totalBytes = total;
    emit downloadUpdated(id, rec, total);
}

void DownloadManager::onTotalBytesChanged() {
    onReceivedBytesChanged();
}

void DownloadManager::onStateChanged(QWebEngineDownloadRequest::DownloadState state) {
    auto* d = qobject_cast<QWebEngineDownloadRequest*>(sender());
    if (!d || !active_.contains(d)) return;
    QString id = idMap_.value(d);
    active_[d].state = state;
    if (state == QWebEngineDownloadRequest::DownloadCompleted) {
        emit downloadFinished(id, true);
    } else if (state == QWebEngineDownloadRequest::DownloadCancelled ||
               state == QWebEngineDownloadRequest::DownloadInterrupted) {
        emit downloadFinished(id, false);
    }
    if (state != QWebEngineDownloadRequest::DownloadInProgress &&
        state != QWebEngineDownloadRequest::DownloadRequested) {
        for (DownloadInfo& item : downloads_) {
            if (item.id == id) {
                item = active_.value(d);
                break;
            }
        }
        active_.remove(d);
        idMap_.remove(d);
    }
}

} // namespace virgin::downloads
