#include "ilp_writer.h"
#include "../ingest/tick_data.h"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <random>

namespace
{
    std::atomic<bool> g_shutdown_requested{false};

    void signal_handler(int signal)
    {
        std::cout << "\nShutdown requested (signal " << signal << ")" << std::endl;
        g_shutdown_requested.store(true);
    }
}

// Simple synthetic tick generator for demo
class SyntheticTickGenerator
{
public:
    SyntheticTickGenerator() : gen_(rd_()) {}

    trading::TickData generate_tick()
    {
        static const std::vector<std::string> symbols = {
            "RELIANCE", "TCS", "INFY", "HDFC", "ICICI"};
        static const std::vector<std::string> exchanges = {"NSE", "BSE"};

        std::uniform_int_distribution<size_t> symbol_dist(0, symbols.size() - 1);
        std::uniform_int_distribution<size_t> exchange_dist(0, exchanges.size() - 1);
        std::uniform_real_distribution<double> price_dist(2000.0, 3000.0);
        std::uniform_int_distribution<std::uint64_t> qty_dist(1, 1000);
        std::uniform_int_distribution<int> side_dist(0, 1);

        return trading::TickData(
            symbols[symbol_dist(gen_)],
            exchanges[exchange_dist(gen_)],
            price_dist(gen_),
            qty_dist(gen_),
            side_dist(gen_) == 0 ? 'B' : 'S');
    }

private:
    std::random_device rd_;
    std::mt19937 gen_;
};

int main(int argc, char *argv[])
{
    // Install signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "Simple ILP Writer Demo" << std::endl;

    // Create ILP writer with default config
    trading::ILPConfig config;
    if (argc > 1)
    {
        config.questdb_host = argv[1];
    }
    if (argc > 2)
    {
        config.questdb_port = std::stoi(argv[2]);
    }

    trading::ILPWriter writer(config);

    if (!writer.start())
    {
        std::cerr << "Failed to start ILP writer" << std::endl;
        return 1;
    }

    std::cout << "ILP Writer started. Generating synthetic ticks..." << std::endl;

    // Generate and send synthetic ticks
    SyntheticTickGenerator generator;

    while (!g_shutdown_requested.load())
    {
        auto tick = generator.generate_tick();
        writer.write_tick(tick);

        // Generate ticks at 10 Hz
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Final flush and cleanup
    writer.flush();
    writer.stop();

    const auto &stats = writer.stats();
    std::cout << "\nFinal Statistics:" << std::endl;
    std::cout << "  Lines sent: " << stats.lines_sent << std::endl;
    std::cout << "  Batches sent: " << stats.batches_sent << std::endl;
    std::cout << "  Errors: " << stats.errors << std::endl;

    return 0;
}