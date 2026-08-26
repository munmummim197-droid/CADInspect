#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace stepcompare::cli {

struct Options final {
    std::string inputAUtf8{};
    std::string inputBUtf8{};
    double linearToleranceMm{0.01};
    double angularToleranceDegrees{0.01};
    bool deep{};
    std::optional<std::string> jsonOutputUtf8{};
    std::optional<std::string> csvOutputUtf8{};
};

enum class ArgumentErrorCode {
    None,
    MissingInput,
    TooManyInputs,
    UnknownOption,
    MissingOptionValue,
    DuplicateOption,
    InvalidTolerance,
    UnsupportedInputExtension,
    InvalidUtf8,
};

struct ParseResult final {
    std::optional<Options> options{};
    ArgumentErrorCode errorCode{ArgumentErrorCode::None};
    std::string message{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return options.has_value();
    }
};

// Arguments exclude argv[0]. Every string is required to contain valid UTF-8.
[[nodiscard]] ParseResult parseArguments(
    std::span<const std::string_view> arguments);

}  // namespace stepcompare::cli
