#pragma once

#include <atomic>
#include <array>
#include <cassert>
#include <cstddef>
#include "common.hpp"

namespace hft {

/**
 * @brief Lock-free Single-Producer Single-Consumer Queue.
 * @tparam T Type of elements.
 * @tparam N Capacity (must be power of 2).
 */
template <typename T, size_t N>
class SPSCQueue {
    static_assert((N > 0) && ((N & (N - 1)) == 0), "Capacity must be power of 2.");

public:
    SPSCQueue() : head_(0), tail_(0), buffer_{} {
    }

    // Producer side (Market Data Replay)
    bool push(const T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (tail + 1) & (N - 1);
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue full
        }
        
        buffer_[tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // Consumer side (Hot Path)
    bool pop(T& item) {
        const size_t head = head_.load(std::memory_order_relaxed);
        
        if (head == tail_.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }
        
        item = buffer_[head];
        head_.store((head + 1) & (N - 1), std::memory_order_release);
        return true;
    }

    // Read only for monitoring
    size_t size() const {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_relaxed);
        return (tail - head) & (N - 1);
    }

private:
    // Padded head to avoid false sharing with tail
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_;
    
    // Padded tail to avoid false sharing with head and buffer
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_;
    
    // Contiguous buffer
    alignas(CACHE_LINE_SIZE) std::array<T, N> buffer_;
};

} // namespace hft
