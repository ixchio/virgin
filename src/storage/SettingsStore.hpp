#pragma once
#include <QObject>
#include <QString>
#include <QVariant>

namespace virgin::storage {

class SettingsStore final : public QObject {
    Q_OBJECT
public:
    explicit SettingsStore(const QString& path, QObject* parent = nullptr);

    bool load();
    bool save() const;

    QVariant value(const QString& key, const QVariant& def = {}) const;
    void setValue(const QString& key, const QVariant& value);

    // Typed helpers
    QString searchEngineUrl() const;
    bool setSearchEngineUrl(const QString& url);
    static bool isValidSearchEngineUrl(const QString& url);

    bool strictMode() const;
    void setStrictMode(bool b);

    int historyRetentionDays() const; // -1 forever, 0 never

private:
    QString path_;
    QVariantMap data_;
};

} // namespace virgin::storage
