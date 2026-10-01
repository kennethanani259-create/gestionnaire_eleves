#pragma once
/**
 * @file HttpServer.hpp
 * @brief Serveur HTTP : cablage des dependances, routes et fichiers statiques.
 *
 * C'est le point d'assemblage (composition root) de l'application :
 * il construit les repositories, les services et les controleurs, puis
 * enregistre les routes. main.cpp se contente de l'instancier.
 */
#include <memory>
#include <string>

#include <httplib/httplib.h>

#include "api/Router.hpp"
#include "core/Config.hpp"
#include "database/Database.hpp"

namespace app::api {

class HttpServer {
public:
    HttpServer(Config config, Database& db);
    ~HttpServer();

    /// Demarre l'ecoute (bloquant). Retourne false si le port est indisponible.
    bool run();
    /// Arrete le serveur depuis un autre thread.
    void stop();
    /// Expose le serveur sous-jacent (utilise par les tests d'integration).
    httplib::Server& server();

private:
    void registerMiddlewares();
    void registerRoutes();
    void registerStaticFiles();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace app::api
