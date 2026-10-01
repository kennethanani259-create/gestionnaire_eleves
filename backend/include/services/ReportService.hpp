#pragma once
/**
 * @file ReportService.hpp
 * @brief Tableau de bord, statistiques, bulletins PDF, imports et exports.
 */
#include <string>
#include <vector>

#include "services/AttendanceService.hpp"
#include "services/ClassService.hpp"
#include "services/GradeService.hpp"
#include "services/StudentService.hpp"

namespace app {

/// Compte rendu d'un import (aucune ligne fautive n'interrompt le lot).
struct ImportReport {
    int inserted = 0;
    int skipped = 0;
    std::vector<nlohmann::json> errors;  ///< { line, message }
    nlohmann::json toJson() const;
};

class ReportService {
public:
    ReportService(StudentService& students, ClassService& classes, GradeService& grades,
                  AttendanceService& attendance, IStudentRepository& studentRepo,
                  IGradeRepository& gradeRepo)
        : students_(students),
          classes_(classes),
          grades_(grades),
          attendance_(attendance),
          studentRepo_(studentRepo),
          gradeRepo_(gradeRepo) {}

    /// Indicateurs du tableau de bord (globaux ou limites a une classe).
    nlohmann::json dashboard(std::optional<long long> classId);

    /// Bulletin d'un eleve au format PDF.
    std::string studentReportPdf(long long studentId, std::optional<int> term);
    /// Rapport de classe (classement + statistiques) au format PDF.
    std::string classReportPdf(long long classId, std::optional<int> term);

    /// Export des eleves au format CSV.
    std::string exportStudentsCsv(const StudentFilter& filter);
    /// Export des eleves au format JSON.
    nlohmann::json exportStudentsJson(const StudentFilter& filter);
    /// Export des notes d'une classe au format CSV.
    std::string exportGradesCsv(std::optional<long long> classId, std::optional<int> term);

    /// Import d'eleves depuis un document CSV (en-tete obligatoire).
    ImportReport importStudentsCsv(const std::string& content,
                                   std::optional<long long> defaultClassId);
    /// Import d'eleves depuis un tableau JSON.
    ImportReport importStudentsJson(const nlohmann::json& payload,
                                    std::optional<long long> defaultClassId);

private:
    StudentService& students_;
    ClassService& classes_;
    GradeService& grades_;
    AttendanceService& attendance_;
    IStudentRepository& studentRepo_;
    IGradeRepository& gradeRepo_;
};

}  // namespace app
