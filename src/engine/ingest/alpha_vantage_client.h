#pragma once

#include "market_data_ingest.h"
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>

namespace trading
{
    // Alpha Vantage API client for BSE market data
    // Uses TIME_SERIES_DAILY API with free tier limits (25 requests/day)
    class AlphaVantageClient : public MarketDataIngest
    {
    public:
        // Constructor with API key and BSE symbols
        explicit AlphaVantageClient(const std::string &api_key,
                                    const std::vector<std::string> &symbols = {"RELIANCE.BSE", "TCS.BSE"});
        ~AlphaVantageClient() override;

        // MarketDataIngest interface
        bool start() override;
        void stop() override;
        bool is_running() const override;
        void set_tick_callback(TickCallback callback) override;

        // Alpha Vantage specific methods
        void add_symbol(const std::string &symbol);
        void set_polling_interval(std::chrono::seconds interval);

        // Get latency metrics
        struct LatencyMetrics
        {
            std::chrono::microseconds avg_api_latency{0};
            std::chrono::microseconds max_api_latency{0};
            std::chrono::microseconds min_api_latency{std::chrono::microseconds::max()};
            size_t request_count{0};
            size_t error_count{0};
        };

        LatencyMetrics get_latency_metrics() const;

    private:
        // HTTP client methods
        std::string make_http_request(const std::string &url) const;
        std::string build_api_url(const std::string &symbol) const;

        // JSON parsing
        bool parse_time_series_response(const std::string &json_response,
                                        const std::string &symbol,
                                        std::vector<TickData> &ticks) const;

        // Data fetching loop
        void data_fetch_loop();

        // Rate limiting for free tier (25 requests/day)
        bool can_make_request() const;
        void record_request();

        // Configuration
        const std::string api_key_;
        std::vector<std::string> symbols_;
        std::chrono::seconds polling_interval_{300}; // 5 minutes default

        // Threading
        std::atomic<bool> running_;
        std::atomic<bool> stop_requested_;
        std::thread fetch_thread_;

        // Rate limiting
        mutable std::mutex rate_limit_mutex_;
        std::vector<std::chrono::system_clock::time_point> request_times_;
        static constexpr size_t MAX_REQUESTS_PER_DAY = 25;

        // Latency tracking
        mutable std::mutex metrics_mutex_;
        LatencyMetrics metrics_;

        // Error handling
        void log_error(const std::string &message) const;
        void log_info(const std::string &message) const;
    };

} // namespace trading
