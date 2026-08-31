#include "ProfileManager.hpp"
#include "VirginProfile.hpp"
#include "ContainerManager.hpp"
#include "app/Paths.hpp"
#include "adblock/AdBlockEngine.hpp"

#include <QDir>

namespace virgin::profiles {

ProfileManager::ProfileManager(QObject* parent) : QObject(parent) {}
ProfileManager::~ProfileManager() { shutdown(); }

bool ProfileManager::initialize() {
    ruleStore_ = std::make_shared<adblock::RuleStore>();
    QString defaultPath = app::Paths::defaultProfilePath();
    QDir().mkpath(defaultPath);
    defaultProfile_ = VirginProfile::createNormal(defaultPath, ruleStore_, this);
    if (!defaultProfile_) return false;

    // Containers directory
    QDir().mkpath(app::Paths::containersRoot());

    // Private profile is lazy — created on demand
    return true;
}

void ProfileManager::shutdown() {
    destroyPrivateProfiles();
    // Qt parent ownership will delete; clear pointers
    containers_.clear();
    defaultProfile_ = nullptr;
    privateProfile_ = nullptr;
}

VirginProfile* ProfileManager::privateProfile() {
    if (!privateProfile_) {
        privateProfile_ = VirginProfile::createPrivate(ruleStore_, this);
        // INV-10: isolated storage — offTheRecord true ensures it
    }
    return privateProfile_;
}

VirginProfile* ProfileManager::containerProfile(const QString& id) {
    if (!ContainerManager::isValidId(id)) return nullptr;
    if (containers_.contains(id)) return containers_.value(id);
    QString path = app::Paths::containersRoot() + "/" + id;
    QDir().mkpath(path);
    auto* p = VirginProfile::createContainer(id, path, ruleStore_, this);
    containers_.insert(id, p);
    return p;
}

QStringList ProfileManager::containerIds() const {
    QDir directory(app::Paths::containersRoot());
    QStringList ids = directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    ids.removeIf([](const QString& id) { return !ContainerManager::isValidId(id); });
    return ids;
}

bool ProfileManager::removeContainer(const QString& id) {
    if (!ContainerManager::isValidId(id)) return false;
    if (VirginProfile* profile = containers_.take(id)) delete profile;
    QDir directory(app::Paths::containersRoot() + "/" + id);
    return !directory.exists() || directory.removeRecursively();
}

void ProfileManager::destroyPrivateProfiles() {
    if (privateProfile_) {
        // Sec 30: erase in-memory objects, destroy OTR instances
        privateProfile_->deleteLater();
        privateProfile_ = nullptr;
    }
    // Also destroy ephemeral containers if any
    // For now, ephemeral containers are those with id starting with "ephemeral-"
    for (auto it = containers_.begin(); it != containers_.end(); ) {
        if (it.key().startsWith("ephemeral-") || it.value()->isOffTheRecord()) {
            it.value()->deleteLater();
            it = containers_.erase(it);
        } else {
            ++it;
        }
    }
}

QList<VirginProfile*> ProfileManager::allProfiles() const {
    QList<VirginProfile*> out;
    if (defaultProfile_) out << defaultProfile_;
    if (privateProfile_) out << privateProfile_;
    out += containers_.values();
    return out;
}

} // namespace virgin::profiles
