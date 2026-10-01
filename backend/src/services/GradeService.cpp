#include "services/GradeService.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

#include "core/Error.hpp"
#include "core/Logger.hpp"
#include "utils/DateTime.hpp"
#include "utils/Validator.hpp"

namespace app {
namespace {

/// Arrondi a deux decimales pour l'affichage et les comparaisons de rang.
double round2(double value) { return std::round(value * 100.0) / 100.0; }

}  // namespace

std::string GradeService::appreciationFor(double average20) {
    if (average20 >= 16.0) return "Excellent";
    if (average20 >= 14.0) return "Tres bien";
    if (average20 >= 12.0) return "Bien";
    if (average20 >= 10.0) return "Assez bien";
    if (average20 >= 8.0) return "Insuffisant";
    return "Tres insuffisant";
}

nlohmann::json SubjectResult::toJson() const {
    return {{"subject_id", subjectId},
            {"subject_name", subjectName},
            {"subject_code", subjectCode},
            {"coefficient", coefficient},
            {"grade_count", gradeCount},
            {"average", average},
            {"best", best},
            {"worst", worst},
            {"appreciation", appreciation}};
}

nlohmann::json StudentResult::toJson() const {
    nlohmann::json subjectsJson = nlohmann::json::array();
    for (const auto& subject : subjects) subjectsJson.push_back(subject.toJson());

    nlohmann::json json{
        {"student", student.toJson()},
        {"subjects", subjectsJson},
        {"general_average", generalAverage},
        {"total_coefficients", totalCoefficients},
        {"grade_count", gradeCount},
        {"rank", rank},
        {"class_size", classSize},
        {"appreciation", appreciation},
        {"term", term.has_value() ? nlohmann::json(*term) : nlohmann::json(nullptr)},
        {"attendance",
         {{"total", attendance.total},
          {"present", attendance.present},
          {"absent", attendance.absent},
          {"excused", attendance.excused},
          {"late", attendance.late},
          {"attendance_rate", attendance.attendanceRate}}}};
    json["best_score"] = bestScore.has_value() ? nlohmann::json(*bestScore) : nlohmann::json(nullptr);
    json["worst_score"] =
        worstScore.has_value() ? nlohmann::json(*worstScore) : nlohmann::json(nullptr);
    return json;
}

nlohmann::json RankingEntry::toJson() const {
    return {{"student_id", studentId},
            {"matricule", matricule},
            {"full_name", fullName},
            {"average", average},
            {"grade_count", gradeCount},
            {"rank", rank}};
}

void GradeService::validate(const Grade& grade) {
    Validator validator;
    validator.date("eval_date", grade.evalDate)
        .intRange("term", grade.term, 1, 3)
        .positive("max_score", grade.maxScore);

    if (grade.score < 0) validator.add("score", "La note ne peut pas etre negative");
    if (grade.maxScore > 0 && grade.score > grade.maxScore) {
        std::ostringstream message;
        message << "La note ne peut pas depasser le bareme (" << std::noshowpoint
                << grade.maxScore << ")";
        validator.add("score", message.str());
    }
    if (grade.maxScore > 1000) validator.add("max_score", "Bareme irrealiste");
    if (datetime::isValidDate(grade.evalDate) && grade.evalDate > datetime::today()) {
        validator.add("eval_date", "La date d'evaluation ne peut pas etre dans le futur");
    }

    auto student = students_.findById(grade.studentId);
    if (!student.has_value()) {
        validator.add("student_id", "L'eleve indique n'existe pas");
    }
    auto subject = subjects_.findById(grade.subjectId);
    if (!subject.has_value()) {
        validator.add("subject_id", "La matiere indiquee n'existe pas");
    }
    // Coherence metier : la matiere doit appartenir a la classe de l'eleve.
    if (student.has_value() && subject.has_value() && student->classId.has_value() &&
        subject->classId != *student->classId) {
        validator.add("subject_id",
                      "Cette matiere n'est pas enseignee dans la classe de l'eleve");
    }
    validator.throwIfInvalid();
}

Grade GradeService::create(Grade input) {
    if (input.evalDate.empty()) input.evalDate = datetime::today();
    validate(input);
    const long long id = grades_.create(input);
    LOG_INFO("grade.service", "Note enregistree: id=" + std::to_string(id) +
                                  " eleve=" + std::to_string(input.studentId));
    return get(id);
}

Grade GradeService::update(long long id, Grade input) {
    auto existing = grades_.findById(id);
    if (!existing.has_value()) throw NotFoundError("Note", id);
    input.id = id;
    if (input.evalDate.empty()) input.evalDate = existing->evalDate;
    validate(input);
    grades_.update(input);
    return get(id);
}

void GradeService::remove(long long id) {
    if (!grades_.remove(id)) throw NotFoundError("Note", id);
}

Grade GradeService::get(long long id) {
    auto grade = grades_.findById(id);
    if (!grade.has_value()) throw NotFoundError("Note", id);
    return *grade;
}

std::vector<Grade> GradeService::list(const GradeFilter& filter) { return grades_.find(filter); }

StudentResult GradeService::studentResult(long long studentId, std::optional<int> term) {
    auto student = students_.findById(studentId);
    if (!student.has_value()) throw NotFoundError("Eleve", studentId);

    StudentResult result;
    result.student = *student;
    result.term = term;
    result.attendance = attendance_.summaryForStudent(studentId);

    double weightedSum = 0.0;
    double coefficientSum = 0.0;
    for (const auto& row : grades_.subjectAverages(studentId, term)) {
        SubjectResult subject;
        subject.subjectId = row.subjectId;
        subject.subjectName = row.subjectName;
        subject.subjectCode = row.subjectCode;
        subject.coefficient = row.coefficient;
        subject.gradeCount = row.gradeCount;
        subject.average = round2(row.average20);
        subject.best = round2(row.best20);
        subject.worst = round2(row.worst20);
        subject.appreciation = appreciationFor(subject.average);

        weightedSum += row.average20 * row.coefficient;
        coefficientSum += row.coefficient;
        result.gradeCount += row.gradeCount;

        if (!result.bestScore.has_value() || subject.best > *result.bestScore) {
            result.bestScore = subject.best;
        }
        if (!result.worstScore.has_value() || subject.worst < *result.worstScore) {
            result.worstScore = subject.worst;
        }
        result.subjects.push_back(std::move(subject));
    }

    result.totalCoefficients = coefficientSum;
    result.generalAverage = coefficientSum > 0 ? round2(weightedSum / coefficientSum) : 0.0;
    result.appreciation = result.gradeCount > 0 ? appreciationFor(result.generalAverage)
                                                : "Aucune note enregistree";

    // Rang au sein de la classe.
    if (student->classId.has_value()) {
        const auto ranking = classRanking(*student->classId, term);
        result.classSize = static_cast<long long>(ranking.size());
        for (const auto& entry : ranking) {
            if (entry.studentId == studentId) {
                result.rank = entry.rank;
                break;
            }
        }
    }
    return result;
}

std::vector<RankingEntry> GradeService::classRanking(long long classId,
                                                     std::optional<int> term) {
    if (!classes_.exists(classId)) throw NotFoundError("Classe", classId);

    const auto students = students_.findByClass(classId, /*activeOnly=*/true);
    const auto rows = grades_.classSubjectAverages(classId, term);

    // Agregation ponderee par eleve.
    struct Accumulator {
        double weighted = 0.0;
        double coefficients = 0.0;
        long long gradeCount = 0;
    };
    std::map<long long, Accumulator> accumulators;
    for (const auto& row : rows) {
        auto& accumulator = accumulators[row.studentId];
        accumulator.weighted += row.average20 * row.coefficient;
        accumulator.coefficients += row.coefficient;
        accumulator.gradeCount += row.gradeCount;
    }

    std::vector<RankingEntry> ranking;
    ranking.reserve(students.size());
    for (const auto& student : students) {
        RankingEntry entry;
        entry.studentId = student.id;
        entry.matricule = student.matricule;
        entry.fullName = student.fullName();
        const auto found = accumulators.find(student.id);
        if (found != accumulators.end() && found->second.coefficients > 0) {
            entry.average = round2(found->second.weighted / found->second.coefficients);
            entry.gradeCount = found->second.gradeCount;
        }
        ranking.push_back(std::move(entry));
    }

    // Tri decroissant ; a moyenne egale, ordre alphabetique stable.
    std::sort(ranking.begin(), ranking.end(), [](const RankingEntry& a, const RankingEntry& b) {
        if (a.average != b.average) return a.average > b.average;
        return a.fullName < b.fullName;
    });

    // Rang "competition" : 1, 2, 2, 4. Les eleves sans note ne sont pas classes.
    int position = 0;
    double previousAverage = -1.0;
    int previousRank = 0;
    for (auto& entry : ranking) {
        ++position;
        if (entry.gradeCount == 0) {
            entry.rank = 0;
            continue;
        }
        if (previousRank != 0 && std::abs(entry.average - previousAverage) < 1e-9) {
            entry.rank = previousRank;
        } else {
            entry.rank = position;
            previousRank = position;
            previousAverage = entry.average;
        }
    }
    return ranking;
}

nlohmann::json GradeService::classSubjectStatistics(long long classId,
                                                    std::optional<int> term) {
    if (!classes_.exists(classId)) throw NotFoundError("Classe", classId);

    struct Accumulator {
        std::string name;
        double sum = 0.0;
        double best = 0.0;
        double worst = 20.0;
        long long students = 0;
        long long grades = 0;
    };
    std::map<long long, Accumulator> bySubject;
    for (const auto& row : grades_.classSubjectAverages(classId, term)) {
        auto& accumulator = bySubject[row.subjectId];
        accumulator.name = row.subjectName;
        accumulator.sum += row.average20;
        accumulator.best = std::max(accumulator.best, row.best20);
        accumulator.worst = std::min(accumulator.worst, row.worst20);
        accumulator.students += 1;
        accumulator.grades += row.gradeCount;
    }

    nlohmann::json out = nlohmann::json::array();
    for (const auto& [subjectId, accumulator] : bySubject) {
        out.push_back({{"subject_id", subjectId},
                       {"subject_name", accumulator.name},
                       {"average", round2(accumulator.sum /
                                          static_cast<double>(accumulator.students))},
                       {"best", round2(accumulator.best)},
                       {"worst", round2(accumulator.worst)},
                       {"student_count", accumulator.students},
                       {"grade_count", accumulator.grades}});
    }
    return out;
}

}  // namespace app
