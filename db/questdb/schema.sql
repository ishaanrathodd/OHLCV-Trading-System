-- Ultra-Low-Latency Trading Schema for QuestDB
-- Optimized for high-frequency tick data ingestion

-- Trades table with timestamp partitioning for efficient writes
-- and queries. Partitioned by day for optimal storage and pruning.

CREATE TABLE IF NOT EXISTS trades (
    timestamp TIMESTAMP,
    symbol SYMBOL CAPACITY 256 CACHE,
    price DOUBLE,
    quantity LONG,
    side SYMBOL CAPACITY 2 CACHE,  -- 'B' for buy, 'S' for sell
    exchange SYMBOL CAPACITY 16 CACHE,
    trade_id STRING,
    broker_timestamp TIMESTAMP,
    ingest_timestamp TIMESTAMP,
    
    -- Performance tracking timestamps (microsecond precision)
    t_receive_us LONG,   -- When we received from broker (µs since epoch)
    t_ingest_us LONG,    -- When ingest processed (µs since epoch) 
    t_ilp_us LONG,       -- When ILP writer processed (µs since epoch)
    
    -- Metadata
    sequence_number LONG,
    session_id SYMBOL CAPACITY 64 CACHE,
    
    -- Deduplication key fields
    source SYMBOL CAPACITY 32 CACHE,
    
    INDEX(symbol) CAPACITY 256,
    INDEX(exchange) CAPACITY 16
) 
TIMESTAMP(timestamp) 
PARTITION BY DAY 
WAL
DEDUP UPSERT KEYS(timestamp, symbol, trade_id);

-- Create indexes for common query patterns
CREATE INDEX trades_symbol_ts ON trades (symbol, timestamp);
CREATE INDEX trades_ingest_ts ON trades (ingest_timestamp);

-- OHLCV materialized view for efficient charting queries
-- Aggregated by minute for real-time chart updates
CREATE TABLE IF NOT EXISTS ohlcv_1m (
    timestamp TIMESTAMP,
    symbol SYMBOL CAPACITY 256 CACHE,
    open DOUBLE,
    high DOUBLE,
    low DOUBLE,  
    close DOUBLE,
    volume LONG,
    vwap DOUBLE,  -- Volume weighted average price
    trade_count LONG,
    
    INDEX(symbol) CAPACITY 256
)
TIMESTAMP(timestamp)
PARTITION BY DAY;

-- Latency tracking table for performance monitoring
CREATE TABLE IF NOT EXISTS latency_metrics (
    timestamp TIMESTAMP,
    metric_name SYMBOL CAPACITY 64 CACHE,  -- e.g., 'ingest_latency', 'ilp_write_latency'
    latency_us LONG,    -- Latency in microseconds
    p50_us LONG,        -- Rolling p50
    p95_us LONG,        -- Rolling p95  
    p99_us LONG,        -- Rolling p99
    sample_count LONG,
    
    INDEX(metric_name) CAPACITY 64
)
TIMESTAMP(timestamp)
PARTITION BY HOUR;

-- Session metadata for tracking data feed sessions
CREATE TABLE IF NOT EXISTS sessions (
    session_id SYMBOL CAPACITY 64 CACHE,
    start_time TIMESTAMP,
    end_time TIMESTAMP,
    source SYMBOL CAPACITY 32 CACHE,
    status SYMBOL CAPACITY 16 CACHE,  -- 'active', 'completed', 'error'
    total_ticks LONG,
    error_count LONG,
    last_heartbeat TIMESTAMP,
    
    INDEX(session_id) CAPACITY 64,
    INDEX(source) CAPACITY 32
)
TIMESTAMP(start_time)
PARTITION BY DAY;

-- Retention policy setup (manual for now, automated in production)
-- DROP PARTITION WHERE timestamp < dateadd('d', -90, now());

-- Performance optimization settings
-- These are applied at the database level in questdb.conf
-- cairo.commit.lag=1000
-- line.tcp.commit.interval.fraction=0.5
-- line.tcp.commit.interval.default=2000