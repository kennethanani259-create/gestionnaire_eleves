#pragma once
/**
 * @file StudentRepository.hpp
 * @brief Acces aux donnees des eleves (interface + implementation SQLite).
 */
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "database/Database.hpp"
#include "models/Student.hpp"

namespace app {

/// Criteres de recherche/filtrage, traduits en SQL parametre.
struct StudentFilter {
    std::optional<std::string> query;      ///< nom, prenom ou matricule (recherche partielle)
    std::optional<long long> classId;
    std::optional<StudentStatus> status;
    std::optional<Gender> gender;
    std::optional<std::string> level;      ///< niveau de la classe
    std::string sortBy = "last_name";      ///< last_name|first_name|matricule|birth_date|created_at
    bool descending = false;
    int limit = 50;                        ///< <= 0 : pas de limite
    int offset = 0;
};

/// Interface de persistance des eleves (permet mocks et autre SGBD).
class IStudentRepository {
public:
    virtual ~IStudentRepository() = default;

    virtual long long create(const Student& student) = 0;
    virtual void update(const Student& student) = 0;
    virtual bool remove(long long id) = 0;
    virtual std::optional<Student> findById(long long id) = 0;
    virtual std::optional<Student> findByMatricule(const std::string& matricule) = 0;
    virtual std::vector<Student> search(const StudentFilter& filter) = 0;
    virtual long long count(const StudentFilter& filter) = 0;
    virtual std::vector<Student> findByClass(long long classId, bool activeOnly) = 0;
    virtual bool matriculeExists(const std::string& matricule,
                                 std::optional<long long> excludeId = std::nullopt) = 0;
    /// Genere le prochain matricule disponible pour une annee (ex: STU-2026-0007).
    virtual std::string nextMatricule(int year) = 0;
};

/// Implementation SQLite.
class StudentRepository : public IStudentRepository {
public:
    explicit StudentRepository(Database& db) : db_(db) {}

    long long create(const Student& student) override;
    void update(const Student& student) override;
    bool remove(long long id) override;
    std::optional<Student> findById(long long id) override;
    std::optional<Student> findByMatricule(const std::string& matricule) override;
    std::vector<Student> search(const StudentFilter& filter) override;
    long long count(const StudentFilter& filter) override;
    std::vector<Student> findByClass(long long classId, bool activeOnly) override;
    bool matriculeExists(const std::string& matricule,
                         std::optional<long long> excludeId = std::nullopt) override;
    std::string nextMatricule(int year) override;

private:
    Database& db_;
};

}  // namespace app
