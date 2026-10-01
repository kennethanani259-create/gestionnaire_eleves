#pragma once
/// Endpoints d'authentification (/api/auth) et de gestion des comptes (/api/users).
#include <chrono>
#include <cstddef>
#include <deque>
#include <map>
#include <mutex>
#include <string>

#include "api/Router.hpp"
#include "services/AuthService.hpp"

namespace app {

/// Limitation des demandes de compte par adresse source : une page publique qui
/// ecrit en base doit etre bornee, sinon elle se fait inonder. L'etat est porte
/// par le controleur (et non par une variable statique) afin que deux serveurs
/// instancies dans le meme processus restent independants.
class RegistrationThrottle {
public:
    explicit RegistrationThrottle(std::size_t maxPerWindow = 5,
                                  std::chrono::minutes window = std::chrono::minutes(15))
        : maxPerWindow_(maxPerWindow), window_(window) {}

    /// Leve AppException(429) si le quota de la fenetre glissante est atteint.
    void check(const std::string& client);

private:
    std::size_t maxPerWindow_;
    std::chrono::minutes window_;
    std::mutex mutex_;
    std::map<std::string, std::deque<std::chrono::steady_clock::time_point>> hits_;
};

class AuthController {
public:
    explicit AuthController(AuthService& auth) : auth_(auth) {}
    void registerRoutes(api::Router& router);

private:
    AuthService& auth_;
    RegistrationThrottle throttle_;
};

}  // namespace app
