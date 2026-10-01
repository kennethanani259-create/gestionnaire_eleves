#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace app {

/// Annee scolaire (ex: 2025-2026).
struct SchoolYear {
    long long id = 0;
    std::string label;       ///< '2025-2026'
    std::string startDate;   ///< YYYY-MM-DD
    std::string endDate;     ///< YYYY-MM-DD
    bool isCurrent = false;
    std::string createdAt;

    nlohmann::json toJson() const;
};

}  // namespace app
