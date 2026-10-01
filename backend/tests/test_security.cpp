/// Tests de securite : primitives cryptographiques (vecteurs officiels),
/// jetons JWT, authentification et autorisation.
#include <doctest/doctest.h>

#include "core/Error.hpp"
#include "services/AuthService.hpp"
#include "utils/Crypto.hpp"
#include "utils/Jwt.hpp"
#include "TestHelpers.hpp"

using namespace app;

// ===========================================================================
// SHA-256 — vecteurs FIPS 180-4
// ===========================================================================

TEST_CASE("SHA-256 : vecteurs de test officiels") {
    CHECK(crypto::sha256Hex("") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(crypto::sha256Hex("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(crypto::sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // Message de plusieurs blocs (1 000 000 de 'a') : verifie le bourrage.
    CHECK(crypto::sha256Hex(std::string(1000000, 'a')) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("SHA-256 : le bourrage est correct aux tailles limites") {
    // 55, 56 et 64 octets encadrent les cas particuliers du bourrage.
    CHECK(crypto::sha256Hex(std::string(55, 'a')) ==
          "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    CHECK(crypto::sha256Hex(std::string(56, 'a')) ==
          "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    CHECK(crypto::sha256Hex(std::string(64, 'a')) ==
          "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
}

// ===========================================================================
// HMAC-SHA256 — vecteurs RFC 4231
// ===========================================================================

TEST_CASE("HMAC-SHA256 : vecteurs de test RFC 4231") {
    const crypto::Bytes key(20, 0x0b);
    const std::string data = "Hi There";
    CHECK(crypto::toHex(crypto::hmacSha256(key, crypto::Bytes(data.begin(), data.end()))) ==
          "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

    CHECK(crypto::toHex(crypto::hmacSha256("Jefe", "what do ya want for nothing?")) ==
          "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

    // Cle plus longue que le bloc (131 octets) : elle doit etre hachee.
    const crypto::Bytes longKey(131, 0xaa);
    const std::string message = "Test Using Larger Than Block-Size Key - Hash Key First";
    CHECK(crypto::toHex(
              crypto::hmacSha256(longKey, crypto::Bytes(message.begin(), message.end()))) ==
          "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
}

// ===========================================================================
// PBKDF2 — vecteurs RFC 7914 / draft PBKDF2-HMAC-SHA256
// ===========================================================================

TEST_CASE("PBKDF2-HMAC-SHA256 : vecteurs de test officiels") {
    const std::string salt = "salt";
    const crypto::Bytes saltBytes(salt.begin(), salt.end());

    CHECK(crypto::toHex(crypto::pbkdf2Sha256("password", saltBytes, 1, 32)) ==
          "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    CHECK(crypto::toHex(crypto::pbkdf2Sha256("password", saltBytes, 2, 32)) ==
          "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
    CHECK(crypto::toHex(crypto::pbkdf2Sha256("password", saltBytes, 4096, 32)) ==
          "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
}

// ===========================================================================
// Base64
// ===========================================================================

TEST_CASE("Base64 : encodage, decodage et variante URL") {
    auto bytes = [](const std::string& s) { return crypto::Bytes(s.begin(), s.end()); };
    CHECK(crypto::base64Encode(bytes("")) == "");
    CHECK(crypto::base64Encode(bytes("f")) == "Zg==");
    CHECK(crypto::base64Encode(bytes("fo")) == "Zm8=");
    CHECK(crypto::base64Encode(bytes("foo")) == "Zm9v");
    CHECK(crypto::base64Encode(bytes("foobar")) == "Zm9vYmFy");

    const auto decoded = crypto::base64Decode("Zm9vYmFy");
    CHECK(std::string(decoded.begin(), decoded.end()) == "foobar");

    // La variante URL n'utilise ni '+', ni '/', ni remplissage.
    const crypto::Bytes raw = {0xfb, 0xff, 0xbe};
    const std::string url = crypto::base64UrlEncode(raw);
    CHECK(url.find('+') == std::string::npos);
    CHECK(url.find('/') == std::string::npos);
    CHECK(url.find('=') == std::string::npos);
    CHECK(crypto::base64UrlDecode(url) == raw);
}

// ===========================================================================
// Mots de passe
// ===========================================================================

TEST_CASE("Les mots de passe sont haches avec sel et verifiables") {
    const std::string password = "Mot2PasseSolide!";
    const std::string hash = crypto::hashPassword(password, 1000);

    CHECK(hash.rfind("pbkdf2_sha256$1000$", 0) == 0);
    CHECK(hash.find(password) == std::string::npos);  // jamais en clair
    CHECK(crypto::verifyPassword(password, hash));
    CHECK_FALSE(crypto::verifyPassword("mauvais", hash));
    CHECK_FALSE(crypto::verifyPassword("", hash));

    // Deux hachages du meme mot de passe different (sel aleatoire).
    CHECK(hash != crypto::hashPassword(password, 1000));
}

TEST_CASE("Une empreinte corrompue ou inconnue est rejetee sans exception") {
    CHECK_FALSE(crypto::verifyPassword("x", ""));
    CHECK_FALSE(crypto::verifyPassword("x", "pas-un-hash"));
    CHECK_FALSE(crypto::verifyPassword("x", "md5$1$sel$hash"));
    CHECK_FALSE(crypto::verifyPassword("x", "pbkdf2_sha256$abc$sel$hash"));
    CHECK_FALSE(crypto::verifyPassword("x", "pbkdf2_sha256$1000$$"));
}

TEST_CASE("La comparaison a temps constant reste fonctionnellement correcte") {
    CHECK(crypto::constantTimeEquals(std::string("abc"), std::string("abc")));
    CHECK_FALSE(crypto::constantTimeEquals(std::string("abc"), std::string("abd")));
    CHECK_FALSE(crypto::constantTimeEquals(std::string("abc"), std::string("ab")));
}

// ===========================================================================
// JWT
// ===========================================================================

TEST_CASE("JWT : un jeton genere est relisible") {
    const std::string secret = "secret-de-test-tres-long-123456";
    jwt::Claims claims;
    claims.userId = 42;
    claims.username = "prof.kone";
    claims.role = "TEACHER";

    const std::string token = jwt::encode(claims, secret, 60);
    CHECK(std::count(token.begin(), token.end(), '.') == 2);

    const auto decoded = jwt::decode(token, secret);
    CHECK(decoded.userId == 42);
    CHECK(decoded.username == "prof.kone");
    CHECK(decoded.role == "TEACHER");
    CHECK(decoded.expiresAt > decoded.issuedAt);
}

TEST_CASE("JWT : les jetons falsifies sont rejetes") {
    const std::string secret = "secret-de-test-tres-long-123456";
    jwt::Claims claims;
    claims.userId = 1;
    claims.username = "admin";
    claims.role = "ADMIN";
    const std::string token = jwt::encode(claims, secret, 60);

    SUBCASE("mauvaise cle de signature") {
        CHECK_THROWS_AS(jwt::decode(token, "une-autre-cle-totalement-fausse"), AuthError);
    }
    SUBCASE("signature modifiee") {
        std::string tampered = token;
        tampered.back() = tampered.back() == 'A' ? 'B' : 'A';
        CHECK_THROWS_AS(jwt::decode(tampered, secret), AuthError);
    }
    SUBCASE("charge utile modifiee (elevation de privilege)") {
        const auto firstDot = token.find('.');
        const auto secondDot = token.find('.', firstDot + 1);
        const auto payloadBytes =
            crypto::base64UrlDecode(token.substr(firstDot + 1, secondDot - firstDot - 1));
        auto payload = nlohmann::json::parse(std::string(payloadBytes.begin(), payloadBytes.end()));
        payload["role"] = "ADMIN";
        payload["sub"] = 999;
        const std::string forgedPayload = payload.dump();
        const std::string forged =
            token.substr(0, firstDot + 1) +
            crypto::base64UrlEncode(
                crypto::Bytes(forgedPayload.begin(), forgedPayload.end())) +
            token.substr(secondDot);
        CHECK_THROWS_AS(jwt::decode(forged, secret), AuthError);
    }
    SUBCASE("jeton tronque") {
        CHECK_THROWS_AS(jwt::decode("abc.def", secret), AuthError);
        CHECK_THROWS_AS(jwt::decode("", secret), AuthError);
    }
    SUBCASE("jeton expire") {
        jwt::Claims expired;
        expired.userId = 1;
        expired.username = "admin";
        expired.role = "ADMIN";
        expired.issuedAt = 1000000;
        expired.expiresAt = 1000060;  // tres loin dans le passe
        const std::string old = jwt::encode(expired, secret, 60);
        CHECK_THROWS_AS(jwt::decode(old, secret), AuthError);
    }
    SUBCASE("algorithme 'none' refuse") {
        const nlohmann::json header{{"alg", "none"}, {"typ", "JWT"}};
        const nlohmann::json payload{{"sub", 1}, {"role", "ADMIN"}, {"exp", 99999999999LL}};
        const std::string headerDump = header.dump();
        const std::string payloadDump = payload.dump();
        const std::string input =
            crypto::base64UrlEncode(crypto::Bytes(headerDump.begin(), headerDump.end())) + "." +
            crypto::base64UrlEncode(crypto::Bytes(payloadDump.begin(), payloadDump.end()));
        // Meme correctement signe en HS256, l'en-tete 'none' doit etre refuse.
        const std::string signature =
            crypto::base64UrlEncode(crypto::hmacSha256(secret, input));
        CHECK_THROWS_AS(jwt::decode(input + "." + signature, secret), AuthError);
    }
}

// ===========================================================================
// AuthService
// ===========================================================================

namespace {

struct AuthFixture {
    std::unique_ptr<Database> db = testing::makeTestDatabase();
    UserRepository users{*db};
    AuthService auth{users, "secret-de-test-suffisamment-long", 60};

    AuthFixture() {
        auth.createUser("admin", "admin@ecole.local", "MotDePasse123", "Administrateur",
                        UserRole::Admin);
    }
};

}  // namespace

TEST_CASE("AuthService : connexion reussie et jeton exploitable") {
    AuthFixture f;
    const auto result = f.auth.login("admin", "MotDePasse123");
    CHECK_FALSE(result.token.empty());
    CHECK(result.expiresIn == 3600);
    CHECK(result.user.username == "admin");

    const auto authenticated = f.auth.authenticate(result.token);
    CHECK(authenticated.id == result.user.id);

    // La reponse JSON ne contient jamais le hash du mot de passe.
    CHECK(result.toJson().dump().find("pbkdf2") == std::string::npos);
}

TEST_CASE("AuthService : la connexion par e-mail fonctionne aussi") {
    AuthFixture f;
    CHECK_NOTHROW(f.auth.login("admin@ecole.local", "MotDePasse123"));
}

TEST_CASE("AuthService : echecs d'authentification") {
    AuthFixture f;
    SUBCASE("mauvais mot de passe") {
        CHECK_THROWS_AS(f.auth.login("admin", "mauvais"), AuthError);
    }
    SUBCASE("utilisateur inconnu") {
        CHECK_THROWS_AS(f.auth.login("inconnu", "MotDePasse123"), AuthError);
    }
    SUBCASE("le message ne revele pas si le compte existe") {
        std::string messageA;
        std::string messageB;
        try { f.auth.login("admin", "mauvais"); } catch (const AuthError& e) { messageA = e.what(); }
        try { f.auth.login("inconnu", "x"); } catch (const AuthError& e) { messageB = e.what(); }
        CHECK(messageA == messageB);
    }
    SUBCASE("champs vides") {
        CHECK_THROWS_AS(f.auth.login("", ""), ValidationError);
    }
    SUBCASE("compte desactive") {
        const auto user = f.users.findByUsername("admin").value();
        f.auth.updateUser(user.id, user.username, user.email, user.fullName, user.role, false);
        CHECK_THROWS_AS(f.auth.login("admin", "MotDePasse123"), ForbiddenError);
    }
}

TEST_CASE("AuthService : hierarchie des roles") {
    CHECK_NOTHROW(AuthService::requireRole(UserRole::Admin, UserRole::Admin));
    CHECK_NOTHROW(AuthService::requireRole(UserRole::Admin, UserRole::Teacher));
    CHECK_NOTHROW(AuthService::requireRole(UserRole::Admin, UserRole::Viewer));
    CHECK_NOTHROW(AuthService::requireRole(UserRole::Teacher, UserRole::Viewer));
    CHECK_THROWS_AS(AuthService::requireRole(UserRole::Teacher, UserRole::Admin), ForbiddenError);
    CHECK_THROWS_AS(AuthService::requireRole(UserRole::Viewer, UserRole::Teacher), ForbiddenError);
    CHECK_THROWS_AS(AuthService::requireRole(UserRole::Viewer, UserRole::Admin), ForbiddenError);
}

TEST_CASE("AuthService : creation de comptes et validation") {
    AuthFixture f;
    SUBCASE("mot de passe trop court") {
        CHECK_THROWS_AS(f.auth.createUser("prof", "p@e.org", "court", "Prof", UserRole::Teacher),
                        ValidationError);
    }
    SUBCASE("e-mail invalide") {
        CHECK_THROWS_AS(
            f.auth.createUser("prof", "pas-un-email", "MotDePasse123", "Prof", UserRole::Teacher),
            ValidationError);
    }
    SUBCASE("nom d'utilisateur deja pris") {
        CHECK_THROWS_AS(
            f.auth.createUser("admin", "autre@e.org", "MotDePasse123", "Autre", UserRole::Viewer),
            ValidationError);
    }
    SUBCASE("e-mail deja utilise") {
        CHECK_THROWS_AS(f.auth.createUser("autre", "admin@ecole.local", "MotDePasse123", "Autre",
                                          UserRole::Viewer),
                        ValidationError);
    }
    SUBCASE("creation valide") {
        const auto user = f.auth.createUser("prof.kone", "kone@ecole.local", "MotDePasse123",
                                            "Mariam Kone", UserRole::Teacher);
        CHECK(user.id > 0);
        CHECK(user.toJson()["role"] == "TEACHER");
        CHECK_NOTHROW(f.auth.login("prof.kone", "MotDePasse123"));
    }
}

TEST_CASE("AuthService : changement et reinitialisation de mot de passe") {
    AuthFixture f;
    const auto admin = f.users.findByUsername("admin").value();

    CHECK_THROWS_AS(f.auth.changePassword(admin.id, "faux", "NouveauMotDePasse1"), AuthError);
    CHECK_THROWS_AS(f.auth.changePassword(admin.id, "MotDePasse123", "court"), ValidationError);

    f.auth.changePassword(admin.id, "MotDePasse123", "NouveauMotDePasse1");
    CHECK_THROWS_AS(f.auth.login("admin", "MotDePasse123"), AuthError);
    CHECK_NOTHROW(f.auth.login("admin", "NouveauMotDePasse1"));

    f.auth.resetPassword(admin.id, "ResetParAdmin123");
    CHECK_NOTHROW(f.auth.login("admin", "ResetParAdmin123"));
}

TEST_CASE("AuthService : protections sur la suppression de comptes") {
    AuthFixture f;
    const auto admin = f.users.findByUsername("admin").value();
    const auto teacher = f.auth.createUser("prof", "prof@ecole.local", "MotDePasse123", "Prof",
                                           UserRole::Teacher);

    CHECK_THROWS_AS(f.auth.removeUser(admin.id, admin.id), ConflictError);   // son propre compte
    CHECK_THROWS_AS(f.auth.removeUser(admin.id, teacher.id), ConflictError); // dernier admin
    CHECK_NOTHROW(f.auth.removeUser(teacher.id, admin.id));
}

TEST_CASE("AuthService : le compte administrateur initial n'est cree qu'une fois") {
    auto db = testing::makeTestDatabase();
    UserRepository users(*db);
    AuthService auth(users, "secret-de-test-suffisamment-long", 60);

    const auto password = auth.ensureInitialAdmin();
    REQUIRE(password.has_value());
    CHECK(users.count() == 1);
    CHECK_NOTHROW(auth.login("admin", *password));

    CHECK_FALSE(auth.ensureInitialAdmin().has_value());  // deuxieme appel : rien
    CHECK(users.count() == 1);
}
