#pragma once
#include <optional>
#include <vector>

#include "database/Database.hpp"
#include "models/SchoolYear.hpp"

namespace app {

class ISchoolYearRepository {
public:
    virtual ~ISchoolYearRepository() = default;
    virtual long long create(const SchoolYear& value) = 0;
    virtual void update(const SchoolYear& value) = 0;
    virtual bool remove(long long id) = 0;
    virtual std::optional<SchoolYear> findById(long long id) = 0;
    virtual std::optional<SchoolYear> findCurrent() = 0;
    virtual std::vector<SchoolYear> findAll() = 0;
    virtual void setCurrent(long long id) = 0;
};

class SchoolYearRepository : public ISchoolYearRepository {
public:
    explicit SchoolYearRepository(Database& db) : db_(db) {}
    long long create(const SchoolYear& value) override;
    void update(const SchoolYear& value) override;
    bool remove(long long id) override;
    std::optional<SchoolYear> findById(long long id) override;
    std::optional<SchoolYear> findCurrent() override;
    std::vector<SchoolYear> findAll() override;
    void setCurrent(long long id) override;

private:
    Database& db_;
};

}  // namespace app
