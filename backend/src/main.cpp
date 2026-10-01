// Composition root — sera etoffe aux etapes 5 a 10.
// A ce stade : verification que la chaine de compilation et les dependances
// (SQLite 3, nlohmann/json, cpp-httplib) sont pleinement fonctionnelles.
#include <cstdlib>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>
#include <sqlite3/sqlite3.h>

int main() {
    nlohmann::json info{
        {"application", "gestionnaire_eleves"},
        {"version", "1.0.0"},
        {"sqlite", sqlite3_libversion()},
        {"cxx_standard", __cplusplus},
    };
    std::cout << info.dump(2) << std::endl;
    return EXIT_SUCCESS;
}
