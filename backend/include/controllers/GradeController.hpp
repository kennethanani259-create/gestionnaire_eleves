#pragma once
/// Endpoints REST des notes (/api/grades) et des presences (/api/attendance).
#include "api/Router.hpp"
#include "services/AttendanceService.hpp"
#include "services/GradeService.hpp"

namespace app {

class GradeController {
public:
    GradeController(GradeService& grades, AttendanceService& attendance)
        : grades_(grades), attendance_(attendance) {}

    void registerRoutes(api::Router& router);

    static Grade gradeFromJson(const nlohmann::json& body);
    static Attendance attendanceFromJson(const nlohmann::json& body);

private:
    GradeService& grades_;
    AttendanceService& attendance_;
};

}  // namespace app
