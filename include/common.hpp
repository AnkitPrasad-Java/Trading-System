#pragma once

#include <cstdint>
#include <x86intrin.h>
#include <atomic>

#define LIKELY(x)       __builtin_expect(!!(x), 1)
#define UNLIKELY(x)     __builtin_expect(!!(x), 0)

namespace hft {

constexpr uint32_t MAX_SYMBOLS = 8;
constexpr size_t CACHE_LINE_SIZE = 64;
constexpr int32_t PRICE_SCALE = 10000;

inline uint64_t rdtscp() {
    unsigned int aux;
    return __rdtscp(&aux);
}

inline uint64_t calibrate_rdtscp() {
    uint64_t start = rdtscp();
    uint64_t end = rdtscp();
    return end - start;
}

struct alignas(CACHE_LINE_SIZE) Tick {
    uint64_t timestamp;
    int32_t bid_price;
    int32_t ask_price;
    int32_t bid_qty;
    int32_t ask_qty;
    uint16_t symbol_id;
};

enum class Side : uint8_t { BUY, SELL, NONE };

// Thread-safe snapshot for Dashboard
struct alignas(CACHE_LINE_SIZE) SymbolStats {
    std::atomic<int32_t> last_mid{0};
    std::atomic<int32_t> position{0};
    std::atomic<int64_t> pnl{0};
    std::atomic<int32_t> signal{0};
    std::atomic<uint32_t> trades{0};
    std::atomic<uint32_t> wins{0}; // Track winning trades for Win Rate
};

struct alignas(CACHE_LINE_SIZE) GlobalStats {
    SymbolStats symbols[MAX_SYMBOLS];
};

inline void cpu_pause() {
    _mm_pause();
}

} // namespace hft
