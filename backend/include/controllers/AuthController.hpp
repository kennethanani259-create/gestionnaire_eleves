#pragma once
/// Endpoints d'authentification (/api/auth) et de gestion des comptes (/api/users).
#include "api/Router.hpp"
#include "services/AuthService.hpp"

namespace app {

class AuthController {
public:
    explicit AuthController(AuthService& auth) : auth_(auth) {}
    void registerRoutes(api::Router& router);

private:
    AuthService& auth_;
};

}  // namespace app
