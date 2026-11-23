#pragma once
// Rng — seeded random source. std::mt19937 is fully specified by the C++
// standard, and the range mapping below is our own, so a seed produces the same
// dungeon on every compiler and platform (FR-P5).

#include <cstdint>
#include <random>

class Rng {
public:
    explicit Rng(std::uint32_t seed = 1) : engine_(seed) {}

    void reseed(std::uint32_t seed) { engine_.seed(seed); }

    std::uint32_t next() { return static_cast<std::uint32_t>(engine_()); }

    // Uniform integer in [lo, hi] (inclusive).
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        std::uint32_t span = static_cast<std::uint32_t>(hi - lo) + 1u;
        return lo + static_cast<int>(next() % span);
    }

    // Uniform real in [0, 1).
    double real() { return next() / 4294967296.0; }

    bool chance(double probability) { return real() < probability; }

private:
    std::mt19937 engine_;
};
