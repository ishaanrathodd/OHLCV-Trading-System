#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace trading
{

    // Simple tick data structure for trading system demo
    struct TickData
    {
        // Core market data
        std::uint64_t timestamp_us; // Microseconds since epoch
        std::string symbol;         // e.g., "RELIANCE"
        std::string exchange;       // e.g., "NSE"
        double price;
        std::uint64_t quantity;
        char side; // 'B' = buy, 'S' = sell

        // Simple latency tracking
        std::uint64_t receive_time_us; // When we received the data
        std::uint64_t process_time_us; // When we processed it

        // Default constructor
        TickData() = default;

        // Simple constructor
        TickData(const std::string &sym, const std::string &exch, double p,
                 std::uint64_t qty, char s)
            : symbol(sym), exchange(exch), price(p), quantity(qty), side(s)
        {
            auto now = std::chrono::system_clock::now();
            timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                               now.time_since_epoch())
                               .count();
            receive_time_us = timestamp_us;
            process_time_us = 0; // Set when processing starts
        }

        // Convert to QuestDB ILP format
        std::string to_ilp_format() const
        {
            return "trades,symbol=" + symbol + ",exchange=" + exchange +
                   " price=" + std::to_string(price) +
                   ",quantity=" + std::to_string(quantity) +
                   ",side=\"" + std::string(1, side) + "\"" +
                   " " + std::to_string(timestamp_us * 1000); // Convert to nanoseconds
        }
    };

} // namespace trading