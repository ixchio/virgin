#pragma once

#include <QWebEngineProfile>
#include <QObject>
#include <memory>

#include "privacy/CookiePolicy.hpp"

namespace virgin::network { class RequestInterceptor; }
namespace virgin::privacy { class PermissionManager; class CookieFilter; }
namespace virgin::adblock { class AdBlockEngine; class FilterUpdater; class RuleStore; }
namespace virgin::downloads { class DownloadManager; }
namespace virgin::browser { class VirginSchemeHandler; }

namespace virgin::profiles {

class VirginProfile final : public QObject {
    Q_OBJECT
public:
    enum class Type {
        Normal,
        Private,
        Container,
        Ephemeral
    };

    static VirginProfile* createNormal(const QString& dataPath,
                                       std::shared_ptr<adblock::RuleStore> ruleStore,
                                       QObject* parent = nullptr);
    static VirginProfile* createPrivate(std::shared_ptr<adblock::RuleStore> ruleStore,
                                        QObject* parent = nullptr);
    static VirginProfile* createContainer(const QString& containerId,
                                          const QString& dataPath,
                                          std::shared_ptr<adblock::RuleStore> ruleStore,
                                          QObject* parent = nullptr);

    ~VirginProfile() override;

    QWebEngineProfile* qtProfile() const { return qtProfile_; }
    Type type() const { return type_; }
    QString id() const { return id_; }
    bool isOffTheRecord() const;

    void applyPrivacySettings();
    void setStrictMode(bool strict);
    void setWebRtcPublicOnly(bool enabled);
    void setCookieMode(privacy::CookieMode mode);
    void setAdblockEnabled(bool enabled);
    bool isStrict() const { return strict_; }
    privacy::CookieMode cookieMode() const;
    bool blocksThirdPartyCookies() const;
    bool canvasReadsBlocked() const { return isOffTheRecord() || strict_; }
    bool webRtcPublicOnly() const { return isOffTheRecord() || webRtcPublicOnly_; }

    network::RequestInterceptor* interceptor() const { return interceptor_; }
    adblock::AdBlockEngine* adblockEngine() const { return adblock_; }
    adblock::FilterUpdater* filterUpdater() const { return filterUpdater_; }
    downloads::DownloadManager* downloadManager() const { return downloads_; }
    privacy::CookieFilter* cookieFilter() const { return cookieFilter_; }
    privacy::PermissionManager* permissionManager() const { return permissionManager_; }

private:
    explicit VirginProfile(QWebEngineProfile* profile, Type type, const QString& id,
                           std::shared_ptr<adblock::RuleStore> ruleStore, QObject* parent);
    void syncYouTubeProtectionScript(bool enabled);

    QWebEngineProfile* qtProfile_ = nullptr;
    Type type_ = Type::Normal;
    QString id_;
    bool strict_ = false;
    bool webRtcPublicOnly_ = true;
    privacy::CookieMode cookieMode_ = privacy::CookieMode::Standard;

    network::RequestInterceptor* interceptor_ = nullptr;
    privacy::PermissionManager* permissionManager_ = nullptr;
    privacy::CookieFilter* cookieFilter_ = nullptr;
    adblock::AdBlockEngine* adblock_ = nullptr;
    adblock::FilterUpdater* filterUpdater_ = nullptr;
    downloads::DownloadManager* downloads_ = nullptr;
    browser::VirginSchemeHandler* schemeHandler_ = nullptr;
};

} // namespace virgin::profiles
