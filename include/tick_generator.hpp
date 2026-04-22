#pragma once

#include "common.hpp"
#include <cstdint>

namespace hft {

struct FastPRNG {
    uint32_t x;
    FastPRNG(uint32_t seed) : x(seed) {}
    inline uint32_t next() {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return x;
    }
};

/**
 * @brief Realistic Market Simulator.
 * Simulates Price-to-Mean reversion for testing strategy edge.
 */
class TickGenerator {
    struct SymbolState {
        int32_t mid_price;
        int32_t target_price; // The mean it reverts to
        int32_t spread;
        int32_t qty_base;
    };

    SymbolState states_[MAX_SYMBOLS];
    FastPRNG prng_;

public:
    TickGenerator(uint32_t seed) : prng_(seed) {
        for (uint32_t i = 0; i < MAX_SYMBOLS; ++i) {
            int32_t start_price = 100 * PRICE_SCALE + (int32_t)(i * 10 * PRICE_SCALE);
            states_[i] = { start_price, start_price, 100, 1000 };
        }
    }

    inline void generate(uint16_t sid, Tick& tick) {
        SymbolState& s = states_[sid];
        
        // 1. Every 100 ticks, change the "Fair Value" target
        if ((prng_.next() % 100) == 0) {
            s.target_price += (int32_t)(prng_.next() % 51) - 25;
        }

        // 2. Mid price reverts to target
        int32_t reversion_force = (s.target_price - s.mid_price) / 10;
        int32_t noise = (int32_t)(prng_.next() % 11) - 5;
        s.mid_price += reversion_force + noise;
        
        tick.symbol_id = sid;
        tick.timestamp = rdtscp();
        tick.bid_price = s.mid_price - s.spread / 2;
        tick.ask_price = s.mid_price + s.spread / 2;
        
        // 3. Imbalance is "predictive" of the next move
        // If mid is below target, we skew the quantities to be BID-heavy
        int32_t bias = (s.target_price - s.mid_price) * 10;
        tick.bid_qty = s.qty_base + bias + (int32_t)(prng_.next() % 200) - 100;
        tick.ask_qty = s.qty_base - bias + (int32_t)(prng_.next() % 200) - 100;
        
        // Ensure non-zero qty
        if (tick.bid_qty < 10) tick.bid_qty = 10;
        if (tick.ask_qty < 10) tick.ask_qty = 10;
    }
};

} // namespace hft
