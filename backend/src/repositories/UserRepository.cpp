#include "repositories/UserRepository.hpp"

#include "core/Error.hpp"

namespace app {
namespace {

constexpr const char* kSelect =
    "SELECT id, username, email, password_hash, full_name, role, is_active, "
    "       last_login_at, created_at, updated_at FROM users ";

User mapRow(const Statement& stmt) {
    User u;
    u.id = stmt.getInt64(0);
    u.username = stmt.getText(1);
    u.email = stmt.getText(2);
    u.passwordHash = stmt.getText(3);
    u.fullName = stmt.getText(4);
    u.role = parseUserRole(stmt.getText(5));
    u.isActive = stmt.getBool(6);
    u.lastLoginAt = stmt.getTextOpt(7);
    u.createdAt = stmt.getText(8);
    u.updatedAt = stmt.getText(9);
    return u;
}

}  // namespace

long long UserRepository::create(const User& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "INSERT INTO users (username, email, password_hash, full_name, role, is_active) "
        "VALUES (?,?,?,?,?,?);");
    stmt.bindAll(value.username, value.email, value.passwordHash, value.fullName,
                 toString(value.role), value.isActive);
    stmt.execute();
    return db_.lastInsertId();
}

void UserRepository::update(const User& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "UPDATE users SET username=?, email=?, full_name=?, role=?, is_active=?, "
        " updated_at=datetime('now') WHERE id=?;");
    stmt.bindAll(value.username, value.email, value.fullName, toString(value.role),
                 value.isActive, value.id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Utilisateur", value.id);
}

void UserRepository::updatePassword(long long id, const std::string& passwordHash) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "UPDATE users SET password_hash=?, updated_at=datetime('now') WHERE id=?;");
    stmt.bindAll(passwordHash, id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Utilisateur", id);
}

void UserRepository::touchLastLogin(long long id) {
    auto stmt = db_.prepare("UPDATE users SET last_login_at=datetime('now') WHERE id=?;");
    stmt.bindAll(id);
    stmt.execute();
}

bool UserRepository::remove(long long id) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare("DELETE FROM users WHERE id=?;");
    stmt.bindAll(id);
    stmt.execute();
    return db_.changes() > 0;
}

std::optional<User> UserRepository::findById(long long id) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE id=?;");
    stmt.bindAll(id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::optional<User> UserRepository::findByUsername(const std::string& username) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE username=?;");
    stmt.bindAll(username);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::optional<User> UserRepository::findByEmail(const std::string& email) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE email=?;");
    stmt.bindAll(email);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<User> UserRepository::findAll() {
    auto stmt = db_.prepare(std::string(kSelect) + "ORDER BY username;");
    std::vector<User> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

long long UserRepository::count() {
    auto stmt = db_.prepare("SELECT COUNT(*) FROM users;");
    if (!stmt.step()) return 0;
    return stmt.getInt64(0);
}

}  // namespace app
