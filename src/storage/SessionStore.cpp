#include "SessionStore.hpp"
#include "Database.hpp"
#include <QSqlQuery>
#include <QJsonArray>
#include <QJsonObject>

namespace virgin::storage {

SessionStore::SessionStore(const QString& dbPath, QObject* parent)
    : QObject(parent), db_(std::make_unique<Database>(dbPath)) { ready_ = db_->open(); }

SessionStore::~SessionStore() = default;

bool SessionStore::saveWindow(const SessionWindow& window) {
    if (!ready_) return false;
    QJsonArray arr;
    for (auto& t : window.tabs) {
        if (t.url.scheme() == "virgin" && t.url.host() == "private") continue;
        if (t.url.isEmpty()) continue;
        // Skip virgin://newtab unless it's the only tab
        if (t.url.toString() == "virgin://newtab" && window.tabs.size()>1) continue;
        QJsonObject o;
        o["url"] = t.url.toString();
        o["title"] = t.title;
        o["pinned"] = t.pinned;
        o["container"] = t.containerId;
        arr.append(o);
    }
    QJsonObject obj;
    obj["tabs"] = arr;
    obj["active"] = window.activeIndex;
    obj["geometry"] = QString::fromLatin1(window.geometry.toBase64());
    obj["version"] = 1;
    QJsonDocument doc(obj);
    QSqlQuery q = db_->prepare("INSERT OR REPLACE INTO sessions (id, data) VALUES (1, ?)");
    q.addBindValue(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    return q.exec();
}

bool SessionStore::restoreWindow(SessionWindow& out) {
    if (!ready_) return false;
    QSqlQuery q = db_->prepare("SELECT data FROM sessions WHERE id = 1");
    if (!q.exec() || !q.next()) return false;
    QJsonDocument doc = QJsonDocument::fromJson(q.value(0).toString().toUtf8());
    if (doc.isNull()) return false;
    QJsonObject obj = doc.object();
    QJsonArray arr = obj["tabs"].toArray();
    out.tabs.clear();
    for (auto v : arr) {
        QJsonObject o = v.toObject();
        SessionTab t;
        t.url = QUrl(o["url"].toString());
        t.title = o["title"].toString();
        t.pinned = o["pinned"].toBool(false);
        t.containerId = o["container"].toString();
        if (!t.url.isValid() || t.url.isEmpty()) continue;
        out.tabs.append(t);
    }
    out.activeIndex = obj["active"].toInt(0);
    out.geometry = QByteArray::fromBase64(obj["geometry"].toString().toLatin1());
    return !out.tabs.isEmpty();
}

void SessionStore::clearPrivateFromSession() {
    if (!ready_) return;
    QSqlQuery read = db_->prepare("SELECT data FROM sessions WHERE id = 1");
    if (!read.exec() || !read.next()) return;
    QJsonDocument doc = QJsonDocument::fromJson(read.value(0).toString().toUtf8());
    if (!doc.isObject()) return;

    QJsonObject object = doc.object();
    const QJsonArray tabs = object["tabs"].toArray();
    QJsonArray sanitized;
    for (const QJsonValue& value : tabs) {
        if (!value.isObject()) {
            // Legacy sessions contain only normal-window URL strings.
            sanitized.append(value);
            continue;
        }
        const QJsonObject tab = value.toObject();
        const QUrl url(tab["url"].toString());
        const bool privateContainer = tab["container"].toString() == "private";
        const bool privateInternal = url.scheme() == "virgin" && url.host() == "private";
        if (!privateContainer && !privateInternal) sanitized.append(tab);
    }
    object["tabs"] = sanitized;
    if (object["active"].toInt() >= sanitized.size()) {
        object["active"] = qMax(0, static_cast<int>(sanitized.size()) - 1);
    }
    QSqlQuery write = db_->prepare("INSERT OR REPLACE INTO sessions (id, data) VALUES (1, ?)");
    write.addBindValue(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    write.exec();
}

} // namespace virgin::storage
