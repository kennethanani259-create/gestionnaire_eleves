#pragma once
/**
 * @file SchoolRepository.hpp
 * @brief Acces aux donnees des etablissements.
 */
#include <optional>
#include <string>
#include <vector>

#include "database/Database.hpp"
#include "models/School.hpp"

namespace app {

class ISchoolRepository {
public:
    virtual ~ISchoolRepository() = default;

    virtual long long create(const School& school) = 0;
    virtual void update(const School& school) = 0;
    virtual std::optional<School> findById(long long id) = 0;
    virtual std::optional<School> findByCode(const std::string& code) = 0;
    virtual std::vector<School> findAll() = 0;
    virtual bool codeExists(const std::string& code) = 0;
    /// Matricule aleatoire non encore utilise (alphabet sans O/0 ni I/1).
    virtual std::string generateCode() = 0;
};

class SchoolRepository : public ISchoolRepository {
public:
    explicit SchoolRepository(Database& db) : db_(db) {}

    long long create(const School& school) override;
    void update(const School& school) override;
    std::optional<School> findById(long long id) override;
    std::optional<School> findByCode(const std::string& code) override;
    std::vector<School> findAll() override;
    bool codeExists(const std::string& code) override;
    std::string generateCode() override;

private:
    Database& db_;
};

}  // namespace app
