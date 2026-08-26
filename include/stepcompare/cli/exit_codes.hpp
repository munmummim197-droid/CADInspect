#pragma once

namespace stepcompare::cli {

enum class ProcessOutcome {
    Pass,
    DifferencesFound,
    CheckRequired,
    InvalidArguments,
    InputFailure,
    ProcessingFailure,
    Cancelled,
};

[[nodiscard]] constexpr int exitCode(ProcessOutcome outcome) noexcept {
    switch (outcome) {
    case ProcessOutcome::Pass:
        return 0;
    case ProcessOutcome::DifferencesFound:
        return 1;
    case ProcessOutcome::CheckRequired:
        return 2;
    case ProcessOutcome::InvalidArguments:
        return 3;
    case ProcessOutcome::InputFailure:
        return 4;
    case ProcessOutcome::ProcessingFailure:
        return 5;
    case ProcessOutcome::Cancelled:
        return 130;
    }
    return 5;
}

}  // namespace stepcompare::cli
