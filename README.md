# qutilite

![C++20](https://img.shields.io/badge/C%2B%2B-20-7f52ff?style=for-the-badge&logo=cplusplus)
![CMake](https://img.shields.io/badge/CMake-Release-7f52ff?style=for-the-badge&logo=cmake)
![CLI](https://img.shields.io/badge/CLI-Monte%20Carlo-7f52ff?style=for-the-badge)

`qutilite` is a fast C++20 command-line utility for local Monte Carlo risk simulation. It runs a multithreaded Geometric Brownian Motion engine directly inside the executable and keeps the UX simple: pass a ticker, and the tool fills in the rest with practical defaults.

GitHub profile: [github.com/Sqwerzyyy](https://github.com/Sqwerzyyy)

Repository: [github.com/Sqwerzyyy/qutilite](https://github.com/Sqwerzyyy/qutilite)

## Why qutilite

- Minimal CLI syntax for quick market experiments.
- Local C++20 simulation engine with no external runtime service.
- `std::jthread` worker chunks for multicore execution.
- `thread_local` random generators to avoid shared RNG contention.
- 64-byte aligned per-thread buffers to reduce false sharing.
- Zero allocation inside the hot simulation loop.
- ANSI terminal UI with a purple startup banner and structured risk table.

## Clone

Clone the repository from GitHub:

```bash
git clone https://github.com/Sqwerzyyy/qutilite.git
cd qutilite
```

If you already cloned it earlier and only see `LICENSE`, update your local copy:

```bash
git pull --ff-only origin main
```

## Build

Configure a Release build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

Compile:

```bash
cmake --build build -j
```

Run the binary:

```bash
./build/qutilite run -t AAPL
```

The CMake target is named `qutilite`. The release configuration uses compiler optimization flags such as `-O3`, `-march=native`, and `-ffast-math` on Clang/GCC-like compilers.

## CLion

1. Open CLion.
2. Choose `Get from VCS`.
3. Paste:

```text
https://github.com/Sqwerzyyy/qutilite.git
```

4. Open the cloned folder as a CMake project.
5. Select a `Release` CMake profile.
6. In Run/Debug Configurations choose the `qutilite` target.
7. Put CLI arguments into `Program arguments`, for example:

```text
run -t AAPL
```

## Commands

### Minimal simulation

```bash
qutilite run -t AAPL
```

Defaults used by `run`:

| Parameter | Default |
| --- | --- |
| simulations | `1,000,000` |
| horizon | `30` days |
| confidence | `0.95` |
| AI calibration | `off` |

### Full simulation

```bash
qutilite run -t BTC -s 5000000 -d 90 --ai
```

This runs `5,000,000` paths for `BTC`, uses a `90` day horizon, and enables deterministic local AI calibration.

### Risk command

```bash
qutilite risk -t AAPL -c 0.99
```

This computes Value at Risk at the `99%` confidence level.

## Flags

| Flag | Long form | Description |
| --- | --- | --- |
| `-t` | `--ticker` | Ticker symbol, required. |
| `-s` | `--sims` | Number of Monte Carlo paths. |
| `-d` | `--days` | Simulation horizon in trading days. |
| `-c` | `--confidence` | VaR confidence level for `risk`. |
| | `--ai` | Enables local AI-style calibration. |

## Example Output

The banner is rendered in purple in the terminal.

```text
+--------------------------------------------------------------------------------+
|    ####   ##   ##  ########  ####  ##        ####  ########  ########           |
|   ##  ##  ##   ##     ##      ##   ##         ##      ##     ##                 |
|   ##  ##  ##   ##     ##      ##   ##         ##      ##     ######             |
|   ##  ##  ##   ##     ##      ##   ##         ##      ##     ##                 |
|    #####   #####      ##     ####  ########  ####     ##     ########           |
|        ##                                                                        |
|   GitHub : https://github.com/Sqwerzyyy                                        |
|   Repo   : https://github.com/Sqwerzyyy/qutilite                               |
+--------------------------------------------------------------------------------+

+--------------------------------+----------------------------+
| qutilite                       | Monte Carlo GBM            |
+--------------------------------+----------------------------+
| Ticker                         | AAPL                       |
| Initial Price                  | $190.00                    |
| Expected Final Price           | $192.26                    |
| Value at Risk                  | $22.79                     |
| Standard Deviation             | $15.95                     |
| Engine Speed                   | 52.75 M paths/s            |
+--------------------------------+----------------------------+
| Simulations                    | 1,000,000                  |
| Horizon                        | 30 days                    |
| Confidence                     | 95.00%                     |
| AI Calibration                 | off                        |
| Elapsed                        | 0.0190 s                   |
+--------------------------------+----------------------------+
```

## Project Layout

```text
qutilite/
|-- CMakeLists.txt
|-- include/
|   |-- LocalSimulationEngine.hpp
|   |-- TerminalVisualizer.hpp
|   `-- Types.hpp
|-- src/
|   |-- LocalSimulationEngine.cpp
|   |-- TerminalVisualizer.cpp
|   `-- main.cpp
`-- third_party/
    `-- CLI11/
```

## Development Notes

The simulation core is intentionally local and deterministic for repeatable CLI experiments. Market profiles are embedded for common symbols such as `AAPL`, `MSFT`, `NVDA`, `SPY`, `TSLA`, `BTC`, and `ETH`; unknown tickers receive deterministic synthetic profiles from their symbol hash.

The project is designed to compile cleanly as C++20 with strict warnings enabled.
