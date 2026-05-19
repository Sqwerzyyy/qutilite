# qutilite

`qutilite` is a compact C++20 console utility for fast local Monte Carlo risk simulation. It embeds a multithreaded GBM engine and keeps the CLI intentionally small: give it a ticker, let the defaults do the rest.

## Features

- C++20 local Monte Carlo engine with `std::jthread` chunk processing.
- `thread_local` `std::mt19937_64` and `std::normal_distribution` per worker.
- 64-byte aligned per-thread result buffers to reduce false sharing.
- Zero allocation inside the simulation hot loop.
- Short CLI flags with smart defaults via CLI11-style command setup.
- ANSI-colored ASCII output with VaR, standard deviation, and engine speed.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Run from the build directory or directly by path:

```bash
./build/qutilite run -t AAPL
```

In CLion, open this folder as a CMake project and select a Release profile for the intended `-O3 -march=native -ffast-math` build.

## CLI

Minimal run:

```bash
qutilite run -t AAPL
```

Defaults:

- simulations: `1,000,000`
- horizon: `30` days
- VaR confidence: `0.95`
- AI calibration: `off`

Full run:

```bash
qutilite run -t BTC -s 5000000 -d 90 --ai
```

Risk-focused run:

```bash
qutilite risk -t AAPL -c 0.99
```

Optional flags:

- `-t, --ticker`: ticker symbol.
- `-s, --sims`: simulation paths.
- `-d, --days`: horizon in days.
- `-c, --confidence`: VaR confidence level for `risk`.
- `--ai`: enable deterministic local AI calibration.

## Example Output

```text
+--------------------------------+----------------------------+
| qutilite                       | Monte Carlo GBM            |
+--------------------------------+----------------------------+
| Ticker                         | AAPL                       |
| Initial Price                  | $190.00                    |
| Expected Final Price           | $192.27                    |
| Value at Risk                  | $23.74                     |
| Standard Deviation             | $15.87                     |
| Engine Speed                   | 14.82 M paths/s            |
+--------------------------------+----------------------------+
| Simulations                    | 1,000,000                  |
| Horizon                        | 30 days                    |
| Confidence                     | 95.00%                     |
| AI Calibration                 | off                        |
| Elapsed                        | 0.0675 s                   |
+--------------------------------+----------------------------+
```

The executable, repository, and project are named `qutilite`.
