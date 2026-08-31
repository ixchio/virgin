#include "BookmarkStore.hpp"
#include "Database.hpp"
#include <QSqlQuery>
#include <QDateTime>

namespace virgin::storage {

BookmarkStore::BookmarkStore(const QString& dbPath, QObject* parent)
    : QObject(parent), db_(std::make_unique<Database>(dbPath)) { ready_ = db_->open(); }

BookmarkStore::~BookmarkStore() = default;

bool BookmarkStore::add(const QUrl& url, const QString& title) {
    if (!ready_) return false;
    if (contains(url)) return false;
    QSqlQuery q = db_->prepare("INSERT INTO bookmarks (url, title, created_at) VALUES (?, ?, ?)");
    q.addBindValue(url.toString());
    q.addBindValue(title);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    bool ok = q.exec();
    if (ok) emit bookmarkAdded(url);
    return ok;
}
bool BookmarkStore::remove(const QUrl& url) {
    if (!ready_) return false;
    QSqlQuery q = db_->prepare("DELETE FROM bookmarks WHERE url = ?");
    q.addBindValue(url.toString());
    bool ok = q.exec();
    if (ok) emit bookmarkRemoved(url);
    return ok;
}
bool BookmarkStore::contains(const QUrl& url) const {
    if (!ready_) return false;
    QSqlQuery q = db_->prepare("SELECT 1 FROM bookmarks WHERE url = ? LIMIT 1");
    q.addBindValue(url.toString());
    if (q.exec() && q.next()) return true;
    return false;
}
QList<QPair<QUrl, QString>> BookmarkStore::all() const {
    QList<QPair<QUrl, QString>> out;
    if (!ready_) return out;
    QSqlQuery q = db_->prepare("SELECT url, title FROM bookmarks ORDER BY created_at DESC");
    if (q.exec()) while (q.next()) out.append({QUrl(q.value(0).toString()), q.value(1).toString()});
    return out;
}
QList<QPair<QUrl, QString>> BookmarkStore::search(const QString& qStr, int limit) const {
    QList<QPair<QUrl, QString>> out;
    if (!ready_) return out;
    if (qStr.isEmpty()) return all();
    QSqlQuery q = db_->prepare("SELECT url, title FROM bookmarks WHERE url LIKE ? OR title LIKE ? ORDER BY created_at DESC LIMIT ?");
    QString pat = "%" + qStr + "%";
    q.addBindValue(pat);
    q.addBindValue(pat);
    q.addBindValue(limit);
    if (q.exec()) while (q.next()) out.append({QUrl(q.value(0).toString()), q.value(1).toString()});
    return out;
}

int BookmarkStore::removeHost(const QString& host) {
    if (host.isEmpty()) return 0;
    const auto bookmarks = all();
    int removed = 0;
    for (const auto& bookmark : bookmarks) {
        const QString candidate = bookmark.first.host().toLower();
        if (candidate == host.toLower() || candidate.endsWith("." + host.toLower())) {
            if (remove(bookmark.first)) ++removed;
        }
    }
    return removed;
}

} // namespace virgin::storage
