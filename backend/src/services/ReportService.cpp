#include "services/ReportService.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>

#include "core/Error.hpp"
#include "core/Logger.hpp"
#include "utils/Csv.hpp"
#include "utils/DateTime.hpp"
#include "utils/Pdf.hpp"

namespace app {
namespace {

double round2(double value) { return std::round(value * 100.0) / 100.0; }

std::string formatNumber(double value, int decimals = 2) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(decimals) << value;
    return os.str();
}

/// Tranche de repartition des moyennes pour l'histogramme du tableau de bord.
struct Bucket {
    const char* label;
    double min;
    double max;
};

constexpr std::array<Bucket, 5> kBuckets = {{{"0-8", 0.0, 8.0},
                                             {"8-10", 8.0, 10.0},
                                             {"10-12", 10.0, 12.0},
                                             {"12-14", 12.0, 14.0},
                                             {"14-20", 14.0, 20.01}}};

std::string lower(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) out.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
    return out;
}

/// Associe les en-tetes CSV (souples) aux champs de l'entite eleve.
std::map<std::string, size_t> mapHeaders(const csv::Row& header) {
    static const std::map<std::string, std::string> kAliases = {
        {"matricule", "matricule"},      {"id", "matricule"},
        {"nom", "last_name"},            {"last_name", "last_name"},
        {"prenom", "first_name"},        {"first_name", "first_name"},
        {"date_naissance", "birth_date"},{"birth_date", "birth_date"},
        {"sexe", "gender"},              {"gender", "gender"},
        {"adresse", "address"},          {"address", "address"},
        {"telephone", "phone"},          {"phone", "phone"},
        {"email", "email"},              {"e-mail", "email"},
        {"parent", "guardian_name"},     {"guardian_name", "guardian_name"},
        {"telephone_parent", "guardian_phone"}, {"guardian_phone", "guardian_phone"},
        {"date_inscription", "enrollment_date"}, {"enrollment_date", "enrollment_date"},
        {"statut", "status"},            {"status", "status"},
        {"classe", "class_id"},          {"class_id", "class_id"}};

    std::map<std::string, size_t> columns;
    for (size_t i = 0; i < header.size(); ++i) {
        const auto found = kAliases.find(lower(header[i]));
        if (found != kAliases.end()) columns[found->second] = i;
    }
    return columns;
}

std::string cell(const csv::Row& row, const std::map<std::string, size_t>& columns,
                 const std::string& field) {
    const auto found = columns.find(field);
    if (found == columns.end() || found->second >= row.size()) return {};
    return row[found->second];
}

}  // namespace

nlohmann::json ImportReport::toJson() const {
    nlohmann::json errorsJson = nlohmann::json::array();
    for (const auto& error : errors) errorsJson.push_back(error);
    return {{"inserted", inserted},
            {"skipped", skipped},
            {"error_count", static_cast<long long>(errors.size())},
            {"errors", errorsJson}};
}

// ===========================================================================
// Tableau de bord
// ===========================================================================

nlohmann::json ReportService::dashboard(std::optional<long long> classId) {
    StudentFilter filter;
    filter.classId = classId;
    filter.limit = 0;  // pas de pagination : on agrege tout
    const auto allStudents = studentRepo_.search(filter);

    long long males = 0;
    long long females = 0;
    long long active = 0;
    for (const auto& student : allStudents) {
        if (student.gender == Gender::Male) ++males; else ++females;
        if (student.status == StudentStatus::Active) ++active;
    }

    // Moyennes par eleve, agregees a partir des classes concernees.
    std::map<long long, std::pair<double, double>> weighted;  // id -> (somme ponderee, coefs)
    std::vector<long long> classIds;
    if (classId.has_value()) {
        classIds.push_back(*classId);
    } else {
        for (const auto& classRoom : classes_.list({})) classIds.push_back(classRoom.id);
    }
    for (long long id : classIds) {
        for (const auto& row : gradeRepo_.classSubjectAverages(id, std::nullopt)) {
            auto& entry = weighted[row.studentId];
            entry.first += row.average20 * row.coefficient;
            entry.second += row.coefficient;
        }
    }

    std::vector<std::pair<long long, double>> averages;
    averages.reserve(weighted.size());
    for (const auto& [studentId, sums] : weighted) {
        if (sums.second > 0) averages.emplace_back(studentId, sums.first / sums.second);
    }

    double total = 0.0;
    long long struggling = 0;
    std::array<long long, kBuckets.size()> distribution{};
    distribution.fill(0);
    for (const auto& [studentId, average] : averages) {
        (void)studentId;
        total += average;
        if (average < 10.0) ++struggling;
        for (size_t i = 0; i < kBuckets.size(); ++i) {
            if (average >= kBuckets[i].min && average < kBuckets[i].max) {
                ++distribution[i];
                break;
            }
        }
    }
    const double classAverage =
        averages.empty() ? 0.0 : round2(total / static_cast<double>(averages.size()));

    // Meilleur eleve.
    nlohmann::json best = nullptr;
    if (!averages.empty()) {
        const auto top = std::max_element(
            averages.begin(), averages.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        if (const auto student = studentRepo_.findById(top->first)) {
            best = {{"student_id", student->id},
                    {"full_name", student->fullName()},
                    {"matricule", student->matricule},
                    {"average", round2(top->second)}};
        }
    }

    // Presences.
    long long absences = 0;
    long long lateCount = 0;
    long long excused = 0;
    long long presenceTotal = 0;
    long long presentCount = 0;
    AttendanceFilter attendanceFilter;
    attendanceFilter.classId = classId;
    for (const auto& student : allStudents) {
        const auto summary = attendance_.summary(student.id);
        absences += summary.absent;
        excused += summary.excused;
        lateCount += summary.late;
        presenceTotal += summary.total;
        presentCount += summary.present;
    }
    const double attendanceRate =
        presenceTotal > 0
            ? round2(static_cast<double>(presentCount + lateCount) * 100.0 /
                     static_cast<double>(presenceTotal))
            : 0.0;

    nlohmann::json distributionJson = nlohmann::json::array();
    for (size_t i = 0; i < kBuckets.size(); ++i) {
        distributionJson.push_back({{"range", kBuckets[i].label}, {"count", distribution[i]}});
    }

    nlohmann::json monthly = nlohmann::json::array();
    for (const auto& [month, count] : attendance_.monthlyAbsences(classId)) {
        monthly.push_back({{"month", month}, {"count", count}});
    }

    nlohmann::json subjectStats = nlohmann::json::array();
    if (classId.has_value()) {
        subjectStats = grades_.classSubjectStatistics(*classId, std::nullopt);
    } else {
        for (long long id : classIds) {
            for (const auto& item : grades_.classSubjectStatistics(id, std::nullopt)) {
                subjectStats.push_back(item);
            }
        }
    }

    return {{"student_count", static_cast<long long>(allStudents.size())},
            {"active_count", active},
            {"male_count", males},
            {"female_count", females},
            {"class_count", static_cast<long long>(classIds.size())},
            {"general_average", classAverage},
            {"graded_student_count", static_cast<long long>(averages.size())},
            {"struggling_count", struggling},
            {"best_student", best},
            {"absence_count", absences},
            {"excused_count", excused},
            {"late_count", lateCount},
            {"attendance_rate", attendanceRate},
            {"average_distribution", distributionJson},
            {"monthly_absences", monthly},
            {"subject_statistics", subjectStats}};
}

// ===========================================================================
// Bulletins et rapports PDF
// ===========================================================================

std::string ReportService::studentReportPdf(long long studentId, std::optional<int> term) {
    const auto result = grades_.studentResult(studentId, term);
    const auto& student = result.student;

    pdf::Document doc;
    doc.text("BULLETIN SCOLAIRE", 18, true);
    doc.horizontalRule(1.2);
    doc.space(6);

    doc.text("Eleve      : " + student.fullName() + "   (" + student.matricule + ")", 11, true);
    doc.text("Classe     : " + student.className.value_or("Non affecte"));
    doc.text("Ne(e) le   : " + student.birthDate + "    Sexe: " + label(student.gender));
    doc.text("Periode    : " +
             (term.has_value() ? "Trimestre " + std::to_string(*term) : std::string("Annee complete")));
    doc.text("Edite le   : " + datetime::today());
    doc.space(8);
    doc.horizontalRule();

    // En-tete du tableau des matieres.
    const double y = doc.cursorY();
    doc.textAt(50, y, "Matiere", 10, true);
    doc.textAt(240, y, "Coef.", 10, true);
    doc.textAt(300, y, "Moyenne", 10, true);
    doc.textAt(380, y, "Min", 10, true);
    doc.textAt(430, y, "Max", 10, true);
    doc.textAt(480, y, "Appreciation", 10, true);
    doc.setCursorY(y - 16);
    doc.horizontalRule(0.4);

    for (const auto& subject : result.subjects) {
        const double row = doc.cursorY();
        doc.textAt(50, row, subject.subjectName, 10);
        doc.textAt(240, row, formatNumber(subject.coefficient, 1), 10);
        doc.textAt(300, row, formatNumber(subject.average) + " / 20", 10, true);
        doc.textAt(380, row, formatNumber(subject.worst), 10);
        doc.textAt(430, row, formatNumber(subject.best), 10);
        doc.textAt(480, row, subject.appreciation, 9);
        doc.setCursorY(row - 15);
    }
    if (result.subjects.empty()) doc.text("Aucune note enregistree pour cette periode.", 10);

    doc.space(4);
    doc.horizontalRule();
    doc.text("MOYENNE GENERALE : " + formatNumber(result.generalAverage) + " / 20", 14, true);
    doc.text("Rang : " + (result.rank > 0 ? std::to_string(result.rank) + " / " +
                                                std::to_string(result.classSize)
                                          : std::string("non classe")));
    doc.text("Appreciation : " + result.appreciation);
    doc.text("Total des coefficients : " + formatNumber(result.totalCoefficients, 1));
    doc.space(8);

    doc.horizontalRule();
    doc.text("ASSIDUITE", 12, true);
    doc.text("Absences injustifiees : " + std::to_string(result.attendance.absent));
    doc.text("Absences justifiees   : " + std::to_string(result.attendance.excused));
    doc.text("Retards               : " + std::to_string(result.attendance.late));
    doc.text("Taux de presence      : " + formatNumber(result.attendance.attendanceRate, 1) + " %");
    doc.space(20);
    doc.text("Signature du responsable pedagogique : ______________________", 10);

    return doc.render();
}

std::string ReportService::classReportPdf(long long classId, std::optional<int> term) {
    const auto classRoom = classes_.get(classId);
    const auto ranking = grades_.classRanking(classId, term);
    const auto statistics = grades_.classSubjectStatistics(classId, term);

    pdf::Document doc;
    doc.text("RAPPORT DE CLASSE", 18, true);
    doc.horizontalRule(1.2);
    doc.space(6);
    doc.text("Classe        : " + classRoom.name + " (" + classRoom.level + ")", 11, true);
    doc.text("Annee         : " + classRoom.schoolYearLabel.value_or("-"));
    doc.text("Professeur    : " + classRoom.mainTeacherName.value_or("Non defini"));
    doc.text("Effectif      : " + std::to_string(classRoom.studentCount) + " eleve(s)");
    doc.text("Periode       : " +
             (term.has_value() ? "Trimestre " + std::to_string(*term) : std::string("Annee complete")));
    doc.space(8);

    doc.horizontalRule();
    doc.text("CLASSEMENT", 13, true);
    const double header = doc.cursorY();
    doc.textAt(50, header, "Rang", 10, true);
    doc.textAt(100, header, "Matricule", 10, true);
    doc.textAt(200, header, "Eleve", 10, true);
    doc.textAt(420, header, "Moyenne", 10, true);
    doc.setCursorY(header - 15);
    doc.horizontalRule(0.4);

    for (const auto& entry : ranking) {
        const double row = doc.cursorY();
        doc.textAt(50, row, entry.rank > 0 ? std::to_string(entry.rank) : "-", 10);
        doc.textAt(100, row, entry.matricule, 9);
        doc.textAt(200, row, entry.fullName, 10);
        doc.textAt(420, row,
                   entry.gradeCount > 0 ? formatNumber(entry.average) + " / 20"
                                        : std::string("aucune note"),
                   10, entry.rank == 1);
        doc.setCursorY(row - 15);
    }

    doc.space(6);
    doc.horizontalRule();
    doc.text("MOYENNES PAR MATIERE", 13, true);
    for (const auto& item : statistics) {
        doc.text(item.value("subject_name", std::string("?")) + " : " +
                     formatNumber(item.value("average", 0.0)) + " / 20   (min " +
                     formatNumber(item.value("worst", 0.0)) + ", max " +
                     formatNumber(item.value("best", 0.0)) + ")",
                 10);
    }
    if (statistics.empty()) doc.text("Aucune note enregistree.", 10);

    doc.space(10);
    doc.text("Document genere le " + datetime::nowIso(), 9);
    return doc.render();
}

// ===========================================================================
// Exports
// ===========================================================================

std::string ReportService::exportStudentsCsv(const StudentFilter& filter) {
    StudentFilter all = filter;
    all.limit = 0;
    const auto students = studentRepo_.search(all);

    std::vector<csv::Row> rows;
    rows.push_back({"matricule", "nom", "prenom", "date_naissance", "sexe", "classe", "niveau",
                    "statut", "adresse", "telephone", "email", "parent", "telephone_parent",
                    "date_inscription"});
    for (const auto& s : students) {
        rows.push_back({s.matricule, s.lastName, s.firstName, s.birthDate, toString(s.gender),
                        s.className.value_or(""), s.classLevel.value_or(""), toString(s.status),
                        s.address.value_or(""), s.phone.value_or(""), s.email.value_or(""),
                        s.guardianName.value_or(""), s.guardianPhone.value_or(""),
                        s.enrollmentDate});
    }
    return csv::write(rows);
}

nlohmann::json ReportService::exportStudentsJson(const StudentFilter& filter) {
    StudentFilter all = filter;
    all.limit = 0;
    nlohmann::json items = nlohmann::json::array();
    for (const auto& student : studentRepo_.search(all)) items.push_back(student.toJson());
    return {{"exported_at", datetime::nowIso()},
            {"count", static_cast<long long>(items.size())},
            {"items", items}};
}

std::string ReportService::exportGradesCsv(std::optional<long long> classId,
                                           std::optional<int> term) {
    GradeFilter filter;
    filter.classId = classId;
    filter.term = term;
    const auto grades = grades_.list(filter);

    std::vector<csv::Row> rows;
    rows.push_back({"eleve", "matiere", "type", "note", "bareme", "note_sur_20", "date",
                    "trimestre", "commentaire"});
    for (const auto& grade : grades) {
        rows.push_back({grade.studentName.value_or(""), grade.subjectName.value_or(""),
                        label(grade.evalType), formatNumber(grade.score),
                        formatNumber(grade.maxScore), formatNumber(grade.normalized20()),
                        grade.evalDate, std::to_string(grade.term),
                        grade.comment.value_or("")});
    }
    return csv::write(rows);
}

// ===========================================================================
// Imports
// ===========================================================================

ImportReport ReportService::importStudentsCsv(const std::string& content,
                                              std::optional<long long> defaultClassId) {
    ImportReport report;
    const auto rows = csv::parse(content);
    if (rows.empty()) throw ValidationError("Le fichier CSV est vide");

    const auto columns = mapHeaders(rows.front());
    if (columns.find("last_name") == columns.end() ||
        columns.find("first_name") == columns.end()) {
        throw ValidationError(
            "En-tete CSV invalide: les colonnes 'nom' et 'prenom' sont obligatoires");
    }

    for (size_t i = 1; i < rows.size(); ++i) {
        const auto& row = rows[i];
        const int lineNumber = static_cast<int>(i) + 1;
        try {
            Student student;
            student.matricule = cell(row, columns, "matricule");
            student.lastName = cell(row, columns, "last_name");
            student.firstName = cell(row, columns, "first_name");
            student.birthDate = cell(row, columns, "birth_date");
            const std::string gender = cell(row, columns, "gender");
            student.gender = gender.empty() ? Gender::Male : parseGender(gender);
            student.address = cell(row, columns, "address");
            student.phone = cell(row, columns, "phone");
            student.email = cell(row, columns, "email");
            student.guardianName = cell(row, columns, "guardian_name");
            student.guardianPhone = cell(row, columns, "guardian_phone");
            student.enrollmentDate = cell(row, columns, "enrollment_date");
            const std::string status = cell(row, columns, "status");
            student.status = status.empty() ? StudentStatus::Active : parseStudentStatus(status);
            student.classId = defaultClassId;

            students_.create(student);
            ++report.inserted;
        } catch (const AppException& e) {
            ++report.skipped;
            report.errors.push_back({{"line", lineNumber}, {"message", e.what()}});
        }
    }
    LOG_INFO("import", "Import CSV: " + std::to_string(report.inserted) + " insere(s), " +
                           std::to_string(report.skipped) + " ignore(s)");
    return report;
}

ImportReport ReportService::importStudentsJson(const nlohmann::json& payload,
                                               std::optional<long long> defaultClassId) {
    ImportReport report;
    const nlohmann::json& items =
        payload.is_array() ? payload
                           : (payload.contains("items") ? payload.at("items") : nlohmann::json());
    if (!items.is_array()) {
        throw ValidationError("Un tableau JSON d'eleves est attendu (ou un objet { items: [...] })");
    }

    int line = 0;
    for (const auto& item : items) {
        ++line;
        try {
            if (!item.is_object()) throw ValidationError("Chaque entree doit etre un objet JSON");
            Student student;
            student.matricule = item.value("matricule", std::string{});
            student.firstName = item.value("first_name", std::string{});
            student.lastName = item.value("last_name", std::string{});
            student.birthDate = item.value("birth_date", std::string{});
            student.gender = parseGender(item.value("gender", std::string("M")));
            student.address = item.value("address", std::string{});
            student.phone = item.value("phone", std::string{});
            student.email = item.value("email", std::string{});
            student.guardianName = item.value("guardian_name", std::string{});
            student.guardianPhone = item.value("guardian_phone", std::string{});
            student.enrollmentDate = item.value("enrollment_date", std::string{});
            student.status = parseStudentStatus(item.value("status", std::string("ACTIVE")));
            if (item.contains("class_id") && item.at("class_id").is_number()) {
                student.classId = item.at("class_id").get<long long>();
            } else {
                student.classId = defaultClassId;
            }
            students_.create(student);
            ++report.inserted;
        } catch (const AppException& e) {
            ++report.skipped;
            report.errors.push_back({{"line", line}, {"message", e.what()}});
        }
    }
    return report;
}

}  // namespace app
