#include "database/Database.hpp"

#include <filesystem>
#include <utility>

#include "core/Error.hpp"
#include "core/Logger.hpp"

namespace app {

// ===========================================================================
// Statement
// ===========================================================================

Statement::Statement(sqlite3* db, const std::string& sql) : db_(db), sql_(sql) {
    const int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr);
    if (rc != SQLITE_OK || stmt_ == nullptr) {
        throw DatabaseError("Preparation SQL impossible: " + std::string(sqlite3_errmsg(db_)) +
                            " | requete: " + sql);
    }
}

Statement::~Statement() {
    if (stmt_ != nullptr) sqlite3_finalize(stmt_);
}

Statement::Statement(Statement&& other) noexcept
    : db_(other.db_), stmt_(other.stmt_), sql_(std::move(other.sql_)) {
    other.stmt_ = nullptr;
    other.db_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_ != nullptr) sqlite3_finalize(stmt_);
        db_ = other.db_;
        stmt_ = other.stmt_;
        sql_ = std::move(other.sql_);
        other.stmt_ = nullptr;
        other.db_ = nullptr;
    }
    return *this;
}

void Statement::checkBind(int rc, int index) const {
    if (rc != SQLITE_OK) {
        throw DatabaseError("Binding du parametre " + std::to_string(index) +
                            " impossible: " + sqlite3_errmsg(db_));
    }
}

Statement& Statement::bind(int index, std::nullptr_t) {
    checkBind(sqlite3_bind_null(stmt_, index), index);
    return *this;
}
Statement& Statement::bindInt64(int index, std::int64_t value) {
    checkBind(sqlite3_bind_int64(stmt_, index, value), index);
    return *this;
}
Statement& Statement::bind(int index, double value) {
    checkBind(sqlite3_bind_double(stmt_, index, value), index);
    return *this;
}
Statement& Statement::bind(int index, bool value) {
    checkBind(sqlite3_bind_int(stmt_, index, value ? 1 : 0), index);
    return *this;
}
Statement& Statement::bind(int index, const char* value) {
    if (value == nullptr) return bind(index, nullptr);
    checkBind(sqlite3_bind_text(stmt_, index, value, -1, SQLITE_TRANSIENT), index);
    return *this;
}
Statement& Statement::bind(int index, const std::string& value) {
    checkBind(sqlite3_bind_text(stmt_, index, value.c_str(),
                                static_cast<int>(value.size()), SQLITE_TRANSIENT),
              index);
    return *this;
}
Statement& Statement::bind(int index, const std::optional<std::string>& value) {
    return value.has_value() ? bind(index, *value) : bind(index, nullptr);
}
Statement& Statement::bind(int index, const std::optional<double>& value) {
    return value.has_value() ? bind(index, *value) : bind(index, nullptr);
}

bool Statement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;

    const std::string message = sqlite3_errmsg(db_);
    const int extended = sqlite3_extended_errcode(db_);
    if (rc == SQLITE_CONSTRAINT) {
        if (extended == SQLITE_CONSTRAINT_UNIQUE ||
            extended == SQLITE_CONSTRAINT_PRIMARYKEY) {
            throw ConflictError("Violation d'unicite: " + message);
        }
        if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
            throw ValidationError("Reference invalide (cle etrangere): " + message);
        }
        throw ValidationError("Contrainte de base de donnees violee: " + message);
    }
    throw DatabaseError("Execution SQL echouee: " + message + " | requete: " + sql_);
}

void Statement::execute() {
    while (step()) {
        // Consomme d'eventuelles lignes (ex: INSERT ... RETURNING).
    }
}

void Statement::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

bool Statement::isNull(int column) const {
    return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}
std::int64_t Statement::getInt64(int column) const {
    return sqlite3_column_int64(stmt_, column);
}
int Statement::getInt(int column) const { return sqlite3_column_int(stmt_, column); }
double Statement::getDouble(int column) const { return sqlite3_column_double(stmt_, column); }
bool Statement::getBool(int column) const { return sqlite3_column_int(stmt_, column) != 0; }

std::string Statement::getText(int column) const {
    const unsigned char* text = sqlite3_column_text(stmt_, column);
    if (text == nullptr) return {};
    const int bytes = sqlite3_column_bytes(stmt_, column);
    return std::string(reinterpret_cast<const char*>(text), static_cast<size_t>(bytes));
}

std::optional<std::string> Statement::getTextOpt(int column) const {
    if (isNull(column)) return std::nullopt;
    return getText(column);
}
std::optional<std::int64_t> Statement::getInt64Opt(int column) const {
    if (isNull(column)) return std::nullopt;
    return getInt64(column);
}
std::optional<double> Statement::getDoubleOpt(int column) const {
    if (isNull(column)) return std::nullopt;
    return getDouble(column);
}

int Statement::columnCount() const { return sqlite3_column_count(stmt_); }

std::string Statement::columnName(int column) const {
    const char* name = sqlite3_column_name(stmt_, column);
    return name != nullptr ? std::string(name) : std::string{};
}

// ===========================================================================
// Database
// ===========================================================================

Database::Database(const std::string& path) : path_(path) {
    if (path != ":memory:" && path.rfind("file:", 0) != 0) {
        const std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }
    }

    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    const int rc = sqlite3_open_v2(path.c_str(), &db_, flags, nullptr);
    if (rc != SQLITE_OK) {
        const std::string message = db_ != nullptr ? sqlite3_errmsg(db_) : "erreur inconnue";
        if (db_ != nullptr) sqlite3_close(db_);
        db_ = nullptr;
        throw DatabaseError("Ouverture de la base impossible (" + path + "): " + message);
    }

    sqlite3_busy_timeout(db_, 5000);
    executeScript(
        "PRAGMA foreign_keys = ON;"
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = NORMAL;"
        "PRAGMA temp_store = MEMORY;");
    LOG_DEBUG("database", "Connexion ouverte sur " + path);
}

Database::~Database() {
    if (db_ != nullptr) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

void Database::executeScript(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    char* errorMessage = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errorMessage);
    if (rc != SQLITE_OK) {
        const std::string message = errorMessage != nullptr ? errorMessage : "erreur inconnue";
        sqlite3_free(errorMessage);
        throw DatabaseError("Execution du script SQL echouee: " + message);
    }
}

Statement Database::prepare(const std::string& sql) { return Statement(db_, sql); }

std::int64_t Database::lastInsertId() const { return sqlite3_last_insert_rowid(db_); }

int Database::changes() const { return sqlite3_changes(db_); }

std::unique_lock<std::recursive_mutex> Database::lockGuard() {
    return std::unique_lock<std::recursive_mutex>(mutex_);
}

// ===========================================================================
// Transaction
// ===========================================================================

Transaction::Transaction(Database& db) : db_(db) {
    db_.executeScript("BEGIN IMMEDIATE;");
    active_ = true;
}

Transaction::~Transaction() {
    if (!active_) return;
    try {
        db_.executeScript("ROLLBACK;");
        LOG_DEBUG("database", "Transaction annulee automatiquement");
    } catch (const std::exception& e) {
        LOG_ERROR("database", std::string("Rollback impossible: ") + e.what());
    }
}

void Transaction::commit() {
    if (!active_) return;
    db_.executeScript("COMMIT;");
    active_ = false;
}

void Transaction::rollback() {
    if (!active_) return;
    db_.executeScript("ROLLBACK;");
    active_ = false;
}

}  // namespace app
