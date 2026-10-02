#include "repositories/AttendanceRepository.hpp"

#include <sstream>

#include "core/Error.hpp"
#include "core/Tenant.hpp"

namespace app {
namespace {

constexpr const char* kSelect =
    "SELECT a.id, a.student_id, a.subject_id, a.att_date, a.att_time, a.status, "
    "       a.justification, a.comment, a.created_at, a.updated_at, sb.name, "
    "       (st.first_name || ' ' || st.last_name) "
    "FROM attendance a "
    "JOIN students st ON st.id = a.student_id "
    "LEFT JOIN subjects sb ON sb.id = a.subject_id ";

Attendance mapRow(const Statement& stmt) {
    Attendance a;
    a.id = stmt.getInt64(0);
    a.studentId = stmt.getInt64(1);
    a.subjectId = stmt.getInt64Opt(2);
    a.date = stmt.getText(3);
    a.time = stmt.getTextOpt(4);
    a.status = parseAttendanceStatus(stmt.getText(5));
    a.justification = stmt.getTextOpt(6);
    a.comment = stmt.getTextOpt(7);
    a.createdAt = stmt.getText(8);
    a.updatedAt = stmt.getText(9);
    a.subjectName = stmt.getTextOpt(10);
    a.studentName = stmt.getTextOpt(11);
    return a;
}

/// Compteurs par statut, calcules en SQL.
constexpr const char* kSummarySelect =
    "SELECT a.student_id, COUNT(*), "
    "       SUM(CASE WHEN a.status='PRESENT' THEN 1 ELSE 0 END), "
    "       SUM(CASE WHEN a.status='ABSENT'  THEN 1 ELSE 0 END), "
    "       SUM(CASE WHEN a.status='EXCUSED' THEN 1 ELSE 0 END), "
    "       SUM(CASE WHEN a.status='LATE'    THEN 1 ELSE 0 END) "
    "FROM attendance a ";

AttendanceSummary mapSummary(const Statement& stmt) {
    AttendanceSummary s;
    s.studentId = stmt.getInt64(0);
    s.total = stmt.getInt64(1);
    s.present = stmt.getInt64(2);
    s.absent = stmt.getInt64(3);
    s.excused = stmt.getInt64(4);
    s.late = stmt.getInt64(5);
    s.attendanceRate =
        s.total > 0 ? static_cast<double>(s.present + s.late) * 100.0 / static_cast<double>(s.total)
                    : 0.0;
    return s;
}

}  // namespace

long long AttendanceRepository::create(const Attendance& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "INSERT INTO attendance (student_id, subject_id, att_date, att_time, status, "
        " justification, comment) VALUES (?,?,?,?,?,?,?);");
    stmt.bindAll(value.studentId, value.subjectId, value.date, value.time,
                 toString(value.status), value.justification, value.comment);
    stmt.execute();
    return db_.lastInsertId();
}

void AttendanceRepository::update(const Attendance& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "UPDATE attendance SET student_id=?, subject_id=?, att_date=?, att_time=?, status=?, "
        " justification=?, comment=? WHERE id=?;");
    stmt.bindAll(value.studentId, value.subjectId, value.date, value.time,
                 toString(value.status), value.justification, value.comment, value.id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Releve de presence", value.id);
}

bool AttendanceRepository::remove(long long id) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(std::string("DELETE FROM attendance WHERE id=? ") +
                            tenant::filterVia("attendance.student_id", "students") + ";");
    stmt.bindAll(id);
    stmt.execute();
    return db_.changes() > 0;
}

std::optional<Attendance> AttendanceRepository::findById(long long id) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE a.id=? " + tenant::filterVia("a.student_id", "students") + ";");
    stmt.bindAll(id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<Attendance> AttendanceRepository::find(const AttendanceFilter& filter) {
    std::ostringstream sql;
    sql << kSelect << "WHERE 1=1 " << tenant::filterVia("a.student_id", "students");
    if (filter.studentId.has_value()) sql << "AND a.student_id = ? ";
    if (filter.classId.has_value()) sql << "AND st.class_id = ? ";
    if (filter.status.has_value()) sql << "AND a.status = ? ";
    if (filter.from.has_value()) sql << "AND a.att_date >= ? ";
    if (filter.to.has_value()) sql << "AND a.att_date <= ? ";
    sql << "ORDER BY a.att_date DESC, a.id DESC ";
    if (filter.limit > 0) sql << "LIMIT ? OFFSET ? ";

    auto stmt = db_.prepare(sql.str());
    int index = 1;
    if (filter.studentId.has_value()) stmt.bind(index++, *filter.studentId);
    if (filter.classId.has_value()) stmt.bind(index++, *filter.classId);
    if (filter.status.has_value()) stmt.bind(index++, toString(*filter.status));
    if (filter.from.has_value()) stmt.bind(index++, *filter.from);
    if (filter.to.has_value()) stmt.bind(index++, *filter.to);
    if (filter.limit > 0) {
        stmt.bind(index++, static_cast<long long>(filter.limit));
        stmt.bind(index++, static_cast<long long>(filter.offset));
    }

    std::vector<Attendance> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

AttendanceSummary AttendanceRepository::summaryForStudent(long long studentId) {
    auto stmt = db_.prepare(std::string(kSummarySelect) +
                            "WHERE a.student_id = ? " + tenant::filterVia("a.student_id", "students") + "GROUP BY a.student_id;");
    stmt.bindAll(studentId);
    if (!stmt.step()) {
        AttendanceSummary empty;
        empty.studentId = studentId;
        return empty;
    }
    return mapSummary(stmt);
}

std::vector<AttendanceSummary> AttendanceRepository::summaryForClass(long long classId) {
    auto stmt = db_.prepare(std::string(kSummarySelect) +
                            "JOIN students st ON st.id = a.student_id "
                            "WHERE st.class_id = ? " + tenant::filterVia("a.student_id", "students") + "GROUP BY a.student_id;");
    stmt.bindAll(classId);
    std::vector<AttendanceSummary> rows;
    while (stmt.step()) rows.push_back(mapSummary(stmt));
    return rows;
}

std::vector<std::pair<std::string, long long>> AttendanceRepository::monthlyAbsences(
    std::optional<long long> classId) {
    std::ostringstream sql;
    sql << "SELECT substr(a.att_date, 1, 7) AS month, COUNT(*) "
           "FROM attendance a JOIN students st ON st.id = a.student_id "
           "WHERE a.status IN ('ABSENT','EXCUSED') " << tenant::filterVia("a.student_id", "students");
    if (classId.has_value()) sql << "AND st.class_id = ? ";
    sql << "GROUP BY month ORDER BY month;";

    auto stmt = db_.prepare(sql.str());
    if (classId.has_value()) stmt.bind(1, *classId);

    std::vector<std::pair<std::string, long long>> rows;
    while (stmt.step()) rows.emplace_back(stmt.getText(0), stmt.getInt64(1));
    return rows;
}

}  // namespace app
