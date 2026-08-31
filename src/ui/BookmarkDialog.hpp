#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>

namespace virgin::storage { class BookmarkStore; }
namespace virgin::browser { class TabManager; }

namespace virgin::ui {

class BookmarkDialog final : public QDialog {
    Q_OBJECT
public:
    explicit BookmarkDialog(virgin::storage::BookmarkStore* store,
                            virgin::browser::TabManager* tabs,
                            QWidget* parent = nullptr);
private slots:
    void refresh();
    void onAddCurrent();
    void onRemoveSelected();
    void onItemActivated(QListWidgetItem* item);
private:
    virgin::storage::BookmarkStore* store_ = nullptr;
    virgin::browser::TabManager* tabs_ = nullptr;
    QListWidget* list_ = nullptr;
};

} // namespace virgin::ui
