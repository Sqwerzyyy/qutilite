#pragma once

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace CLI {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ParseError : public Error {
public:
    using Error::Error;
};

class CallForHelp : public Error {
public:
    using Error::Error;
};

class ValidationError : public Error {
public:
    using Error::Error;
};

class Option;

class PositiveNumberValidator {
public:
    std::string operator()(const std::string& value) const {
        char* end = nullptr;
        const double parsed = std::strtod(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0' || parsed <= 0.0) {
            return "value must be positive";
        }
        return {};
    }
};

inline const PositiveNumberValidator PositiveNumber{};

class Range {
public:
    Range(double low, double high) : low_(low), high_(high) {}

    std::string operator()(const std::string& value) const {
        char* end = nullptr;
        const double parsed = std::strtod(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0' || parsed < low_ || parsed > high_) {
            std::ostringstream stream;
            stream << "value must be in [" << low_ << ", " << high_ << ']';
            return stream.str();
        }
        return {};
    }

private:
    double low_;
    double high_;
};

class Option {
public:
    using Binder = std::function<void(const std::string&)>;
    using Validator = std::function<std::string(const std::string&)>;

    Option(std::vector<std::string> names, std::string description, std::string defaultText, Binder binder, bool flag)
        : names_(std::move(names)),
          description_(std::move(description)),
          defaultText_(std::move(defaultText)),
          binder_(std::move(binder)),
          flag_(flag) {}

    Option* required() {
        required_ = true;
        return this;
    }

    Option* capture_default_str() {
        showDefault_ = !defaultText_.empty();
        return this;
    }

    template <typename ValidatorType>
    Option* check(ValidatorType validator) {
        validator_ = [validator](const std::string& value) {
            return validator(value);
        };
        return this;
    }

    bool matches(const std::string& name) const {
        return std::find(names_.begin(), names_.end(), name) != names_.end();
    }

    bool isFlag() const {
        return flag_;
    }

    bool isRequired() const {
        return required_;
    }

    bool wasSeen() const {
        return seen_;
    }

    std::string displayName() const {
        return names_.empty() ? std::string{} : names_.front();
    }

    std::string namesForHelp() const {
        std::ostringstream stream;
        for (std::size_t index = 0; index < names_.size(); ++index) {
            if (index > 0) {
                stream << ", ";
            }
            stream << names_[index];
        }
        return stream.str();
    }

    const std::string& description() const {
        return description_;
    }

    std::string helpDescription() const {
        if (!showDefault_) {
            return description_;
        }
        return description_ + " (default: " + defaultText_ + ')';
    }

    void parse(const std::string& value) {
        if (validator_) {
            const std::string message = validator_(value);
            if (!message.empty()) {
                throw ValidationError(displayName() + ": " + message);
            }
        }
        binder_(value);
        seen_ = true;
    }

private:
    std::vector<std::string> names_;
    std::string description_;
    std::string defaultText_;
    Binder binder_;
    Validator validator_;
    bool flag_;
    bool required_ = false;
    bool seen_ = false;
    bool showDefault_ = false;
};

class App {
public:
    explicit App(std::string description = {}, std::string name = {})
        : description_(std::move(description)), name_(std::move(name)) {}

    App* add_subcommand(const std::string& name, const std::string& description = {}) {
        subcommands_.push_back(std::make_unique<App>(description, name));
        return subcommands_.back().get();
    }

    template <typename Target>
    Option* add_option(const std::string& names, Target& target, const std::string& description = {}) {
        options_.push_back(std::make_unique<Option>(
            splitNames(names),
            description,
            stringify(target),
            [&target](const std::string& value) {
                assignValue(value, target);
            },
            false
        ));
        return options_.back().get();
    }

    Option* add_flag(const std::string& names, bool& target, const std::string& description = {}) {
        options_.push_back(std::make_unique<Option>(
            splitNames(names),
            description,
            std::string{},
            [&target](const std::string&) {
                target = true;
            },
            true
        ));
        return options_.back().get();
    }

    void require_subcommand(int count) {
        requiredSubcommands_ = count;
    }

    void callback(std::function<void()> function) {
        callback_ = std::move(function);
    }

    void parse(int argc, char** argv) {
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(std::max(0, argc - 1)));
        for (int index = 1; index < argc; ++index) {
            args.emplace_back(argv[index]);
        }

        if (args.empty()) {
            if (requiredSubcommands_ > 0) {
                throw ParseError("subcommand required");
            }
            runCallback();
            return;
        }

        if (isHelpToken(args.front())) {
            throw CallForHelp(help());
        }

        if (!subcommands_.empty()) {
            App* command = findSubcommand(args.front());
            if (command == nullptr) {
                throw ParseError("unknown subcommand: " + args.front());
            }
            std::vector<std::string> commandArgs(args.begin() + 1, args.end());
            command->parseOptions(commandArgs);
            command->runCallback();
            return;
        }

        parseOptions(args);
        runCallback();
    }

    int exit(const Error& error) const {
        const bool helpRequested = dynamic_cast<const CallForHelp*>(&error) != nullptr;
        std::ostream& stream = helpRequested ? std::cout : std::cerr;
        stream << error.what() << '\n';
        if (!helpRequested) {
            stream << help();
        }
        return helpRequested ? 0 : 1;
    }

private:
    static std::vector<std::string> splitNames(const std::string& names) {
        std::vector<std::string> result;
        std::string current;
        for (char c : names) {
            if (c == ',') {
                if (!current.empty()) {
                    result.push_back(trim(current));
                    current.clear();
                }
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty()) {
            result.push_back(trim(current));
        }
        return result;
    }

    static std::string trim(std::string value) {
        const auto first = value.find_first_not_of(' ');
        const auto last = value.find_last_not_of(' ');
        if (first == std::string::npos) {
            return {};
        }
        return value.substr(first, last - first + 1U);
    }

    static bool isHelpToken(const std::string& token) {
        return token == "-h" || token == "--help";
    }

    template <typename Target>
    static std::string stringify(const Target& value) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    }

    template <typename Target>
    static void assignValue(const std::string& value, Target& target) {
        if constexpr (std::is_same_v<Target, std::string>) {
            target = value;
        } else if constexpr (std::is_integral_v<Target> && !std::is_same_v<Target, bool>) {
            assignInteger(value, target);
        } else if constexpr (std::is_floating_point_v<Target>) {
            assignFloating(value, target);
        } else {
            static_assert(std::is_same_v<Target, std::string>, "unsupported option type");
        }
    }

    template <typename Target>
    static void assignInteger(const std::string& value, Target& target) {
        using ParseType = std::conditional_t<std::is_signed_v<Target>, long long, unsigned long long>;
        ParseType parsed{};
        const auto* begin = value.data();
        const auto* end = value.data() + value.size();
        const auto result = std::from_chars(begin, end, parsed);
        if (result.ec != std::errc{} || result.ptr != end) {
            throw ParseError("invalid integer: " + value);
        }
        if (parsed > static_cast<ParseType>(std::numeric_limits<Target>::max())) {
            throw ParseError("integer out of range: " + value);
        }
        target = static_cast<Target>(parsed);
    }

    template <typename Target>
    static void assignFloating(const std::string& value, Target& target) {
        char* end = nullptr;
        const double parsed = std::strtod(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0') {
            throw ParseError("invalid number: " + value);
        }
        target = static_cast<Target>(parsed);
    }

    void parseOptions(const std::vector<std::string>& args) {
        for (std::size_t index = 0; index < args.size(); ++index) {
            const std::string& token = args[index];
            if (isHelpToken(token)) {
                throw CallForHelp(help());
            }

            Option* option = findOption(token);
            if (option == nullptr) {
                throw ParseError("unknown option: " + token);
            }

            if (option->isFlag()) {
                option->parse("true");
                continue;
            }

            if (index + 1U >= args.size()) {
                throw ParseError("missing value for option: " + token);
            }
            option->parse(args[++index]);
        }

        for (const auto& option : options_) {
            if (option->isRequired() && !option->wasSeen()) {
                throw ParseError("required option missing: " + option->displayName());
            }
        }
    }

    App* findSubcommand(const std::string& name) const {
        for (const auto& command : subcommands_) {
            if (command->name_ == name) {
                return command.get();
            }
        }
        return nullptr;
    }

    Option* findOption(const std::string& name) const {
        for (const auto& option : options_) {
            if (option->matches(name)) {
                return option.get();
            }
        }
        return nullptr;
    }

    void runCallback() {
        if (callback_) {
            callback_();
        }
    }

    std::string help() const {
        std::ostringstream stream;
        stream << "Usage: qutilite";
        if (!name_.empty()) {
            stream << ' ' << name_;
        }
        if (!subcommands_.empty()) {
            stream << " <subcommand>";
        }
        stream << " [options]\n";
        if (!description_.empty()) {
            stream << description_ << '\n';
        }

        if (!subcommands_.empty()) {
            stream << "Subcommands:\n";
            for (const auto& command : subcommands_) {
                stream << "  " << std::left << std::setw(12) << command->name_ << command->description_ << '\n';
            }
        }

        if (!options_.empty()) {
            stream << "Options:\n";
            stream << "  " << std::left << std::setw(20) << "-h, --help" << "Show help\n";
            for (const auto& option : options_) {
                stream << "  " << std::left << std::setw(20) << option->namesForHelp() << option->helpDescription() << '\n';
            }
        }
        return stream.str();
    }

    std::string description_;
    std::string name_;
    std::vector<std::unique_ptr<Option>> options_;
    std::vector<std::unique_ptr<App>> subcommands_;
    std::function<void()> callback_;
    int requiredSubcommands_ = 0;
};

}  // namespace CLI

#define CLI11_PARSE(app, argc, argv) \
    do { \
        try { \
            (app).parse((argc), (argv)); \
        } catch (const ::CLI::Error& error) { \
            return (app).exit(error); \
        } \
    } while (false)
