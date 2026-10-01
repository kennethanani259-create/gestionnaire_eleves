#pragma once
#include <optional>
#include <vector>

#include "database/Database.hpp"
#include "models/Attendance.hpp"

namespace app {

/// Compteurs de presence agreges en SQL.
struct AttendanceSummary {
    long long studentId = 0;
    long long total = 0;
    long long present = 0;
    long long absent = 0;      ///< absences injustifiees
    long long excused = 0;     ///< absences justifiees
    long long late = 0;
    double attendanceRate = 0.0;  ///< (present + late) / total * 100
};

struct AttendanceFilter {
    std::optional<long long> studentId;
    std::optional<long long> classId;
    std::optional<AttendanceStatus> status;
    std::optional<std::string> from;  ///< YYYY-MM-DD inclus
    std::optional<std::string> to;    ///< YYYY-MM-DD inclus
    int limit = 0;
    int offset = 0;
};

class IAttendanceRepository {
public:
    virtual ~IAttendanceRepository() = default;
    virtual long long create(const Attendance& value) = 0;
    virtual void update(const Attendance& value) = 0;
    virtual bool remove(long long id) = 0;
    virtual std::optional<Attendance> findById(long long id) = 0;
    virtual std::vector<Attendance> find(const AttendanceFilter& filter) = 0;
    virtual AttendanceSummary summaryForStudent(long long studentId) = 0;
    virtual std::vector<AttendanceSummary> summaryForClass(long long classId) = 0;
    /// Absences (ABSENT + EXCUSED) agregees par mois 'YYYY-MM'.
    virtual std::vector<std::pair<std::string, long long>> monthlyAbsences(
        std::optional<long long> classId) = 0;
};

class AttendanceRepository : public IAttendanceRepository {
public:
    explicit AttendanceRepository(Database& db) : db_(db) {}
    long long create(const Attendance& value) override;
    void update(const Attendance& value) override;
    bool remove(long long id) override;
    std::optional<Attendance> findById(long long id) override;
    std::vector<Attendance> find(const AttendanceFilter& filter) override;
    AttendanceSummary summaryForStudent(long long studentId) override;
    std::vector<AttendanceSummary> summaryForClass(long long classId) override;
    std::vector<std::pair<std::string, long long>> monthlyAbsences(
        std::optional<long long> classId) override;

private:
    Database& db_;
};

}  // namespace app
