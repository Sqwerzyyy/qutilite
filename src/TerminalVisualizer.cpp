#include "TerminalVisualizer.hpp"

#include <array>
#include <cstddef>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace qutilite {
namespace {

constexpr std::string_view Reset = "\033[0m";
constexpr std::string_view Cyan = "\033[36m";
constexpr std::string_view Green = "\033[32m";
constexpr std::string_view Yellow = "\033[33m";
constexpr std::string_view Red = "\033[31m";
constexpr std::string_view Purple = "\033[95m";
constexpr std::string_view Bold = "\033[1m";

std::string money(double value) {
    std::ostringstream stream;
    stream << '$' << std::fixed << std::setprecision(2) << value;
    return stream.str();
}

std::string fixedNumber(double value, int precision) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string percent(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value * 100.0 << '%';
    return stream.str();
}

std::string integerWithCommas(std::uint64_t value) {
    std::string text = std::to_string(value);
    for (std::ptrdiff_t position = static_cast<std::ptrdiff_t>(text.size()) - 3; position > 0; position -= 3) {
        text.insert(static_cast<std::size_t>(position), ",");
    }
    return text;
}

std::string riskLabel(const SimulationResult& result) {
    const double riskRatio = result.initialPrice > 0.0 ? result.valueAtRisk / result.initialPrice : 0.0;
    if (riskRatio >= 0.12) {
        return std::string(Red);
    }
    if (riskRatio >= 0.06) {
        return std::string(Yellow);
    }
    return std::string(Green);
}

void border(std::ostream& out, int labelWidth, int valueWidth) {
    out << Cyan << '+'
        << std::string(static_cast<std::size_t>(labelWidth + 2), '-')
        << '+'
        << std::string(static_cast<std::size_t>(valueWidth + 2), '-')
        << '+'
        << Reset << '\n';
}

void row(std::ostream& out, std::string_view label, const std::string& value, std::string_view color, int labelWidth, int valueWidth) {
    out << Cyan << '|' << Reset << ' '
        << std::left << std::setw(labelWidth) << label << ' '
        << Cyan << '|' << Reset << ' '
        << color << std::left << std::setw(valueWidth) << value << Reset << ' '
        << Cyan << '|' << Reset << '\n';
}

void bannerBorder(std::ostream& out, std::size_t width) {
    out << Purple << '+'
        << std::string(width + 2U, '-')
        << '+'
        << Reset << '\n';
}

void bannerLine(std::ostream& out, std::string_view text, std::size_t width) {
    out << Purple << '|' << Reset << ' '
        << Bold << Purple << std::left << std::setw(static_cast<int>(width)) << text << Reset << ' '
        << Purple << '|' << Reset << '\n';
}

void printBanner(std::ostream& out) {
    constexpr std::size_t width = 78;
    constexpr std::array<std::string_view, 8> lines = {
        "   ####   ##   ##  ########  ####  ##        ####  ########  ########",
        "  ##  ##  ##   ##     ##      ##   ##         ##      ##     ##      ",
        "  ##  ##  ##   ##     ##      ##   ##         ##      ##     ######  ",
        "  ##  ##  ##   ##     ##      ##   ##         ##      ##     ##      ",
        "   #####   #####      ##     ####  ########  ####     ##     ########",
        "       ##                                                               ",
        "  GitHub : https://github.com/Sqwerzyyy",
        "  Repo   : https://github.com/Sqwerzyyy/qutilite"
    };

    bannerBorder(out, width);
    for (std::string_view line : lines) {
        bannerLine(out, line, width);
    }
    bannerBorder(out, width);
    out << '\n';
}

}  // namespace

void TerminalVisualizer::print(const SimulationResult& result, std::ostream& out) const {
    constexpr int labelWidth = 30;
    constexpr int valueWidth = 26;

    printBanner(out);
    border(out, labelWidth, valueWidth);
    row(out, "qutilite", "Monte Carlo GBM", Bold, labelWidth, valueWidth);
    border(out, labelWidth, valueWidth);
    row(out, "Ticker", result.ticker, Green, labelWidth, valueWidth);
    row(out, "Initial Price", money(result.initialPrice), Green, labelWidth, valueWidth);
    row(out, "Expected Final Price", money(result.expectedFinalPrice), Green, labelWidth, valueWidth);
    row(out, "Value at Risk", money(result.valueAtRisk), riskLabel(result), labelWidth, valueWidth);
    row(out, "Standard Deviation", money(result.standardDeviation), Yellow, labelWidth, valueWidth);
    row(out, "Engine Speed", fixedNumber(result.throughputMillionPathsPerSecond, 2) + " M paths/s", Green, labelWidth, valueWidth);
    border(out, labelWidth, valueWidth);
    row(out, "Simulations", integerWithCommas(result.simulations), Cyan, labelWidth, valueWidth);
    row(out, "Horizon", std::to_string(result.days) + " days", Cyan, labelWidth, valueWidth);
    row(out, "Confidence", percent(result.confidence), Cyan, labelWidth, valueWidth);
    row(out, "AI Calibration", result.aiCalibration ? "on" : "off", result.aiCalibration ? Green : Yellow, labelWidth, valueWidth);
    row(out, "Elapsed", fixedNumber(result.elapsedSeconds, 4) + " s", Cyan, labelWidth, valueWidth);
    border(out, labelWidth, valueWidth);
}

}  // namespace qutilite
