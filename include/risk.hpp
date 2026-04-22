#pragma once

#include "common.hpp"

namespace hft {

/**
 * @brief Risk Engine.
 * Inline pre-trade checks to minimize function overhead.
 */
class RiskEngine {
public:
    static constexpr int32_t MAX_POSITION = 1000;
    static constexpr int32_t MAX_ORDER_QTY = 100;
    
    int32_t current_position = 0;

    /**
     * @brief Pre-trade risk check.
     * All checks must be branchless if possible.
     */
    inline bool check_risk(Side side, int32_t qty) {
        // Simple order size check
        const bool qty_ok = (qty <= MAX_ORDER_QTY);
        
        // Simple position check
        const int32_t new_pos = (side == Side::BUY) ? (current_position + qty) : (current_position - qty);
        const bool pos_ok = (new_pos <= MAX_POSITION && new_pos >= -MAX_POSITION);
        
        // Combine (branchless if compiled with -O3)
        return qty_ok && pos_ok;
    }
    
    inline void update_position(Side side, int32_t qty) {
        // For HFT simulation, update position immediately after "execution"
        current_position += (side == Side::BUY) ? qty : -qty;
    }
};

} // namespace hft
