#include "PermissionManager.hpp"
#include <QWebEngineProfile>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QDir>

namespace virgin::privacy {

QHash<QWebEngineProfile*, PermissionManager*> PermissionManager::s_registry;

PermissionManager::PermissionManager(QWebEngineProfile* profile, QObject* parent)
    : QObject(parent), profile_(profile) {
    if (profile_ && !profile_->isOffTheRecord() && !profile_->persistentStoragePath().isEmpty()) {
        storagePath_ = profile_->persistentStoragePath() + "/permissions.json";
        load();
    }
}

void PermissionManager::registerProfile(QWebEngineProfile* p, PermissionManager* mgr) {
    s_registry.insert(p, mgr);
}
void PermissionManager::unregisterProfile(QWebEngineProfile* p) {
    s_registry.remove(p);
}
PermissionManager* PermissionManager::instanceForProfile(QWebEngineProfile* p) {
    return s_registry.value(p, nullptr);
}

PermissionManager::Decision PermissionManager::defaultForFeature(QWebEnginePermission::PermissionType f) {
    // Sec 20: default deny
    switch (f) {
        case QWebEnginePermission::PermissionType::Geolocation:
        case QWebEnginePermission::PermissionType::MediaAudioCapture:
        case QWebEnginePermission::PermissionType::MediaVideoCapture:
        case QWebEnginePermission::PermissionType::MediaAudioVideoCapture:
        case QWebEnginePermission::PermissionType::DesktopVideoCapture:
        case QWebEnginePermission::PermissionType::DesktopAudioVideoCapture:
        case QWebEnginePermission::PermissionType::Notifications:
            return Decision::Denied;
        default:
            return Decision::Denied;
    }
}

QString PermissionManager::keyFor(const QUrl& origin, QWebEnginePermission::PermissionType f) const {
    const QString normalizedOrigin = origin.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment)
                                         .toString(QUrl::FullyEncoded);
    return normalizedOrigin + "#" + QString::number(static_cast<int>(f));
}

PermissionManager::Decision PermissionManager::storedDecision(const QUrl& origin, QWebEnginePermission::PermissionType f) const {
    QString k = keyFor(origin, f);
    auto it = decisions_.find(k);
    if (it != decisions_.end()) return it.value();
    return defaultForFeature(f);
}

bool PermissionManager::hasEntry(const QUrl& origin, QWebEnginePermission::PermissionType f) const {
    return decisions_.contains(keyFor(origin, f));
}

PermissionManager::Decision PermissionManager::requestPermission(const QUrl& origin, QWebEnginePermission::PermissionType feature) {
    // Sec 20: policy -> saved -> prompt
    QString k = keyFor(origin, feature);
    if (decisions_.contains(k)) return decisions_.value(k);

    return defaultForFeature(feature);
}

void PermissionManager::setPermission(const QUrl& origin, QWebEnginePermission::PermissionType feature, Decision d) {
    decisions_.insert(keyFor(origin, feature), d);
    save();
}

void PermissionManager::resetPermissions(const QUrl& origin) {
    const QString normalizedOrigin = origin.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment)
                                         .toString(QUrl::FullyEncoded);
    const QString prefix = normalizedOrigin + "#";
    for (auto it = decisions_.begin(); it != decisions_.end(); ) {
        if (it.key().startsWith(prefix)) it = decisions_.erase(it);
        else ++it;
    }
    save();
}

void PermissionManager::load() {
    QFile file(storagePath_);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) return;
    const QJsonObject object = document.object();
    int accepted = 0;
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (++accepted > 10'000 || it.key().size() > 2048 || !it.value().isDouble()) break;
        const int raw = it.value().toInt(-1);
        if (raw < static_cast<int>(Decision::Granted) || raw > static_cast<int>(Decision::Ask)) continue;
        decisions_.insert(it.key(), static_cast<Decision>(raw));
    }
}

void PermissionManager::save() const {
    if (storagePath_.isEmpty()) return;
    QJsonObject object;
    for (auto it = decisions_.cbegin(); it != decisions_.cend(); ++it) {
        object.insert(it.key(), static_cast<int>(it.value()));
    }
    QDir().mkpath(QFileInfo(storagePath_).absolutePath());
    QSaveFile file(storagePath_);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.commit();
}

} // namespace virgin::privacy
