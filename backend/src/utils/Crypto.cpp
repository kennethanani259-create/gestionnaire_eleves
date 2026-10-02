#include "utils/Crypto.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <random>
#include <sstream>
#include <stdexcept>

#include "core/Error.hpp"

namespace app::crypto {
namespace {

// ===========================================================================
// SHA-256 (FIPS 180-4)
// ===========================================================================

constexpr std::array<std::uint32_t, 64> kK = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2};

inline std::uint32_t rotr(std::uint32_t value, int bits) {
    return (value >> bits) | (value << (32 - bits));
}

/// Compression d'un bloc de 64 octets.
void sha256Block(const std::uint8_t* block, std::array<std::uint32_t, 8>& state) {
    std::array<std::uint32_t, 64> w{};
    for (int i = 0; i < 16; ++i) {
        w[static_cast<size_t>(i)] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                                    (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                                    (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                                    static_cast<std::uint32_t>(block[i * 4 + 3]);
    }
    for (size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];

    for (size_t i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t temp1 = h + s1 + ch + kK[i] + w[i];
        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

constexpr const char* kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr const char* kBase64UrlAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string encodeWith(const Bytes& data, const char* alphabet, bool padding) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 2 < data.size()) {
        const std::uint32_t triple =
            (static_cast<std::uint32_t>(data[i]) << 16) |
            (static_cast<std::uint32_t>(data[i + 1]) << 8) | data[i + 2];
        out.push_back(alphabet[(triple >> 18) & 0x3F]);
        out.push_back(alphabet[(triple >> 12) & 0x3F]);
        out.push_back(alphabet[(triple >> 6) & 0x3F]);
        out.push_back(alphabet[triple & 0x3F]);
        i += 3;
    }
    const size_t remaining = data.size() - i;
    if (remaining == 1) {
        const std::uint32_t triple = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(alphabet[(triple >> 18) & 0x3F]);
        out.push_back(alphabet[(triple >> 12) & 0x3F]);
        if (padding) out.append("==");
    } else if (remaining == 2) {
        const std::uint32_t triple = (static_cast<std::uint32_t>(data[i]) << 16) |
                                     (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out.push_back(alphabet[(triple >> 18) & 0x3F]);
        out.push_back(alphabet[(triple >> 12) & 0x3F]);
        out.push_back(alphabet[(triple >> 6) & 0x3F]);
        if (padding) out.push_back('=');
    }
    return out;
}

Bytes decodeWith(const std::string& data, const char* alphabet) {
    std::array<int, 256> lookup{};
    lookup.fill(-1);
    for (int i = 0; i < 64; ++i) {
        lookup[static_cast<unsigned char>(alphabet[i])] = i;
    }

    Bytes out;
    out.reserve(data.size() * 3 / 4 + 3);
    std::uint32_t buffer = 0;
    int bits = 0;
    for (char c : data) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        const int value = lookup[static_cast<unsigned char>(c)];
        if (value < 0) throw ValidationError("Chaine base64 invalide");
        buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

}  // namespace

// ===========================================================================
// API publique
// ===========================================================================

Bytes sha256(const Bytes& data) {
    std::array<std::uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    const std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8;
    size_t offset = 0;
    while (offset + 64 <= data.size()) {
        sha256Block(data.data() + offset, state);
        offset += 64;
    }

    // Bourrage final : 0x80, des zeros, puis la longueur en bits (big endian).
    Bytes tail(data.begin() + static_cast<long>(offset), data.end());
    tail.push_back(0x80);
    while (tail.size() % 64 != 56) tail.push_back(0x00);
    for (int i = 7; i >= 0; --i) {
        tail.push_back(static_cast<std::uint8_t>((bitLength >> (i * 8)) & 0xFF));
    }
    for (size_t i = 0; i < tail.size(); i += 64) sha256Block(tail.data() + i, state);

    Bytes digest(32);
    for (size_t i = 0; i < 8; ++i) {
        digest[i * 4] = static_cast<std::uint8_t>((state[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<std::uint8_t>((state[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<std::uint8_t>((state[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(state[i] & 0xFF);
    }
    return digest;
}

Bytes sha256(const std::string& data) {
    return sha256(Bytes(data.begin(), data.end()));
}

std::string sha256Hex(const std::string& data) { return toHex(sha256(data)); }

Bytes hmacSha256(const Bytes& key, const Bytes& message) {
    constexpr size_t kBlockSize = 64;
    Bytes normalizedKey = key.size() > kBlockSize ? sha256(key) : key;
    normalizedKey.resize(kBlockSize, 0x00);

    Bytes innerPad(kBlockSize);
    Bytes outerPad(kBlockSize);
    for (size_t i = 0; i < kBlockSize; ++i) {
        innerPad[i] = static_cast<std::uint8_t>(normalizedKey[i] ^ 0x36);
        outerPad[i] = static_cast<std::uint8_t>(normalizedKey[i] ^ 0x5c);
    }

    Bytes inner = innerPad;
    inner.insert(inner.end(), message.begin(), message.end());
    const Bytes innerHash = sha256(inner);

    Bytes outer = outerPad;
    outer.insert(outer.end(), innerHash.begin(), innerHash.end());
    return sha256(outer);
}

Bytes hmacSha256(const std::string& key, const std::string& message) {
    return hmacSha256(Bytes(key.begin(), key.end()), Bytes(message.begin(), message.end()));
}

Bytes pbkdf2Sha256(const std::string& password, const Bytes& salt, int iterations,
                   size_t keyLength) {
    if (iterations <= 0) throw ValidationError("Le nombre d'iterations doit etre positif");

    const Bytes key(password.begin(), password.end());
    Bytes output;
    output.reserve(keyLength);

    std::uint32_t blockIndex = 1;
    while (output.size() < keyLength) {
        Bytes input = salt;
        input.push_back(static_cast<std::uint8_t>((blockIndex >> 24) & 0xFF));
        input.push_back(static_cast<std::uint8_t>((blockIndex >> 16) & 0xFF));
        input.push_back(static_cast<std::uint8_t>((blockIndex >> 8) & 0xFF));
        input.push_back(static_cast<std::uint8_t>(blockIndex & 0xFF));

        Bytes u = hmacSha256(key, input);
        Bytes block = u;
        for (int iteration = 1; iteration < iterations; ++iteration) {
            u = hmacSha256(key, u);
            for (size_t i = 0; i < block.size(); ++i) block[i] ^= u[i];
        }
        output.insert(output.end(), block.begin(), block.end());
        ++blockIndex;
    }
    output.resize(keyLength);
    return output;
}

std::string toHex(const Bytes& data) {
    static const char* kDigits = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (std::uint8_t byte : data) {
        out.push_back(kDigits[byte >> 4]);
        out.push_back(kDigits[byte & 0x0F]);
    }
    return out;
}

std::string base64Encode(const Bytes& data) { return encodeWith(data, kBase64Alphabet, true); }
Bytes base64Decode(const std::string& data) { return decodeWith(data, kBase64Alphabet); }
std::string base64UrlEncode(const Bytes& data) {
    return encodeWith(data, kBase64UrlAlphabet, false);
}
Bytes base64UrlDecode(const std::string& data) { return decodeWith(data, kBase64UrlAlphabet); }

Bytes randomBytes(size_t count) {
    static thread_local std::random_device device;
    std::uniform_int_distribution<int> distribution(0, 255);
    Bytes out(count);
    for (size_t i = 0; i < count; ++i) {
        out[i] = static_cast<std::uint8_t>(distribution(device));
    }
    return out;
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char difference = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        difference |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return difference == 0;
}

bool constantTimeEquals(const Bytes& a, const Bytes& b) {
    if (a.size() != b.size()) return false;
    unsigned char difference = 0;
    for (size_t i = 0; i < a.size(); ++i) difference |= a[i] ^ b[i];
    return difference == 0;
}

std::string hashPassword(const std::string& password, int iterations) {
    const Bytes salt = randomBytes(16);
    const Bytes derived = pbkdf2Sha256(password, salt, iterations, 32);
    std::ostringstream out;
    out << "pbkdf2_sha256$" << iterations << '$' << base64Encode(salt) << '$'
        << base64Encode(derived);
    return out.str();
}

bool verifyPassword(const std::string& password, const std::string& encoded) {
    // Format attendu : pbkdf2_sha256$<iterations>$<sel>$<hash>
    std::vector<std::string> parts;
    std::istringstream stream(encoded);
    std::string part;
    while (std::getline(stream, part, '$')) parts.push_back(part);
    if (parts.size() != 4 || parts[0] != "pbkdf2_sha256") return false;

    int iterations = 0;
    try {
        iterations = std::stoi(parts[1]);
    } catch (const std::exception&) {
        return false;
    }
    if (iterations <= 0) return false;

    Bytes salt;
    Bytes expected;
    try {
        salt = base64Decode(parts[2]);
        expected = base64Decode(parts[3]);
    } catch (const std::exception&) {
        return false;
    }
    if (salt.empty() || expected.empty()) return false;

    const Bytes actual = pbkdf2Sha256(password, salt, iterations, expected.size());
    return constantTimeEquals(actual, expected);
}

}  // namespace app::crypto
