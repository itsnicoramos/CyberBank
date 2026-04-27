#include "Crypto.h"
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <vector>

namespace {

uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

void sha256Block(const uint8_t* block, uint32_t H[8]) {
    uint32_t W[64];
    for (int i = 0; i < 16; i++) {
        W[i] = (uint32_t(block[i * 4]) << 24) |
               (uint32_t(block[i * 4 + 1]) << 16) |
               (uint32_t(block[i * 4 + 2]) << 8) |
               (uint32_t(block[i * 4 + 3]));
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(W[i - 15], 7) ^ rotr(W[i - 15], 18) ^ (W[i - 15] >> 3);
        uint32_t s1 = rotr(W[i - 2], 17) ^ rotr(W[i - 2], 19) ^ (W[i - 2] >> 10);
        W[i] = W[i - 16] + s0 + W[i - 7] + s1;
    }

    uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
    uint32_t e = H[4], f = H[5], g = H[6], h = H[7];

    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + K[i] + W[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e;
        e = d + t1;
        d = c; c = b; b = a;
        a = t1 + t2;
    }
    H[0] += a; H[1] += b; H[2] += c; H[3] += d;
    H[4] += e; H[5] += f; H[6] += g; H[7] += h;
}

void sha256Raw(const uint8_t* data, size_t len, uint8_t out[32]) {
    uint32_t H[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };

    uint64_t bitLen = uint64_t(len) * 8;
    size_t i = 0;
    while (i + 64 <= len) {
        sha256Block(data + i, H);
        i += 64;
    }

    uint8_t tail[128] = {0};
    size_t rem = len - i;
    if (rem > 0) std::memcpy(tail, data + i, rem);
    tail[rem] = 0x80;
    size_t padLen = (rem < 56) ? 64 : 128;
    for (int b = 0; b < 8; b++) {
        tail[padLen - 1 - b] = uint8_t((bitLen >> (b * 8)) & 0xff);
    }
    sha256Block(tail, H);
    if (padLen == 128) sha256Block(tail + 64, H);

    for (int j = 0; j < 8; j++) {
        out[j * 4] = uint8_t((H[j] >> 24) & 0xff);
        out[j * 4 + 1] = uint8_t((H[j] >> 16) & 0xff);
        out[j * 4 + 2] = uint8_t((H[j] >> 8) & 0xff);
        out[j * 4 + 3] = uint8_t(H[j] & 0xff);
    }
}

std::string toHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; i++) oss << std::setw(2) << int(data[i]);
    return oss.str();
}

bool fromHex(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned int byte;
        std::istringstream iss(hex.substr(i, 2));
        iss >> std::hex >> byte;
        if (iss.fail()) return false;
        out.push_back(uint8_t(byte));
    }
    return true;
}

}

namespace Crypto {

std::string sha256Hex(const std::string& data) {
    uint8_t out[32];
    sha256Raw(reinterpret_cast<const uint8_t*>(data.data()), data.size(), out);
    return toHex(out, 32);
}

std::string randomSaltHex() {
    std::random_device rd;
    std::mt19937_64 gen(uint64_t(rd()) ^
                       uint64_t(std::chrono::high_resolution_clock::now()
                                .time_since_epoch().count()));
    std::uniform_int_distribution<int> dist(0, 255);
    uint8_t salt[SALT_BYTES];
    for (int i = 0; i < SALT_BYTES; i++) salt[i] = uint8_t(dist(gen));
    return toHex(salt, SALT_BYTES);
}

std::string hashPin(const std::string& pin, const std::string& saltHex, int iterations) {
    std::vector<uint8_t> salt;
    if (!fromHex(saltHex, salt)) salt.clear();

    std::vector<uint8_t> buf;
    buf.reserve(salt.size() + pin.size());
    buf.insert(buf.end(), salt.begin(), salt.end());
    buf.insert(buf.end(), pin.begin(), pin.end());

    uint8_t digest[32];
    sha256Raw(buf.empty() ? reinterpret_cast<const uint8_t*>("") : buf.data(),
              buf.size(), digest);

    uint8_t next[32 + 64];
    for (int i = 1; i < iterations; i++) {
        std::memcpy(next, digest, 32);
        if (!salt.empty()) std::memcpy(next + 32, salt.data(),
                                       salt.size() > 64 ? 64 : salt.size());
        size_t n = 32 + (salt.size() > 64 ? 64 : salt.size());
        sha256Raw(next, n, digest);
    }
    return toHex(digest, 32);
}

}
