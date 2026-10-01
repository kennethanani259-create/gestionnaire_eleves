#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <nlohmann/json.hpp>
#include <sqlite3/sqlite3.h>

TEST_CASE("la chaine de compilation et SQLite sont operationnelles") {
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(":memory:", &db) == SQLITE_OK);
    CHECK(sqlite3_exec(db, "CREATE TABLE t(id INTEGER PRIMARY KEY);", nullptr,
                       nullptr, nullptr) == SQLITE_OK);
    sqlite3_close(db);

    nlohmann::json j = {{"ok", true}};
    CHECK(j["ok"].get<bool>());
}
