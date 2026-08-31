#pragma once
#include <QObject>
#include <QUrl>
#include <memory>

namespace virgin::storage {
class Database;
class BookmarkStore final : public QObject {
    Q_OBJECT
public:
    explicit BookmarkStore(const QString& dbPath, QObject* parent = nullptr);
    ~BookmarkStore() override;
    bool add(const QUrl& url, const QString& title);
    bool remove(const QUrl& url);
    bool contains(const QUrl& url) const;
    QList<QPair<QUrl, QString>> all() const;
    QList<QPair<QUrl, QString>> search(const QString& query, int limit = 20) const;
    int removeHost(const QString& host);
    bool isReady() const { return ready_; }
signals:
    void bookmarkAdded(const QUrl& url);
    void bookmarkRemoved(const QUrl& url);
private:
    std::unique_ptr<Database> db_;
    bool ready_ = false;
};
} // namespace virgin::storage
