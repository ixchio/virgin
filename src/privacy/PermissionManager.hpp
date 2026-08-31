#pragma once

#include <QObject>
#include <QUrl>
#include <QWebEnginePermission>
#include <QHash>

class QWebEngineProfile;

namespace virgin::privacy {

class PermissionManager final : public QObject {
    Q_OBJECT
public:
    enum class Decision { Granted, Denied, Ask };

    explicit PermissionManager(QWebEngineProfile* profile, QObject* parent = nullptr);

    Decision requestPermission(const QUrl& origin, QWebEnginePermission::PermissionType feature);
    void setPermission(const QUrl& origin, QWebEnginePermission::PermissionType feature, Decision d);
    void resetPermissions(const QUrl& origin);
    Decision storedDecision(const QUrl& origin, QWebEnginePermission::PermissionType feature) const;
    bool hasEntry(const QUrl& origin, QWebEnginePermission::PermissionType feature) const;

    // Global registry for VirginPage lookup (weak refs)
    static void registerProfile(QWebEngineProfile* p, PermissionManager* mgr);
    static void unregisterProfile(QWebEngineProfile* p);
    static PermissionManager* instanceForProfile(QWebEngineProfile* p);

    static Decision defaultForFeature(QWebEnginePermission::PermissionType f);

private:
    QString keyFor(const QUrl& origin, QWebEnginePermission::PermissionType f) const;
    void load();
    void save() const;

    QWebEngineProfile* profile_ = nullptr;
    QHash<QString, Decision> decisions_;
    QString storagePath_;

    static QHash<QWebEngineProfile*, PermissionManager*> s_registry;
};

} // namespace virgin::privacy
