#pragma once

#include "common.hpp"

namespace hft {

/**
 * @brief Limit Order Book state for a single symbol.
 */
struct alignas(CACHE_LINE_SIZE) SymbolBook {
    int32_t best_bid_price;
    int32_t best_ask_price;
    int32_t best_bid_qty;
    int32_t best_ask_qty;
    
    inline void update(const Tick& tick) {
        best_bid_price = tick.bid_price;
        best_ask_price = tick.ask_price;
        best_bid_qty = tick.bid_qty;
        best_ask_qty = tick.ask_qty;
    }
};

/**
 * @brief Manages LOBs for all symbols.
 * Flat array of books for L1/L2 cache efficiency.
 */
class LimitOrderBookManager {
    SymbolBook books_[MAX_SYMBOLS];

public:
    inline void on_tick(const Tick& tick) {
        // Direct array indexing (no map lookup)
        books_[tick.symbol_id].update(tick);
    }
    
    inline const SymbolBook& get_book(uint16_t sid) const {
        return books_[sid];
    }
};

} // namespace hft
