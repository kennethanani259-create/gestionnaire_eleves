#include "utils/DateTime.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace app::datetime {
namespace {

bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month) {
    static const std::array<int, 12> kDays = {31, 28, 31, 30, 31, 30,
                                              31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && isLeapYear(year)) return 29;
    return kDays[static_cast<size_t>(month - 1)];
}

std::tm localNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

bool parseDate(const std::string& value, int& year, int& month, int& day) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') return false;
    for (size_t i = 0; i < value.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (value[i] < '0' || value[i] > '9') return false;
    }
    year = std::stoi(value.substr(0, 4));
    month = std::stoi(value.substr(5, 2));
    day = std::stoi(value.substr(8, 2));
    return true;
}

}  // namespace

bool isValidDate(const std::string& value) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (!parseDate(value, year, month, day)) return false;
    if (year < 1900 || year > 2200) return false;
    if (month < 1 || month > 12) return false;
    return day >= 1 && day <= daysInMonth(year, month);
}

bool isValidTime(const std::string& value) {
    if (value.size() != 5 || value[2] != ':') return false;
    for (size_t i = 0; i < value.size(); ++i) {
        if (i == 2) continue;
        if (value[i] < '0' || value[i] > '9') return false;
    }
    const int hours = std::stoi(value.substr(0, 2));
    const int minutes = std::stoi(value.substr(3, 2));
    return hours >= 0 && hours <= 23 && minutes >= 0 && minutes <= 59;
}

std::string today() {
    const std::tm tm = localNow();
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%d");
    return os.str();
}

std::string nowIso() {
    const std::tm tm = localNow();
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return os.str();
}

int currentYear() { return localNow().tm_year + 1900; }

int ageFromBirthDate(const std::string& birthDate) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (!isValidDate(birthDate) || !parseDate(birthDate, year, month, day)) return -1;
    const std::tm tm = localNow();
    int age = (tm.tm_year + 1900) - year;
    if ((tm.tm_mon + 1) < month || ((tm.tm_mon + 1) == month && tm.tm_mday < day)) --age;
    return age;
}

bool isBefore(const std::string& a, const std::string& b) { return a < b; }

}  // namespace app::datetime
