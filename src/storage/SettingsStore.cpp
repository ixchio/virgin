#include "SettingsStore.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>

namespace virgin::storage {

SettingsStore::SettingsStore(const QString& path, QObject* parent)
    : QObject(parent), path_(path) {
    // defaults
    data_["search_engine"] = "https://duckduckgo.com/?q=%s";
    data_["strict_mode"] = false;
    data_["history_retention"] = -1; // forever
    data_["homepage"] = "virgin://newtab";
    data_["adblock_enabled"] = true;
    data_["https_first"] = true;
    data_["cookie_mode"] = "standard";
    data_["webrtc_public_only"] = true;
    data_["filter_auto_update"] = true;
}

bool SettingsStore::load() {
    QFile f(path_);
    if (!f.exists()) return false;
    if (!f.open(QIODevice::ReadOnly)) return false;
    if (f.size() > 1024 * 1024) return false;
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QVariantMap loaded = doc.object().toVariantMap();
    for (auto it = loaded.cbegin(); it != loaded.cend(); ++it) data_.insert(it.key(), it.value());
    return true;
}

bool SettingsStore::save() const {
    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QJsonObject object = QJsonObject::fromVariantMap(data_);
    if (file.write(QJsonDocument(object).toJson()) < 0) return false;
    return file.commit();
}

QVariant SettingsStore::value(const QString& key, const QVariant& def) const {
    return data_.value(key, def);
}
void SettingsStore::setValue(const QString& key, const QVariant& value) {
    data_.insert(key, value);
}

QString SettingsStore::searchEngineUrl() const {
    const QString candidate = value("search_engine", "https://duckduckgo.com/?q=%s").toString();
    return isValidSearchEngineUrl(candidate)
        ? candidate
        : QStringLiteral("https://duckduckgo.com/?q=%s");
}
bool SettingsStore::isValidSearchEngineUrl(const QString& url) {
    const QString candidate = url.trimmed();
    QString check = candidate;
    check.replace("%s", "query");
    const QUrl parsed(check);
    return candidate.contains("%s") && parsed.isValid() &&
           parsed.scheme().toLower() == QStringLiteral("https") &&
           !parsed.host().isEmpty();
}
bool SettingsStore::setSearchEngineUrl(const QString& url) {
    const QString candidate = url.trimmed();
    if (!isValidSearchEngineUrl(candidate)) {
        setValue("search_engine", QStringLiteral("https://duckduckgo.com/?q=%s"));
        return false;
    }
    setValue("search_engine", candidate);
    return true;
}

bool SettingsStore::strictMode() const { return value("strict_mode", false).toBool(); }
void SettingsStore::setStrictMode(bool b) { setValue("strict_mode", b); }

int SettingsStore::historyRetentionDays() const { return value("history_retention", -1).toInt(); }

} // namespace virgin::storage
