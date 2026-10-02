#pragma once
/**
 * @file Router.hpp
 * @brief Enregistrement des routes avec gestion d'erreurs et controle d'acces.
 *
 * Chaque route declare le niveau d'acces requis. Le garde effectif
 * (authentification + roles) est injecte par le serveur : les controleurs
 * n'ont aucune connaissance du mecanisme d'authentification.
 */
#include <functional>
#include <string>

#include <httplib/httplib.h>

#include "middleware/ErrorHandler.hpp"

namespace app::api {

/// Niveau d'acces exige par une route.
enum class Access {
    Public,        ///< aucune authentification (login, sante du service)
    Authenticated, ///< tout compte connecte, y compris un parent (profil, son ecole)
    Viewer,   ///< lecture de l'etablissement entier (exclut donc les parents)
    Teacher,  ///< enseignant ou administrateur (ecriture pedagogique)
    Admin     ///< administrateur uniquement
};

using Handler = middleware::Handler;
/// Garde appele avant chaque handler ; leve AuthError/ForbiddenError si refus.
using Guard = std::function<void(const httplib::Request&, Access)>;

class Router {
public:
    Router(httplib::Server& server, Guard guard) : server_(server), guard_(std::move(guard)) {}

    void get(const std::string& pattern, Access access, Handler handler);
    void post(const std::string& pattern, Access access, Handler handler);
    void put(const std::string& pattern, Access access, Handler handler);
    void del(const std::string& pattern, Access access, Handler handler);

private:
    Handler wrap(const std::string& method, const std::string& pattern, Access access,
                 Handler handler);

    httplib::Server& server_;
    Guard guard_;
};

}  // namespace app::api
