#include "repositories/ClassRepository.hpp"

#include <sstream>

#include "core/Error.hpp"
#include "core/Tenant.hpp"

namespace app {
namespace {

constexpr const char* kSelect =
    "SELECT c.id, c.name, c.level, c.school_year_id, c.main_teacher_id, c.room, c.capacity, "
    "       c.created_at, c.updated_at, y.label, "
    "       CASE WHEN t.id IS NULL THEN NULL ELSE (t.first_name || ' ' || t.last_name) END, "
    "       (SELECT COUNT(*) FROM students s WHERE s.class_id = c.id), "
    "       (SELECT COUNT(*) FROM subjects sb WHERE sb.class_id = c.id) "
    "FROM classes c "
    "LEFT JOIN school_years y ON y.id = c.school_year_id "
    "LEFT JOIN teachers t ON t.id = c.main_teacher_id ";

ClassRoom mapRow(const Statement& stmt) {
    ClassRoom c;
    c.id = stmt.getInt64(0);
    c.name = stmt.getText(1);
    c.level = stmt.getText(2);
    c.schoolYearId = stmt.getInt64(3);
    c.mainTeacherId = stmt.getInt64Opt(4);
    c.room = stmt.getTextOpt(5);
    c.capacity = stmt.getInt64Opt(6);
    c.createdAt = stmt.getText(7);
    c.updatedAt = stmt.getText(8);
    c.schoolYearLabel = stmt.getTextOpt(9);
    c.mainTeacherName = stmt.getTextOpt(10);
    c.studentCount = stmt.getInt64(11);
    c.subjectCount = stmt.getInt64(12);
    return c;
}

}  // namespace

long long ClassRepository::create(const ClassRoom& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "INSERT INTO classes (name, level, school_year_id, main_teacher_id, room, capacity, "
        "                     school_id) VALUES (?,?,?,?,?,?,?);");
    stmt.bindAll(value.name, value.level, value.schoolYearId, value.mainTeacherId, value.room,
                 value.capacity, tenant::requireCurrentSchool());
    stmt.execute();
    return db_.lastInsertId();
}

void ClassRepository::update(const ClassRoom& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "UPDATE classes SET name=?, level=?, school_year_id=?, main_teacher_id=?, room=?, "
        " capacity=?, updated_at=datetime('now') WHERE id=? " + tenant::filter("") + ";");
    stmt.bindAll(value.name, value.level, value.schoolYearId, value.mainTeacherId, value.room,
                 value.capacity, value.id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Classe", value.id);
}

bool ClassRepository::remove(long long id) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(std::string("DELETE FROM classes WHERE id=? ") + tenant::filter("") + ";");
    stmt.bindAll(id);
    stmt.execute();
    return db_.changes() > 0;
}

std::optional<ClassRoom> ClassRepository::findById(long long id) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE c.id = ? " + tenant::filter("c") + ";");
    stmt.bindAll(id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<ClassRoom> ClassRepository::findAll(const ClassFilter& filter) {
    std::ostringstream sql;
    sql << kSelect << "WHERE 1=1 " << tenant::filter("c");
    if (filter.schoolYearId.has_value()) sql << "AND c.school_year_id = ? ";
    if (filter.level.has_value() && !filter.level->empty()) sql << "AND c.level = ? ";
    if (filter.query.has_value() && !filter.query->empty()) sql << "AND c.name LIKE ? ";
    sql << "ORDER BY c.level, c.name;";

    auto stmt = db_.prepare(sql.str());
    int index = 1;
    if (filter.schoolYearId.has_value()) stmt.bind(index++, *filter.schoolYearId);
    if (filter.level.has_value() && !filter.level->empty()) stmt.bind(index++, *filter.level);
    if (filter.query.has_value() && !filter.query->empty()) {
        stmt.bind(index++, "%" + *filter.query + "%");
    }

    std::vector<ClassRoom> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

bool ClassRepository::exists(long long id) {
    auto stmt = db_.prepare(std::string("SELECT 1 FROM classes WHERE id=? ") + tenant::filter("") +
                            "LIMIT 1;");
    stmt.bindAll(id);
    return stmt.step();
}

}  // namespace app
