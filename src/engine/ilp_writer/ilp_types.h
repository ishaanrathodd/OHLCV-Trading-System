#pragma once

#include <string>
#include <vector>

namespace trading
{

    // Simple configuration for ILP writer
    struct ILPConfig
    {
        std::string questdb_host = "localhost";
        int questdb_port = 9009; // ILP port
        size_t batch_size = 100; // Simple batching
    };

    // Simple statistics
    struct ILPStats
    {
        size_t lines_sent = 0;
        size_t batches_sent = 0;
        size_t errors = 0;

        void reset()
        {
            lines_sent = 0;
            batches_sent = 0;
            errors = 0;
        }
    };

}