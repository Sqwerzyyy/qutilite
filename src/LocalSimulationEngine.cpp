#include "LocalSimulationEngine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <functional>
#include <limits>
#include <new>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace qutilite {
namespace {

template <typename T, std::size_t Alignment>
class AlignedAllocator {
public:
    using value_type = T;

    AlignedAllocator() noexcept = default;

    template <typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t count) {
        if (count == 0) {
            return nullptr;
        }
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
        return static_cast<T*>(::operator new(count * sizeof(T), std::align_val_t{Alignment}));
    }

    void deallocate(T* pointer, std::size_t) noexcept {
        ::operator delete(pointer, std::align_val_t{Alignment});
    }

    template <typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };
};

template <typename T, typename U, std::size_t Alignment>
bool operator==(const AlignedAllocator<T, Alignment>&, const AlignedAllocator<U, Alignment>&) noexcept {
    return true;
}

template <typename T, typename U, std::size_t Alignment>
bool operator!=(const AlignedAllocator<T, Alignment>&, const AlignedAllocator<U, Alignment>&) noexcept {
    return false;
}

using AlignedPrices = std::vector<double, AlignedAllocator<double, 64>>;

struct alignas(64) ThreadResultBuffer {
    AlignedPrices terminalPrices;
    double sum = 0.0;
    double sumSquares = 0.0;
    std::uint64_t count = 0;
};

std::string normalizeTicker(std::string ticker) {
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return ticker;
}

MarketProfile baseProfileFor(const std::string& ticker) {
    if (ticker == "AAPL") {
        return {ticker, 190.0, 0.10, 0.24};
    }
    if (ticker == "MSFT") {
        return {ticker, 430.0, 0.11, 0.22};
    }
    if (ticker == "NVDA") {
        return {ticker, 920.0, 0.16, 0.46};
    }
    if (ticker == "SPY") {
        return {ticker, 520.0, 0.07, 0.18};
    }
    if (ticker == "TSLA") {
        return {ticker, 175.0, 0.13, 0.55};
    }
    if (ticker == "BTC") {
        return {ticker, 65000.0, 0.20, 0.78};
    }
    if (ticker == "ETH") {
        return {ticker, 3200.0, 0.18, 0.85};
    }

    const auto hashValue = static_cast<std::uint64_t>(std::hash<std::string>{}(ticker));
    const double price = 40.0 + static_cast<double>(hashValue % 46'000U) / 100.0;
    const double drift = 0.04 + static_cast<double>((hashValue >> 8U) % 900U) / 10'000.0;
    const double volatility = 0.18 + static_cast<double>((hashValue >> 20U) % 45U) / 100.0;
    return {ticker, price, drift, volatility};
}

void applyAiCalibration(MarketProfile& profile) {
    const auto hashValue = static_cast<std::uint64_t>(std::hash<std::string>{}(profile.ticker));
    const double driftSignal = static_cast<double>(hashValue % 2'001U) / 1'000.0 - 1.0;
    const double volSignal = static_cast<double>((hashValue >> 18U) % 1'001U) / 1'000.0;

    profile.annualDrift = std::clamp(profile.annualDrift + driftSignal * 0.04, -0.15, 0.35);
    profile.annualVolatility = std::clamp(profile.annualVolatility * (0.90 + volSignal * 0.20), 0.08, 1.35);
}

std::uint64_t seedFor(const SimulationConfig& config, const MarketProfile& profile) {
    auto seed = 0x9E3779B97F4A7C15ULL;
    seed ^= static_cast<std::uint64_t>(std::hash<std::string>{}(profile.ticker));
    seed ^= config.simulations + 0xBF58476D1CE4E5B9ULL;
    seed ^= static_cast<std::uint64_t>(config.days) << 32U;
    seed ^= static_cast<std::uint64_t>(config.aiCalibration) << 7U;
    return seed;
}

void validateConfig(const SimulationConfig& config) {
    if (config.ticker.empty()) {
        throw std::invalid_argument("Ticker must not be empty");
    }
    if (config.simulations == 0) {
        throw std::invalid_argument("Simulation count must be positive");
    }
    if (config.days == 0) {
        throw std::invalid_argument("Horizon days must be positive");
    }
    if (config.confidence <= 0.0 || config.confidence >= 1.0) {
        throw std::invalid_argument("Confidence must be between 0 and 1");
    }
}

}  // namespace

SimulationResult LocalSimulationEngine::run(const SimulationConfig& config) const {
    validateConfig(config);

    const MarketProfile profile = resolveMarketProfile(config);
    const auto startedAt = std::chrono::steady_clock::now();
    const auto hardwareThreads = std::thread::hardware_concurrency();
    const std::size_t maxThreads = hardwareThreads == 0U ? 4U : static_cast<std::size_t>(hardwareThreads);
    const std::size_t threadCount = std::max<std::size_t>(
        1U,
        std::min<std::size_t>(maxThreads, static_cast<std::size_t>(config.simulations))
    );

    std::vector<std::uint64_t> chunkSizes(threadCount, config.simulations / threadCount);
    for (std::size_t index = 0; index < static_cast<std::size_t>(config.simulations % threadCount); ++index) {
        ++chunkSizes[index];
    }

    std::vector<ThreadResultBuffer> buffers(threadCount);
    for (std::size_t index = 0; index < threadCount; ++index) {
        buffers[index].terminalPrices.resize(static_cast<std::size_t>(chunkSizes[index]));
    }

    const double years = static_cast<double>(config.days) / 252.0;
    const double growth = (profile.annualDrift - 0.5 * profile.annualVolatility * profile.annualVolatility) * years;
    const double shockScale = profile.annualVolatility * std::sqrt(years);
    const std::uint64_t seedBase = seedFor(config, profile);

    {
        std::vector<std::jthread> workers;
        workers.reserve(threadCount);

        std::uint64_t offset = 0;
        for (std::size_t threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
            const std::uint64_t startPath = offset;
            const std::uint64_t pathCount = chunkSizes[threadIndex];
            offset += pathCount;

            workers.emplace_back([&, threadIndex, startPath, pathCount]() {
                // Thread loop entry
                thread_local std::mt19937_64 rng;
                thread_local std::normal_distribution<double> normal(0.0, 1.0);

                rng.seed(seedBase ^ (startPath + 0xD6E8FEB86659FD93ULL * (threadIndex + 1U)));
                normal.reset();

                auto& buffer = buffers[threadIndex];
                auto& prices = buffer.terminalPrices;
                double localSum = 0.0;
                double localSumSquares = 0.0;

                for (std::uint64_t path = 0; path < pathCount; ++path) {
                    const double z = normal(rng);
                    const double terminalPrice = profile.initialPrice * std::exp(growth + shockScale * z);
                    prices[static_cast<std::size_t>(path)] = terminalPrice;
                    localSum += terminalPrice;
                    localSumSquares += terminalPrice * terminalPrice;
                }

                buffer.sum = localSum;
                buffer.sumSquares = localSumSquares;
                buffer.count = pathCount;
            });
        }
    }

    AlignedPrices allPrices;
    allPrices.reserve(static_cast<std::size_t>(config.simulations));

    double totalSum = 0.0;
    double totalSumSquares = 0.0;
    std::uint64_t totalCount = 0;

    for (const auto& buffer : buffers) {
        allPrices.insert(allPrices.end(), buffer.terminalPrices.begin(), buffer.terminalPrices.end());
        totalSum += buffer.sum;
        totalSumSquares += buffer.sumSquares;
        totalCount += buffer.count;
    }

    const double mean = totalSum / static_cast<double>(totalCount);
    const double variance = std::max(0.0, totalSumSquares / static_cast<double>(totalCount) - mean * mean);
    const double standardDeviation = std::sqrt(variance);
    const double tailProbability = std::clamp(1.0 - config.confidence, 0.0, 1.0);
    const auto varIndex = static_cast<std::size_t>(
        std::floor(tailProbability * static_cast<double>(allPrices.size() - 1U))
    );

    std::nth_element(allPrices.begin(), allPrices.begin() + static_cast<std::ptrdiff_t>(varIndex), allPrices.end());
    const double downsideQuantile = allPrices[varIndex];
    const double valueAtRisk = std::max(0.0, profile.initialPrice - downsideQuantile);

    const auto finishedAt = std::chrono::steady_clock::now();
    const double elapsedSeconds = std::chrono::duration<double>(finishedAt - startedAt).count();
    const double throughput = elapsedSeconds > 0.0
        ? static_cast<double>(totalCount) / elapsedSeconds / 1'000'000.0
        : 0.0;

    return {
        profile.ticker,
        profile.initialPrice,
        mean,
        valueAtRisk,
        standardDeviation,
        throughput,
        elapsedSeconds,
        totalCount,
        config.days,
        config.confidence,
        config.aiCalibration
    };
}

MarketProfile LocalSimulationEngine::resolveMarketProfile(const SimulationConfig& config) const {
    MarketProfile profile = baseProfileFor(normalizeTicker(config.ticker));
    if (config.aiCalibration) {
        applyAiCalibration(profile);
    }
    return profile;
}

}  // namespace qutilite
