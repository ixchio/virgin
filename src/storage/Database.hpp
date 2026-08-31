#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

namespace virgin::storage {

class Database {
public:
    explicit Database(const QString& path);
    ~Database();

    bool open();
    void close();
    bool isOpen() const;

    bool exec(const QString& sql);
    QSqlQuery prepare(const QString& sql) const;
    QString lastError() const { return lastError_; }

    static bool enableWAL(QSqlDatabase& db);
    bool migrate(); // versioned migrations

private:
    QString path_;
    QString connectionName_;
    QString lastError_;
    QSqlDatabase db_;
    bool isOpen_ = false;
};

} // namespace virgin::storage
