# Trading System Demo - Intern Interview

## Overview
This is a simplified high-frequency trading data pipeline demonstrating:
- **Market data ingestion** (tick data from CSV files)
- **Real-time processing** (parsing and validation)
- **Database persistence** (QuestDB with ILP protocol)
- **Basic performance tracking** (latency measurements)

## Quick Demo (5 minutes)

### 1. Build the System
```bash
cd build
make -j4
```

### 2. Start QuestDB Database
```bash
# Terminal 1 - Start QuestDB
questdb start
# Access web console: http://localhost:9000
```

### 3. Run the Ingest Pipeline
```bash
# Terminal 2 - Run market data ingest
cd build
./src/engine/ingest/ingest_app replay ../test_data/sample_ticks.csv
```

### 4. View Results in QuestDB
```sql
-- Go to http://localhost:9000 and run:
SELECT * FROM trades ORDER BY timestamp DESC LIMIT 10;
```

## Architecture for Interview Discussion

### Core Components
1. **TickData**: Simple data structure for market ticks
2. **MarketDataIngest**: Base class for data ingestion
3. **WebSocketClient**: Simulates real market data feeds
4. **ILPWriter**: Writes data to QuestDB using InfluxDB Line Protocol

### Key Design Decisions
- **CSV replay mode**: Easy testing without external dependencies
- **Simple threading**: Producer-consumer pattern for data flow
- **Direct TCP sockets**: Minimal overhead for database writes
- **Basic batching**: Groups writes for better performance

### Performance Considerations
- **Zero-copy where possible**: Pass data by reference
- **Simple memory pools**: Avoid allocation in hot paths
- **Configurable batch sizes**: Tune for latency vs throughput
- **Basic latency tracking**: Measure end-to-end pipeline

## Code Walkthrough Points

### 1. Data Structure (`tick_data.h`)
```cpp
struct TickData {
    std::string symbol;
    double price;
    int64_t quantity;
    std::string side;
    // Simple, readable structure for intern-level understanding
};
```

### 2. Processing Pipeline (`websocket_client.cpp`)
- Read CSV line by line
- Parse into TickData structure
- Send to processing queue
- Basic error handling

### 3. Database Integration (`ilp_writer.cpp`)
- TCP socket connection to QuestDB
- Batch multiple ticks for efficiency
- ILP format conversion
- Simple retry logic

## Interview Questions This Demonstrates

### Technical Questions
1. **"How would you handle high-frequency data?"**
   - Show batching strategy
   - Discuss memory management
   - Explain threading model

2. **"What about error handling?"**
   - Connection failures
   - Data validation
   - Graceful degradation

3. **"How do you measure performance?"**
   - End-to-end latency tracking
   - Throughput measurements
   - Database write metrics

### System Design Questions
1. **"How would you scale this system?"**
   - Multiple data sources
   - Horizontal scaling
   - Load balancing

2. **"What about reliability?"**
   - Data durability
   - Failover strategies
   - Monitoring and alerting

## Next Steps for Production
1. Add comprehensive error handling
2. Implement connection pooling
3. Add monitoring and metrics
4. Create proper configuration management
5. Add unit and integration tests
6. Implement proper logging

## Files to Review Before Interview
- `src/engine/ingest/websocket_client.cpp` - Data ingestion
- `src/engine/ilp_writer/ilp_writer.cpp` - Database integration
- `src/engine/common/tick_data.h` - Core data structures
- `CMakeLists.txt` - Build system understanding