#include "repositories/TeacherRepository.hpp"

#include "core/Error.hpp"
#include "core/Tenant.hpp"

namespace app {
namespace {

constexpr const char* kSelect =
    "SELECT id, user_id, first_name, last_name, email, phone, speciality, "
    "       created_at, updated_at FROM teachers ";

Teacher mapRow(const Statement& stmt) {
    Teacher t;
    t.id = stmt.getInt64(0);
    t.userId = stmt.getInt64Opt(1);
    t.firstName = stmt.getText(2);
    t.lastName = stmt.getText(3);
    t.email = stmt.getTextOpt(4);
    t.phone = stmt.getTextOpt(5);
    t.speciality = stmt.getTextOpt(6);
    t.createdAt = stmt.getText(7);
    t.updatedAt = stmt.getText(8);
    return t;
}

}  // namespace

long long TeacherRepository::create(const Teacher& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "INSERT INTO teachers (user_id, first_name, last_name, email, phone, speciality, "
        "                      school_id) VALUES (?,?,?,?,?,?,?);");
    stmt.bindAll(value.userId, value.firstName, value.lastName, value.email, value.phone,
                 value.speciality, tenant::requireCurrentSchool());
    stmt.execute();
    return db_.lastInsertId();
}

void TeacherRepository::update(const Teacher& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "UPDATE teachers SET user_id=?, first_name=?, last_name=?, email=?, phone=?, "
        " speciality=?, updated_at=datetime('now') WHERE id=? " + tenant::filter("") + ";");
    stmt.bindAll(value.userId, value.firstName, value.lastName, value.email, value.phone,
                 value.speciality, value.id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Enseignant", value.id);
}

bool TeacherRepository::remove(long long id) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(std::string("DELETE FROM teachers WHERE id=? ") + tenant::filter("") + ";");
    stmt.bindAll(id);
    stmt.execute();
    return db_.changes() > 0;
}

std::optional<Teacher> TeacherRepository::findById(long long id) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE id = ? " + tenant::filter("") + ";");
    stmt.bindAll(id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<Teacher> TeacherRepository::findAll() {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE 1=1 " + tenant::filter("") + "ORDER BY last_name, first_name;");
    std::vector<Teacher> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

bool TeacherRepository::exists(long long id) {
    auto stmt = db_.prepare(std::string("SELECT 1 FROM teachers WHERE id=? ") + tenant::filter("") +
                            "LIMIT 1;");
    stmt.bindAll(id);
    return stmt.step();
}

}  // namespace app
