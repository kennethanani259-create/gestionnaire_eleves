#include "core/Tenant.hpp"

#include "core/Error.hpp"

namespace app::tenant {
namespace {

// Une portee par thread : le pool HTTP traite une requete par thread.
thread_local std::optional<long long> g_schoolId;

}  // namespace

void setCurrentSchool(std::optional<long long> schoolId) { g_schoolId = schoolId; }

std::optional<long long> currentSchool() { return g_schoolId; }

long long requireCurrentSchool() {
    if (!g_schoolId.has_value()) {
        throw ForbiddenError(
            "Votre compte n'est rattache a aucun etablissement. "
            "Rejoignez une ecole avec son matricule ou creez la votre.");
    }
    return *g_schoolId;
}

std::string filter(const std::string& alias) {
    if (!g_schoolId.has_value()) return " ";  // contexte systeme : aucune restriction
    const std::string prefix = alias.empty() ? std::string() : alias + ".";
    return " AND " + prefix + "school_id = " + std::to_string(*g_schoolId) + " ";
}

std::string filterVia(const std::string& column, const std::string& parentTable) {
    if (!g_schoolId.has_value()) return " ";
    return " AND EXISTS (SELECT 1 FROM " + parentTable + " tenant_parent WHERE tenant_parent.id = " +
           column + " AND tenant_parent.school_id = " + std::to_string(*g_schoolId) + ") ";
}

}  // namespace app::tenant
