#include "repositories/StudentRepository.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "core/Error.hpp"

namespace app {
namespace {

/// Liste de colonnes partagee par toutes les lectures (ordre = index de mapRow).
constexpr const char* kSelect =
    "SELECT s.id, s.matricule, s.first_name, s.last_name, s.birth_date, s.gender, "
    "       s.address, s.phone, s.email, s.guardian_name, s.guardian_phone, "
    "       s.enrollment_date, s.status, s.photo_path, s.class_id, "
    "       s.created_at, s.updated_at, c.name, c.level "
    "FROM students s LEFT JOIN classes c ON c.id = s.class_id ";

Student mapRow(const Statement& stmt) {
    Student s;
    s.id = stmt.getInt64(0);
    s.matricule = stmt.getText(1);
    s.firstName = stmt.getText(2);
    s.lastName = stmt.getText(3);
    s.birthDate = stmt.getText(4);
    s.gender = parseGender(stmt.getText(5));
    s.address = stmt.getTextOpt(6);
    s.phone = stmt.getTextOpt(7);
    s.email = stmt.getTextOpt(8);
    s.guardianName = stmt.getTextOpt(9);
    s.guardianPhone = stmt.getTextOpt(10);
    s.enrollmentDate = stmt.getText(11);
    s.status = parseStudentStatus(stmt.getText(12));
    s.photoPath = stmt.getTextOpt(13);
    s.classId = stmt.getInt64Opt(14);
    s.createdAt = stmt.getText(15);
    s.updatedAt = stmt.getText(16);
    s.className = stmt.getTextOpt(17);
    s.classLevel = stmt.getTextOpt(18);
    return s;
}

/// Liste blanche des colonnes de tri : empeche toute injection via sortBy.
std::string safeSortColumn(const std::string& requested) {
    static const std::vector<std::pair<std::string, std::string>> kAllowed = {
        {"last_name", "s.last_name"},   {"first_name", "s.first_name"},
        {"matricule", "s.matricule"},   {"birth_date", "s.birth_date"},
        {"created_at", "s.created_at"}, {"status", "s.status"},
        {"class", "c.name"},
    };
    for (const auto& [key, column] : kAllowed) {
        if (key == requested) return column;
    }
    return "s.last_name";
}

}  // namespace

long long StudentRepository::create(const Student& student) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "INSERT INTO students (matricule, first_name, last_name, birth_date, gender, "
        " address, phone, email, guardian_name, guardian_phone, enrollment_date, "
        " status, photo_path, class_id) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
    stmt.bindAll(student.matricule, student.firstName, student.lastName, student.birthDate,
                 toString(student.gender), student.address, student.phone, student.email,
                 student.guardianName, student.guardianPhone, student.enrollmentDate,
                 toString(student.status), student.photoPath, student.classId);
    stmt.execute();
    return db_.lastInsertId();
}

void StudentRepository::update(const Student& student) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "UPDATE students SET matricule=?, first_name=?, last_name=?, birth_date=?, gender=?, "
        " address=?, phone=?, email=?, guardian_name=?, guardian_phone=?, enrollment_date=?, "
        " status=?, photo_path=?, class_id=? WHERE id=?;");
    stmt.bindAll(student.matricule, student.firstName, student.lastName, student.birthDate,
                 toString(student.gender), student.address, student.phone, student.email,
                 student.guardianName, student.guardianPhone, student.enrollmentDate,
                 toString(student.status), student.photoPath, student.classId, student.id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Eleve", student.id);
}

bool StudentRepository::remove(long long id) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare("DELETE FROM students WHERE id=?;");
    stmt.bindAll(id);
    stmt.execute();
    return db_.changes() > 0;
}

std::optional<Student> StudentRepository::findById(long long id) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE s.id = ?;");
    stmt.bindAll(id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::optional<Student> StudentRepository::findByMatricule(const std::string& matricule) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE s.matricule = ?;");
    stmt.bindAll(matricule);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

namespace {

/// Applique les filtres communs a search() et count().
/// Retourne le SQL et lie les parametres dans l'ordre d'apparition.
std::string filterSql(const StudentFilter& filter) {
    std::ostringstream sql;
    sql << "WHERE 1=1 ";
    if (filter.query.has_value() && !filter.query->empty()) {
        sql << "AND (s.last_name LIKE ? OR s.first_name LIKE ? OR s.matricule LIKE ? "
               "OR (s.first_name || ' ' || s.last_name) LIKE ?) ";
    }
    if (filter.classId.has_value()) sql << "AND s.class_id = ? ";
    if (filter.status.has_value()) sql << "AND s.status = ? ";
    if (filter.gender.has_value()) sql << "AND s.gender = ? ";
    if (filter.level.has_value() && !filter.level->empty()) sql << "AND c.level = ? ";
    return sql.str();
}

int bindFilter(Statement& stmt, const StudentFilter& filter) {
    int index = 1;
    if (filter.query.has_value() && !filter.query->empty()) {
        const std::string pattern = "%" + *filter.query + "%";
        stmt.bind(index++, pattern);
        stmt.bind(index++, pattern);
        stmt.bind(index++, pattern);
        stmt.bind(index++, pattern);
    }
    if (filter.classId.has_value()) stmt.bind(index++, *filter.classId);
    if (filter.status.has_value()) stmt.bind(index++, toString(*filter.status));
    if (filter.gender.has_value()) stmt.bind(index++, toString(*filter.gender));
    if (filter.level.has_value() && !filter.level->empty()) stmt.bind(index++, *filter.level);
    return index;
}

}  // namespace

std::vector<Student> StudentRepository::search(const StudentFilter& filter) {
    std::ostringstream sql;
    sql << kSelect << filterSql(filter) << "ORDER BY " << safeSortColumn(filter.sortBy)
        << (filter.descending ? " DESC" : " ASC") << ", s.id ASC ";
    if (filter.limit > 0) sql << "LIMIT ? OFFSET ? ";

    auto stmt = db_.prepare(sql.str());
    int index = bindFilter(stmt, filter);
    if (filter.limit > 0) {
        stmt.bind(index++, static_cast<long long>(filter.limit));
        stmt.bind(index++, static_cast<long long>(std::max(0, filter.offset)));
    }

    std::vector<Student> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

long long StudentRepository::count(const StudentFilter& filter) {
    const std::string sql =
        "SELECT COUNT(*) FROM students s LEFT JOIN classes c ON c.id = s.class_id " +
        filterSql(filter);
    auto stmt = db_.prepare(sql);
    bindFilter(stmt, filter);
    if (!stmt.step()) return 0;
    return stmt.getInt64(0);
}

std::vector<Student> StudentRepository::findByClass(long long classId, bool activeOnly) {
    std::string sql = std::string(kSelect) + "WHERE s.class_id = ? ";
    if (activeOnly) sql += "AND s.status = 'ACTIVE' ";
    sql += "ORDER BY s.last_name, s.first_name;";

    auto stmt = db_.prepare(sql);
    stmt.bindAll(classId);
    std::vector<Student> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

bool StudentRepository::matriculeExists(const std::string& matricule,
                                        std::optional<long long> excludeId) {
    auto stmt = db_.prepare(
        "SELECT 1 FROM students WHERE matricule = ? AND (? IS NULL OR id <> ?) LIMIT 1;");
    stmt.bindAll(matricule, excludeId, excludeId);
    return stmt.step();
}

std::string StudentRepository::nextMatricule(int year) {
    auto lock = db_.lockGuard();
    const std::string prefix = "STU-" + std::to_string(year) + "-";
    auto stmt = db_.prepare(
        "SELECT matricule FROM students WHERE matricule LIKE ? "
        "ORDER BY matricule DESC LIMIT 1;");
    stmt.bindAll(prefix + "%");

    long long next = 1;
    if (stmt.step()) {
        const std::string last = stmt.getText(0);
        const std::string suffix = last.substr(prefix.size());
        try {
            next = std::stoll(suffix) + 1;
        } catch (const std::exception&) {
            next = 1;
        }
    }

    std::ostringstream os;
    os << prefix << std::setfill('0') << std::setw(4) << next;
    return os.str();
}

}  // namespace app
