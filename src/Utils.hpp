#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <random>

inline int sgn(int v) { return (v > 0) - (v < 0); }

struct Timer
{
    using clock = std::chrono::steady_clock;
    clock::time_point t0;
    explicit Timer() : t0(clock::now()) {}
    int elapsed_ms() const
    {
        return (int)std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count();
    }
};

inline std::mt19937_64 &rng()
{
    static thread_local std::mt19937_64 gen{0xA57F1234C0FFEEULL};
    return gen;
}
