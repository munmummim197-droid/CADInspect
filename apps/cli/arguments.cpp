#include <stepcompare/cli/arguments.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stepcompare::cli {
namespace {

[[nodiscard]] bool isContinuation(unsigned char value) noexcept {
    return value >= 0x80U && value <= 0xbfU;
}

[[nodiscard]] bool isValidUtf8(std::string_view value) noexcept {
    std::size_t index = 0;
    while (index < value.size()) {
        const auto first = static_cast<unsigned char>(value[index]);
        if (first <= 0x7fU) {
            ++index;
            continue;
        }
        if (first >= 0xc2U && first <= 0xdfU) {
            if (index + 1 >= value.size() ||
                !isContinuation(static_cast<unsigned char>(value[index + 1]))) {
                return false;
            }
            index += 2;
            continue;
        }
        if (first >= 0xe0U && first <= 0xefU) {
            if (index + 2 >= value.size()) {
                return false;
            }
            const auto second = static_cast<unsigned char>(value[index + 1]);
            const auto third = static_cast<unsigned char>(value[index + 2]);
            if (!isContinuation(third) ||
                (first == 0xe0U && (second < 0xa0U || second > 0xbfU)) ||
                (first == 0xedU && (second < 0x80U || second > 0x9fU)) ||
                (first != 0xe0U && first != 0xedU &&
                 !isContinuation(second))) {
                return false;
            }
            index += 3;
            continue;
        }
        if (first >= 0xf0U && first <= 0xf4U) {
            if (index + 3 >= value.size()) {
                return false;
            }
            const auto second = static_cast<unsigned char>(value[index + 1]);
            if ((first == 0xf0U && (second < 0x90U || second > 0xbfU)) ||
                (first == 0xf4U && (second < 0x80U || second > 0x8fU)) ||
                (first != 0xf0U && first != 0xf4U &&
                 !isContinuation(second)) ||
                !isContinuation(static_cast<unsigned char>(value[index + 2])) ||
                !isContinuation(static_cast<unsigned char>(value[index + 3]))) {
                return false;
            }
            index += 4;
            continue;
        }
        return false;
    }
    return true;
}

[[nodiscard]] ParseResult error(ArgumentErrorCode code, std::string message) {
    return {.options = std::nullopt,
            .errorCode = code,
            .message = std::move(message)};
}

[[nodiscard]] bool hasStepExtension(std::string_view path) noexcept {
    constexpr std::string_view step = ".step";
    constexpr std::string_view stp = ".stp";
    const auto endsWithIgnoringAsciiCase = [path](std::string_view suffix) {
        if (path.size() <= suffix.size()) {
            return false;
        }
        const auto offset = path.size() - suffix.size();
        for (std::size_t index = 0; index < suffix.size(); ++index) {
            auto character = path[offset + index];
            if (character >= 'A' && character <= 'Z') {
                character = static_cast<char>(character - 'A' + 'a');
            }
            if (character != suffix[index]) {
                return false;
            }
        }
        return true;
    };
    return endsWithIgnoringAsciiCase(step) || endsWithIgnoringAsciiCase(stp);
}

[[nodiscard]] bool parsePositiveDouble(std::string_view text,
                                       double& output) noexcept {
    double value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(),
                                        value, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        !std::isfinite(value) || value <= 0.0) {
        return false;
    }
    output = value;
    return true;
}

}  // namespace

ParseResult parseArguments(std::span<const std::string_view> arguments) {
    for (const auto argument : arguments) {
        if (!isValidUtf8(argument)) {
            return error(ArgumentErrorCode::InvalidUtf8,
                         "Every command-line argument must be valid UTF-8");
        }
    }

    Options options{};
    std::vector<std::string> positionalInputs;
    bool sawLinearTolerance = false;
    bool sawAngularTolerance = false;
    bool sawDeep = false;
    bool sawJson = false;
    bool sawCsv = false;

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto argument = arguments[index];
        const auto requireValue = [&]() -> std::optional<std::string_view> {
            if (index + 1 >= arguments.size() ||
                arguments[index + 1].starts_with("--")) {
                return std::nullopt;
            }
            ++index;
            return arguments[index];
        };

        if (argument == "--deep") {
            if (sawDeep) {
                return error(ArgumentErrorCode::DuplicateOption,
                             "--deep may only be specified once");
            }
            sawDeep = true;
            options.deep = true;
            continue;
        }

        if (argument == "--linear-tol" || argument == "--angular-tol") {
            auto& seen = argument == "--linear-tol" ? sawLinearTolerance
                                                     : sawAngularTolerance;
            if (seen) {
                return error(ArgumentErrorCode::DuplicateOption,
                             std::string(argument) +
                                 " may only be specified once");
            }
            seen = true;
            const auto value = requireValue();
            if (!value) {
                return error(ArgumentErrorCode::MissingOptionValue,
                             std::string(argument) + " requires a value");
            }
            auto& target = argument == "--linear-tol"
                               ? options.linearToleranceMm
                               : options.angularToleranceDegrees;
            if (!parsePositiveDouble(*value, target)) {
                return error(ArgumentErrorCode::InvalidTolerance,
                             std::string(argument) +
                                 " must be a finite number greater than zero");
            }
            continue;
        }

        if (argument == "--json" || argument == "--csv") {
            auto& seen = argument == "--json" ? sawJson : sawCsv;
            if (seen) {
                return error(ArgumentErrorCode::DuplicateOption,
                             std::string(argument) +
                                 " may only be specified once");
            }
            seen = true;
            const auto value = requireValue();
            if (!value || value->empty()) {
                return error(ArgumentErrorCode::MissingOptionValue,
                             std::string(argument) +
                                 " requires a non-empty output path");
            }
            if (argument == "--json") {
                options.jsonOutputUtf8 = std::string(*value);
            } else {
                options.csvOutputUtf8 = std::string(*value);
            }
            continue;
        }

        if (argument.starts_with("--")) {
            return error(ArgumentErrorCode::UnknownOption,
                         "Unknown option: " + std::string(argument));
        }

        positionalInputs.emplace_back(argument);
        if (positionalInputs.size() > 2) {
            return error(ArgumentErrorCode::TooManyInputs,
                         "Exactly two STEP input files are required");
        }
    }

    if (positionalInputs.size() < 2) {
        return error(ArgumentErrorCode::MissingInput,
                     "Exactly two STEP input files are required");
    }
    if (!hasStepExtension(positionalInputs[0]) ||
        !hasStepExtension(positionalInputs[1])) {
        return error(ArgumentErrorCode::UnsupportedInputExtension,
                     "Input files must use the .step or .stp extension");
    }

    options.inputAUtf8 = std::move(positionalInputs[0]);
    options.inputBUtf8 = std::move(positionalInputs[1]);
    return {.options = std::move(options),
            .errorCode = ArgumentErrorCode::None,
            .message = {}};
}

}  // namespace stepcompare::cli
