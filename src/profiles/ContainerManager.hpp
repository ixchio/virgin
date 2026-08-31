#pragma once

#include <QObject>
#include <QStringList>
#include <QHash>

namespace virgin::profiles {
class VirginProfile;
class ProfileManager;

class ContainerManager final : public QObject {
    Q_OBJECT
public:
    explicit ContainerManager(ProfileManager* pm, QObject* parent = nullptr);

    QStringList containerIds() const;
    VirginProfile* getOrCreate(const QString& id);
    bool removeContainer(const QString& id); // deletes storage after confirmation
    bool exists(const QString& id) const;
    static bool isValidId(const QString& id);

private:
    ProfileManager* pm_ = nullptr;
};

} // namespace virgin::profiles
