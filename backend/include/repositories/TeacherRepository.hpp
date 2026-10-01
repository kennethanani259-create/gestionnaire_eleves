#pragma once
#include <optional>
#include <vector>

#include "database/Database.hpp"
#include "models/Teacher.hpp"

namespace app {

class ITeacherRepository {
public:
    virtual ~ITeacherRepository() = default;
    virtual long long create(const Teacher& value) = 0;
    virtual void update(const Teacher& value) = 0;
    virtual bool remove(long long id) = 0;
    virtual std::optional<Teacher> findById(long long id) = 0;
    virtual std::vector<Teacher> findAll() = 0;
    virtual bool exists(long long id) = 0;
};

class TeacherRepository : public ITeacherRepository {
public:
    explicit TeacherRepository(Database& db) : db_(db) {}
    long long create(const Teacher& value) override;
    void update(const Teacher& value) override;
    bool remove(long long id) override;
    std::optional<Teacher> findById(long long id) override;
    std::vector<Teacher> findAll() override;
    bool exists(long long id) override;

private:
    Database& db_;
};

}  // namespace app
