#include "DownloadShelf.hpp"
#include "DownloadManager.hpp"
#include <QDesktopServices>
#include <QUrl>
#include <QListWidgetItem>
#include <QHBoxLayout>
#include <QDir>
#include <QStandardPaths>

namespace virgin::downloads {

DownloadShelf::DownloadShelf(DownloadManager* dm, QWidget* parent)
    : QWidget(parent), dm_(dm)
{
    setWindowTitle("Downloads");
    setFixedHeight(160);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8,8,8,8);
    layout->setSpacing(4);

    auto* header = new QHBoxLayout();
    header->addWidget(new QLabel("Downloads", this));
    header->addStretch();
    openFolderBtn_ = new QPushButton("Show folder", this);
    clearBtn_ = new QPushButton("Clear", this);
    clearBtn_->setProperty("danger", true);
    header->addWidget(openFolderBtn_);
    header->addWidget(clearBtn_);
    layout->addLayout(header);

    list_ = new QListWidget(this);
    layout->addWidget(list_);

    connect(openFolderBtn_, &QPushButton::clicked, []{
        QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (dir.isEmpty()) dir = QDir::homePath()+"/Downloads";
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    connect(clearBtn_, &QPushButton::clicked, [this]{ list_->clear(); items_.clear(); hide(); });
    connect(list_, &QListWidget::itemDoubleClicked, [](QListWidgetItem* it){
        QString path = it->data(Qt::UserRole).toString();
        if (!path.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    if (dm_) {
        connect(dm_, &DownloadManager::downloadStarted, this, &DownloadShelf::onDownloadStarted);
        connect(dm_, &DownloadManager::downloadUpdated, this, &DownloadShelf::onDownloadUpdated);
        connect(dm_, &DownloadManager::downloadFinished, this, &DownloadShelf::onDownloadFinished);
    }
    hide();
}

void DownloadShelf::addItem(const QString& id, const QString& name, const QString& path) {
    auto* item = new QListWidgetItem(name + " — 0%", list_);
    item->setData(Qt::UserRole, path);
    list_->addItem(item);
    items_.insert(id, item);
    show();
}

void DownloadShelf::updateItem(const QString& id, qint64 rec, qint64 total, bool done, bool ok) {
    auto* it = items_.value(id, nullptr);
    if (!it) return;
    QString base = it->text().split(" — ").first();
    if (done) {
        it->setText(base + (ok ? " — Completed" : " — Failed"));
        if (ok) it->setForeground(QBrush(QColor("#7ab07a")));
        else it->setForeground(QBrush(QColor("#c07a7a")));
    } else {
        int pct = total>0 ? int(rec*100/total) : 0;
        it->setText(base + QString(" — %1% (%2/%3)").arg(pct).arg(rec).arg(total>0?QString::number(total):"?"));
    }
}

void DownloadShelf::onDownloadStarted(const DownloadInfo& info) {
    addItem(info.id, info.fileName, info.fullPath);
}

void DownloadShelf::onDownloadUpdated(const QString& id, qint64 rec, qint64 total) {
    updateItem(id, rec, total, false, true);
}

void DownloadShelf::onDownloadFinished(const QString& id, bool ok) {
    // Find last received values
    updateItem(id, 0, 0, true, ok);
}

} // namespace virgin::downloads
