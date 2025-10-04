# BSE Trading System

**Real Indian market data pipeline demonstrating:**
- **Exchange website data retrieval** (Alpha Vantage BSE API)
- **QuestDB time-series storage** (ILP protocol)  
- **Interactive Qt UI**
- 
## Implementation

### Alpha Vantage BSE Integration
- **Real BSE Data**: RELIANCE.BSE, TCS.BSE market prices
- **HTTP Client**: Production-ready with error handling
- **Rate Limiting**: Respects 25 requests/day (free tier)
- **JSON Parsing**: Custom TIME_SERIES_DAILY parser
- **Latency Tracking**: End-to-end performance measurement

### QuestDB Time-Series Storage  
- **ILP Protocol**: High-performance line protocol writer
- **Batching**: Optimized for throughput
- **Real-time Storage**: Live BSE data → QuestDB
- **Performance**: <60ms write latency (p99)  

## Demo

### 1. Build System
```bash
cmake --build build
```

### 2. BSE → QuestDB Pipeline (Primary)
```bash
# Start QuestDB first
questdb start

# Run complete BSE data pipeline  
./build/alpha_vantage_pipeline YOUR_API_KEY
```

### 3. BSE Data Fetching (Testing)
```bash
# Just fetch and display BSE data
./build/src/engine/ingest/ingest_app YOUR_API_KEY
```

### 4. Personal Demo Script
```bash
# Interactive demo with your API key
./scripts/your_demo.sh
```
