#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <memory>

namespace virgin::profiles {
class VirginProfile;
}
namespace virgin::adblock { class RuleStore; }

namespace virgin::profiles {

class ProfileManager final : public QObject {
    Q_OBJECT
public:
    explicit ProfileManager(QObject* parent = nullptr);
    ~ProfileManager() override;

    bool initialize();
    void shutdown();

    VirginProfile* defaultProfile() const { return defaultProfile_; }
    VirginProfile* privateProfile(); // lazy-create off-record
    VirginProfile* containerProfile(const QString& id);
    QStringList containerIds() const;
    bool removeContainer(const QString& id);

    // Sec 30 panic
    void destroyPrivateProfiles();

    QList<VirginProfile*> allProfiles() const;

private:
    VirginProfile* defaultProfile_ = nullptr;
    VirginProfile* privateProfile_ = nullptr;
    QHash<QString, VirginProfile*> containers_;
    std::shared_ptr<adblock::RuleStore> ruleStore_;
};

} // namespace virgin::profiles
