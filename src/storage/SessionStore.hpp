#pragma once
#include <QObject>
#include <QUrl>
#include <QJsonDocument>
#include <QByteArray>
#include <memory>

namespace virgin::storage {
class Database;

struct SessionTab {
    QUrl url;
    QString title;
    bool pinned = false;
    QString containerId; // default empty = default profile
};

struct SessionWindow {
    QByteArray geometry;
    QList<SessionTab> tabs;
    int activeIndex = 0;
};

class SessionStore final : public QObject {
    Q_OBJECT
public:
    explicit SessionStore(const QString& dbPath, QObject* parent = nullptr);
    ~SessionStore() override;

    bool saveWindow(const SessionWindow& window);
    bool restoreWindow(SessionWindow& out);
    bool isReady() const { return ready_; }

    void clearPrivateFromSession();

private:
    std::unique_ptr<Database> db_;
    bool ready_ = false;
};

} // namespace virgin::storage
