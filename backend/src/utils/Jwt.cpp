#include "utils/Jwt.hpp"

#include <chrono>
#include <vector>

#include "core/Error.hpp"
#include "utils/Crypto.hpp"

namespace app::jwt {
namespace {

long long nowEpoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

crypto::Bytes toBytes(const std::string& value) {
    return crypto::Bytes(value.begin(), value.end());
}

std::string signPayload(const std::string& signingInput, const std::string& secret) {
    return crypto::base64UrlEncode(crypto::hmacSha256(secret, signingInput));
}

}  // namespace

std::string encode(const Claims& claims, const std::string& secret, int ttlMinutes) {
    const long long issuedAt = claims.issuedAt > 0 ? claims.issuedAt : nowEpoch();
    const long long expiresAt =
        claims.expiresAt > 0 ? claims.expiresAt
                             : issuedAt + static_cast<long long>(ttlMinutes) * 60;

    const nlohmann::json header{{"alg", "HS256"}, {"typ", "JWT"}};
    const nlohmann::json payload{{"sub", claims.userId},
                                 {"username", claims.username},
                                 {"role", claims.role},
                                 {"iat", issuedAt},
                                 {"exp", expiresAt}};

    const std::string signingInput = crypto::base64UrlEncode(toBytes(header.dump())) + "." +
                                     crypto::base64UrlEncode(toBytes(payload.dump()));
    return signingInput + "." + signPayload(signingInput, secret);
}

Claims decode(const std::string& token, const std::string& secret) {
    const auto firstDot = token.find('.');
    const auto secondDot = token.find('.', firstDot == std::string::npos ? 0 : firstDot + 1);
    if (firstDot == std::string::npos || secondDot == std::string::npos) {
        throw AuthError("Jeton mal forme");
    }

    const std::string signingInput = token.substr(0, secondDot);
    const std::string signature = token.substr(secondDot + 1);

    // Signature verifiee AVANT toute interpretation du contenu.
    if (!crypto::constantTimeEquals(signature, signPayload(signingInput, secret))) {
        throw AuthError("Signature du jeton invalide");
    }

    nlohmann::json header;
    nlohmann::json payload;
    try {
        const auto headerBytes = crypto::base64UrlDecode(token.substr(0, firstDot));
        const auto payloadBytes =
            crypto::base64UrlDecode(token.substr(firstDot + 1, secondDot - firstDot - 1));
        header = nlohmann::json::parse(std::string(headerBytes.begin(), headerBytes.end()));
        payload = nlohmann::json::parse(std::string(payloadBytes.begin(), payloadBytes.end()));
    } catch (const std::exception&) {
        throw AuthError("Contenu du jeton illisible");
    }

    // Refus explicite de tout algorithme autre que HS256.
    if (!header.contains("alg") || header["alg"] != "HS256") {
        throw AuthError("Algorithme de jeton non supporte");
    }

    Claims claims;
    claims.userId = payload.value("sub", 0LL);
    claims.username = payload.value("username", std::string{});
    claims.role = payload.value("role", std::string{});
    claims.issuedAt = payload.value("iat", 0LL);
    claims.expiresAt = payload.value("exp", 0LL);

    if (claims.userId <= 0) throw AuthError("Jeton sans sujet valide");
    if (claims.expiresAt <= 0 || claims.expiresAt < nowEpoch()) {
        throw AuthError("Jeton expire, veuillez vous reconnecter");
    }
    return claims;
}

}  // namespace app::jwt
