/// Tests d'integration de l'API REST : un vrai serveur HTTP est demarre,
/// interroge par un vrai client HTTP, sur une base temporaire.
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

#include <httplib/httplib.h>

#include "api/HttpServer.hpp"
#include "TestHelpers.hpp"

using namespace app;

namespace {

/// Demarre le serveur sur un port dedie et garantit son arret.
class ApiFixture {
public:
    ApiFixture() {
        dbPath_ = (std::filesystem::temp_directory_path() /
                   ("gestionnaire_test_" + std::to_string(::getpid()) + "_" +
                    std::to_string(counter()++) + ".db"))
                      .string();

        Config config;
        config.host = "127.0.0.1";
        config.port = basePort() + static_cast<int>(counter());
        config.dbPath = dbPath_;
        config.migrationsDir = testing::migrationsDir();
        config.frontendDir = "";  // pas de frontend dans les tests
        config.jwtSecret = "secret-de-test-suffisamment-long";

        db_ = std::make_unique<Database>(dbPath_);
        Migrator(*db_, config.migrationsDir).migrate();

        port_ = config.port;
        server_ = std::make_unique<api::HttpServer>(config, *db_);
        thread_ = std::thread([this] { server_->run(); });

        // Attend que le port soit effectivement a l'ecoute.
        httplib::Client probe("127.0.0.1", port_);
        probe.set_connection_timeout(0, 200000);
        for (int attempt = 0; attempt < 100; ++attempt) {
            if (auto res = probe.Get("/api/health"); res && res->status == 200) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        FAIL("Le serveur de test n'a pas demarre");
    }

    ~ApiFixture() {
        server_->stop();
        if (thread_.joinable()) thread_.join();
        server_.reset();
        db_.reset();
        std::error_code ec;
        std::filesystem::remove(dbPath_, ec);
        std::filesystem::remove(dbPath_ + "-wal", ec);
        std::filesystem::remove(dbPath_ + "-shm", ec);
    }

    httplib::Client client() const {
        httplib::Client client("127.0.0.1", port_);
        client.set_read_timeout(5, 0);
        return client;
    }

    static nlohmann::json body(const httplib::Result& res) {
        REQUIRE(res);
        return nlohmann::json::parse(res->body, nullptr, false);
    }

private:
    static std::atomic<int>& counter() {
        static std::atomic<int> value{0};
        return value;
    }
    static int basePort() { return 18200; }

    std::string dbPath_;
    int port_ = 0;
    std::unique_ptr<Database> db_;
    std::unique_ptr<api::HttpServer> server_;
    std::thread thread_;
};

const char* kJson = "application/json";

}  // namespace

TEST_CASE("API: l'endpoint de sante repond 200") {
    ApiFixture api;
    auto client = api.client();
    auto res = client.Get("/api/health");
    REQUIRE(res);
    CHECK(res->status == 200);
    CHECK(ApiFixture::body(res)["status"] == "ok");
}

TEST_CASE("API: cycle complet de gestion d'un eleve") {
    ApiFixture api;
    auto client = api.client();

    // Annee scolaire -> 201
    auto res = client.Post("/api/school-years", R"({"label":"2025-2026",
        "start_date":"2025-09-01","end_date":"2026-07-15","is_current":true})", kJson);
    REQUIRE(res);
    CHECK(res->status == 201);

    // Classe -> 201
    res = client.Post("/api/classes", R"({"name":"6e A","school_year_id":1})", kJson);
    REQUIRE(res);
    CHECK(res->status == 201);
    CHECK(ApiFixture::body(res)["level"] == "6e");

    // Creation d'un eleve -> 201 + matricule genere
    res = client.Post("/api/students", R"({"first_name":"Awa","last_name":"Dossou",
        "birth_date":"2012-04-18","gender":"F","class_id":1})", kJson);
    REQUIRE(res);
    CHECK(res->status == 201);
    const auto created = ApiFixture::body(res);
    const long long id = created["id"].get<long long>();
    CHECK(created["matricule"].get<std::string>().rfind("STU-", 0) == 0);
    CHECK(created["class_name"] == "6e A");

    // Lecture -> 200
    res = client.Get(("/api/students/" + std::to_string(id)).c_str());
    REQUIRE(res);
    CHECK(res->status == 200);
    CHECK(ApiFixture::body(res)["full_name"] == "Awa Dossou");

    // Liste paginee -> 200
    res = client.Get("/api/students?limit=10");
    REQUIRE(res);
    CHECK(res->status == 200);
    CHECK(ApiFixture::body(res)["total"] == 1);

    // Modification -> 200
    res = client.Put(("/api/students/" + std::to_string(id)).c_str(),
                     R"({"first_name":"Awa","last_name":"Dossou-Kone",
                         "birth_date":"2012-04-18","gender":"F","class_id":1})", kJson);
    REQUIRE(res);
    CHECK(res->status == 200);
    CHECK(ApiFixture::body(res)["last_name"] == "Dossou-Kone");

    // Suppression -> 204 puis 404
    res = client.Delete(("/api/students/" + std::to_string(id)).c_str());
    REQUIRE(res);
    CHECK(res->status == 204);
    res = client.Get(("/api/students/" + std::to_string(id)).c_str());
    REQUIRE(res);
    CHECK(res->status == 404);
    CHECK(ApiFixture::body(res)["error"]["code"] == "NOT_FOUND");
}

TEST_CASE("API: les erreurs renvoient le bon code HTTP et un corps JSON exploitable") {
    ApiFixture api;
    auto client = api.client();

    SUBCASE("404 sur une ressource inexistante") {
        auto res = client.Get("/api/students/9999");
        REQUIRE(res);
        CHECK(res->status == 404);
        const auto body = ApiFixture::body(res);
        CHECK(body["error"]["code"] == "NOT_FOUND");
        // Le corps du controleur ne doit pas etre ecrase par le handler d'erreur.
        CHECK(body["error"]["message"].get<std::string>().find("Eleve") != std::string::npos);
    }

    SUBCASE("404 sur un endpoint inconnu") {
        auto res = client.Get("/api/inexistant");
        REQUIRE(res);
        CHECK(res->status == 404);
        CHECK(ApiFixture::body(res)["error"]["code"] == "NOT_FOUND");
    }

    SUBCASE("400 sur un JSON mal forme") {
        auto res = client.Post("/api/students", "{ceci n'est pas du json", kJson);
        REQUIRE(res);
        CHECK(res->status == 400);
        CHECK(ApiFixture::body(res)["error"]["code"] == "VALIDATION_ERROR");
    }

    SUBCASE("400 avec le detail de chaque champ fautif") {
        auto res = client.Post("/api/students",
                               R"({"first_name":"","last_name":"X",
                                   "birth_date":"2012-02-30","email":"invalide"})", kJson);
        REQUIRE(res);
        CHECK(res->status == 400);
        const auto fields = ApiFixture::body(res)["error"]["details"]["fields"];
        CHECK(fields.contains("first_name"));
        CHECK(fields.contains("birth_date"));
        CHECK(fields.contains("email"));
    }

    SUBCASE("400 sur un corps vide") {
        auto res = client.Post("/api/students", "", kJson);
        REQUIRE(res);
        CHECK(res->status == 400);
    }

    SUBCASE("409 sur une suppression interdite") {
        client.Post("/api/school-years", R"({"label":"2025-2026","start_date":"2025-09-01",
            "end_date":"2026-07-15","is_current":true})", kJson);
        client.Post("/api/classes", R"({"name":"6e A","school_year_id":1})", kJson);
        client.Post("/api/students", R"({"first_name":"Awa","last_name":"Dossou",
            "birth_date":"2012-04-18","gender":"F","class_id":1})", kJson);

        auto res = client.Delete("/api/classes/1");
        REQUIRE(res);
        CHECK(res->status == 409);
        CHECK(ApiFixture::body(res)["error"]["code"] == "CONFLICT");
    }
}

TEST_CASE("API: notes, moyennes et classement de bout en bout") {
    ApiFixture api;
    auto client = api.client();

    client.Post("/api/school-years", R"({"label":"2025-2026","start_date":"2025-09-01",
        "end_date":"2026-07-15","is_current":true})", kJson);
    client.Post("/api/classes", R"({"name":"6e A","school_year_id":1})", kJson);
    client.Post("/api/subjects",
                R"({"name":"Mathematiques","code":"MATH","coefficient":4,"class_id":1})", kJson);
    client.Post("/api/subjects",
                R"({"name":"Francais","code":"FR","coefficient":1,"class_id":1})", kJson);

    auto addStudent = [&](const char* first, const char* last) {
        auto res = client.Post("/api/students",
                               std::string(R"({"first_name":")") + first + R"(","last_name":")" +
                                   last + R"(","birth_date":"2012-04-18","gender":"F","class_id":1})",
                               kJson);
        REQUIRE(res);
        REQUIRE(res->status == 201);
        return ApiFixture::body(res)["id"].get<long long>();
    };

    const auto first = addStudent("Awa", "Dossou");
    const auto second = addStudent("Kodjo", "Houngbo");

    // 15 en maths (coef 4) et 10 en francais (coef 1) -> moyenne 14
    auto res = client.Post(("/api/students/" + std::to_string(first) + "/grades").c_str(),
                           R"({"subject_id":1,"score":15,"max_score":20,
                               "eval_date":"2025-10-10","eval_type":"EXAM"})", kJson);
    REQUIRE(res);
    CHECK(res->status == 201);
    client.Post(("/api/students/" + std::to_string(first) + "/grades").c_str(),
                R"({"subject_id":2,"score":10,"max_score":20,"eval_date":"2025-10-10"})", kJson);
    client.Post(("/api/students/" + std::to_string(second) + "/grades").c_str(),
                R"({"subject_id":1,"score":9,"max_score":20,"eval_date":"2025-10-10"})", kJson);

    SUBCASE("le bilan d'un eleve expose moyenne, rang et appreciation") {
        res = client.Get(("/api/students/" + std::to_string(first) + "/results").c_str());
        REQUIRE(res);
        CHECK(res->status == 200);
        const auto body = ApiFixture::body(res);
        CHECK(body["general_average"].get<double>() == doctest::Approx(14.0));
        CHECK(body["rank"] == 1);
        CHECK(body["class_size"] == 2);
        CHECK(body["appreciation"] == "Tres bien");
        CHECK(body["subjects"].size() == 2);
    }

    SUBCASE("le classement de la classe est trie et numerote") {
        res = client.Get("/api/classes/1/ranking");
        REQUIRE(res);
        CHECK(res->status == 200);
        const auto items = ApiFixture::body(res)["items"];
        REQUIRE(items.size() == 2);
        CHECK(items[0]["rank"] == 1);
        CHECK(items[0]["student_id"] == first);
        CHECK(items[1]["rank"] == 2);
    }

    SUBCASE("une note hors bareme est rejetee avec 400") {
        res = client.Post("/api/grades",
                          R"({"student_id":1,"subject_id":1,"score":25,"max_score":20,
                              "eval_date":"2025-10-10"})", kJson);
        REQUIRE(res);
        CHECK(res->status == 400);
        CHECK(ApiFixture::body(res)["error"]["details"]["fields"].contains("score"));
    }

    SUBCASE("les notes d'un eleve sont listees") {
        res = client.Get(("/api/students/" + std::to_string(first) + "/grades").c_str());
        REQUIRE(res);
        CHECK(ApiFixture::body(res)["items"].size() == 2);
    }
}

TEST_CASE("API: recherche, filtres et pagination des eleves") {
    ApiFixture api;
    auto client = api.client();

    client.Post("/api/school-years", R"({"label":"2025-2026","start_date":"2025-09-01",
        "end_date":"2026-07-15","is_current":true})", kJson);
    client.Post("/api/classes", R"({"name":"6e A","school_year_id":1})", kJson);
    client.Post("/api/students", R"({"first_name":"Awa","last_name":"Dossou",
        "birth_date":"2012-04-18","gender":"F","class_id":1})", kJson);
    client.Post("/api/students", R"({"first_name":"Kodjo","last_name":"Houngbo",
        "birth_date":"2011-02-10","gender":"M","class_id":1})", kJson);

    SUBCASE("recherche textuelle") {
        auto res = client.Get("/api/students?q=houng");
        REQUIRE(res);
        const auto body = ApiFixture::body(res);
        CHECK(body["total"] == 1);
        CHECK(body["items"][0]["full_name"] == "Kodjo Houngbo");
    }
    SUBCASE("filtre par sexe") {
        auto res = client.Get("/api/students?gender=F");
        REQUIRE(res);
        CHECK(ApiFixture::body(res)["total"] == 1);
    }
    SUBCASE("pagination") {
        auto res = client.Get("/api/students?limit=1&offset=1");
        REQUIRE(res);
        const auto body = ApiFixture::body(res);
        CHECK(body["items"].size() == 1);
        CHECK(body["total"] == 2);
    }
    SUBCASE("valeur d'enumeration inconnue -> 400") {
        auto res = client.Get("/api/students?gender=X");
        REQUIRE(res);
        CHECK(res->status == 400);
    }
    SUBCASE("parametre numerique invalide -> 400") {
        auto res = client.Get("/api/students?class_id=abc");
        REQUIRE(res);
        CHECK(res->status == 400);
    }
}

TEST_CASE("API: appel groupe des presences") {
    ApiFixture api;
    auto client = api.client();

    client.Post("/api/school-years", R"({"label":"2025-2026","start_date":"2025-09-01",
        "end_date":"2026-07-15","is_current":true})", kJson);
    client.Post("/api/classes", R"({"name":"6e A","school_year_id":1})", kJson);
    client.Post("/api/students", R"({"first_name":"Awa","last_name":"Dossou",
        "birth_date":"2012-04-18","gender":"F","class_id":1})", kJson);
    client.Post("/api/students", R"({"first_name":"Kodjo","last_name":"Houngbo",
        "birth_date":"2011-02-10","gender":"M","class_id":1})", kJson);

    auto res = client.Post("/api/attendance/bulk",
                           R"({"class_id":1,"date":"2025-10-06","time":"08:00","entries":[
                               {"student_id":1,"status":"PRESENT"},
                               {"student_id":2,"status":"LATE"}]})", kJson);
    REQUIRE(res);
    CHECK(res->status == 201);
    CHECK(ApiFixture::body(res)["created"] == 2);

    res = client.Get("/api/students/2/attendance");
    REQUIRE(res);
    const auto body = ApiFixture::body(res);
    CHECK(body["items"].size() == 1);
    CHECK(body["summary"]["late"] == 1);

    SUBCASE("entries manquant -> 400") {
        res = client.Post("/api/attendance/bulk", R"({"class_id":1,"date":"2025-10-07"})", kJson);
        REQUIRE(res);
        CHECK(res->status == 400);
    }
}

TEST_CASE("API: en-tetes de securite presents sur les reponses") {
    ApiFixture api;
    auto client = api.client();
    auto res = client.Get("/api/health");
    REQUIRE(res);
    CHECK(res->get_header_value("X-Content-Type-Options") == "nosniff");
    CHECK(res->get_header_value("X-Frame-Options") == "SAMEORIGIN");
    CHECK(res->get_header_value("Content-Type").find("application/json") != std::string::npos);
}

TEST_CASE("API: le serveur survit a une rafale de requetes invalides") {
    ApiFixture api;
    auto client = api.client();
    for (int i = 0; i < 40; ++i) {
        client.Post("/api/students", "{\"corrompu\":", kJson);
        client.Get("/api/students/abc");
        client.Delete("/api/classes/999999");
    }
    auto res = client.Get("/api/health");
    REQUIRE(res);
    CHECK(res->status == 200);  // toujours vivant
}
