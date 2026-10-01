#pragma once
/// Suivi de presence : saisie unitaire, saisie groupee, statistiques.
#include <vector>

#include "repositories/AttendanceRepository.hpp"
#include "repositories/ClassRepository.hpp"
#include "repositories/StudentRepository.hpp"
#include "repositories/SubjectRepository.hpp"

namespace app {

class AttendanceService {
public:
    AttendanceService(IAttendanceRepository& attendance, IStudentRepository& students,
                      ISubjectRepository& subjects, IClassRepository& classes)
        : attendance_(attendance), students_(students), subjects_(subjects), classes_(classes) {}

    Attendance create(Attendance input);
    Attendance update(long long id, Attendance input);
    void remove(long long id);
    Attendance get(long long id);
    std::vector<Attendance> list(const AttendanceFilter& filter);

    /// Saisie groupee pour une classe (appel quotidien). Retourne le nombre de releves crees.
    int markClass(long long classId, const std::string& date,
                  const std::vector<std::pair<long long, AttendanceStatus>>& entries,
                  const std::optional<std::string>& time);

    AttendanceSummary summary(long long studentId);
    nlohmann::json classStatistics(long long classId);
    std::vector<std::pair<std::string, long long>> monthlyAbsences(
        std::optional<long long> classId);

private:
    void validate(const Attendance& value);

    IAttendanceRepository& attendance_;
    IStudentRepository& students_;
    ISubjectRepository& subjects_;
    IClassRepository& classes_;
};

}  // namespace app
