#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

namespace virgin::downloads {
struct DownloadInfo;
class DownloadManager;

class DownloadShelf final : public QWidget {
    Q_OBJECT
public:
    explicit DownloadShelf(virgin::downloads::DownloadManager* dm, QWidget* parent = nullptr);

public slots:
    void onDownloadStarted(const virgin::downloads::DownloadInfo& info);
    void onDownloadUpdated(const QString& id, qint64 received, qint64 total);
    void onDownloadFinished(const QString& id, bool success);

private:
    void addItem(const QString& id, const QString& name, const QString& path);
    void updateItem(const QString& id, qint64 rec, qint64 total, bool done, bool ok);

    virgin::downloads::DownloadManager* dm_ = nullptr;
    QListWidget* list_ = nullptr;
    QPushButton* openFolderBtn_ = nullptr;
    QPushButton* clearBtn_ = nullptr;
    QHash<QString, QListWidgetItem*> items_;
};

} // namespace virgin::downloads
