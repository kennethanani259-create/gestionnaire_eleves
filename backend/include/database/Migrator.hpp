#pragma once
/**
 * @file Migrator.hpp
 * @brief Application versionnee des migrations SQL.
 *
 * Chaque fichier "NNN_nom.sql" du repertoire de migrations est applique une
 * seule fois, dans l'ordre lexicographique, a l'interieur d'une transaction.
 * L'historique est conserve dans la table schema_migrations.
 */
#include <string>
#include <vector>

#include "database/Database.hpp"

namespace app {

class Migrator {
public:
    Migrator(Database& db, std::string migrationsDir);

    /// Applique les migrations manquantes. Retourne le nombre de migrations appliquees.
    int migrate();
    /// Liste des migrations deja appliquees (noms de fichiers).
    std::vector<std::string> appliedMigrations();

private:
    void ensureMigrationTable();

    Database& db_;
    std::string dir_;
};

}  // namespace app
