#pragma once
/**
 * @file Tenant.hpp
 * @brief Portee d'etablissement de la requete courante (multi-etablissements).
 *
 * Chaque requete HTTP est traitee par un thread du pool : la portee est donc
 * stockee en thread_local et posee par le middleware d'authentification a
 * partir du compte authentifie. Les depots l'appliquent systematiquement en
 * SQL, de sorte qu'une requete ne peut pas atteindre les donnees d'un autre
 * etablissement, meme si un identifiant d'une autre ecole est devine.
 *
 * Le filtre inline la valeur entiere dans le SQL : il s'agit d'un identifiant
 * numerique issu du jeton verifie, jamais d'une saisie utilisateur, donc sans
 * risque d'injection (le type long long interdit toute autre forme).
 */
#include <optional>
#include <string>

namespace app::tenant {

/// Pose la portee pour le thread courant (nullopt = contexte systeme).
void setCurrentSchool(std::optional<long long> schoolId);
/// Portee courante, ou nullopt en contexte systeme (migrations, amorcage,
/// recherche d'un compte au moment de la connexion).
std::optional<long long> currentSchool();
/// Portee courante ; leve ForbiddenError si le compte n'est rattache a aucun
/// etablissement (compte en cours de creation d'ecole, par exemple).
long long requireCurrentSchool();

/**
 * Fragment SQL a concatener dans un WHERE pour une table portant school_id.
 * @param alias alias de la table dans la requete (ex: "s" pour students s)
 * @return " AND s.school_id = 12 " ou une chaine neutre en contexte systeme.
 */
std::string filter(const std::string& alias);

/**
 * Variante pour les tables sans colonne school_id : la portee est verifiee
 * via la table parente.
 * @param column     colonne portant la cle etrangere (ex: "g.student_id")
 * @param parentTable table parente portant school_id (ex: "students")
 */
std::string filterVia(const std::string& column, const std::string& parentTable);

/// Execute un bloc en contexte systeme (sans portee), puis restaure l'etat.
class SystemScope {
public:
    SystemScope() : previous_(currentSchool()) { setCurrentSchool(std::nullopt); }
    ~SystemScope() { setCurrentSchool(previous_); }
    SystemScope(const SystemScope&) = delete;
    SystemScope& operator=(const SystemScope&) = delete;

private:
    std::optional<long long> previous_;
};

/// Pose une portee explicite le temps d'un bloc (tests, taches d'amorcage).
class Scope {
public:
    explicit Scope(long long schoolId) : previous_(currentSchool()) {
        setCurrentSchool(schoolId);
    }
    ~Scope() { setCurrentSchool(previous_); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    std::optional<long long> previous_;
};

}  // namespace app::tenant
