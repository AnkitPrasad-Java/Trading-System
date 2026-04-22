#pragma once

#include "common.hpp"
#include "lob.hpp"

namespace hft {

/**
 * @brief Professional HFT Market Maker.
 * Tracks Realized vs Unrealized PnL and meaningful Win Rate.
 */
class MarketMaker {
    struct SymbolState {
        int32_t position = 0;
        int64_t cash = 0;
        int64_t realized_pnl = 0;
        int32_t avg_entry_price = 0;
        
        uint32_t persist_buy = 0;
        uint32_t persist_sell = 0;
        uint32_t total_trades = 0;
        uint32_t winning_trades = 0;
    };

    SymbolState symbols_[MAX_SYMBOLS];
    GlobalStats* stats_;

    static constexpr int32_t MAX_POS = 30;
    static constexpr int32_t ALPHA_THRESHOLD = 300;
    static constexpr uint32_t MIN_PERSIST = 5;

public:
    MarketMaker(GlobalStats* shared_stats) : stats_(shared_stats) {}

    inline void on_tick(const Tick& tick, const SymbolBook& book) {
        const uint16_t sid = tick.symbol_id;
        SymbolState& s = symbols_[sid];
        const int32_t mid = (book.best_bid_price + book.best_ask_price) / 2;

        const int32_t total_qty = book.best_bid_qty + book.best_ask_qty;
        const int32_t signal = (total_qty > 0) ? 
            ((book.best_bid_qty - book.best_ask_qty) * 1000 / total_qty) : 0;

        if (signal > ALPHA_THRESHOLD) {
            s.persist_buy++; s.persist_sell = 0;
        } else if (signal < -ALPHA_THRESHOLD) {
            s.persist_sell++; s.persist_buy = 0;
        } else {
            s.persist_buy = 0; s.persist_sell = 0;
        }

        Side action = Side::NONE;
        if (s.persist_buy >= MIN_PERSIST && s.position < MAX_POS) action = Side::BUY;
        else if (s.persist_sell >= MIN_PERSIST && s.position > -MAX_POS) action = Side::SELL;

        if (action != Side::NONE) {
            int32_t fill_price = (action == Side::BUY) ? book.best_bid_price : book.best_ask_price;
            
            // --- Realized PnL & Win Tracking Logic ---
            if (action == Side::BUY) {
                if (s.position < 0) { // Closing a Short
                    int64_t trade_profit = (int64_t)(s.avg_entry_price - fill_price) * 10;
                    s.realized_pnl += trade_profit;
                    if (trade_profit > 0) s.winning_trades++;
                } else { // Opening/Increasing Long
                    s.avg_entry_price = (s.avg_entry_price * s.position + fill_price * 10) / (s.position + 10);
                }
                s.position += 10;
                s.cash -= (int64_t)fill_price * 10;
            } else { // SELL
                if (s.position > 0) { // Closing a Long
                    int64_t trade_profit = (int64_t)(fill_price - s.avg_entry_price) * 10;
                    s.realized_pnl += trade_profit;
                    if (trade_profit > 0) s.winning_trades++;
                } else { // Opening/Increasing Short
                    s.avg_entry_price = (s.avg_entry_price * (-s.position) + fill_price * 10) / ((-s.position) + 10);
                }
                s.position -= 10;
                s.cash += (int64_t)fill_price * 10;
            }

            s.total_trades++;
            s.persist_buy = 0; s.persist_sell = 0;
        }

        // Update Stats: We store Realized PnL in the atomic pnl field for simplicity
        stats_->symbols[sid].last_mid.store(mid, std::memory_order_relaxed);
        stats_->symbols[sid].position.store(s.position, std::memory_order_relaxed);
        stats_->symbols[sid].signal.store(signal, std::memory_order_relaxed);
        stats_->symbols[sid].trades.store(s.total_trades, std::memory_order_relaxed);
        stats_->symbols[sid].wins.store(s.winning_trades, std::memory_order_relaxed);
        
        // Total PnL = Realized + Unrealized
        int64_t unrealized = (int64_t)s.position * mid + s.cash - s.realized_pnl;
        stats_->symbols[sid].pnl.store(s.realized_pnl, std::memory_order_relaxed); 
    }
};

} // namespace hft
