#include "LocalSimulationEngine.hpp"
#include "TerminalVisualizer.hpp"

#include <CLI/CLI.hpp>

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    qutilite::SimulationConfig config;

    CLI::App app{"qutilite high-performance local Monte Carlo simulator"};
    app.require_subcommand(1);

    auto addCoreOptions = [&](CLI::App& command) {
        command.add_option("-t,--ticker", config.ticker, "Ticker symbol")->required();
        command.add_option("-s,--sims", config.simulations, "Simulation paths")
            ->capture_default_str()
            ->check(CLI::PositiveNumber);
        command.add_option("-d,--days", config.days, "Horizon in days")
            ->capture_default_str()
            ->check(CLI::PositiveNumber);
        command.add_flag("--ai", config.aiCalibration, "Enable AI calibration");
    };

    auto* run = app.add_subcommand("run", "Run Monte Carlo pricing simulation");
    addCoreOptions(*run);
    run->callback([&]() {
        config.mode = qutilite::CommandMode::Run;
    });

    auto* risk = app.add_subcommand("risk", "Run Value at Risk simulation");
    addCoreOptions(*risk);
    risk->add_option("-c,--confidence", config.confidence, "VaR confidence level")
        ->capture_default_str()
        ->check(CLI::Range(0.50, 0.999));
    risk->callback([&]() {
        config.mode = qutilite::CommandMode::Risk;
    });

    CLI11_PARSE(app, argc, argv);

    try {
        const qutilite::LocalSimulationEngine engine;
        const qutilite::TerminalVisualizer visualizer;
        const qutilite::SimulationResult result = engine.run(config);
        visualizer.print(result, std::cout);
    } catch (const std::exception& error) {
        std::cerr << "qutilite error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
