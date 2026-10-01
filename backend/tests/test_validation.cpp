/// Tests unitaires de la validation et des utilitaires de date.
#include <doctest/doctest.h>

#include "core/Error.hpp"
#include "utils/DateTime.hpp"
#include "utils/Validator.hpp"

using namespace app;

TEST_CASE("Validation des adresses e-mail") {
    CHECK(Validator::isEmail("awa.dossou@example.org"));
    CHECK(Validator::isEmail("a+b@sub.domaine.bj"));
    CHECK_FALSE(Validator::isEmail("sans-arobase.org"));
    CHECK_FALSE(Validator::isEmail("deux@@arobases.org"));
    CHECK_FALSE(Validator::isEmail("@domaine.org"));
    CHECK_FALSE(Validator::isEmail("utilisateur@domaine"));
    CHECK_FALSE(Validator::isEmail(""));
}

TEST_CASE("Validation des numeros de telephone") {
    CHECK(Validator::isPhone("+229 01 97 00 00 00"));
    CHECK(Validator::isPhone("0197000000"));
    CHECK_FALSE(Validator::isPhone("abc"));
    CHECK_FALSE(Validator::isPhone("12"));
}

TEST_CASE("Validation des dates reelles") {
    CHECK(datetime::isValidDate("2024-02-29"));       // annee bissextile
    CHECK_FALSE(datetime::isValidDate("2023-02-29"));  // non bissextile
    CHECK_FALSE(datetime::isValidDate("2024-13-01"));
    CHECK_FALSE(datetime::isValidDate("2024-04-31"));
    CHECK_FALSE(datetime::isValidDate("01/01/2024"));
    CHECK_FALSE(datetime::isValidDate("2024-1-1"));
    CHECK(datetime::isValidTime("23:59"));
    CHECK_FALSE(datetime::isValidTime("24:00"));
    CHECK_FALSE(datetime::isValidTime("8:00"));
}

TEST_CASE("Le validateur accumule toutes les erreurs d'un formulaire") {
    Validator validator;
    validator.required("first_name", "   ")
        .email("email", "invalide")
        .date("birth_date", "32-01-2000")
        .positive("max_score", -5);

    CHECK_FALSE(validator.valid());
    CHECK(validator.errors().size() == 4);

    const auto json = validator.toJson();
    CHECK(json["fields"].contains("first_name"));
    CHECK(json["fields"].contains("email"));

    CHECK_THROWS_AS(validator.throwIfInvalid(), ValidationError);
    try {
        validator.throwIfInvalid();
    } catch (const ValidationError& e) {
        CHECK(e.httpStatus() == 400);
        CHECK(e.code() == "VALIDATION_ERROR");
        CHECK(e.details()["fields"].size() == 4);
    }
}

TEST_CASE("Le validateur accepte une saisie correcte") {
    Validator validator;
    validator.required("name", "Awa").email("email", "awa@example.org").date("d", "2012-04-18");
    CHECK(validator.valid());
    CHECK_NOTHROW(validator.throwIfInvalid());
}

TEST_CASE("Calcul de l'age a partir de la date de naissance") {
    CHECK(datetime::ageFromBirthDate("date-invalide") == -1);
    const std::string today = datetime::today();
    CHECK(datetime::ageFromBirthDate(today) == 0);
    const std::string tenYearsAgo =
        std::to_string(std::stoi(today.substr(0, 4)) - 10) + today.substr(4);
    CHECK(datetime::ageFromBirthDate(tenYearsAgo) == 10);
}
