#include "HistoryDialog.hpp"
#include "storage/HistoryStore.hpp"
#include "browser/TabManager.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>

namespace virgin::ui {

HistoryDialog::HistoryDialog(virgin::storage::HistoryStore* store,
                             virgin::browser::TabManager* tabs,
                             QWidget* parent)
    : QDialog(parent), store_(store), tabs_(tabs)
{
    setWindowTitle("Virgin — History");
    resize(720, 480);

    auto* layout = new QVBoxLayout(this);
    auto* top = new QHBoxLayout();
    top->addWidget(new QLabel("Search history:", this));
    search_ = new QLineEdit(this);
    search_->setPlaceholderText("Filter by URL or title — local only");
    top->addWidget(search_);
    layout->addLayout(top);

    list_ = new QListWidget(this);
    list_->setAlternatingRowColors(true);
    layout->addWidget(list_);

    auto* btnRow = new QHBoxLayout();
    deleteBtn_ = new QPushButton("Delete Selected", this);
    clearBtn_ = new QPushButton("Clear All History", this);
    auto* closeBtn = new QPushButton("Close", this);
    clearBtn_->setProperty("danger", true);
    btnRow->addWidget(deleteBtn_);
    btnRow->addWidget(clearBtn_);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    connect(search_, &QLineEdit::textChanged, this, &HistoryDialog::onSearch);
    connect(list_, &QListWidget::itemDoubleClicked, this, &HistoryDialog::onItemActivated);
    connect(deleteBtn_, &QPushButton::clicked, this, &HistoryDialog::onDeleteSelected);
    connect(clearBtn_, &QPushButton::clicked, this, &HistoryDialog::onClearAll);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    refresh();
}

void HistoryDialog::refresh() {
    list_->clear();
    if (!store_) return;
    auto items = search_->text().isEmpty() ? store_->recent(200) : store_->search(search_->text(), 200);
    for (auto& p : items) {
        QString title = p.second.isEmpty() ? p.first.toString() : p.second;
        auto* item = new QListWidgetItem(title, list_);
        item->setData(Qt::UserRole, p.first.toString());
        item->setToolTip(p.first.toString());
        list_->addItem(item);
    }
}

void HistoryDialog::onSearch(const QString& text) {
    Q_UNUSED(text)
    refresh();
}

void HistoryDialog::onItemActivated(QListWidgetItem* item) {
    if (!item || !tabs_) return;
    QUrl url(item->data(Qt::UserRole).toString());
    if (url.isValid()) {
        tabs_->createTab(url, true);
        accept();
    }
}

void HistoryDialog::onDeleteSelected() {
    auto* item = list_->currentItem();
    if (!item || !store_) return;
    QUrl url(item->data(Qt::UserRole).toString());
    store_->remove(url);
    delete item;
}

void HistoryDialog::onClearAll() {
    if (QMessageBox::question(this, "Clear History", "Delete all history? This cannot be undone.") == QMessageBox::Yes) {
        if (store_) store_->clearAll();
        refresh();
    }
}

} // namespace virgin::ui
