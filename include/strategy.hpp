#pragma once

#include "common.hpp"
#include "lob.hpp"

namespace hft {

/**
 * @brief HFT Alpha Strategy.
 * Computes alpha signal and decides order side.
 * Optimized for minimal CPU cycles and no division.
 */
class AlphaStrategy {
public:
    // Simple imbalance signal to decide side
    // signal = (bid_price * ask_qty) - (ask_price * bid_qty)
    // Buy if imbalance > threshold
    // Sell if imbalance < -threshold
    
    // Threshold in "price * qty" units
    static constexpr double SIGNAL_THRESHOLD = 0.5;

    inline Side compute_signal(const LOB& lob, double& signal_out) {
        // Cross-product imbalance calculation (no division)
        // This is a proxy for the 'micro-price' direction
        const double bid_vol_contribution = lob.best_bid_price * lob.best_ask_qty;
        const double ask_vol_contribution = lob.best_ask_price * lob.best_bid_qty;
        
        signal_out = bid_vol_contribution - ask_vol_contribution;
        
        // Ternary results in CMOV assembly (branchless)
        return (signal_out > 0) ? Side::BUY : Side::SELL;
    }
    
    inline bool should_trade(double signal) {
        // Simple threshold check
        // signal is the difference of cross-products
        // If the absolute value is high, it indicates price pressure
        return (signal > SIGNAL_THRESHOLD || signal < -SIGNAL_THRESHOLD);
    }
};

} // namespace hft
