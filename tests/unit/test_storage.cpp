#include "storage/Database.hpp"
#include "storage/SessionStore.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char* label) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << label << '\n';
    if (!condition) ++failures;
}
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "temporary storage directory created");

    const QString databasePath = directory.filePath("virgin.db");
    {
        virgin::storage::Database database(databasePath);
        check(database.open(), "database opens and migrations succeed");
        check(database.isOpen(), "database reports ready state");
    }

    virgin::storage::SessionWindow saved;
    saved.activeIndex = 1;
    saved.geometry = QByteArray("geometry");
    saved.tabs = {
        {QUrl("https://one.example"), QStringLiteral("One"), false, QStringLiteral("default")},
        {QUrl("https://two.example"), QStringLiteral("Two"), false, QStringLiteral("default")}
    };
    {
        virgin::storage::SessionStore sessions(databasePath);
        check(sessions.isReady(), "session store reports database readiness");
        check(sessions.saveWindow(saved), "session save reports success");
    }
    {
        virgin::storage::SessionStore sessions(databasePath);
        virgin::storage::SessionWindow restored;
        check(sessions.restoreWindow(restored), "session survives process-style reopen");
        check(restored.tabs.size() == 2 && restored.activeIndex == 1,
              "session restores all tabs and active index");
    }

    virgin::storage::Database impossible(QStringLiteral("/dev/null/virgin.db"));
    check(!impossible.open(), "invalid database path fails closed");

    return failures == 0 ? 0 : 1;
}
