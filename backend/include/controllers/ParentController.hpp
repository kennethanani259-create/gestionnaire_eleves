#pragma once
/**
 * @file ParentController.hpp
 * @brief Espace parent (/api/parent) et rattachement des enfants (/api/users).
 *
 * Un parent n'a acces qu'aux eleves qui lui sont explicitement rattaches par
 * l'administration. Aucune route de cet espace n'accepte un identifiant
 * d'eleve sans verifier ce lien.
 */
#include "api/Router.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AttendanceService.hpp"
#include "services/GradeService.hpp"
#include "services/StudentService.hpp"

namespace app {

class ParentController {
public:
    ParentController(StudentService& students, GradeService& grades,
                     AttendanceService& attendance, IUserRepository& users)
        : students_(students), grades_(grades), attendance_(attendance), users_(users) {}

    void registerRoutes(api::Router& router);

private:
    /// Verifie que l'eleve est bien rattache au parent connecte.
    /// Repond 404 plutot que 403 : un parent n'a pas a apprendre qu'un eleve
    /// existe ailleurs dans l'etablissement.
    void requireOwnChild(long long studentId);

    StudentService& students_;
    GradeService& grades_;
    AttendanceService& attendance_;
    IUserRepository& users_;
};

}  // namespace app
