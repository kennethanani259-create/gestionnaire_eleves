#pragma once
/// Regles metier des classes, matieres et enseignants.
#include <vector>

#include "repositories/ClassRepository.hpp"
#include "repositories/SchoolYearRepository.hpp"
#include "repositories/StudentRepository.hpp"
#include "repositories/SubjectRepository.hpp"
#include "repositories/TeacherRepository.hpp"

namespace app {

class ClassService {
public:
    ClassService(IClassRepository& classes, ISubjectRepository& subjects,
                 IStudentRepository& students, ITeacherRepository& teachers,
                 ISchoolYearRepository& years)
        : classes_(classes),
          subjects_(subjects),
          students_(students),
          teachers_(teachers),
          years_(years) {}

    // --- Classes ---
    ClassRoom create(ClassRoom input);
    ClassRoom update(long long id, ClassRoom input);
    /// Supprime une classe. Refuse si des eleves y sont encore inscrits.
    void remove(long long id);
    ClassRoom get(long long id);
    std::vector<ClassRoom> list(const ClassFilter& filter);

    // --- Matieres ---
    Subject createSubject(Subject input);
    Subject updateSubject(long long id, Subject input);
    void removeSubject(long long id);
    Subject getSubject(long long id);
    std::vector<Subject> subjectsOfClass(long long classId);
    std::vector<Subject> allSubjects();

    // --- Enseignants ---
    Teacher createTeacher(Teacher input);
    Teacher updateTeacher(long long id, Teacher input);
    void removeTeacher(long long id);
    std::vector<Teacher> allTeachers();

    // --- Annees scolaires ---
    SchoolYear createYear(SchoolYear input);
    std::vector<SchoolYear> allYears();
    SchoolYear currentYear();

private:
    void validateClass(const ClassRoom& value);
    void validateSubject(const Subject& value);
    void validateTeacher(const Teacher& value);

    IClassRepository& classes_;
    ISubjectRepository& subjects_;
    IStudentRepository& students_;
    ITeacherRepository& teachers_;
    ISchoolYearRepository& years_;
};

}  // namespace app
