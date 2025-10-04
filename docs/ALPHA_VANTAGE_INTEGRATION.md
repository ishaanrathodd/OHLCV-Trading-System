# Alpha Vantage BSE Integration

This document describes the implementation of Alpha Vantage BSE integration to replace the WebSocket simulation in our trading system.

## Overview

The Alpha Vantage integration provides real BSE (Bombay Stock Exchange) market data through HTTP API calls, replacing the previous WebSocket simulation. The system maintains the same latency measurement and batching pipeline while fetching actual market data.

## Implementation Details

### 1. AlphaVantageClient Class

**File**: `src/engine/ingest/alpha_vantage_client.h/.cpp`

Key features:
- **HTTP Client**: Uses curl for reliable HTTP requests
- **Rate Limiting**: Respects Alpha Vantage free tier (25 requests/day)
- **JSON Parsing**: Handles TIME_SERIES_DAILY API responses
- **Latency Tracking**: Measures end-to-end API call latency
- **Error Handling**: Comprehensive error detection and retry logic

```cpp
class AlphaVantageClient : public MarketDataIngest
{
public:
    explicit AlphaVantageClient(const std::string &api_key, 
                               const std::vector<std::string> &symbols);
    
    // Latency metrics
    struct LatencyMetrics {
        std::chrono::microseconds avg_api_latency{0};
        std::chrono::microseconds max_api_latency{0};
        std::chrono::microseconds min_api_latency{std::chrono::microseconds::max()};
        size_t request_count{0};
        size_t error_count{0};
    };
};
```

### 2. API Integration

**Endpoint**: `https://www.alphavantage.co/query`
**Function**: `TIME_SERIES_DAILY`
**Symbols**: `RELIANCE.BSE`, `TCS.BSE` (configurable)

Example API call:
```
https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=RELIANCE.BSE&apikey=YOUR_API_KEY
```

### 3. Data Transformation

The system converts Alpha Vantage JSON responses to our TickData format:

```cpp
// Input: JSON with OHLC data
{
  "Time Series (Daily)": {
    "2025-09-05": {
      "1. open": "1365.9000",
      "2. high": "1380.8000", 
      "3. low": "1359.3500",
      "4. close": "1374.3000",
      "5. volume": "371446"
    }
  }
}

// Output: Multiple TickData objects
TickData open_tick("RELIANCE", "BSE", 1365.9, 92861, 'B');
TickData high_tick("RELIANCE", "BSE", 1380.8, 92861, 'B');
TickData low_tick("RELIANCE", "BSE", 1359.35, 92861, 'S');
TickData close_tick("RELIANCE", "BSE", 1374.3, 92861, 'B');
```

### 4. Pipeline Architecture

```
Alpha Vantage API → HTTP Client → JSON Parser → TickData → QuestDB ILP
       ↓              ↓            ↓            ↓          ↓
   Real BSE Data → Latency Tracking → Format Conversion → Database Storage
```

### 5. Applications

#### Individual Components

1. **Ingest App**: `build/src/engine/ingest/ingest_app`
   ```bash
   ./ingest_app alphavantage YOUR_API_KEY
   ```

2. **ILP Writer App**: `build/src/engine/ilp_writer/ilp_writer_app`

#### Complete Pipeline

3. **Alpha Vantage Pipeline**: `build/alpha_vantage_pipeline`
   ```bash
   ./alpha_vantage_pipeline YOUR_API_KEY [questdb_host] [questdb_port]
   ```

## Configuration

### Alpha Vantage Settings
- **API Key**: Required for authentication
- **Symbols**: Default `["RELIANCE.BSE", "TCS.BSE"]`
- **Polling Interval**: Default 5 minutes (configurable)
- **Rate Limit**: 25 requests per 24 hours (free tier)

### QuestDB Settings
- **Host**: Default `localhost`
- **Port**: Default `9009` (ILP protocol)
- **Batch Size**: Default 10 ticks

## Usage Examples

### 1. Basic Data Fetching
```bash
# Test Alpha Vantage integration
./scripts/alpha_vantage_demo.sh YOUR_API_KEY
```

### 2. Complete Pipeline
```bash
# Full BSE → QuestDB pipeline
./scripts/complete_pipeline_demo.sh YOUR_API_KEY
```

### 3. Custom Configuration
```cpp
// C++ code example
std::vector<std::string> symbols = {"RELIANCE.BSE", "TCS.BSE", "INFY.BSE"};
auto client = std::make_unique<AlphaVantageClient>(api_key, symbols);
client->set_polling_interval(std::chrono::seconds(180)); // 3 minutes
```

## Latency Metrics

The system tracks comprehensive latency metrics:

```
=== Alpha Vantage Latency Metrics ===
Total requests: 2
Error count: 0
Average latency: 699300 μs
Min latency: 665371 μs  
Max latency: 733230 μs
```

## Error Handling

1. **HTTP Errors**: Connection timeouts, DNS failures
2. **API Errors**: Invalid symbols, rate limits, authentication
3. **JSON Parsing**: Malformed responses, missing fields
4. **Rate Limiting**: Automatic throttling for free tier

## Performance Characteristics

- **API Latency**: ~650-750ms (typical for Alpha Vantage)
- **Processing Latency**: <100μs (JSON parsing + conversion)
- **Memory Usage**: Minimal (streaming processing)
- **CPU Usage**: Low (polling-based, not real-time)

## Integration with Existing System

The Alpha Vantage client seamlessly integrates with the existing architecture:

1. **MarketDataIngest Interface**: Same interface as WebSocketClient
2. **TickData Format**: No changes to data structures
3. **QuestDB ILP Writer**: Unchanged batching and writing logic
4. **Latency Measurement**: Enhanced with API-specific metrics

## Limitations

1. **Free Tier**: 25 requests/day limit
2. **Polling Based**: Not real-time (minutes delay)
3. **Daily Data**: Only daily OHLC, not tick-by-tick
4. **BSE Focus**: Primarily Indian market data

## Future Enhancements

1. **Premium Tier**: Support for intraday data
2. **WebSocket Upgrade**: Real-time data when available
3. **Multiple Exchanges**: NSE, MCX support
4. **Caching**: Local data storage for replay

## Getting Started

1. **Get API Key**: Register at https://www.alphavantage.co/support/#api-key
2. **Build System**: `cmake --build build`
3. **Start QuestDB**: Ensure ILP is enabled on port 9009
4. **Run Pipeline**: `./scripts/complete_pipeline_demo.sh YOUR_API_KEY`

## Troubleshooting

### Common Issues

1. **"Rate limit exceeded"**: Wait 24 hours or upgrade to premium
2. **"Connection failed"**: Check internet connectivity
3. **"Invalid API call"**: Verify API key and symbol format
4. **"QuestDB not accessible"**: Ensure QuestDB is running on port 9009

### Debug Mode

Enable verbose logging by modifying log levels in the source code or using debug builds.
