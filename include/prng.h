#pragma once

// ============================================================
/// @file prng.h
/// @brief Gerador de Numeros Pseudoaleatorios Xoshiro128++ para identificacao.
// ============================================================

#include <Arduino.h>

class Xoshiro128PlusPlus {
private:
    uint32_t s[4];

    static inline uint32_t rotl(const uint32_t x, int k) {
        return (x << k) | (x >> (32 - k));
    }

    static inline uint64_t splitmix64(uint64_t &x) {
        x += 0x9e3779b97f4a7c15ULL;
        uint64_t z = x;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

public:
    Xoshiro128PlusPlus() {
        seed(0);
    }

    void seed(uint32_t userSeed) {
        if (userSeed == 0) {
            // Semente aleatoria via hardware TRNG da ESP32 (ruido termico/RF)
            s[0] = esp_random();
            s[1] = esp_random();
            s[2] = esp_random();
            s[3] = esp_random();
        } else {
            // Semente deterministica expandida via SplitMix64
            uint64_t sm = (uint64_t)userSeed;
            uint64_t p1 = splitmix64(sm);
            uint64_t p2 = splitmix64(sm);
            s[0] = (uint32_t)(p1 & 0xFFFFFFFF);
            s[1] = (uint32_t)(p1 >> 32);
            s[2] = (uint32_t)(p2 & 0xFFFFFFFF);
            s[3] = (uint32_t)(p2 >> 32);
        }
        // Garante que o estado nao seja completamente nulo
        if (s[0] == 0 && s[1] == 0 && s[2] == 0 && s[3] == 0) {
            s[0] = 1;
        }
    }

    inline uint32_t next() {
        const uint32_t result = rotl(s[0] + s[3], 7) + s[0];
        const uint32_t t = s[1] << 9;

        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];

        s[2] ^= t;
        s[3] = rotl(s[3], 11);

        return result;
    }

    /**
     * @brief Retorna um indice pseudoaleatorio uniforme no intervalo [0, count - 1] sem vies de modulo.
     */
    inline int nextRange(int count) {
        if (count <= 1) return 0;
        uint32_t r = next();
        return (int)(((uint64_t)r * (uint64_t)count) >> 32);
    }
};
