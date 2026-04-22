# Low-Latency HFT Tick-to-Trade Pipeline

This project implements a deterministic, sub-2 microsecond (300–600 CPU cycles) "tick-to-trade" pipeline in C++20.

## Architecture

- **Core 1**: Market Data Replay Engine (Producer)
- **Core 2**: Hot Path Engine (Consumer)
- **Queue**: Lock-free SPSC with cache-line padding to avoid false sharing.
- **LOB**: Array-based, L1-cache resident.
- **Strategy**: Division-free alpha signal using cross-product imbalance.
- **Risk**: Inline, branchless pre-trade checks.
- **Execution**: Inline simulator to avoid context switches.

## Performance Optimizations

1.  **Cache Locality**:
    - All hot data structures (`LOB`, `Tick`, `OrderRequest`) are `alignas(64)` and sized to fit in L1 (32KB).
    - Contiguous memory layout in the SPSC queue.
2.  **Lock-Free Design**:
    - SPSC queue uses `std::memory_order_acquire/release` for zero-lock synchronization.
    - Capacity is power-of-2, allowing `(i + 1) & (N - 1)` bitmasking instead of expensive modulo.
3.  **Branch Efficiency**:
    - Alpha signals and risk checks are designed to compile into `CMOV` (conditional move) instructions, avoiding pipeline flushes from mispredictions.
4.  **Tail Latency Control**:
    - `mlockall` (Linux) or pinning avoids page faults.
    - `_mm_pause()` in spin-loops prevents pipeline stalling and excessive power draw.
    - `rdtscp` for high-precision, serialized timestamping.

## Compilation

### Linux (Recommended)
```bash
make
./hft_simulation
```

### Windows (MinGW/GCC)
```bash
g++ -O3 -march=native -std=c++20 -Iinclude src/main.cpp -o hft_simulation.exe -pthread
./hft_simulation.exe
```

## System Tuning (Linux)

To achieve the lowest jitter, apply these kernel parameters:
- `isolcpus=1,2`: Isolate cores 1 and 2 from the OS scheduler.
- `nohz_full=1,2`: Disable timer interrupts on these cores.
- `rcu_nocbs=1,2`: Move RCU callbacks away from hot cores.

## Latency Measurement

We use `rdtscp` for cycle-level accuracy. The benchmarking harness subtracts the `rdtscp` overhead (calibrated at startup) to provide the purest "internal" latency metric.

Target Results (@ 3.0GHz+):
- **P50**: ~200-400 cycles
- **P99**: ~600-1200 cycles
- **P99.9**: <2000 cycles (<1µs)
