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
#include "services/AuthService.hpp"
#include "utils/Jwt.hpp"
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

        // Comptes de test (crees avant le serveur : ensureInitialAdmin ne
        // s'appliquera donc pas) couvrant les trois roles.
        users_ = std::make_unique<UserRepository>(*db_);
        AuthService auth(*users_, config.jwtSecret, config.jwtTtlMinutes);
        auth.createUser("admin", "admin@test.local", "MotDePasse123", "Admin", UserRole::Admin);
        auth.createUser("prof", "prof@test.local", "MotDePasse123", "Prof", UserRole::Teacher);
        auth.createUser("lecteur", "lecteur@test.local", "MotDePasse123", "Lecteur",
                        UserRole::Viewer);

        port_ = config.port;
        server_ = std::make_unique<api::HttpServer>(config, *db_);
        thread_ = std::thread([this] { server_->run(); });

        // Attend que le port soit effectivement a l'ecoute.
        httplib::Client probe("127.0.0.1", port_);
        probe.set_connection_timeout(0, 200000);
        bool ready = false;
        for (int attempt = 0; attempt < 100 && !ready; ++attempt) {
            if (auto res = probe.Get("/api/health"); res && res->status == 200) ready = true;
            else std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        REQUIRE_MESSAGE(ready, "Le serveur de test n'a pas demarre");

        adminToken_ = login("admin");
        teacherToken_ = login("prof");
        viewerToken_ = login("lecteur");
    }

    ~ApiFixture() {
        server_->stop();
        if (thread_.joinable()) thread_.join();
        server_.reset();
        users_.reset();
        db_.reset();
        std::error_code ec;
        std::filesystem::remove(dbPath_, ec);
        std::filesystem::remove(dbPath_ + "-wal", ec);
        std::filesystem::remove(dbPath_ + "-shm", ec);
    }

    /// Client authentifie en administrateur (acces complet).
    httplib::Client client() const { return clientWithToken(adminToken_); }
    httplib::Client teacherClient() const { return clientWithToken(teacherToken_); }
    httplib::Client viewerClient() const { return clientWithToken(viewerToken_); }
    /// Client sans jeton.
    httplib::Client anonymousClient() const { return clientWithToken(""); }

    httplib::Client clientWithToken(const std::string& token) const {
        httplib::Client client("127.0.0.1", port_);
        client.set_read_timeout(10, 0);
        if (!token.empty()) {
            client.set_default_headers({{"Authorization", "Bearer " + token}});
        }
        return client;
    }

    /// Recupere un jeton via l'API de connexion.
    std::string login(const std::string& username) const {
        httplib::Client client("127.0.0.1", port_);
        client.set_read_timeout(10, 0);
        auto res = client.Post("/api/auth/login",
                               R"({"username":")" + username + R"(","password":"MotDePasse123"})",
                               "application/json");
        REQUIRE(res);
        REQUIRE(res->status == 200);
        return nlohmann::json::parse(res->body)["token"].get<std::string>();
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
    std::string adminToken_;
    std::string teacherToken_;
    std::string viewerToken_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<UserRepository> users_;
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
    CHECK(res->get_header_value("Referrer-Policy") == "same-origin");
    CHECK(res->get_header_value("Content-Type").find("application/json") != std::string::npos);

    // L'encadrement est autorise volontairement (previsualisation, portail),
    // via CSP plutot que par l'ancien X-Frame-Options: SAMEORIGIN.
    CHECK(res->get_header_value("Content-Security-Policy") == "frame-ancestors *");
    CHECK(res->get_header_value("X-Frame-Options").empty());

    // Les reponses de l'API ne doivent jamais etre mises en cache.
    CHECK(res->get_header_value("Cache-Control") == "no-store");
}

TEST_CASE("API: les fichiers du frontend ne sont pas mis en cache") {
    ApiFixture api;
    auto client = api.client();
    // Une ressource statique inexistante suffit : l'en-tete est pose par le
    // post-routing handler, qui s'applique a toutes les reponses hors /api/.
    auto res = client.Get("/index.html");
    REQUIRE(res);
    CHECK(res->get_header_value("Cache-Control") == "no-cache, must-revalidate");
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

// ===========================================================================
// Authentification et autorisation via l'API
// ===========================================================================

TEST_CASE("API: connexion et profil utilisateur") {
    ApiFixture api;
    auto anonymous = api.anonymousClient();

    SUBCASE("connexion valide -> 200 + jeton") {
        auto res = anonymous.Post("/api/auth/login",
                                  R"({"username":"admin","password":"MotDePasse123"})", kJson);
        REQUIRE(res);
        CHECK(res->status == 200);
        const auto body = ApiFixture::body(res);
        CHECK_FALSE(body["token"].get<std::string>().empty());
        CHECK(body["token_type"] == "Bearer");
        CHECK(body["user"]["role"] == "ADMIN");
        // Le hash ne doit jamais transiter.
        CHECK(res->body.find("pbkdf2") == std::string::npos);
    }

    SUBCASE("mauvais mot de passe -> 401") {
        auto res = anonymous.Post("/api/auth/login",
                                  R"({"username":"admin","password":"mauvais"})", kJson);
        REQUIRE(res);
        CHECK(res->status == 401);
        CHECK(ApiFixture::body(res)["error"]["code"] == "UNAUTHORIZED");
    }

    SUBCASE("utilisateur inconnu -> 401 avec le meme message") {
        auto res = anonymous.Post("/api/auth/login",
                                  R"({"username":"fantome","password":"MotDePasse123"})", kJson);
        REQUIRE(res);
        CHECK(res->status == 401);
    }

    SUBCASE("profil de l'utilisateur connecte") {
        auto client = api.teacherClient();
        auto res = client.Get("/api/auth/me");
        REQUIRE(res);
        CHECK(res->status == 200);
        CHECK(ApiFixture::body(res)["username"] == "prof");
        CHECK(ApiFixture::body(res)["role"] == "TEACHER");
    }
}

TEST_CASE("API: les routes protegees exigent un jeton valide") {
    ApiFixture api;
    auto anonymous = api.anonymousClient();

    SUBCASE("sans jeton -> 401") {
        auto res = anonymous.Get("/api/students");
        REQUIRE(res);
        CHECK(res->status == 401);
        CHECK(ApiFixture::body(res)["error"]["code"] == "UNAUTHORIZED");
    }
    SUBCASE("jeton bidon -> 401") {
        auto client = api.clientWithToken("ceci.nest.pas.un.jeton");
        auto res = client.Get("/api/students");
        REQUIRE(res);
        CHECK(res->status == 401);
    }
    SUBCASE("jeton signe avec une autre cle -> 401") {
        jwt::Claims claims;
        claims.userId = 1;
        claims.username = "admin";
        claims.role = "ADMIN";
        auto client = api.clientWithToken(jwt::encode(claims, "cle-pirate-pirate-pirate", 60));
        auto res = client.Get("/api/students");
        REQUIRE(res);
        CHECK(res->status == 401);
    }
    SUBCASE("la sante du service reste publique") {
        auto res = anonymous.Get("/api/health");
        REQUIRE(res);
        CHECK(res->status == 200);
    }
}

TEST_CASE("API: les permissions par role sont appliquees cote serveur") {
    ApiFixture api;
    auto admin = api.client();
    auto teacher = api.teacherClient();
    auto viewer = api.viewerClient();

    admin.Post("/api/school-years", R"({"label":"2025-2026","start_date":"2025-09-01",
        "end_date":"2026-07-15","is_current":true})", kJson);
    admin.Post("/api/classes", R"({"name":"6e A","school_year_id":1})", kJson);

    const char* student = R"({"first_name":"Awa","last_name":"Dossou",
        "birth_date":"2012-04-18","gender":"F","class_id":1})";

    SUBCASE("le lecteur peut consulter") {
        auto res = viewer.Get("/api/students");
        REQUIRE(res);
        CHECK(res->status == 200);
    }
    SUBCASE("le lecteur ne peut pas creer un eleve -> 403") {
        auto res = viewer.Post("/api/students", student, kJson);
        REQUIRE(res);
        CHECK(res->status == 403);
        CHECK(ApiFixture::body(res)["error"]["code"] == "FORBIDDEN");
    }
    SUBCASE("l'enseignant peut creer un eleve et saisir des notes") {
        auto res = teacher.Post("/api/students", student, kJson);
        REQUIRE(res);
        CHECK(res->status == 201);
    }
    SUBCASE("l'enseignant ne peut pas supprimer un eleve -> 403") {
        auto created = teacher.Post("/api/students", student, kJson);
        REQUIRE(created);
        const auto id = ApiFixture::body(created)["id"].get<long long>();
        auto res = teacher.Delete(("/api/students/" + std::to_string(id)).c_str());
        REQUIRE(res);
        CHECK(res->status == 403);
    }
    SUBCASE("l'enseignant ne peut pas creer une classe -> 403") {
        auto res = teacher.Post("/api/classes", R"({"name":"5e B","school_year_id":1})", kJson);
        REQUIRE(res);
        CHECK(res->status == 403);
    }
    SUBCASE("l'enseignant ne peut pas lister les comptes -> 403") {
        auto res = teacher.Get("/api/users");
        REQUIRE(res);
        CHECK(res->status == 403);
    }
    SUBCASE("l'administrateur peut tout faire") {
        auto created = admin.Post("/api/students", student, kJson);
        REQUIRE(created);
        CHECK(created->status == 201);
        const auto id = ApiFixture::body(created)["id"].get<long long>();
        auto res = admin.Delete(("/api/students/" + std::to_string(id)).c_str());
        REQUIRE(res);
        CHECK(res->status == 204);
        CHECK(admin.Get("/api/users")->status == 200);
    }
}

TEST_CASE("API: gestion des comptes par un administrateur") {
    ApiFixture api;
    auto admin = api.client();

    auto res = admin.Post("/api/users",
                          R"({"username":"nouveau","email":"nouveau@test.local",
                              "password":"MotDePasse123","full_name":"Nouveau","role":"TEACHER"})",
                          kJson);
    REQUIRE(res);
    CHECK(res->status == 201);
    const auto id = ApiFixture::body(res)["id"].get<long long>();
    CHECK(res->body.find("password") == std::string::npos);

    SUBCASE("le nouveau compte peut se connecter") {
        auto anonymous = api.anonymousClient();
        auto login = anonymous.Post("/api/auth/login",
                                    R"({"username":"nouveau","password":"MotDePasse123"})", kJson);
        REQUIRE(login);
        CHECK(login->status == 200);
    }
    SUBCASE("mot de passe trop court -> 400") {
        auto bad = admin.Post("/api/users",
                              R"({"username":"x2","email":"x2@test.local","password":"court",
                                  "full_name":"X","role":"VIEWER"})", kJson);
        REQUIRE(bad);
        CHECK(bad->status == 400);
    }
    SUBCASE("suppression du compte") {
        auto del = admin.Delete(("/api/users/" + std::to_string(id)).c_str());
        REQUIRE(del);
        CHECK(del->status == 204);
    }
    SUBCASE("un administrateur ne peut pas supprimer son propre compte -> 409") {
        auto me = admin.Get("/api/auth/me");
        const auto myId = ApiFixture::body(me)["id"].get<long long>();
        auto del = admin.Delete(("/api/users/" + std::to_string(myId)).c_str());
        REQUIRE(del);
        CHECK(del->status == 409);
    }
}

// ===========================================================================
// Tableau de bord, rapports, imports et exports
// ===========================================================================

namespace {

/// Prepare une classe avec deux eleves notes.
void seedSchool(httplib::Client& client) {
    client.Post("/api/school-years", R"({"label":"2025-2026","start_date":"2025-09-01",
        "end_date":"2026-07-15","is_current":true})", kJson);
    client.Post("/api/classes", R"({"name":"6e A","school_year_id":1})", kJson);
    client.Post("/api/subjects",
                R"({"name":"Mathematiques","code":"MATH","coefficient":4,"class_id":1})", kJson);
    client.Post("/api/students", R"({"first_name":"Awa","last_name":"Dossou",
        "birth_date":"2012-04-18","gender":"F","class_id":1})", kJson);
    client.Post("/api/students", R"({"first_name":"Kodjo","last_name":"Houngbo",
        "birth_date":"2011-02-10","gender":"M","class_id":1})", kJson);
    client.Post("/api/grades",
                R"({"student_id":1,"subject_id":1,"score":16,"max_score":20,
                    "eval_date":"2025-10-10"})", kJson);
    client.Post("/api/grades",
                R"({"student_id":2,"subject_id":1,"score":8,"max_score":20,
                    "eval_date":"2025-10-10"})", kJson);
    client.Post("/api/attendance",
                R"({"student_id":2,"date":"2025-10-01","status":"ABSENT"})", kJson);
}

}  // namespace

TEST_CASE("API: le tableau de bord agrege les indicateurs de la classe") {
    ApiFixture api;
    auto client = api.client();
    seedSchool(client);

    auto res = client.Get("/api/dashboard");
    REQUIRE(res);
    CHECK(res->status == 200);
    const auto body = ApiFixture::body(res);

    CHECK(body["student_count"] == 2);
    CHECK(body["male_count"] == 1);
    CHECK(body["female_count"] == 1);
    CHECK(body["general_average"].get<double>() == doctest::Approx(12.0));  // (16+8)/2
    CHECK(body["struggling_count"] == 1);                                   // Kodjo < 10
    CHECK(body["best_student"]["full_name"] == "Awa Dossou");
    CHECK(body["absence_count"] == 1);
    CHECK(body["average_distribution"].is_array());
    CHECK(body["monthly_absences"][0]["month"] == "2025-10");
}

TEST_CASE("API: generation des bulletins et rapports PDF") {
    ApiFixture api;
    auto client = api.client();
    seedSchool(client);

    SUBCASE("bulletin individuel") {
        auto res = client.Get("/api/reports/students/1/pdf");
        REQUIRE(res);
        CHECK(res->status == 200);
        CHECK(res->get_header_value("Content-Type") == "application/pdf");
        CHECK(res->body.rfind("%PDF-1.4", 0) == 0);          // en-tete PDF valide
        CHECK(res->body.find("%%EOF") != std::string::npos);  // fin de fichier valide
        CHECK(res->body.size() > 500);
        CHECK(res->get_header_value("Content-Disposition").find("bulletin-1.pdf") !=
              std::string::npos);
    }
    SUBCASE("rapport de classe") {
        auto res = client.Get("/api/reports/classes/1/pdf");
        REQUIRE(res);
        CHECK(res->status == 200);
        CHECK(res->body.rfind("%PDF", 0) == 0);
    }
    SUBCASE("eleve inexistant -> 404") {
        auto res = client.Get("/api/reports/students/999/pdf");
        REQUIRE(res);
        CHECK(res->status == 404);
    }
}

TEST_CASE("API: exports CSV et JSON") {
    ApiFixture api;
    auto client = api.client();
    seedSchool(client);

    SUBCASE("export CSV des eleves") {
        auto res = client.Get("/api/export/students.csv");
        REQUIRE(res);
        CHECK(res->status == 200);
        CHECK(res->get_header_value("Content-Type").find("text/csv") != std::string::npos);
        CHECK(res->body.find("matricule,nom,prenom") != std::string::npos);
        CHECK(res->body.find("Dossou") != std::string::npos);
        CHECK(res->body.find("Houngbo") != std::string::npos);
    }
    SUBCASE("export CSV filtre") {
        auto res = client.Get("/api/export/students.csv?gender=F");
        REQUIRE(res);
        CHECK(res->body.find("Dossou") != std::string::npos);
        CHECK(res->body.find("Houngbo") == std::string::npos);
    }
    SUBCASE("export JSON des eleves") {
        auto res = client.Get("/api/export/students.json");
        REQUIRE(res);
        CHECK(res->status == 200);
        const auto body = ApiFixture::body(res);
        CHECK(body["count"] == 2);
        CHECK(body["items"].size() == 2);
    }
    SUBCASE("export CSV des notes") {
        auto res = client.Get("/api/export/grades.csv");
        REQUIRE(res);
        CHECK(res->status == 200);
        CHECK(res->body.find("eleve,matiere,type") != std::string::npos);
        CHECK(res->body.find("Mathematiques") != std::string::npos);
    }
}

TEST_CASE("API: import d'eleves depuis CSV") {
    ApiFixture api;
    auto client = api.client();
    seedSchool(client);

    SUBCASE("import valide") {
        const std::string csv =
            "nom,prenom,date_naissance,sexe,email\r\n"
            "Adjovi,Bintou,2012-05-20,F,bintou@test.org\r\n"
            "Zinsou,Moussa,2011-11-03,M,\r\n";
        auto res = client.Post("/api/import/students/csv?class_id=1", csv, "text/csv");
        REQUIRE(res);
        CHECK(res->status == 200);
        const auto body = ApiFixture::body(res);
        CHECK(body["inserted"] == 2);
        CHECK(body["skipped"] == 0);

        auto list = client.Get("/api/students");
        CHECK(ApiFixture::body(list)["total"] == 4);
    }

    SUBCASE("les lignes fautives sont signalees sans bloquer les autres") {
        const std::string csv =
            "nom,prenom,date_naissance,sexe\r\n"
            "Adjovi,Bintou,2012-05-20,F\r\n"
            ",SansNom,2012-05-20,F\r\n"          // nom manquant
            "Zinsou,Moussa,date-invalide,M\r\n"  // date invalide
            "Kponou,Sena,2010-01-15,F\r\n";
        auto res = client.Post("/api/import/students/csv?class_id=1", csv, "text/csv");
        REQUIRE(res);
        const auto body = ApiFixture::body(res);
        CHECK(body["inserted"] == 2);
        CHECK(body["skipped"] == 2);
        CHECK(body["errors"].size() == 2);
        CHECK(body["errors"][0]["line"] == 3);  // numero de ligne exact
    }

    SUBCASE("en-tete invalide -> 400") {
        auto res = client.Post("/api/import/students/csv", "colonne1,colonne2\r\na,b\r\n",
                               "text/csv");
        REQUIRE(res);
        CHECK(res->status == 400);
    }

    SUBCASE("un enseignant ne peut pas importer -> 403") {
        auto teacher = api.teacherClient();
        auto res = teacher.Post("/api/import/students/csv", "nom,prenom\r\nA,B\r\n", "text/csv");
        REQUIRE(res);
        CHECK(res->status == 403);
    }
}

TEST_CASE("API: import d'eleves depuis JSON") {
    ApiFixture api;
    auto client = api.client();
    seedSchool(client);

    auto res = client.Post("/api/import/students/json",
                           R"({"class_id":1,"items":[
                               {"first_name":"Bintou","last_name":"Adjovi",
                                "birth_date":"2012-05-20","gender":"F"},
                               {"first_name":"","last_name":"Invalide","birth_date":"2012-05-20"}
                           ]})", kJson);
    REQUIRE(res);
    CHECK(res->status == 200);
    const auto body = ApiFixture::body(res);
    CHECK(body["inserted"] == 1);
    CHECK(body["skipped"] == 1);
    CHECK(body["errors"][0]["line"] == 2);
}

TEST_CASE("API: aller-retour export puis import") {
    ApiFixture api;
    auto client = api.client();
    seedSchool(client);

    // Exporte les eleves existants...
    auto exported = client.Get("/api/export/students.csv");
    REQUIRE(exported);
    std::string csv = exported->body;

    // ... supprime les matricules pour eviter les doublons, puis reimporte.
    std::string reimported = "nom,prenom,date_naissance,sexe\r\n";
    reimported += "Dossou,Awa,2012-04-18,F\r\nHoungbo,Kodjo,2011-02-10,M\r\n";
    auto res = client.Post("/api/import/students/csv?class_id=1", reimported, "text/csv");
    REQUIRE(res);
    CHECK(ApiFixture::body(res)["inserted"] == 2);

    auto list = client.Get("/api/students");
    CHECK(ApiFixture::body(list)["total"] == 4);
}
