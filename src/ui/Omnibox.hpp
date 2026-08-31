#pragma once

#include <QLineEdit>
#include <QUrl>
#include <QCompleter>
#include <QStringListModel>

namespace virgin::storage { class HistoryStore; class BookmarkStore; }

namespace virgin::ui {

class Omnibox final : public QLineEdit {
    Q_OBJECT
public:
    explicit Omnibox(virgin::storage::HistoryStore* history = nullptr,
                     virgin::storage::BookmarkStore* bookmarks = nullptr,
                     QWidget* parent = nullptr);

    void setHistoryStore(virgin::storage::HistoryStore* h) { history_ = h; }
    void setBookmarkStore(virgin::storage::BookmarkStore* b) { bookmarks_ = b; }

    void setUrl(const QUrl& url);
    QUrl url() const;

signals:
    void returnPressedWithText(const QString& text);
    void completionSelected(const QUrl& url);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;

private slots:
    void updateCompletions(const QString& text);
    void onCompletionActivated(const QString& text);

private:
    void updateSecurityIndicator(const QUrl& url);
    void rebuildModel(const QStringList& items);

    virgin::storage::HistoryStore* history_ = nullptr;
    virgin::storage::BookmarkStore* bookmarks_ = nullptr;
    QCompleter* completer_ = nullptr;
    QStringListModel* model_ = nullptr;
    QHash<QString, QUrl> completionMap_;
};

} // namespace virgin::ui
