#pragma once
/**
 * @file Csv.hpp
 * @brief Lecture/ecriture CSV conforme RFC 4180 (guillemets, separateurs, sauts de ligne).
 */
#include <string>
#include <vector>

namespace app::csv {

using Row = std::vector<std::string>;

/// Analyse un document CSV complet. Detecte automatiquement ',' ou ';'.
std::vector<Row> parse(const std::string& content);
/// Echappe une valeur (guillemets doubles si necessaire).
std::string escape(const std::string& value);
/// Serialise des lignes en document CSV.
std::string write(const std::vector<Row>& rows, char delimiter = ',');

}  // namespace app::csv
