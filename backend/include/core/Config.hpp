#pragma once
/**
 * @file Config.hpp
 * @brief Configuration de l'application, chargee depuis l'environnement
 *        (et optionnellement depuis un fichier .env).
 */
#include <string>

#include "core/Logger.hpp"

namespace app {

/// Politique d'inscription autonome (APP_SELF_REGISTRATION).
enum class SelfRegistration {
    Off,       ///< aucune inscription publique : seul un administrateur cree les comptes
    Approval,  ///< inscription publique, compte inactif jusqu'a validation par un administrateur
    Open       ///< inscription publique immediatement utilisable (role Consultation)
};

SelfRegistration selfRegistrationFromString(const std::string& value);
std::string toString(SelfRegistration mode);

struct Config {
    std::string host = "0.0.0.0";
    int port = 8080;
    std::string dbPath = "data/school.db";
    std::string migrationsDir = "backend/migrations";
    std::string frontendDir = "frontend";
    std::string uploadsDir = "data/uploads";
    std::string jwtSecret;              ///< APP_JWT_SECRET (genere aleatoirement si absent)
    int jwtTtlMinutes = 480;
    LogLevel logLevel = LogLevel::Info;
    std::string logFile;                ///< vide = console uniquement
    int threadPoolSize = 8;
    /// Par defaut : inscription ouverte, acces immediat (le matricule de
    /// l'etablissement fait foi). Passer a 'approval' pour exiger la
    /// validation d'un administrateur, ou 'off' pour fermer les inscriptions.
    SelfRegistration selfRegistration = SelfRegistration::Open;

    /// Charge la configuration depuis les variables d'environnement.
    static Config fromEnvironment();
    /// Charge un fichier .env (KEY=VALUE) dans l'environnement, sans ecraser l'existant.
    static void loadDotEnv(const std::string& path);
    /// Leve ValidationError si la configuration est incoherente.
    void validate() const;
};

}  // namespace app
