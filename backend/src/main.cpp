/**
 * @file main.cpp
 * @brief Point d'entree : configuration, base de donnees, migrations, serveur.
 *
 * Aucune logique metier ici : tout est delegue aux couches dediees.
 */
#include <csignal>
#include <cstdlib>
#include <exception>
#include <memory>

#include "api/HttpServer.hpp"
#include "core/Config.hpp"
#include "core/Error.hpp"
#include "core/Logger.hpp"
#include "database/Database.hpp"
#include "database/Migrator.hpp"

namespace {

app::api::HttpServer* g_server = nullptr;

void handleSignal(int signal) {
    LOG_INFO("main", "Signal " + std::to_string(signal) + " recu, arret en cours...");
    if (g_server != nullptr) g_server->stop();
}

}  // namespace

int main() {
    try {
        app::Config::loadDotEnv(".env");
        auto config = app::Config::fromEnvironment();
        config.validate();

        auto& logger = app::Logger::instance();
        logger.setLevel(config.logLevel);
        if (!config.logFile.empty()) logger.setFile(config.logFile);

        LOG_INFO("main", "Gestionnaire d'eleves 1.0.0");

        app::Database db(config.dbPath);
        app::Migrator migrator(db, config.migrationsDir);
        migrator.migrate();

        app::api::HttpServer server(config, db);
        g_server = &server;
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        if (!server.run()) {
            LOG_CRITICAL("main", "Le serveur n'a pas pu demarrer");
            return EXIT_FAILURE;
        }
        LOG_INFO("main", "Serveur arrete proprement");
        return EXIT_SUCCESS;
    } catch (const app::AppException& e) {
        LOG_CRITICAL("main", std::string("Erreur applicative: ") + e.what());
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        LOG_CRITICAL("main", std::string("Erreur fatale: ") + e.what());
        return EXIT_FAILURE;
    }
}
