#pragma once
/**
 * @file Crypto.hpp
 * @brief Primitives cryptographiques : SHA-256, HMAC-SHA256, PBKDF2, base64(url).
 *
 * Implementation autonome (aucune dependance systeme type OpenSSL), conforme
 * aux specifications FIPS 180-4 (SHA-256), RFC 2104 (HMAC), RFC 8018 (PBKDF2)
 * et RFC 4648 (base64 / base64url). Les vecteurs de test officiels de ces
 * documents sont verifies dans backend/tests/test_crypto.cpp.
 *
 * Les mots de passe sont stockes au format :
 *   pbkdf2_sha256$<iterations>$<sel_base64>$<hash_base64>
 */
#include <cstdint>
#include <string>
#include <vector>

namespace app::crypto {

using Bytes = std::vector<std::uint8_t>;

/// Condensat SHA-256 (32 octets).
Bytes sha256(const Bytes& data);
Bytes sha256(const std::string& data);
std::string sha256Hex(const std::string& data);

/// HMAC-SHA256 (RFC 2104).
Bytes hmacSha256(const Bytes& key, const Bytes& message);
Bytes hmacSha256(const std::string& key, const std::string& message);

/// Derivation de cle PBKDF2-HMAC-SHA256 (RFC 8018).
Bytes pbkdf2Sha256(const std::string& password, const Bytes& salt, int iterations,
                   size_t keyLength);

std::string toHex(const Bytes& data);
std::string base64Encode(const Bytes& data);
Bytes base64Decode(const std::string& data);
/// Variante URL-safe sans remplissage, utilisee par les JWT.
std::string base64UrlEncode(const Bytes& data);
Bytes base64UrlDecode(const std::string& data);

/// Octets aleatoires issus du generateur du systeme.
Bytes randomBytes(size_t count);

/// Comparaison a temps constant : empeche les attaques temporelles.
bool constantTimeEquals(const std::string& a, const std::string& b);
bool constantTimeEquals(const Bytes& a, const Bytes& b);

// --- Mots de passe --------------------------------------------------------

/// Nombre d'iterations PBKDF2 par defaut (compromis securite / latence).
constexpr int kDefaultIterations = 120000;

/// Hache un mot de passe avec un sel aleatoire. Format: pbkdf2_sha256$iter$sel$hash.
std::string hashPassword(const std::string& password, int iterations = kDefaultIterations);
/// Verifie un mot de passe face a son empreinte stockee (temps constant).
bool verifyPassword(const std::string& password, const std::string& encoded);

}  // namespace app::crypto
