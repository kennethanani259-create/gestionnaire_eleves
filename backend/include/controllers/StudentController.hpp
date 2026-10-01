#pragma once
/// Endpoints REST des eleves (/api/students).
#include "api/Router.hpp"
#include "services/AttendanceService.hpp"
#include "services/GradeService.hpp"
#include "services/StudentService.hpp"

namespace app {

class StudentController {
public:
    StudentController(StudentService& students, GradeService& grades,
                      AttendanceService& attendance)
        : students_(students), grades_(grades), attendance_(attendance) {}

    void registerRoutes(api::Router& router);

    /// Construit une entite Student a partir d'un corps JSON (expose pour l'import CSV/JSON).
    static Student fromJson(const nlohmann::json& body);
    /// Construit un filtre de recherche a partir des parametres d'URL.
    static StudentFilter filterFromQuery(const httplib::Request& req);

private:
    StudentService& students_;
    GradeService& grades_;
    AttendanceService& attendance_;
};

}  // namespace app
