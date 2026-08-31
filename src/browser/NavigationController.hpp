#pragma once

#include <QUrl>
#include <QString>
#include <QObject>

namespace virgin::browser {
class TabManager;
}
namespace virgin::storage { class SettingsStore; }

namespace virgin::browser {

class NavigationController final : public QObject {
    Q_OBJECT
public:
    explicit NavigationController(TabManager* tabs,
                                  virgin::storage::SettingsStore* settings,
                                  QObject* parent = nullptr);

    void navigateCurrent(const QString& input);
    void navigate(const QUrl& url);

    static QUrl resolveInput(const QString& input);
    static QUrl resolveInput(const QString& input,
                             const QString& searchTemplate,
                             bool httpsFirst);
    static bool isProbablyUrl(const QString& input);

private:
    TabManager* tabs_ = nullptr;
    virgin::storage::SettingsStore* settings_ = nullptr;
};

} // namespace virgin::browser
