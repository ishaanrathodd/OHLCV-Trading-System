#pragma once

#include "tick_data.h"
#include <functional>
#include <memory>
#include <string>

namespace trading
{

    // Simple callback for tick data processing
    using TickCallback = std::function<void(const TickData&)>; 

    // Simple base class for market data ingestion
    class MarketDataIngest
    {
    public:
        virtual ~MarketDataIngest() = default;

        // Start ingestion
        virtual bool start() = 0;

        // Stop ingestion
        virtual void stop() = 0;

        // Check if running
        virtual bool is_running() const = 0;

        // Set callback for processed ticks
        virtual void set_tick_callback(TickCallback callback) = 0;

    protected:
        // Helper to invoke callback safely
        void invoke_callback(const TickData &tick)
        {
            if (tick_callback_)
            {
                tick_callback_(tick);
            }
        }

        TickCallback tick_callback_;
    };

} // namespace trading