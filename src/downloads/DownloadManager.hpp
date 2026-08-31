#pragma once

#include <QObject>
#include <QHash>
#include <QUrl>
#include <QWebEngineDownloadRequest>
#include <QList>

namespace virgin::downloads {

struct DownloadInfo {
    QString id;
    QUrl url;
    QUrl pageUrl;
    QString fileName;
    QString directory;
    QString fullPath;
    qint64 totalBytes = -1;
    qint64 receivedBytes = 0;
    QWebEngineDownloadRequest::DownloadState state = QWebEngineDownloadRequest::DownloadRequested;
    bool isDangerous = false;
};

class DownloadManager final : public QObject {
    Q_OBJECT
public:
    explicit DownloadManager(QObject* parent = nullptr);

    void handleDownload(QWebEngineDownloadRequest* download);
    QList<DownloadInfo> downloads() const { return downloads_; }
    int count() const { return static_cast<int>(downloads_.size()); }

signals:
    void downloadStarted(const DownloadInfo& info);
    void downloadUpdated(const QString& id, qint64 received, qint64 total);
    void downloadFinished(const QString& id, bool success);
    void downloadCancelled(const QString& id);

private slots:
    void onReceivedBytesChanged();
    void onTotalBytesChanged();
    void onStateChanged(QWebEngineDownloadRequest::DownloadState state);

private:
    QString sanitizeFileName(const QString& name) const;
    bool isDangerousExtension(const QString& name) const;
    QString downloadDirectory() const;

    QHash<QWebEngineDownloadRequest*, DownloadInfo> active_;
    QList<DownloadInfo> downloads_;
    QHash<QWebEngineDownloadRequest*, QString> idMap_;
};

} // namespace virgin::downloads
