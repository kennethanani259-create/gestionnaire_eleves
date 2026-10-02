#pragma once
/// Utilitaires de dates ISO-8601 (YYYY-MM-DD) sans dependance externe.
#include <string>

namespace app::datetime {

/// Valide une date reelle : format YYYY-MM-DD + existence (annees bissextiles comprises).
bool isValidDate(const std::string& value);
/// Valide une heure HH:MM (24 h).
bool isValidTime(const std::string& value);

std::string today();            ///< date du jour, YYYY-MM-DD
std::string nowIso();           ///< horodatage YYYY-MM-DD HH:MM:SS
int currentYear();

/// Age en annees revolues a la date du jour. Retourne -1 si la date est invalide.
int ageFromBirthDate(const std::string& birthDate);
/// Comparaison lexicographique sure de deux dates ISO (a < b).
bool isBefore(const std::string& a, const std::string& b);

}  // namespace app::datetime
