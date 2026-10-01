#pragma once
/**
 * @file Logger.hpp
 * @brief Journalisation thread-safe a niveaux (DEBUG..CRITICAL).
 *
 * Format : 2026-10-01 12:00:00 | INFO     | contexte | message
 * Sortie : console (toujours) + fichier optionnel.
 */
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

namespace app {

enum class LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3, Critical = 4 };

std::string toString(LogLevel level);
LogLevel logLevelFromString(const std::string& value, LogLevel fallback = LogLevel::Info);

/// Logger global (unique point d'etat partage de l'application).
class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel level);
    LogLevel level() const;
    /// Active l'ecriture dans un fichier (cree les repertoires parents si besoin).
    void setFile(const std::string& path);

    void log(LogLevel level, const std::string& context, const std::string& message);

    void debug(const std::string& ctx, const std::string& msg) { log(LogLevel::Debug, ctx, msg); }
    void info(const std::string& ctx, const std::string& msg) { log(LogLevel::Info, ctx, msg); }
    void warning(const std::string& ctx, const std::string& msg) { log(LogLevel::Warning, ctx, msg); }
    void error(const std::string& ctx, const std::string& msg) { log(LogLevel::Error, ctx, msg); }
    void critical(const std::string& ctx, const std::string& msg) { log(LogLevel::Critical, ctx, msg); }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    mutable std::mutex mutex_;
    LogLevel level_ = LogLevel::Info;
    std::ofstream file_;
};

#define LOG_DEBUG(ctx, msg) ::app::Logger::instance().debug((ctx), (msg))
#define LOG_INFO(ctx, msg) ::app::Logger::instance().info((ctx), (msg))
#define LOG_WARN(ctx, msg) ::app::Logger::instance().warning((ctx), (msg))
#define LOG_ERROR(ctx, msg) ::app::Logger::instance().error((ctx), (msg))
#define LOG_CRITICAL(ctx, msg) ::app::Logger::instance().critical((ctx), (msg))

}  // namespace app
