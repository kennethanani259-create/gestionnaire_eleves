#pragma once
#include <optional>
#include <vector>

#include "database/Database.hpp"
#include "models/Subject.hpp"

namespace app {

class ISubjectRepository {
public:
    virtual ~ISubjectRepository() = default;
    virtual long long create(const Subject& value) = 0;
    virtual void update(const Subject& value) = 0;
    virtual bool remove(long long id) = 0;
    virtual std::optional<Subject> findById(long long id) = 0;
    virtual std::vector<Subject> findByClass(long long classId) = 0;
    virtual std::vector<Subject> findAll() = 0;
    virtual bool exists(long long id) = 0;
};

class SubjectRepository : public ISubjectRepository {
public:
    explicit SubjectRepository(Database& db) : db_(db) {}
    long long create(const Subject& value) override;
    void update(const Subject& value) override;
    bool remove(long long id) override;
    std::optional<Subject> findById(long long id) override;
    std::vector<Subject> findByClass(long long classId) override;
    std::vector<Subject> findAll() override;
    bool exists(long long id) override;

private:
    Database& db_;
};

}  // namespace app
