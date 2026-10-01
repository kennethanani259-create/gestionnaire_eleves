#include "database/Migrator.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "core/Error.hpp"
#include "core/Logger.hpp"

namespace fs = std::filesystem;

namespace app {

Migrator::Migrator(Database& db, std::string migrationsDir)
    : db_(db), dir_(std::move(migrationsDir)) {}

void Migrator::ensureMigrationTable() {
    db_.executeScript(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "  name       TEXT PRIMARY KEY,"
        "  applied_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");");
}

std::vector<std::string> Migrator::appliedMigrations() {
    ensureMigrationTable();
    std::vector<std::string> names;
    auto stmt = db_.prepare("SELECT name FROM schema_migrations ORDER BY name;");
    while (stmt.step()) names.push_back(stmt.getText(0));
    return names;
}

int Migrator::migrate() {
    ensureMigrationTable();

    std::error_code ec;
    if (!fs::exists(dir_, ec) || !fs::is_directory(dir_, ec)) {
        throw DatabaseError("Repertoire de migrations introuvable: " + dir_);
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir_, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sql") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    const auto applied = appliedMigrations();
    int count = 0;

    for (const auto& file : files) {
        const std::string name = file.filename().string();
        if (std::find(applied.begin(), applied.end(), name) != applied.end()) continue;

        std::ifstream in(file);
        if (!in) throw DatabaseError("Lecture impossible de la migration " + name);
        std::ostringstream buffer;
        buffer << in.rdbuf();

        LOG_INFO("migrator", "Application de la migration " + name);
        Transaction tx(db_);
        db_.executeScript(buffer.str());
        auto stmt = db_.prepare("INSERT INTO schema_migrations(name) VALUES (?);");
        stmt.bindAll(name);
        stmt.execute();
        tx.commit();
        ++count;
    }

    if (count == 0) {
        LOG_INFO("migrator", "Base a jour, aucune migration a appliquer");
    } else {
        LOG_INFO("migrator", std::to_string(count) + " migration(s) appliquee(s)");
    }
    return count;
}

}  // namespace app
