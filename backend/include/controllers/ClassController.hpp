#pragma once
/// Endpoints REST des classes, matieres, enseignants et annees scolaires.
#include "api/Router.hpp"
#include "services/ClassService.hpp"
#include "services/GradeService.hpp"
#include "services/StudentService.hpp"

namespace app {

class ClassController {
public:
    ClassController(ClassService& classes, StudentService& students, GradeService& grades)
        : classes_(classes), students_(students), grades_(grades) {}

    void registerRoutes(api::Router& router);

private:
    ClassService& classes_;
    StudentService& students_;
    GradeService& grades_;
};

}  // namespace app
