#pragma once
/**
 * @file GradeService.hpp
 * @brief Notes, moyennes ponderees, classement et bulletins.
 *
 * Toutes les notes sont ramenees sur 20 avant agregation :
 *   note_20 = score * 20 / max_score
 *   moyenne_matiere = moyenne des note_20 de la matiere
 *   moyenne_generale = somme(moyenne_matiere * coef) / somme(coef)
 * Le classement utilise la methode "competition" : 1, 2, 2, 4.
 */
#include <optional>
#include <string>
#include <vector>

#include "repositories/AttendanceRepository.hpp"
#include "repositories/ClassRepository.hpp"
#include "repositories/GradeRepository.hpp"
#include "repositories/StudentRepository.hpp"
#include "repositories/SubjectRepository.hpp"

namespace app {

/// Resultat d'un eleve dans une matiere.
struct SubjectResult {
    long long subjectId = 0;
    std::string subjectName;
    std::string subjectCode;
    double coefficient = 1.0;
    long long gradeCount = 0;
    double average = 0.0;   ///< sur 20
    double best = 0.0;
    double worst = 0.0;
    std::string appreciation;
    nlohmann::json toJson() const;
};

/// Bilan complet d'un eleve (base du bulletin).
struct StudentResult {
    Student student;
    std::vector<SubjectResult> subjects;
    double generalAverage = 0.0;
    double totalCoefficients = 0.0;
    long long gradeCount = 0;
    int rank = 0;              ///< 0 si non classe (aucune note)
    long long classSize = 0;
    std::string appreciation;
    std::optional<double> bestScore;
    std::optional<double> worstScore;
    AttendanceSummary attendance;
    std::optional<int> term;
    nlohmann::json toJson() const;
};

/// Ligne de classement d'une classe.
struct RankingEntry {
    long long studentId = 0;
    std::string matricule;
    std::string fullName;
    double average = 0.0;
    long long gradeCount = 0;
    int rank = 0;
    nlohmann::json toJson() const;
};

class GradeService {
public:
    GradeService(IGradeRepository& grades, IStudentRepository& students,
                 ISubjectRepository& subjects, IClassRepository& classes,
                 IAttendanceRepository& attendance)
        : grades_(grades),
          students_(students),
          subjects_(subjects),
          classes_(classes),
          attendance_(attendance) {}

    Grade create(Grade input);
    Grade update(long long id, Grade input);
    void remove(long long id);
    Grade get(long long id);
    std::vector<Grade> list(const GradeFilter& filter);

    /// Bilan d'un eleve, rang inclus (calcule sur sa classe).
    StudentResult studentResult(long long studentId, std::optional<int> term);
    /// Classement complet d'une classe (eleves actifs).
    std::vector<RankingEntry> classRanking(long long classId, std::optional<int> term);
    /// Moyenne de la classe par matiere.
    nlohmann::json classSubjectStatistics(long long classId, std::optional<int> term);

    /// Appreciation litterale associee a une moyenne sur 20.
    static std::string appreciationFor(double average20);

private:
    void validate(const Grade& grade);

    IGradeRepository& grades_;
    IStudentRepository& students_;
    ISubjectRepository& subjects_;
    IClassRepository& classes_;
    IAttendanceRepository& attendance_;
};

}  // namespace app
