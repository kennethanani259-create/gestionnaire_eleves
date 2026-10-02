#include "repositories/SubjectRepository.hpp"

#include "core/Error.hpp"
#include "core/Tenant.hpp"

namespace app {
namespace {

constexpr const char* kSelect =
    "SELECT sb.id, sb.name, sb.code, sb.coefficient, sb.class_id, sb.teacher_id, "
    "       sb.created_at, sb.updated_at, c.name, "
    "       CASE WHEN t.id IS NULL THEN NULL ELSE (t.first_name || ' ' || t.last_name) END "
    "FROM subjects sb "
    "LEFT JOIN classes c ON c.id = sb.class_id "
    "LEFT JOIN teachers t ON t.id = sb.teacher_id ";

Subject mapRow(const Statement& stmt) {
    Subject s;
    s.id = stmt.getInt64(0);
    s.name = stmt.getText(1);
    s.code = stmt.getText(2);
    s.coefficient = stmt.getDouble(3);
    s.classId = stmt.getInt64(4);
    s.teacherId = stmt.getInt64Opt(5);
    s.createdAt = stmt.getText(6);
    s.updatedAt = stmt.getText(7);
    s.className = stmt.getTextOpt(8);
    s.teacherName = stmt.getTextOpt(9);
    return s;
}

}  // namespace

long long SubjectRepository::create(const Subject& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "INSERT INTO subjects (name, code, coefficient, class_id, teacher_id) "
        "VALUES (?,?,?,?,?);");
    stmt.bindAll(value.name, value.code, value.coefficient, value.classId, value.teacherId);
    stmt.execute();
    return db_.lastInsertId();
}

void SubjectRepository::update(const Subject& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "UPDATE subjects SET name=?, code=?, coefficient=?, class_id=?, teacher_id=?, "
        " updated_at=datetime('now') WHERE id=?;");
    stmt.bindAll(value.name, value.code, value.coefficient, value.classId, value.teacherId,
                 value.id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Matiere", value.id);
}

bool SubjectRepository::remove(long long id) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(std::string("DELETE FROM subjects WHERE id=? ") +
                            tenant::filterVia("subjects.class_id", "classes") + ";");
    stmt.bindAll(id);
    stmt.execute();
    return db_.changes() > 0;
}

std::optional<Subject> SubjectRepository::findById(long long id) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE sb.id = ? " + tenant::filterVia("sb.class_id", "classes") + ";");
    stmt.bindAll(id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<Subject> SubjectRepository::findByClass(long long classId) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE sb.class_id = ? " + tenant::filterVia("sb.class_id", "classes") + "ORDER BY sb.name;");
    stmt.bindAll(classId);
    std::vector<Subject> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

std::vector<Subject> SubjectRepository::findAll() {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE 1=1 " + tenant::filterVia("sb.class_id", "classes") + "ORDER BY c.name, sb.name;");
    std::vector<Subject> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

bool SubjectRepository::exists(long long id) {
    auto stmt = db_.prepare(std::string("SELECT 1 FROM subjects WHERE id=? ") +
                            tenant::filterVia("subjects.class_id", "classes") + "LIMIT 1;");
    stmt.bindAll(id);
    return stmt.step();
}

}  // namespace app
