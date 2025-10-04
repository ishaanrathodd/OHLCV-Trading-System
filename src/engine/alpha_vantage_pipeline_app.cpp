#include "../ingest/alpha_vantage_client.h"
#include "../ilp_writer/ilp_writer.h"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <iomanip>

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

    if (argc < 6)
    {
        std::cerr << "Usage: " << argv[0] << " <API_KEY> <QUESTDB_HOST> <QUESTDB_PORT> <BATCH_SIZE> <POLLING_INTERVAL> [SYMBOL1] [SYMBOL2] ...\n";
        std::cerr << "\nExample:\n";
        std::cerr << "  " << argv[0] << " YOUR_API_KEY localhost 9009 10 120 RELIANCE.BSE TCS.BSE\n";
        std::cerr << "\nThis application fetches real BSE market data from Alpha Vantage\n";
        std::cerr << "and writes it directly to QuestDB using ILP protocol.\n";
        std::cerr << "\nAll configuration is now passed via command line.\n";
        return 1;
    }

    const std::string api_key = argv[1];
    const std::string questdb_host = argv[2];
    const int questdb_port = std::stoi(argv[3]);
    const int batch_size = std::stoi(argv[4]);
    const int polling_interval = std::stoi(argv[5]);

    // Extract symbols from remaining arguments (default to RELIANCE.BSE, TCS.BSE if none provided)
    std::vector<std::string> symbols;
    if (argc > 6)
    {
        for (int i = 6; i < argc; ++i)
        {
            symbols.push_back(argv[i]);
        }
    }
    else
    {
        symbols = {"RELIANCE.BSE", "TCS.BSE"}; // Fallback
    }

    std::cout << "=== Alpha Vantage → QuestDB Pipeline ===" << std::endl;
    std::cout << "API Key: " << api_key.substr(0, 8) << "..." << std::endl;
    std::cout << "QuestDB: " << questdb_host << ":" << questdb_port << std::endl;
    std::cout << "Batch size: " << batch_size << std::endl;
    std::cout << "Polling interval: " << polling_interval << "s" << std::endl;
    std::cout << "Symbols: ";
    for (size_t i = 0; i < symbols.size(); ++i)
    {
        std::cout << symbols[i];
        if (i < symbols.size() - 1)
            std::cout << ", ";
    }
    std::cout << std::endl;
    std::cout << std::endl;

    // Create QuestDB ILP writer
    trading::ILPConfig ilp_config;
    ilp_config.questdb_host = questdb_host;
    ilp_config.questdb_port = questdb_port;
    ilp_config.batch_size = batch_size;

    auto ilp_writer = std::make_unique<trading::ILPWriter>(ilp_config);

    if (!ilp_writer->start())
    {
        std::cerr << "Failed to start ILP writer for QuestDB at " << questdb_host << ":" << questdb_port << std::endl;
        std::cerr << "Make sure QuestDB is running and ILP is enabled." << std::endl;
        return 1;
    }

    std::cout << "Connected to QuestDB successfully!" << std::endl;

    // Create Alpha Vantage client with configurable symbols
    auto av_client = std::make_unique<trading::AlphaVantageClient>(api_key, symbols);

    // Set configurable polling interval
    av_client->set_polling_interval(std::chrono::seconds(polling_interval));

    // Counters for metrics
    std::atomic<size_t> ticks_received{0};
    std::atomic<size_t> ticks_written{0};

    // Set up callback to write ticks to QuestDB
    av_client->set_tick_callback([&](const trading::TickData& tick)
    {
        ticks_received++;
        
        // Set process time for latency measurement
        auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        trading::TickData processed_tick = tick;
        processed_tick.process_time_us = now_us;
        
        // Calculate end-to-end latency
        auto total_latency = processed_tick.process_time_us - processed_tick.timestamp_us;
        auto receive_latency = processed_tick.receive_time_us - processed_tick.timestamp_us;
        auto processing_latency = processed_tick.process_time_us - processed_tick.receive_time_us;
        
        std::cout << "Tick: " << tick.symbol << "@" << tick.exchange
                  << " price=" << tick.price << " qty=" << tick.quantity
                  << " side=" << tick.side << std::endl;
        std::cout << "  → Receive latency: " << receive_latency << "μs" << std::endl;
        std::cout << "  → Processing latency: " << processing_latency << "μs" << std::endl;
        std::cout << "  → Total latency: " << total_latency << "μs" << std::endl;
        
        // Write to QuestDB
        ilp_writer->write_tick(processed_tick);
        ticks_written++;
        std::cout << "  ✓ Written to QuestDB" << std::endl;
        
        std::cout << std::endl; 
    });

    // Start Alpha Vantage client
    if (!av_client->start())
    {
        std::cerr << "Failed to start Alpha Vantage client" << std::endl;
        return 1;
    }

    std::cout << "Pipeline started! Fetching BSE data every 2 minutes..." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;

    // Main loop - print statistics every 30 seconds
    auto last_stats_time = std::chrono::steady_clock::now();

    while (!g_shutdown_requested.load() && av_client->is_running())
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        auto now = std::chrono::steady_clock::now();
        if (now - last_stats_time >= std::chrono::seconds(30))
        {
            // Print statistics
            auto av_metrics = av_client->get_latency_metrics();
            auto ilp_stats = ilp_writer->stats();

            std::cout << "=== Pipeline Statistics ===" << std::endl;
            std::cout << "Ticks received: " << ticks_received.load() << std::endl;
            std::cout << "Ticks written to QuestDB: " << ticks_written.load() << std::endl;
            std::cout << "ILP lines sent: " << ilp_stats.lines_sent << std::endl;
            std::cout << "ILP batches sent: " << ilp_stats.batches_sent << std::endl;
            std::cout << "ILP errors: " << ilp_stats.errors << std::endl;
            std::cout << "API requests: " << av_metrics.request_count << std::endl;
            std::cout << "API errors: " << av_metrics.error_count << std::endl;
            std::cout << "Average API latency: " << av_metrics.avg_api_latency.count() << "μs" << std::endl;
            std::cout << std::endl;

            last_stats_time = now;
        }
    }

    // Cleanup
    av_client->stop();
    ilp_writer->stop();

    // Final statistics
    auto final_metrics = av_client->get_latency_metrics();
    auto final_ilp_stats = ilp_writer->stats();

    std::cout << "\n=== Final Pipeline Statistics ===" << std::endl;
    std::cout << "Ticks received: " << ticks_received.load() << std::endl;
    std::cout << "Ticks written to QuestDB: " << ticks_written.load() << std::endl;
    std::cout << "ILP lines sent: " << final_ilp_stats.lines_sent << std::endl;
    std::cout << "ILP batches sent: " << final_ilp_stats.batches_sent << std::endl;
    std::cout << "ILP errors: " << final_ilp_stats.errors << std::endl;
    std::cout << "API requests: " << final_metrics.request_count << std::endl;
    std::cout << "API errors: " << final_metrics.error_count << std::endl;
    std::cout << "Average API latency: " << final_metrics.avg_api_latency.count() << "μs" << std::endl;
    std::cout << "Min API latency: " << final_metrics.min_api_latency.count() << "μs" << std::endl;
    std::cout << "Max API latency: " << final_metrics.max_api_latency.count() << "μs" << std::endl;

    if (ticks_received.load() > 0)
    {
        double success_rate = (double)ticks_written.load() / ticks_received.load() * 100.0;
        std::cout << "Success rate: " << std::fixed << std::setprecision(1) << success_rate << "%" << std::endl;
    }

    std::cout << "\nPipeline stopped successfully." << std::endl;
    return 0;
}