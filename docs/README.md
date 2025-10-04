# BSE Trading System - Interview Project

**Real Indian market data pipeline demonstrating:**
- ✅ **Exchange website data retrieval** (Alpha Vantage BSE API)
- ✅ **QuestDB time-series storage** (ILP protocol)  
- 🚧 **Interactive Qt UI** (Next phase)

**Interview Focus**: "Retrieve data from exchange website, store in QuestDB and make an interactive Qt UI"

## Current Implementation ✅

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

## Quick Demo (Interview Ready) ⚡

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

# View results: http://localhost:9000
```

### 3. Explore Code
See `INTERN_DEMO.md` for detailed code walkthrough and interview talking points.

### Architecture Overview

```
BSE Market Data (Alpha Vantage) → HTTP Client → JSON Parser → TickData → QuestDB
                                      ↓             ↓           ↓         ↓  
                               API Latency → Processing → Batching → Time Series
                                ~1000ms       <100μs      <60ms       Storage
```

**Next Phase**: Qt UI → QuestDB queries → Interactive charts & analytics

### Core Components

#### `src/engine/ingest/alpha_vantage_client.*`
Production HTTP client for BSE market data:
- **Symbols**: RELIANCE.BSE, TCS.BSE (configurable)
- **API**: Alpha Vantage TIME_SERIES_DAILY
- **Rate Limiting**: 25 requests/day (free tier)
- **Performance**: Full latency tracking

#### `src/engine/ilp_writer/`  
QuestDB ILP protocol writer:
- **Batching**: Optimized throughput
- **Protocol**: InfluxDB Line Protocol over TCP
- **Performance**: <60ms p99 write latency

#### `alpha_vantage_pipeline`
Complete BSE → QuestDB application:
- **End-to-end**: API calls → Database storage
- **Metrics**: Request/response/error tracking
- **Production ready**: Error handling & monitoring
- WAL-safe deduplication
- Configurable batch size/timeout
- Prometheus metrics export

#### `scripts/hawkes_gen.py`
Synthetic market data generator using Hawkes process for realistic tick clustering.

#### `db/questdb/`
QuestDB schema and Docker configuration with:
- Partitioned `trades` table
- Retention policy (90 days)
- Optimized for write-heavy workloads

### Performance Validation

```bash
# Run performance tests
make perf

# Generate performance report
./scripts/run_perf_tests.sh
cat perf_report.json
```

### Memory Discipline

Hot paths (marked with `// HOT PATH` comments) follow strict rules:
- No dynamic allocation (`new`, `malloc`)
- Pre-allocated arenas (TODO: Phase 2)
- Stack allocation or memory pools only
- `mlockall()` for resident memory (TODO: Phase 2)

### Development Workflow

```bash
# Format code
make format

# Run tests
make test

# Full CI pipeline locally
./.github/workflows/ci.yml  # via act or similar
```

### Metrics

Prometheus metrics exposed on `:8080/metrics`:
- `ingest_duration_seconds` - Ingestion latency histogram
- `ilp_write_duration_seconds` - ILP write latency histogram  
- `ilp_batch_size` - Records per batch
- `ilp_queue_depth` - Pending writes

### Directory Structure

```
├── src/engine/ingest/        # Market data ingestion
├── src/engine/ilp_writer/    # QuestDB persistence  
├── db/questdb/              # Database schema
├── scripts/                 # Automation scripts
├── tests/                   # Unit and performance tests
└── .github/workflows/       # CI/CD pipeline
```

### Phase 2 Preview

Next phase will add:
- Lock-free ring buffers (LMAX Disruptor style)
- io_uring networking
- Memory arenas with `std::pmr`
- CPU pinning and NUMA awareness