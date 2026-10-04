#include "Paths.hpp"

#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QFile>

namespace virgin::app {

QString Paths::virginRoot() {
    // QStandardPaths::AppDataLocation on Linux -> ~/.local/share/Virgin
    // We canonicalize to ~/.local/share/virgin (lowercase, spec)
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // Force lowercase "virgin" for consistency with spec: ~/.local/share/virgin
    // If Qt returns .../Virgin, normalize.
    if (base.endsWith("/Virgin", Qt::CaseSensitive)) {
        base.chop(6);
        base += "virgin";
    } else if (!base.contains("virgin", Qt::CaseInsensitive)) {
        // Fallback: ~/.local/share/virgin
        base = QDir::homePath() + "/.local/share/virgin";
    }
    // Ensure lowercase final component
    if (!base.endsWith("virgin")) {
        // Replace last component regardless
        const qsizetype slash = base.lastIndexOf('/');
        if (slash != -1) base = base.left(slash + 1) + "virgin";
    }
    return base;
}

QString Paths::dataRoot() { return virginRoot(); }
QString Paths::profilesRoot() { return virginRoot() + "/profiles"; }
QString Paths::defaultProfilePath() { return virginRoot() + "/profiles/default"; }
QString Paths::containersRoot() { return virginRoot() + "/profiles/containers"; }
QString Paths::databasePath() { return virginRoot() + "/virgin.db"; }
QString Paths::settingsPath() { return virginRoot() + "/settings.json"; }
QString Paths::filtersDir() { return virginRoot() + "/filters"; }
QString Paths::compiledFiltersDir() { return virginRoot() + "/filters/compiled"; }
QString Paths::builtinFiltersDir() {
#ifdef VIRGIN_INSTALLED_FILTER_DIR
    const QString installed = QStringLiteral(VIRGIN_INSTALLED_FILTER_DIR);
    if (QDir(installed).exists()) return installed;
#endif
#ifdef VIRGIN_SOURCE_FILTER_DIR
    const QString source = QStringLiteral(VIRGIN_SOURCE_FILTER_DIR);
    if (QDir(source).exists()) return source;
#endif
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + "/share/virgin/filters";
#else
    return QCoreApplication::applicationDirPath() + "/../share/virgin/filters";
#endif
}
QString Paths::sessionsDir() { return virginRoot() + "/sessions"; }
QString Paths::crashDir() { return virginRoot() + "/crash"; }
QString Paths::logsDir() { return virginRoot() + "/logs"; }

bool Paths::ensureDirectories() {
    const QStringList dirs = {
        virginRoot(),
        profilesRoot(),
        defaultProfilePath(),
        containersRoot(),
        filtersDir(),
        compiledFiltersDir(),
        sessionsDir(),
        crashDir(),
        logsDir(),
    };
    for (const auto& d : dirs) {
        QDir dir(d);
        if (!dir.exists() && !dir.mkpath(".")) {
            return false;
        }
    }
    const QString oldSettingsPath = virginRoot() + "/settings.toml";
    if (!QFile::exists(settingsPath()) && QFile::exists(oldSettingsPath) &&
        !QFile::rename(oldSettingsPath, settingsPath())) {
        return false;
    }
    return true;
}

} // namespace virgin::app
