#pragma once
/**
 * @file StudentService.hpp
 * @brief Regles metier des eleves : validation, matricule, coherence de classe.
 */
#include <string>
#include <vector>

#include "repositories/ClassRepository.hpp"
#include "repositories/StudentRepository.hpp"

namespace app {

/// Resultat pagine d'une recherche.
struct StudentPage {
    std::vector<Student> items;
    long long total = 0;
    int limit = 0;
    int offset = 0;
    nlohmann::json toJson() const;
};

class StudentService {
public:
    StudentService(IStudentRepository& students, IClassRepository& classes)
        : students_(students), classes_(classes) {}

    /// Cree un eleve. Le matricule est genere s'il n'est pas fourni.
    Student create(Student input);
    /// Met a jour un eleve existant (remplacement complet des champs modifiables).
    Student update(long long id, Student input);
    /// Supprime definitivement un eleve (la confirmation releve de l'interface).
    void remove(long long id);
    Student get(long long id);
    StudentPage list(const StudentFilter& filter);
    std::vector<Student> byClass(long long classId, bool activeOnly);
    long long countAll();

private:
    /// Valide les champs et leve ValidationError en cas de probleme.
    void validate(const Student& student, std::optional<long long> existingId);

    IStudentRepository& students_;
    IClassRepository& classes_;
};

}  // namespace app
