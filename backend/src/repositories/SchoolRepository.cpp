#include "repositories/SchoolRepository.hpp"

#include <random>

#include "core/Error.hpp"

namespace app {
namespace {

constexpr const char* kSelect =
    "SELECT id, code, name, city, country, phone, email, address, is_active, "
    "       created_at, updated_at FROM schools ";

School mapRow(const Statement& stmt) {
    School s;
    s.id = stmt.getInt64(0);
    s.code = stmt.getText(1);
    s.name = stmt.getText(2);
    s.city = stmt.getTextOpt(3);
    s.country = stmt.getTextOpt(4);
    s.phone = stmt.getTextOpt(5);
    s.email = stmt.getTextOpt(6);
    s.address = stmt.getTextOpt(7);
    s.isActive = stmt.getBool(8);
    s.createdAt = stmt.getText(9);
    s.updatedAt = stmt.getText(10);
    return s;
}

}  // namespace

long long SchoolRepository::create(const School& school) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "INSERT INTO schools (code, name, city, country, phone, email, address, is_active) "
        "VALUES (?,?,?,?,?,?,?,?);");
    stmt.bindAll(school.code, school.name, school.city, school.country, school.phone,
                 school.email, school.address, school.isActive);
    stmt.execute();
    return db_.lastInsertId();
}

void SchoolRepository::update(const School& school) {
    auto lock = db_.lockGuard();
    auto stmt = db_.prepare(
        "UPDATE schools SET code=?, name=?, city=?, country=?, phone=?, email=?, "
        "address=?, is_active=? WHERE id=?;");
    stmt.bindAll(school.code, school.name, school.city, school.country, school.phone,
                 school.email, school.address, school.isActive, school.id);
    stmt.execute();
    if (db_.changes() == 0) throw NotFoundError("Etablissement", school.id);
}

std::optional<School> SchoolRepository::findById(long long id) {
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE id=?;");
    stmt.bindAll(id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::optional<School> SchoolRepository::findByCode(const std::string& code) {
    // Comparaison insensible a la casse : un matricule se recopie a la main.
    auto stmt = db_.prepare(std::string(kSelect) + "WHERE UPPER(code)=UPPER(?);");
    stmt.bindAll(code);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<School> SchoolRepository::findAll() {
    std::vector<School> out;
    auto stmt = db_.prepare(std::string(kSelect) + "ORDER BY name;");
    while (stmt.step()) out.push_back(mapRow(stmt));
    return out;
}

bool SchoolRepository::codeExists(const std::string& code) {
    auto stmt = db_.prepare("SELECT 1 FROM schools WHERE UPPER(code)=UPPER(?) LIMIT 1;");
    stmt.bindAll(code);
    return stmt.step();
}

std::string SchoolRepository::generateCode() {
    // Alphabet sans caracteres ambigus : ni O/0, ni I/1, ni U (confusion V).
    static const char* kAlphabet = "ABCDEFGHJKLMNPQRSTVWXYZ23456789";
    static const int kAlphabetSize = 31;
    static const int kLength = 8;

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> dist(0, kAlphabetSize - 1);

    // 31^8 ~ 8.5e11 combinaisons : la collision est improbable, mais on la
    // traite tout de meme plutot que de la supposer impossible.
    for (int attempt = 0; attempt < 20; ++attempt) {
        std::string code;
        code.reserve(kLength);
        for (int i = 0; i < kLength; ++i) code.push_back(kAlphabet[dist(gen)]);
        if (!codeExists(code)) return code;
    }
    throw DatabaseError("Impossible de generer un matricule d'etablissement unique");
}

}  // namespace app
