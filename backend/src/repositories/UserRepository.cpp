#include "repositories/UserRepository.hpp"

#include "core/Error.hpp"
#include "core/Tenant.hpp"

namespace app {
namespace {

constexpr const char* kSelect =
    "SELECT id, username, email, password_hash, full_name, role, is_active, "
    "       last_login_at, created_at, updated_at, school_id FROM users ";

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
    u.schoolId = stmt.getInt64Opt(10);
    return u;
}

}  // namespace

long long UserRepository::create(const User& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "INSERT INTO users (username, email, password_hash, full_name, role, is_active, "
        "                   school_id) VALUES (?,?,?,?,?,?,?);");
    stmt.bindAll(value.username, value.email, value.passwordHash, value.fullName,
                 toString(value.role), value.isActive, value.schoolId);
    stmt.execute();
    return db_.lastInsertId();
}

void UserRepository::update(const User& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "UPDATE users SET username=?, email=?, full_name=?, role=?, is_active=?, "
        " updated_at=datetime('now') WHERE id=? " + tenant::filter("") + ";");
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
    auto stmt = db_.prepare(std::string("DELETE FROM users WHERE id=? ") +
                            tenant::filter("") + ";");
    stmt.bindAll(id);
    stmt.execute();
    return db_.changes() > 0;
}

std::optional<User> UserRepository::findById(long long id) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE id=? " + tenant::filter("") + ";");
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
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE 1=1 " + tenant::filter("") +
                            "ORDER BY username;");
    std::vector<User> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

long long UserRepository::count() {
    auto stmt = db_.prepare(std::string("SELECT COUNT(*) FROM users WHERE 1=1 ") +
                            tenant::filter("") + ";");
    if (!stmt.step()) return 0;
    return stmt.getInt64(0);
}

// --------------------------------------------------- Rattachement parent
// Les requetes passent par students : la portee d'etablissement s'applique
// donc aussi aux liens, un parent ne pouvant etre lie qu'a un eleve de son
// ecole.
std::vector<long long> UserRepository::childrenOf(long long parentUserId) {
    std::vector<long long> ids;
    auto stmt = db_.prepare(
        std::string("SELECT ps.student_id FROM parent_students ps "
                    "JOIN students st ON st.id = ps.student_id "
                    "WHERE ps.user_id = ? ") +
        tenant::filter("st") + "ORDER BY st.last_name, st.first_name;");
    stmt.bindAll(parentUserId);
    while (stmt.step()) ids.push_back(stmt.getInt64(0));
    return ids;
}

void UserRepository::linkChild(long long parentUserId, long long studentId,
                               const std::optional<std::string>& relation) {
    auto lock = db_.lockGuard();
    // L'eleve doit appartenir a l'etablissement courant.
    auto check = db_.prepare(std::string("SELECT 1 FROM students WHERE id=? ") +
                             tenant::filter("") + "LIMIT 1;");
    check.bindAll(studentId);
    if (!check.step()) throw NotFoundError("Eleve", studentId);

    auto stmt = db_.prepare(
        "INSERT OR IGNORE INTO parent_students (user_id, student_id, relation) "
        "VALUES (?,?,?);");
    stmt.bindAll(parentUserId, studentId, relation);
    stmt.execute();
}

bool UserRepository::unlinkChild(long long parentUserId, long long studentId) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "DELETE FROM parent_students WHERE user_id=? AND student_id=?;");
    stmt.bindAll(parentUserId, studentId);
    stmt.execute();
    return db_.changes() > 0;
}

bool UserRepository::hasChild(long long parentUserId, long long studentId) {
    auto stmt = db_.prepare(
        std::string("SELECT 1 FROM parent_students ps "
                    "JOIN students st ON st.id = ps.student_id "
                    "WHERE ps.user_id = ? AND ps.student_id = ? ") +
        tenant::filter("st") + "LIMIT 1;");
    stmt.bindAll(parentUserId, studentId);
    return stmt.step();
}

}  // namespace app
