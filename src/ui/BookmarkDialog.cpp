#include "BookmarkDialog.hpp"
#include "storage/BookmarkStore.hpp"
#include "browser/TabManager.hpp"
#include "browser/BrowserTab.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

namespace virgin::ui {

BookmarkDialog::BookmarkDialog(virgin::storage::BookmarkStore* store,
                               virgin::browser::TabManager* tabs,
                               QWidget* parent)
    : QDialog(parent), store_(store), tabs_(tabs)
{
    setWindowTitle("Virgin — Bookmarks");
    resize(640, 420);

    auto* layout = new QVBoxLayout(this);
    list_ = new QListWidget(this);
    layout->addWidget(list_);

    auto* row = new QHBoxLayout();
    auto* openBtn = new QPushButton("Open", this);
    auto* removeBtn = new QPushButton("Remove", this);
    auto* closeBtn = new QPushButton("Close", this);
    removeBtn->setProperty("danger", true);
    row->addWidget(openBtn);
    row->addWidget(removeBtn);
    row->addStretch();
    row->addWidget(closeBtn);
    layout->addLayout(row);

    connect(list_, &QListWidget::itemDoubleClicked, this, &BookmarkDialog::onItemActivated);
    connect(openBtn, &QPushButton::clicked, [this]{
        if (auto* it = list_->currentItem()) onItemActivated(it);
    });
    connect(removeBtn, &QPushButton::clicked, this, &BookmarkDialog::onRemoveSelected);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    refresh();
}

void BookmarkDialog::refresh() {
    list_->clear();
    if (!store_) return;
    for (auto& p : store_->all()) {
        QString title = p.second.isEmpty() ? p.first.toString() : p.second + " — " + p.first.toString();
        auto* item = new QListWidgetItem(title, list_);
        item->setData(Qt::UserRole, p.first.toString());
        item->setToolTip(p.first.toString());
        list_->addItem(item);
    }
}

void BookmarkDialog::onItemActivated(QListWidgetItem* item) {
    if (!item || !tabs_) return;
    QUrl url(item->data(Qt::UserRole).toString());
    tabs_->createTab(url, true);
    accept();
}

void BookmarkDialog::onRemoveSelected() {
    auto* it = list_->currentItem();
    if (!it || !store_) return;
    QUrl url(it->data(Qt::UserRole).toString());
    store_->remove(url);
    delete it;
}

void BookmarkDialog::onAddCurrent() { refresh(); }

} // namespace virgin::ui
