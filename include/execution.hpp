#pragma once

#include "common.hpp"
#include <iostream>

namespace hft {

/**
 * @brief Inline Execution Simulator.
 * Simulates immediate execution for low-latency simulation.
 */
class ExecutionSimulator {
public:
    /**
     * @brief Send order and record tick-to-order latency.
     */
    inline void execute_order(const OrderRequest& order, uint64_t& t_exit) {
        // Record exit timestamp as the moment "execution" starts
        t_exit = rdtscp();
        
        // In simulation, we immediately "fill" the order.
        // No thread hop or async callback here to keep pipeline sub-microsecond.
    }
};

} // namespace hft
