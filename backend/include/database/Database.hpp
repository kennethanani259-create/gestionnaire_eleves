#pragma once
/**
 * @file Database.hpp
 * @brief Wrappers RAII autour de SQLite 3 : Database, Statement, Transaction.
 *
 * Garanties :
 *  - aucune ressource sqlite3_stmt n'est fuitee (destructeur) ;
 *  - toute valeur variable passe par un binding parametre (anti-injection SQL) ;
 *  - une transaction non committee est annulee automatiquement (exception safety).
 */
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <string>
#include <vector>

#include <sqlite3/sqlite3.h>

namespace app {

class Database;

/// Requete preparee. Non copiable, deplacable.
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    // --- Binding (index 1-base, comme SQLite) ---
    // Les entiers passent par un gabarit SFINAE : tout type entier (int, long,
    // long long, enum sous-jacent...) est accepte sans ambiguite de surcharge.
    Statement& bind(int index, std::nullptr_t);
    Statement& bind(int index, double value);
    Statement& bind(int index, bool value);
    Statement& bind(int index, const char* value);
    Statement& bind(int index, const std::string& value);
    Statement& bind(int index, const std::optional<std::string>& value);
    Statement& bind(int index, const std::optional<double>& value);

    template <typename T,
              std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    Statement& bind(int index, T value) {
        return bindInt64(index, static_cast<std::int64_t>(value));
    }

    template <typename T,
              std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    Statement& bind(int index, const std::optional<T>& value) {
        return value.has_value() ? bindInt64(index, static_cast<std::int64_t>(*value))
                                 : bind(index, nullptr);
    }

    /// Lie toutes les valeurs dans l'ordre, a partir de l'index 1.
    template <typename... Args>
    Statement& bindAll(const Args&... args) {
        int index = 1;
        (void)std::initializer_list<int>{(bind(index++, args), 0)...};
        return *this;
    }

    /// Avance d'une ligne. Retourne true s'il y a une ligne a lire.
    bool step();
    /// Execute une requete sans resultat attendu (INSERT/UPDATE/DELETE/DDL).
    void execute();
    void reset();

    // --- Lecture de colonnes (index 0-base, comme SQLite) ---
    bool isNull(int column) const;
    std::int64_t getInt64(int column) const;
    int getInt(int column) const;
    double getDouble(int column) const;
    bool getBool(int column) const;
    std::string getText(int column) const;
    std::optional<std::string> getTextOpt(int column) const;
    std::optional<std::int64_t> getInt64Opt(int column) const;
    std::optional<double> getDoubleOpt(int column) const;

    int columnCount() const;
    std::string columnName(int column) const;

private:
    Statement& bindInt64(int index, std::int64_t value);
    void checkBind(int rc, int index) const;

    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
    std::string sql_;
};

/// Connexion SQLite. Non copiable.
class Database {
public:
    /// @param path chemin du fichier, ou ":memory:" pour une base en memoire.
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /// Execute un script SQL (potentiellement multi-instructions, sans parametres).
    void executeScript(const std::string& sql);
    /// Prepare une requete parametree.
    Statement prepare(const std::string& sql);

    std::int64_t lastInsertId() const;
    int changes() const;
    const std::string& path() const noexcept { return path_; }
    sqlite3* handle() noexcept { return db_; }

    /// Serialise les sections critiques multi-instructions (ex: insert + lastInsertId).
    std::unique_lock<std::recursive_mutex> lockGuard();

private:
    sqlite3* db_ = nullptr;
    std::string path_;
    std::recursive_mutex mutex_;
};

/// Transaction RAII : rollback automatique si commit() n'est pas appele.
class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();
    void rollback();

private:
    Database& db_;
    bool active_ = false;
};

}  // namespace app
