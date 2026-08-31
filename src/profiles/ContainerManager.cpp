#include "ContainerManager.hpp"
#include "ProfileManager.hpp"
#include "VirginProfile.hpp"
#include "app/Paths.hpp"
#include <QDir>
#include <QRegularExpression>

namespace virgin::profiles {

ContainerManager::ContainerManager(ProfileManager* pm, QObject* parent)
    : QObject(parent), pm_(pm) {}

QStringList ContainerManager::containerIds() const {
    QDir dir(app::Paths::containersRoot());
    QStringList ids = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    ids.removeIf([](const QString& id) { return !isValidId(id); });
    return ids;
}

VirginProfile* ContainerManager::getOrCreate(const QString& id) {
    if (!isValidId(id)) return nullptr;
    return pm_->containerProfile(id);
}

bool ContainerManager::exists(const QString& id) const {
    if (!isValidId(id)) return false;
    return QDir(app::Paths::containersRoot() + "/" + id).exists();
}

bool ContainerManager::removeContainer(const QString& id) {
    if (!exists(id)) return false;
    QDir dir(app::Paths::containersRoot() + "/" + id);
    return dir.removeRecursively();
}

bool ContainerManager::isValidId(const QString& id) {
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));
    return valid.match(id).hasMatch();
}

} // namespace virgin::profiles
