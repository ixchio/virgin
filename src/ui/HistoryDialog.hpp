#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>

namespace virgin::storage { class HistoryStore; }
namespace virgin::browser { class TabManager; }

namespace virgin::ui {

class HistoryDialog final : public QDialog {
    Q_OBJECT
public:
    explicit HistoryDialog(virgin::storage::HistoryStore* store,
                           virgin::browser::TabManager* tabs,
                           QWidget* parent = nullptr);

private slots:
    void refresh();
    void onSearch(const QString& text);
    void onItemActivated(QListWidgetItem* item);
    void onDeleteSelected();
    void onClearAll();

private:
    virgin::storage::HistoryStore* store_ = nullptr;
    virgin::browser::TabManager* tabs_ = nullptr;
    QListWidget* list_ = nullptr;
    QLineEdit* search_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
    QPushButton* clearBtn_ = nullptr;
};

} // namespace virgin::ui
