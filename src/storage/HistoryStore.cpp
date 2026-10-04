#include "HistoryStore.hpp"
#include "Database.hpp"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

namespace virgin::storage {

HistoryStore::HistoryStore(const QString& dbPath, QObject* parent)
    : QObject(parent), dbPath_(dbPath)
{
    db_ = std::make_unique<Database>(dbPath_);
    ready_ = db_->open();

    flushTimer_ = new QTimer(this);
    flushTimer_->setInterval(1000);
    flushTimer_->setSingleShot(false);
    connect(flushTimer_, &QTimer::timeout, this, &HistoryStore::onFlushTimeout);
    if (ready_) flushTimer_->start();
}

HistoryStore::~HistoryStore() {
    flush();
}

void HistoryStore::enqueueVisit(const QUrl& url, const QString& title) {
    if (url.isEmpty() || url.scheme() == "virgin" || url.scheme() == "data" || url.scheme() == "about" || url.scheme() == "blob") return;
    if (url.host().isEmpty()) return;
    QMutexLocker lock(&mutex_);
    queue_.enqueue({url, title, QDateTime::currentSecsSinceEpoch()});
    if (queue_.size() > 100) {
        lock.unlock();
        flush();
    }
}

void HistoryStore::flush() {
    QQueue<PendingVisit> local;
    {
        QMutexLocker lock(&mutex_);
        if (queue_.isEmpty() || !ready_ || !db_ || !db_->isOpen()) return;
        local = std::move(queue_);
        queue_.clear();
    }

    QSqlQuery q = db_->prepare("INSERT INTO history (url, title, visited_at) VALUES (?, ?, ?)");
    bool committed = db_->exec("BEGIN TRANSACTION;");
    for (const auto& v : local) {
        if (!committed) break;
        q.bindValue(0, v.url.toString());
        q.bindValue(1, v.title);
        q.bindValue(2, v.ts);
        committed = q.exec();
    }
    if (committed) committed = db_->exec("COMMIT;");
    if (committed) return;

    db_->exec("ROLLBACK;");
    QMutexLocker lock(&mutex_);
    // Preserve old visits ahead of visits queued while the transaction ran.
    while (!queue_.isEmpty()) local.enqueue(queue_.dequeue());
    queue_ = std::move(local);
}

void HistoryStore::onFlushTimeout() { flush(); }

QList<QPair<QUrl, QString>> HistoryStore::recent(int limit) {
    QList<QPair<QUrl, QString>> out;
    if (!db_ || !db_->isOpen()) return out;
    QSqlQuery q = db_->prepare("SELECT url, title FROM history ORDER BY visited_at DESC LIMIT ?");
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next()) {
            out.append({QUrl(q.value(0).toString()), q.value(1).toString()});
        }
    }
    return out;
}

QList<QPair<QUrl, QString>> HistoryStore::search(const QString& query, int limit) {
    QList<QPair<QUrl, QString>> out;
    if (query.isEmpty() || !db_ || !db_->isOpen()) return out;
    QSqlQuery q = db_->prepare("SELECT url, title FROM history WHERE url LIKE ? OR title LIKE ? ORDER BY visited_at DESC LIMIT ?");
    QString pat = "%" + query + "%";
    q.addBindValue(pat);
    q.addBindValue(pat);
    q.addBindValue(limit);
    if (q.exec()) while (q.next()) out.append({QUrl(q.value(0).toString()), q.value(1).toString()});
    return out;
}

QList<QPair<QUrl, QString>> HistoryStore::completions(const QString& prefix, int limit) {
    if (prefix.length() < 2) return {};
    return search(prefix, limit);
}

void HistoryStore::clearAll() {
    if (db_) {
        db_->exec("DELETE FROM history;");
        emit historyCleared();
    }
}

bool HistoryStore::remove(const QUrl& url) {
    if (!db_ || !db_->isOpen()) return false;
    QSqlQuery q = db_->prepare("DELETE FROM history WHERE url = ?");
    q.addBindValue(url.toString());
    return q.exec();
}

int HistoryStore::removeHost(const QString& host) {
    if (host.isEmpty() || !db_ || !db_->isOpen()) return 0;
    flush();
    QList<qint64> ids;
    QSqlQuery select = db_->prepare("SELECT id, url FROM history");
    if (select.exec()) {
        while (select.next()) {
            const QUrl url(select.value(1).toString());
            const QString candidate = url.host().toLower();
            if (candidate == host.toLower() || candidate.endsWith("." + host.toLower())) {
                ids.append(select.value(0).toLongLong());
            }
        }
    }
    QSqlQuery removeQuery = db_->prepare("DELETE FROM history WHERE id = ?");
    int removed = 0;
    for (qint64 id : ids) {
        removeQuery.bindValue(0, id);
        if (removeQuery.exec()) ++removed;
    }
    return removed;
}

void HistoryStore::deleteExpired(int keepDays) {
    if (keepDays < 0) return;
    if (keepDays == 0) { clearAll(); return; }
    qint64 cutoff = QDateTime::currentSecsSinceEpoch() - keepDays * 86400;
    QSqlQuery q = db_->prepare("DELETE FROM history WHERE visited_at < ?");
    q.addBindValue(cutoff);
    q.exec();
}

} // namespace virgin::storage
