#include <stepcompare/cli/arguments.hpp>
#include <stepcompare/cli/exit_codes.hpp>

#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

int run(std::span<const std::string_view> arguments) {
    const auto parsed = stepcompare::cli::parseArguments(arguments);
    if (!parsed) {
        std::cerr << "Argument error: " << parsed.message << '\n'
                  << "Usage: stepcompare-cli A.step B.step "
                     "[--linear-tol mm] [--angular-tol degrees] [--deep] "
                     "[--json path] [--csv path]\n";
        return stepcompare::cli::exitCode(
            stepcompare::cli::ProcessOutcome::InvalidArguments);
    }

    // The coordinator will replace this fail-closed boundary when the
    // application composition root is connected. Never emit requested reports
    // or claim PASS without comparison evidence.
    std::cerr << "CHECK: comparison processing is unavailable because no "
                 "comparison coordinator is connected; no reports were written.\n";
    return stepcompare::cli::exitCode(
        stepcompare::cli::ProcessOutcome::CheckRequired);
}

#ifdef _WIN32
[[nodiscard]] bool appendUtf8(std::wstring_view value, std::string& output) {
    if (value.empty()) {
        output.clear();
        return true;
    }
    const auto required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return false;
    }
    output.resize(static_cast<std::size_t>(required));
    const auto converted = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        output.data(), required, nullptr, nullptr);
    return converted == required;
}
#endif

}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::string> storage;
    storage.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int index = 1; index < argc; ++index) {
        std::string converted;
        if (!appendUtf8(argv[index], converted)) {
            std::cerr << "Argument error: Windows argument conversion to UTF-8 "
                         "failed.\n";
            return stepcompare::cli::exitCode(
                stepcompare::cli::ProcessOutcome::InvalidArguments);
        }
        storage.push_back(std::move(converted));
    }
    std::vector<std::string_view> arguments;
    arguments.reserve(storage.size());
    for (const auto& argument : storage) {
        arguments.emplace_back(argument);
    }
    return run(arguments);
}
#else
int main(int argc, char* argv[]) {
    std::vector<std::string_view> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return run(arguments);
}
#endif
