#pragma once

#include <QString>
#include <QDir>

namespace virgin::app {

class Paths {
public:
    static QString dataRoot();
    static QString profilesRoot();
    static QString defaultProfilePath();
    static QString containersRoot();
    static QString databasePath();
    static QString settingsPath();
    static QString filtersDir();
    static QString compiledFiltersDir();
    static QString builtinFiltersDir();
    static QString sessionsDir();
    static QString crashDir();
    static QString logsDir();

    // Ensure all required dirs exist. Returns false on failure.
    static bool ensureDirectories();

    // Returns ~/.local/share/virgin or QStandardPaths::AppDataLocation
    static QString virginRoot();
};

} // namespace virgin::app
