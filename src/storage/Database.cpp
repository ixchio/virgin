#include "Database.hpp"
#include <QSqlError>
#include <QVariant>
#include <QUuid>

namespace virgin::storage {

Database::Database(const QString& path) : path_(path) {
    connectionName_ = "virgin_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    db_ = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
    db_.setDatabaseName(path_);
}

Database::~Database() {
    close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName_);
}

bool Database::open() {
    if (isOpen_) return true;
    if (!db_.open()) {
        lastError_ = db_.lastError().text();
        return false;
    }
    if (!enableWAL(db_)) {
        lastError_ = db_.lastError().text();
        db_.close();
        return false;
    }
    isOpen_ = true;
    if (!migrate()) {
        db_.close();
        isOpen_ = false;
        return false;
    }
    return true;
}

void Database::close() {
    if (db_.isOpen()) db_.close();
    isOpen_ = false;
}

bool Database::isOpen() const { return isOpen_ && db_.isOpen(); }

bool Database::exec(const QString& sql) {
    QSqlQuery q(db_);
    const bool ok = q.exec(sql);
    if (!ok) lastError_ = q.lastError().text();
    return ok;
}

QSqlQuery Database::prepare(const QString& sql) const {
    QSqlQuery q(db_);
    q.prepare(sql);
    return q;
}

bool Database::enableWAL(QSqlDatabase& db) {
    QSqlQuery q(db);
    return q.exec("PRAGMA journal_mode=WAL;") &&
           q.exec("PRAGMA synchronous=NORMAL;") &&
           q.exec("PRAGMA foreign_keys=ON;");
}

bool Database::migrate() {
    if (!db_.transaction()) {
        lastError_ = db_.lastError().text();
        return false;
    }
    const QStringList statements = {
        QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL,
            title TEXT,
            visited_at INTEGER NOT NULL,
            transition INTEGER NOT NULL DEFAULT 0
        );
    )"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_history_visited ON history(visited_at DESC);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_history_url ON history(url);"),
        QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS bookmarks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL,
            title TEXT,
            folder TEXT,
            created_at INTEGER NOT NULL
        );
    )"),
        QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY,
            data TEXT
        );
    )"),
        QStringLiteral("PRAGMA user_version=1;")
    };

    for (const QString& statement : statements) {
        if (!exec(statement)) {
            db_.rollback();
            return false;
        }
    }
    if (!db_.commit()) {
        lastError_ = db_.lastError().text();
        db_.rollback();
        return false;
    }
    return true;
}

} // namespace virgin::storage
