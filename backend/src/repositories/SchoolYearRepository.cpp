#include "repositories/SchoolYearRepository.hpp"

#include "core/Error.hpp"
#include "core/Tenant.hpp"

namespace app {
namespace {

constexpr const char* kSelect =
    "SELECT id, label, start_date, end_date, is_current, created_at FROM school_years ";

SchoolYear mapRow(const Statement& stmt) {
    SchoolYear y;
    y.id = stmt.getInt64(0);
    y.label = stmt.getText(1);
    y.startDate = stmt.getText(2);
    y.endDate = stmt.getText(3);
    y.isCurrent = stmt.getBool(4);
    y.createdAt = stmt.getText(5);
    return y;
}

}  // namespace

long long SchoolYearRepository::create(const SchoolYear& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "INSERT INTO school_years (label, start_date, end_date, is_current, school_id) "
        "VALUES (?,?,?,?,?);");
    stmt.bindAll(value.label, value.startDate, value.endDate, value.isCurrent,
                 tenant::requireCurrentSchool());
    stmt.execute();
    const long long id = db_.lastInsertId();
    if (value.isCurrent) setCurrent(id);
    return id;
}

void SchoolYearRepository::update(const SchoolYear& value) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        std::string("UPDATE school_years SET label=?, start_date=?, end_date=? WHERE id=? ") +
        tenant::filter("") + ";");
    stmt.bindAll(value.label, value.startDate, value.endDate, value.id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Annee scolaire", value.id);
    if (value.isCurrent) setCurrent(value.id);
}

bool SchoolYearRepository::remove(long long id) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(std::string("DELETE FROM school_years WHERE id=? ") + tenant::filter("") + ";");
    stmt.bindAll(id);
    stmt.execute();
    return db_.changes() > 0;
}

std::optional<SchoolYear> SchoolYearRepository::findById(long long id) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE id=? " + tenant::filter("") + ";");
    stmt.bindAll(id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::optional<SchoolYear> SchoolYearRepository::findCurrent() {
    auto stmt = db_.prepare(std::string(kSelect) +
                            "WHERE is_current = 1 " + tenant::filter("") +
                            "ORDER BY start_date DESC LIMIT 1;");
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<SchoolYear> SchoolYearRepository::findAll() {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE 1=1 " + tenant::filter("") + "ORDER BY start_date DESC;");
    std::vector<SchoolYear> results;
    while (stmt.step()) results.push_back(mapRow(stmt));
    return results;
}

void SchoolYearRepository::setCurrent(long long id) {
    auto lock = db_.lockGuard();
    Transaction tx(db_);
    db_.executeScript("UPDATE school_years SET is_current = 0;");
    auto stmt = db_.prepare(std::string("UPDATE school_years SET is_current = 1 WHERE id=? ") +
                            tenant::filter("") + ";");
    stmt.bindAll(id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Annee scolaire", id);
    tx.commit();
}

}  // namespace app
