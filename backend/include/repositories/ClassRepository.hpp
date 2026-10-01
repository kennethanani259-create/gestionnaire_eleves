#pragma once
#include <optional>
#include <string>
#include <vector>

#include "database/Database.hpp"
#include "models/ClassRoom.hpp"

namespace app {

struct ClassFilter {
    std::optional<long long> schoolYearId;
    std::optional<std::string> level;
    std::optional<std::string> query;
};

class IClassRepository {
public:
    virtual ~IClassRepository() = default;
    virtual long long create(const ClassRoom& value) = 0;
    virtual void update(const ClassRoom& value) = 0;
    virtual bool remove(long long id) = 0;
    virtual std::optional<ClassRoom> findById(long long id) = 0;
    virtual std::vector<ClassRoom> findAll(const ClassFilter& filter) = 0;
    virtual bool exists(long long id) = 0;
};

class ClassRepository : public IClassRepository {
public:
    explicit ClassRepository(Database& db) : db_(db) {}
    long long create(const ClassRoom& value) override;
    void update(const ClassRoom& value) override;
    bool remove(long long id) override;
    std::optional<ClassRoom> findById(long long id) override;
    std::vector<ClassRoom> findAll(const ClassFilter& filter) override;
    bool exists(long long id) override;

private:
    Database& db_;
};

}  // namespace app
