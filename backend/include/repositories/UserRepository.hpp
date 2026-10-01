#pragma once
#include <optional>
#include <string>
#include <vector>

#include "database/Database.hpp"
#include "models/User.hpp"

namespace app {

class IUserRepository {
public:
    virtual ~IUserRepository() = default;
    virtual long long create(const User& value) = 0;
    virtual void update(const User& value) = 0;
    virtual void updatePassword(long long id, const std::string& passwordHash) = 0;
    virtual void touchLastLogin(long long id) = 0;
    virtual bool remove(long long id) = 0;
    virtual std::optional<User> findById(long long id) = 0;
    virtual std::optional<User> findByUsername(const std::string& username) = 0;
    virtual std::optional<User> findByEmail(const std::string& email) = 0;
    virtual std::vector<User> findAll() = 0;
    virtual long long count() = 0;
};

class UserRepository : public IUserRepository {
public:
    explicit UserRepository(Database& db) : db_(db) {}
    long long create(const User& value) override;
    void update(const User& value) override;
    void updatePassword(long long id, const std::string& passwordHash) override;
    void touchLastLogin(long long id) override;
    bool remove(long long id) override;
    std::optional<User> findById(long long id) override;
    std::optional<User> findByUsername(const std::string& username) override;
    std::optional<User> findByEmail(const std::string& email) override;
    std::vector<User> findAll() override;
    long long count() override;

private:
    Database& db_;
};

}  // namespace app
