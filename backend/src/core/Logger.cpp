#include "core/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace app {

std::string toString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
    }
    return "INFO";
}

LogLevel logLevelFromString(const std::string& value, LogLevel fallback) {
    std::string v;
    v.reserve(value.size());
    for (char c : value) v.push_back(static_cast<char>(::toupper(static_cast<unsigned char>(c))));
    if (v == "DEBUG") return LogLevel::Debug;
    if (v == "INFO") return LogLevel::Info;
    if (v == "WARNING" || v == "WARN") return LogLevel::Warning;
    if (v == "ERROR") return LogLevel::Error;
    if (v == "CRITICAL" || v == "FATAL") return LogLevel::Critical;
    return fallback;
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return level_;
}

void Logger::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code ec;
    const std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    file_.open(path, std::ios::app);
}

static std::string nowTimestamp() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto t = clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) % 1000;
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
       << std::setw(3) << ms.count();
    return os.str();
}

void Logger::log(LogLevel level, const std::string& context, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(level_)) return;

    std::ostringstream line;
    line << nowTimestamp() << " | " << std::left << std::setw(8) << toString(level)
         << " | " << std::setw(18) << context << " | " << message;
    const std::string text = line.str();

    if (static_cast<int>(level) >= static_cast<int>(LogLevel::Error)) {
        std::cerr << text << std::endl;
    } else {
        std::cout << text << std::endl;
    }
    if (file_.is_open()) {
        file_ << text << std::endl;
    }
}

}  // namespace app
