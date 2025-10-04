#pragma once

#include "ilp_types.h"
#include "../ingest/tick_data.h"
#include <vector>
#include <string>
#include <atomic>

namespace trading
{

    // Simple ILP writer for QuestDB
    class ILPWriter
    {
    public:
        explicit ILPWriter(const ILPConfig &config);
        ~ILPWriter();

        // Lifecycle
        bool start();
        void stop();
        bool is_running() const;

        // Submit tick for writing (simple batching)
        void write_tick(const TickData &tick);

        // Force flush current batch
        void flush();

        // Statistics
        const ILPStats &stats() const { return stats_; }

    private:
        // Send batch to QuestDB
        bool send_batch(const std::vector<std::string> &lines);

        ILPConfig config_;
        ILPStats stats_;
        std::vector<std::string> current_batch_;
        std::atomic<bool> running_;
        int socket_fd_ = -1;
    };

} // namespace trading