

# Trading System MVP - Interview Edition

## Project Goals

- Retrieve real-time BSE (Bombay Stock Exchange) data via Alpha Vantage API
- Store data in QuestDB with efficient batching
- (Optional) Display data in a simple Qt UI
- Demonstrate clean, efficient code with latency measurement

## Unique Value

- Focus on minimal, clean code with strong emphasis on latency and throughput
- Real Indian market data (BSE) integration via Alpha Vantage
- Measure and report data pipeline latency to showcase efficiency

## Data Source: Alpha Vantage BSE Integration

**API Details:**
- **Exchange**: BSE (Bombay Stock Exchange) - free tier supported
- **Symbols**: RELIANCE.BSE, TCS.BSE, INFY.BSE, etc.
- **Functions**: TIME_SERIES_DAILY, TIME_SERIES_INTRADAY (premium)
- **Rate Limits**: 25 requests/day (free), 500/day (premium $25/month)
- **Format**: JSON/CSV responses

**Implementation Strategy:**
- Replace WebSocket simulation with Alpha Vantage HTTP API polling
- Use TIME_SERIES_DAILY for free tier (end-of-day data)
- Upgrade to premium for intraday real-time data if needed
- Poll API every 1-5 minutes (respecting rate limits)

## Minimal Architecture

1. **Data Ingest** 
   - HTTP client to Alpha Vantage API for BSE data
   - JSON parsing and tick data conversion
   - Basic error handling and retry logic
   - Timestamp data at each stage for latency measurement

2. **Batch Writer**
   - Batch incoming API responses
   - Write batches to QuestDB via ILP protocol
   - Record API latency and database write latency

3. **(Optional) UI**
   - Simple Qt app to display BSE trades
   - Show real symbol prices (RELIANCE.BSE, etc.)
   - (Optional) Display latency stats and API call metrics

## What's Not Included

- Advanced performance optimizations (ring buffers, lock-free, memory arenas)
- Monitoring/metrics (Prometheus, OpenTelemetry)
- Complex serialization formats
- WebSocket connections, shared memory, risk management, CI/CD, etc.

## Current Status

✅ **Completed:**
- Basic pipeline working with CSV simulation
- QuestDB ILP writer implementation
- Tick data structures and batching
- Build system and basic error handling

## Next Steps

1. **Alpha Vantage Integration**
   - Replace WebSocketClient with AlphaVantageClient
   - Implement HTTP requests to Alpha Vantage BSE endpoints
   - Add JSON parsing for TIME_SERIES_DAILY responses
   - Configure API key and rate limiting

2. **Real BSE Data Testing**
   - Test with actual BSE symbols (RELIANCE.BSE, TCS.BSE)
   - Verify end-to-end pipeline with real market data
   - Measure and optimize API call latency

3. **Enhanced Features** (if time permits)
   - Simple Qt UI for live BSE data display
   - Latency measurement dashboard
   - Multiple BSE symbol support