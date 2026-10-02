#include "core/Config.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>

#include "core/Error.hpp"

namespace app {
namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string envString(const char* key, const std::string& fallback) {
    const char* v = std::getenv(key);
    if (v == nullptr) return fallback;
    const std::string value = trim(v);
    return value.empty() ? fallback : value;
}

int envInt(const char* key, int fallback) {
    const std::string v = envString(key, "");
    if (v.empty()) return fallback;
    try {
        return std::stoi(v);
    } catch (const std::exception&) {
        return fallback;
    }
}

/// Secret aleatoire de repli (developpement uniquement) : les jetons deviennent
/// invalides a chaque redemarrage, ce qui est volontaire et journalise.
std::string randomSecret() {
    static const char* kAlphabet =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> dist(0, 61);
    std::string out;
    out.reserve(64);
    for (int i = 0; i < 64; ++i) out.push_back(kAlphabet[dist(gen)]);
    return out;
}

}  // namespace

SelfRegistration selfRegistrationFromString(const std::string& value) {
    std::string v;
    for (char c : value) v.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (v == "off" || v == "false" || v == "0" || v == "none") return SelfRegistration::Off;
    if (v == "open" || v == "immediate") return SelfRegistration::Open;
    if (v == "approval" || v == "true" || v == "1" || v == "moderated") {
        return SelfRegistration::Approval;
    }
    LOG_WARN("config", "APP_SELF_REGISTRATION inconnu (" + value +
                           ") : valeur 'approval' appliquee par defaut.");
    return SelfRegistration::Approval;
}

std::string toString(SelfRegistration mode) {
    switch (mode) {
        case SelfRegistration::Off: return "off";
        case SelfRegistration::Open: return "open";
        case SelfRegistration::Approval: break;
    }
    return "approval";
}

void Config::loadDotEnv(const std::string& path) {
    std::ifstream in(path);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(trimmed.substr(0, eq));
        std::string value = trim(trimmed.substr(eq + 1));
        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') &&
            value.back() == value.front()) {
            value = value.substr(1, value.size() - 2);
        }
        if (key.empty()) continue;
#if defined(_WIN32)
        _putenv_s(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 0);  // 0 = ne pas ecraser l'existant
#endif
    }
}

Config Config::fromEnvironment() {
    Config cfg;
    cfg.host = envString("APP_HOST", cfg.host);
    cfg.port = envInt("APP_PORT", cfg.port);
    cfg.dbPath = envString("APP_DB_PATH", cfg.dbPath);
    cfg.migrationsDir = envString("APP_MIGRATIONS_DIR", cfg.migrationsDir);
    cfg.frontendDir = envString("APP_FRONTEND_DIR", cfg.frontendDir);
    cfg.uploadsDir = envString("APP_UPLOADS_DIR", cfg.uploadsDir);
    cfg.jwtTtlMinutes = envInt("APP_JWT_TTL_MINUTES", cfg.jwtTtlMinutes);
    cfg.threadPoolSize = envInt("APP_THREAD_POOL_SIZE", cfg.threadPoolSize);
    cfg.logLevel = logLevelFromString(envString("APP_LOG_LEVEL", "INFO"));
    cfg.logFile = envString("APP_LOG_FILE", "");
    cfg.selfRegistration = selfRegistrationFromString(envString("APP_SELF_REGISTRATION", "open"));

    cfg.jwtSecret = envString("APP_JWT_SECRET", "");
    if (cfg.jwtSecret.empty()) {
        cfg.jwtSecret = randomSecret();
        LOG_WARN("config",
                 "APP_JWT_SECRET absent : un secret aleatoire est genere. "
                 "Les sessions seront invalidees au redemarrage. "
                 "Definissez APP_JWT_SECRET en production.");
    }
    return cfg;
}

void Config::validate() const {
    if (port <= 0 || port > 65535) {
        throw ValidationError("APP_PORT doit etre compris entre 1 et 65535");
    }
    if (dbPath.empty()) throw ValidationError("APP_DB_PATH ne peut pas etre vide");
    if (jwtSecret.size() < 16) {
        throw ValidationError("APP_JWT_SECRET doit contenir au moins 16 caracteres");
    }
    if (jwtTtlMinutes <= 0) throw ValidationError("APP_JWT_TTL_MINUTES doit etre positif");
    if (threadPoolSize <= 0) throw ValidationError("APP_THREAD_POOL_SIZE doit etre positif");
}

}  // namespace app
