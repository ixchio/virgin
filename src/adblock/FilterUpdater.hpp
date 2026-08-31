#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QThread>
#include <atomic>

class QNetworkAccessManager;
class QNetworkReply;

namespace virgin::adblock {
class AdBlockEngine;

class FilterUpdater final : public QObject {
    Q_OBJECT
public:
    explicit FilterUpdater(AdBlockEngine* engine, QObject* parent = nullptr);
    ~FilterUpdater() override;

    void setSources(const QList<QUrl>& sources);
    void updateNow(); // local files -> background compile -> validation -> atomic swap
    void updateFromNetwork(); // HTTPS fetch -> validate -> background compile -> atomic swap
    bool isUpdating() const { return updating_.load(); }
    bool needsNetworkUpdate(int maxAgeDays = 7) const;

signals:
    void updateFinished(bool success, const QString& message);
    void updateProgress(int percent);
    void updateStarted();

private:
    void compilePaths(const QStringList& paths);
    void fetchNext();
    void finishNetworkFailure(const QString& message);

    AdBlockEngine* engine_ = nullptr;
    QList<QUrl> sources_;
    QNetworkAccessManager* network_ = nullptr;
    QList<QByteArray> downloadedLists_;
    int sourceIndex_ = 0;
    std::atomic<bool> updating_{false};
    static constexpr qint64 kMaxListSize = 10 * 1024 * 1024;
    static constexpr int kMaxRuleLength = 8192;
};

} // namespace virgin::adblock
