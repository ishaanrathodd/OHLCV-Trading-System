#include "market_data_ingest.h"
#include "alpha_vantage_client.h"
#include <iostream>
#include <memory>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

namespace
{
    std::atomic<bool> g_shutdown_requested{false};

    void signal_handler(int signal)
    {
        std::cout << "\nShutdown requested (signal " << signal << ")" << std::endl;
        g_shutdown_requested.store(true);
    }
}

int main(int argc, char *argv[])
{
    // Install signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <API_KEY> [mode]\n";
        std::cerr << "  API_KEY: Your Alpha Vantage API key\n";
        std::cerr << "  mode: 'alphavantage' (default) or 'replay' (for testing)\n";
        std::cerr << "\nExamples:\n";
        std::cerr << "  " << argv[0] << " YOUR_API_KEY\n";
        std::cerr << "  " << argv[0] << " YOUR_API_KEY alphavantage\n";
        std::cerr << "  " << argv[0] << " test_data/sample_ticks.csv replay\n";
        std::cerr << "\nPrimary mode: Alpha Vantage BSE data (RELIANCE.BSE, TCS.BSE)\n";
        return 1;
    }

    const std::string config = argv[1];
    const std::string mode_str = (argc > 2) ? argv[2] : "alphavantage";

    // Only Alpha Vantage ingestion is supported
    std::vector<std::string> symbols = {"RELIANCE.BSE", "TCS.BSE"};
    auto av_client = std::make_unique<trading::AlphaVantageClient>(config, symbols);
    // Set polling interval (5 minutes for demo, can be adjusted)
    av_client->set_polling_interval(std::chrono::seconds(300));
    std::unique_ptr<trading::MarketDataIngest> ingest = std::move(av_client);

    // Set up tick callback - just print for demo
    ingest->set_tick_callback([](const trading::TickData &tick)
                              { std::cout << "Received tick: " << tick.symbol << "@" << tick.exchange
                                          << " price=" << tick.price << " qty=" << tick.quantity
                                          << " side=" << tick.side
                                          << " latency=" << (tick.receive_time_us - tick.timestamp_us) << "us"
                                          << std::endl; });

    // Start processing
    if (!ingest->start())
    {
        std::cerr << "Failed to start market data ingest" << std::endl;
        return 1;
    }

    std::cout << "Market data ingest started. Press Ctrl+C to stop." << std::endl;

    // Wait for shutdown
    while (!g_shutdown_requested.load() && ingest->is_running())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    ingest->stop();

    // Print latency metrics
    auto av_client_ptr = dynamic_cast<trading::AlphaVantageClient *>(ingest.get());
    if (av_client_ptr)
    {
        auto metrics = av_client_ptr->get_latency_metrics();
        std::cout << "\n=== Alpha Vantage BSE Performance Metrics ===" << std::endl;
        std::cout << "Total API requests: " << metrics.request_count << std::endl;
        std::cout << "Error count: " << metrics.error_count << std::endl;
        std::cout << "Average latency: " << metrics.avg_api_latency.count() << " μs" << std::endl;
        std::cout << "Min latency: " << metrics.min_api_latency.count() << " μs" << std::endl;
        std::cout << "Max latency: " << metrics.max_api_latency.count() << " μs" << std::endl;
    }

    std::cout << "BSE market data ingest completed." << std::endl;

    return 0;
}