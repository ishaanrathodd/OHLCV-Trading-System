# Ultra‑Low‑Latency Trading MVP — **Qt Quick + Shared‑Memory Edition**

> **Agentic IDE Build Script** — follow steps linearly, phase by phase. This doc keeps the original playbook’s low‑latency research, SLOs, and CI/perf gates intact, but replaces the React frontend with a *Qt Quick (QML + C++)* UI and makes **shared memory** (lock‑free rings) the primary UI IPC.

---

## Purpose & Constraints

**Mission:** Build a private trading MVP for Indian markets that meets stringent latency targets from your research playbook: **p99 end‑to‑end UI < 250 ms**, **p99 tick‑to‑signal < 5 ms**, while following regulatory guardrails (no public live data, retain logs, etc.).

**Constraints carried from the playbook:**

* ILP (QuestDB) for writes, PostgreSQL protocol for read queries.
* Python is orchestration/analytics only; C++ implements time‑critical paths.
* CI must include latency gates and fail on regressions >5%.
* Use synthetic/sandbox feeds for demos.

**Primary deviations from the original file:**

1. Replace React frontend with **Qt Quick (QML + C++)** using Qt Charts / Qt Graphs for visuals.
2. Make **shared memory + lock‑free ring buffers** the primary mechanism for UI updates. WebSockets/BFF remain optional fallbacks.

(Original research playbook is the authoritative source for low‑latency design and must be followed for engine, IO, QuestDB, and compliance.)

---

## Repo Layout (single authoritative repo)

```
/
├── docs/
│   ├── adr/
│   └── ARCHITECTURE.md   # this file (Qt Quick + Shared‑Memory Edition)
├── src/
│   ├── engine/           # C++: ingest, core, sbe, ring buffers
│   ├── ui/               # Qt Quick app (qml + cpp) + shared‑mem bridge
│   ├── analytics/        # Python: backtests, notebooks
│   ├── ops/              # scripts to run QuestDB, metrics, simulate feeds
│   └── tools/            # perf harness, tracing utilities
├── db/questdb/
├── tests/
└── .github/workflows/
```

---

## Phase 1 — Foundation & Data Integration

**Goal:** Establish reproducible development environment, get sample market data flowing into QuestDB via ILP, and prove ingest SLO.

### Tasks (agent)

* Provision dev VM with `clang`, `cmake`, `python3.11`, `qt6`, `perf`, `hugepages` and `mlockall` enabled.
* Implement `src/engine/ingest/` C++ connector for broker WebSocket (reference: Kite Connect binary frames). Provide two modes: live (if key) and *replay* from tick files.
* Produce a synthetic Hawkes generator (Python) for stress testing.
* Deploy QuestDB locally; create `trades` table with `timestamp` as designated ts and `PARTITION BY DAY`.
* Implement an ILP writer service that batches and writes ticks to QuestDB (WAL + DEDUP UPSERT keys: `timestamp,symbol`).

**Acceptance:** Replayed feed → QuestDB rowcount verified; ingest **p99 < 60 ms** under normal ingestion load.

---

## Phase 2 — C++ Core Engine (low‑latency heart)

**Goal:** Implement the lock‑free ingestion fanout and C++ trading engine with strict memory discipline.

### Design & Tasks

* Implement LMAX Disruptor‑style single‑writer multi‑reader ring buffer(s) in `src/engine/ring/`. Provide an `IProducer` and `IConsumer` API with checkpointing for replay.
* Threading rules: pin threads, assign a CPU core set per thread group (io, engine, ilp\_writer, shm\_publisher). Use `numactl` aware allocation.
* Memory: pre‑allocate arenas via `std::pmr::monotonic_buffer_resource`. `mlockall()` at startup; enable huge pages. Forbid runtime `new` in hot paths.
* Networking: prefer `io_uring` multishot for broker sockets; fallback to efficient `epoll` implementation.
* Serialization: use SBE for wire format and logging (schema in `src/engine/sbe/`).
* Provide `shm_publisher` which serializes immutable snapshots/deltas into a lock‑free shared‑memory ring for UI consumption.

**Acceptance:** Under stress harness, tick → signal processing **p99 < 5 ms**; no allocator syscalls in hot path (verified via perf and allocator hooks).

---

## Phase 3 — QuestDB & Persistence

**Goal:** Robust, idempotent persistence with efficient schema and retention.

### Tasks

* Finalize `trades` schema and implement ILP batching/flush policies (size/time thresholds).
* Implement dedup/upssert semantics with WAL. Add partition management job to prune older partitions (policy: drop >90 days).
* Expose read adapter: PostgreSQL wire compatible read endpoints for the UI worker (via QPSQL in Qt). Optionally provide a FastAPI BFF for remote tools.

**Acceptance:** Sustained ILP ingestion with **p99 < 60 ms**, WAL lag metrics in Prometheus within acceptable thresholds.

---

## Phase 4 — Qt Quick UI + Shared Memory (primary UI path)

**Goal:** Create a high‑throughput, responsive Qt Quick UI that subscribes to the engine via shared memory and paints efficiently using Qt Charts / Qt Graphs.

### High‑level choices (rationale)

* **Qt Quick + C++**: hardware‑accelerated scene graph; declarative UI for fast iteration; heavy computations & data storage remain in C++ models to avoid QML/JS GC pressure.
* **Shared Memory Ring (primary IPC)**: minimal latency for UI updates; use immutable snapshots or compact deltas. Provide a lock‑free design.
* **QWebSocket / BFF (fallback)**: for remote clients or when shared memory is not available.
* **Qt Graphs (if available)** or **Qt Charts** for rendering; preallocate buffers and update in batches.

### UI Project layout

```
src/ui/
├── qtapp/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── qml/                 # Main.qml and components
│   ├── models/              # C++ QAbstract models
│   ├── shmbridge/           # shared‑mem ring client
│   ├── wsbridge/            # optional QWebSocket client
│   └── workers/             # SQL/history workers
```

### Detailed tasks for agentic IDE

1. **Scaffold Qt Quick app** with `QQmlApplicationEngine` + `Main.qml` shell (Market Watch, Chart, Order Entry, Latency HUD). Register C++ models to QML.
2. **Implement `ShmUiFeed`**: consumer for the engine’s shared‑memory ring. Provide safe mapping and a lock‑free consumer pointer. Deliver *immutable* snapshots into C++ models via queued signal/slot.
3. **Models in C++**:

   * `OrderBookModel : QAbstractTableModel` (fixed N levels per side, contiguous memory buffer, batched `dataChanged` updates).
   * `TradesModel : QAbstractListModel` (ring buffer backed, bounded size, batched `rowsInserted`).
   * `LatencyModel` maintaining histogram buckets for p50/p95/p99 (ring histogram client side).
4. **Coalescing & Backpressure**:

   * Maintain per‑symbol latest snapshot. A `QTimer` at \~16ms drains latest snapshots to models. If backlog grows > threshold, drop older frames and keep latest.
5. **Charts**:

   * Use `QXYSeries` or `QCandlestickSeries`. Pre‑allocate capacity and update via batched `replace()` or `append()` calls. Keep visible points bounded (e.g., 2–5k) and slide window instead of redrawing all.
6. **Historical Worker**:

   * `QThread` + `QSqlDatabase(QPSQL)` to perform async queries to QuestDB for OHLC/aggregations. Emit results to chart series as ready.
7. **Order Entry & Risk Checks**:

   * Local pre‑trade checks (price band, notional, margin approximation). On submit, send to engine via shm command queue or ZeroMQ to the broker simulator/engine.
8. **Latency HUD & Telemetry**:

   * Pull timestamps inserted by engine (`t_ingest`, `t_engine`, `t_shm_publish`) and compute ui\_recv and paint deltas. Maintain local ring histograms and show rolling p50/p95/p99.
9. **Fallback Path**:

   * Implement `WebSocketUiFeed` to connect to BFF for environments where shared memory is unavailable (remote usage, demos).

**UI Performance Rules**

* No heavy logic in QML JS; keep QML as thin view layer.
* Avoid QVariant churn; precompute role data in plain arrays and expose read‑only views.
* Batched model updates; minimize signals/slots per tick.

**Acceptance:** UI demonstrates **p99 < 250 ms** tick→paint under burst profiles and no GUI thread blocking; sustained 60 fps under realistic load.

---

## Phase 5 — Broker Simulator & E2E Validation

**Goal:** Deterministic matching engine for integration tests, and a comprehensive E2E stress harness.

### Tasks

* Implement small matching engine with price‑time priority and nine timestamp checkpoints across lifecycle.
* Create scenario scripts: pre‑open burst, macro news spike, continuous normal day. Use Hawkes generator for realistic clustering.
* Run E2E tests: measure histograms (ingest, engine, shm publish, ui recv, paint). Assert SLOs. CI must fail on regressions >5%.

**Acceptance:** All SLOs meet targets across defined scenarios.

---

## Observability, CI & Perf Gates

* **Metrics**: Expose Prometheus metrics in engine and ILP writer: `ingest_duration_seconds`, `engine_duration_seconds`, `shm_publish_duration_seconds`, `ui_paint_duration_seconds`. Use histogram buckets around SLOs.
* **Tracing**: OpenTelemetry spans for each step; correlate via trace IDs placed in messages for sampling tail events.
* **CI**: Jobs include sanitizers (ASan/TSan/UBSan), unit tests, and a performance replay that must assert p50/p99 targets. CI should capture flamegraphs and artifacts.

---

## Security & Compliance (unchanged from playbook)

* No public displays of live provider data. Use synthetic/sandbox feeds for demos.
* Retain API usage logs for 5 years.
* Respect provider rate limits and licensing.
* Enforce pre‑trade risk gates client‑side and server‑side.

---

## Agent Prompts (PROMPT\_LIBRARY.md snippets)

* **`generate-qt-skeleton`**: "Create a Qt Quick app scaffold (CMake) with Main.qml, MarketWatch, Chart, OrderEntry, LatencyHud. Register OrderBookModel and TradesModel C++ types. Provide a `ShmUiFeed` stub that pushes synthetic ticks."
* **`impl-shm-ring`**: "Implement a lock‑free shared memory ring buffer for producer (engine) and consumer (ui) with head/tail index and wraparound safety. Add a small C ABI shim for mapping from engine process."
* **`engine-io-uring`**: "Implement io\_uring multishot receiver for broker WebSocket with microbench harness; baseline vs epoll."
* **`ilp-writer`**: "Implement batched ILP writer with WAL-safe dedup and partition routine. Expose metrics (batch\_size, flush\_time)."
* **`e2e-stress`**: "Create harness to replay scenarios: pre-open burst, macro spike, hawkes synthetic. Collect histograms and flamegraphs. Fail if SLOs breached."

---

## Deliverables & Acceptance Matrix

| Phase |               Deliverable | Acceptance Criteria                                        |
| ----- | ------------------------: | ---------------------------------------------------------- |
| 1     |          Ingest + QuestDB | Replayed feed ingestion verified; ingest p99 < 60 ms       |
| 2     | C++ engine + ring buffers | Tick→signal p99 < 5 ms; no runtime allocations in hot path |
| 3     |               ILP & reads | WAL stable; partition pruning; QPSQL reads work            |
| 4     |               Qt Quick UI | UI p99 < 250 ms; stable 60 fps under burst                 |
| 5     |                 E2E + sim | All SLOs pass across scenarios; CI gates pass              |

---

## Appendix — Implementation Hints

* **Shared memory ring**: store sequence numbers and compact deltas. Protect with single writer semantics and atomic index writes. Map via `mmap` with `MAP_SHARED`. Expose a small C shim for safe mapping by both processes.
* **QML performance**: keep JS minimal; call into C++ for heavy ops; use `QQuickWindow::beforeRendering()` hooks for timed flushes if necessary.
* **Charts**: preallocate series capacity; update in batch with `replace()` to minimize object churn.
* **Profiling**: use `perf`, `pprof` (via C++), and `FlameGraph` artifacts in CI.

---

**End of spec.**

Follow this file strictly, step by step. The engine, IO, and persistence layers should follow the original playbook’s low‑latency best practices—only the frontend approach and primary IPC choice have been updated as requested.