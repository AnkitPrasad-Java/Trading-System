#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <windows.h>
#include <atomic>

#include "common.hpp"
#include "spsc_queue.hpp"
#include "tick_generator.hpp"
#include "lob.hpp"
#include "trading_engine.hpp"

using namespace hft;

// Shared Global Statistics for Monitor Thread
GlobalStats g_stats;
std::atomic<bool> g_running{true};

// Global queue for communication
constexpr size_t QUEUE_SIZE = 1024 * 64;
SPSCQueue<Tick, QUEUE_SIZE> market_data_queue;

// Latency tracking
constexpr size_t MAX_LATENCY_SAMPLES = 1000000;
uint64_t latency_buffer[MAX_LATENCY_SAMPLES];
std::atomic<size_t> latency_count{0};

void pin_thread(uint32_t core_id, int priority) {
    HANDLE thread = GetCurrentThread();
    DWORD_PTR mask = 1ULL << core_id;
    SetThreadAffinityMask(thread, mask);
    SetThreadPriority(thread, priority);
}

// Producer Thread: Continuous Market Data (Core 1)
DWORD WINAPI producer_thread(LPVOID lpParam) {
    size_t tick_count = *static_cast<size_t*>(lpParam);
    pin_thread(1, THREAD_PRIORITY_HIGHEST);
    TickGenerator gen(12345);
    
    for (size_t i = 0; i < tick_count && g_running; ++i) {
        Tick tick;
        gen.generate((uint16_t)(i % MAX_SYMBOLS), tick);
        while (UNLIKELY(!market_data_queue.push(tick)) && g_running) cpu_pause();
        for (int j = 0; j < 2000; ++j) cpu_pause();
    }
    g_running = false;
    return 0;
}

// Consumer Thread: Hot Path (Core 2)
DWORD WINAPI consumer_thread(LPVOID lpParam) {
    pin_thread(2, THREAD_PRIORITY_TIME_CRITICAL);
    LimitOrderBookManager lob;
    MarketMaker mm(&g_stats);
    const uint64_t rdtscp_overhead = calibrate_rdtscp();

    while (g_running) {
        Tick tick;
        if (LIKELY(market_data_queue.pop(tick))) {
            const uint64_t t0 = tick.timestamp;
            lob.on_tick(tick);
            mm.on_tick(tick, lob.get_book(tick.symbol_id));

            uint64_t t_exit = rdtscp();
            size_t idx = latency_count.fetch_add(1, std::memory_order_relaxed);
            if (LIKELY(idx < MAX_LATENCY_SAMPLES)) {
                latency_buffer[idx] = (t_exit > t0 + rdtscp_overhead) ? (t_exit - t0 - rdtscp_overhead) : 0;
            }
        } else {
            cpu_pause();
        }
    }
    return 0;
}

// Monitor Thread: Dashboard (Core 0)
DWORD WINAPI monitor_thread(LPVOID lpParam) {
    pin_thread(0, THREAD_PRIORITY_NORMAL);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hOut, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &cursorInfo);

    while (g_running) {
        std::cout << "\033[H"; 
        
        std::cout << "===================================================================\n";
        std::cout << "        HFT REAL-TIME DISCIPLINED TRADING TERMINAL [LIVE]          \n";
        std::cout << "===================================================================\n";
        std::cout << "SYM | MID_PRICE | POS | PNL ($) | SIG | TRADES |  PPT   | WIN% \n";
        std::cout << "-------------------------------------------------------------------\n";
        
        int64_t total_pnl = 0;
        uint32_t total_trades = 0;
        uint32_t total_wins = 0;
        
        for (uint16_t i = 0; i < MAX_SYMBOLS; ++i) {
            int32_t mid = g_stats.symbols[i].last_mid.load(std::memory_order_relaxed);
            int32_t pos = g_stats.symbols[i].position.load(std::memory_order_relaxed);
            int64_t pnl = g_stats.symbols[i].pnl.load(std::memory_order_relaxed);
            int32_t sig = g_stats.symbols[i].signal.load(std::memory_order_relaxed);
            uint32_t t   = g_stats.symbols[i].trades.load(std::memory_order_relaxed);
            uint32_t w   = g_stats.symbols[i].wins.load(std::memory_order_relaxed);
            
            total_pnl += pnl;
            total_trades += t;
            total_wins += w;

            double pnl_val = (double)pnl/PRICE_SCALE;
            double ppt = (t > 0) ? (pnl_val / t) : 0.0;
            double win_rate = (t > 0) ? ((double)w / t * 100.0) : 0.0;

            std::cout << std::setw(3) << i << " | "
                      << std::fixed << std::setprecision(3) << std::setw(9) << (double)mid/PRICE_SCALE << " | "
                      << std::setw(3) << pos << " | "
                      << std::setprecision(2) << std::setw(7) << pnl_val << " | "
                      << std::setw(3) << sig << " | "
                      << std::setw(6) << t << " | "
                      << std::setprecision(4) << std::setw(6) << ppt << " | "
                      << std::setprecision(1) << std::setw(4) << win_rate << "%\n";
        }
        
        std::cout << "-------------------------------------------------------------------\n";
        double total_pnl_val = (double)total_pnl/PRICE_SCALE;
        double total_win_rate = (total_trades > 0) ? ((double)total_wins / total_trades * 100.0) : 0.0;

        std::cout << "TOTAL PNL: $" << std::fixed << std::setprecision(2) << total_pnl_val 
                  << " | TRADES: " << total_trades 
                  << " | WIN RATE: " << total_win_rate << "%\n";
        
        size_t l_count = latency_count.load(std::memory_order_relaxed);
        if (l_count > 1000) {
            std::cout << "LATENCY (last): " << latency_buffer[(l_count-1) % MAX_LATENCY_SAMPLES] << " cy\n";
        }

        std::cout << "\n[Press ESC to Exit]\n";
        if (GetAsyncKeyState(VK_ESCAPE)) g_running = false;
        Sleep(150); 
    }

    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hOut, &cursorInfo);
    return 0;
}

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

int main() {
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const size_t TICK_COUNT = 100000000; 
    std::cout << "Initializing Disciplined HFT Engine...\n";
    Sleep(1000);

    HANDLE threads[3];
    threads[0] = CreateThread(NULL, 0, producer_thread, (void*)&TICK_COUNT, 0, NULL);
    threads[1] = CreateThread(NULL, 0, consumer_thread, NULL, 0, NULL);
    threads[2] = CreateThread(NULL, 0, monitor_thread, NULL, 0, NULL);

    WaitForSingleObject(threads[0], INFINITE);
    WaitForSingleObject(threads[1], INFINITE);
    WaitForSingleObject(threads[2], INFINITE);
    
    return 0;
}
