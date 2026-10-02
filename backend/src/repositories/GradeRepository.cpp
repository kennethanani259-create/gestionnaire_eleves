#include "repositories/GradeRepository.hpp"

#include <sstream>

#include "core/Error.hpp"
#include "core/Tenant.hpp"

namespace app {
namespace {

constexpr const char* kSelect =
    "SELECT g.id, g.student_id, g.subject_id, g.eval_type, g.score, g.max_score, "
    "       g.eval_date, g.term, g.comment, g.created_at, g.updated_at, "
    "       sb.name, (st.first_name || ' ' || st.last_name), sb.coefficient "
    "FROM grades g "
    "JOIN subjects sb ON sb.id = g.subject_id "
    "JOIN students st ON st.id = g.student_id ";

Grade mapRow(const Statement& stmt) {
    Grade g;
    g.id = stmt.getInt64(0);
    g.studentId = stmt.getInt64(1);
    g.subjectId = stmt.getInt64(2);
    g.evalType = parseEvalType(stmt.getText(3));
    g.score = stmt.getDouble(4);
    g.maxScore = stmt.getDouble(5);
    g.evalDate = stmt.getText(6);
    g.term = stmt.getInt(7);
    g.comment = stmt.getTextOpt(8);
    g.createdAt = stmt.getText(9);
    g.updatedAt = stmt.getText(10);
    g.subjectName = stmt.getTextOpt(11);
    g.studentName = stmt.getTextOpt(12);
    g.coefficient = stmt.getDoubleOpt(13);
    return g;
}

SubjectAverageRow mapAverage(const Statement& stmt) {
    SubjectAverageRow row;
    row.studentId = stmt.getInt64(0);
    row.subjectId = stmt.getInt64(1);
    row.subjectName = stmt.getText(2);
    row.subjectCode = stmt.getText(3);
    row.coefficient = stmt.getDouble(4);
    row.gradeCount = stmt.getInt64(5);
    row.average20 = stmt.getDouble(6);
    row.best20 = stmt.getDouble(7);
    row.worst20 = stmt.getDouble(8);
    return row;
}

/// Agregation commune : notes ramenees sur 20 avant moyenne.
constexpr const char* kAverageSelect =
    "SELECT g.student_id, g.subject_id, sb.name, sb.code, sb.coefficient, "
    "       COUNT(*), AVG(g.score * 20.0 / g.max_score), "
    "       MAX(g.score * 20.0 / g.max_score), MIN(g.score * 20.0 / g.max_score) "
    "FROM grades g JOIN subjects sb ON sb.id = g.subject_id ";

}  // namespace

long long GradeRepository::create(const Grade& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "INSERT INTO grades (student_id, subject_id, eval_type, score, max_score, "
        " eval_date, term, comment) VALUES (?,?,?,?,?,?,?,?);");
    stmt.bindAll(value.studentId, value.subjectId, toString(value.evalType), value.score,
                 value.maxScore, value.evalDate, value.term, value.comment);
    stmt.execute();
    return db_.lastInsertId();
}

void GradeRepository::update(const Grade& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "UPDATE grades SET student_id=?, subject_id=?, eval_type=?, score=?, max_score=?, "
        " eval_date=?, term=?, comment=? WHERE id=?;");
    stmt.bindAll(value.studentId, value.subjectId, toString(value.evalType), value.score,
                 value.maxScore, value.evalDate, value.term, value.comment, value.id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Note", value.id);
}

bool GradeRepository::remove(long long id) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(std::string("DELETE FROM grades WHERE id=? ") +
                            tenant::filterVia("grades.student_id", "students") + ";");
    stmt.bindAll(id);
    stmt.execute();
    return db_.changes() > 0;
}

std::optional<Grade> GradeRepository::findById(long long id) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE g.id=? " + tenant::filterVia("g.student_id", "students") + ";");
    stmt.bindAll(id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<Grade> GradeRepository::find(const GradeFilter& filter) {
    std::ostringstream sql;
    sql << kSelect << "WHERE 1=1 " << tenant::filterVia("g.student_id", "students");
    if (filter.studentId.has_value()) sql << "AND g.student_id = ? ";
    if (filter.subjectId.has_value()) sql << "AND g.subject_id = ? ";
    if (filter.classId.has_value()) sql << "AND sb.class_id = ? ";
    if (filter.term.has_value()) sql << "AND g.term = ? ";
    if (filter.evalType.has_value()) sql << "AND g.eval_type = ? ";
    sql << "ORDER BY g.eval_date DESC, g.id DESC ";
    if (filter.limit > 0) sql << "LIMIT ? OFFSET ? ";

    auto stmt = db_.prepare(sql.str());
    int index = 1;
    if (filter.studentId.has_value()) stmt.bind(index++, *filter.studentId);
    if (filter.subjectId.has_value()) stmt.bind(index++, *filter.subjectId);
    if (filter.classId.has_value()) stmt.bind(index++, *filter.classId);
    if (filter.term.has_value()) stmt.bind(index++, *filter.term);
    if (filter.evalType.has_value()) stmt.bind(index++, toString(*filter.evalType));
    if (filter.limit > 0) {
        stmt.bind(index++, static_cast<long long>(filter.limit));
        stmt.bind(index++, static_cast<long long>(filter.offset));
    }

    std::vector<Grade> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

std::vector<SubjectAverageRow> GradeRepository::subjectAverages(long long studentId,
                                                                std::optional<int> term) {
    std::ostringstream sql;
    sql << kAverageSelect << "WHERE g.student_id = ? " << tenant::filterVia("g.student_id", "students");
    if (term.has_value()) sql << "AND g.term = ? ";
    sql << "GROUP BY g.student_id, g.subject_id ORDER BY sb.name;";

    auto stmt = db_.prepare(sql.str());
    int index = 1;
    stmt.bind(index++, studentId);
    if (term.has_value()) stmt.bind(index++, *term);

    std::vector<SubjectAverageRow> rows;
    while (stmt.step()) rows.push_back(mapAverage(stmt));
    return rows;
}

std::vector<SubjectAverageRow> GradeRepository::classSubjectAverages(long long classId,
                                                                     std::optional<int> term) {
    std::ostringstream sql;
    sql << kAverageSelect << "WHERE sb.class_id = ? " << tenant::filterVia("g.student_id", "students");
    if (term.has_value()) sql << "AND g.term = ? ";
    sql << "GROUP BY g.student_id, g.subject_id ORDER BY g.student_id, sb.name;";

    auto stmt = db_.prepare(sql.str());
    int index = 1;
    stmt.bind(index++, classId);
    if (term.has_value()) stmt.bind(index++, *term);

    std::vector<SubjectAverageRow> rows;
    while (stmt.step()) rows.push_back(mapAverage(stmt));
    return rows;
}

}  // namespace app
