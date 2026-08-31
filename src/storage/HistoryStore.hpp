#pragma once

#include <QObject>
#include <QUrl>
#include <QQueue>
#include <QTimer>
#include <QMutex>
#include <memory>

namespace virgin::storage {
class Database;

class HistoryStore final : public QObject {
    Q_OBJECT
public:
    explicit HistoryStore(const QString& dbPath, QObject* parent = nullptr);
    ~HistoryStore() override;

    void enqueueVisit(const QUrl& url, const QString& title);
    void flush();
    QList<QPair<QUrl, QString>> recent(int limit = 100);
    QList<QPair<QUrl, QString>> search(const QString& query, int limit = 20);
    QList<QPair<QUrl, QString>> completions(const QString& prefix, int limit = 8);
    bool isReady() const { return ready_; }

    void clearAll();
    void deleteExpired(int keepDays);
    bool remove(const QUrl& url);
    int removeHost(const QString& host);

signals:
    void historyCleared();

private slots:
    void onFlushTimeout();

private:
    struct PendingVisit { QUrl url; QString title; qint64 ts; };

    QString dbPath_;
    std::unique_ptr<Database> db_;
    QQueue<PendingVisit> queue_;
    QTimer* flushTimer_ = nullptr;
    QMutex mutex_;
    bool ready_ = false;
};

} // namespace virgin::storage
