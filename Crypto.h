#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>
#include <cstdint>

namespace Crypto {
    static const int PIN_HASH_ITERATIONS = 100000;
    static const int SALT_BYTES = 16;

    std::string sha256Hex(const std::string& data);
    std::string randomSaltHex();
    std::string hashPin(const std::string& pin, const std::string& saltHex,
                        int iterations = PIN_HASH_ITERATIONS);
}

#endif
