#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <stop_token>

namespace stepcompare::scheduler {

enum class ProgressPhase {
    Idle,
    Importing,
    AssemblyIndexing,
    Fingerprinting,
    Matching,
    PlacementAnalysis,
    DeepComparison,
    SurfaceDeviation,
    Reporting,
    Complete,
};

struct ProgressSnapshot final {
    ProgressPhase phase{ProgressPhase::Idle};
    std::uint64_t processed{};
    std::uint64_t total{};
    std::int64_t elapsedMilliseconds{};
    bool cancellationRequested{};
};

class ProgressTracker final {
public:
    void begin(ProgressPhase phase, std::uint64_t total) noexcept;
    void advance(std::uint64_t amount = 1) noexcept;
    void complete() noexcept;

    void requestCancel() noexcept;
    [[nodiscard]] bool stopRequested() const noexcept;
    [[nodiscard]] std::stop_token stopToken() const noexcept;
    [[nodiscard]] ProgressSnapshot snapshot() const noexcept;

private:
    mutable std::mutex mutex_;
    ProgressPhase phase_{ProgressPhase::Idle};
    std::uint64_t processed_{};
    std::uint64_t total_{};
    std::chrono::steady_clock::time_point started_{
        std::chrono::steady_clock::now()};
    std::stop_source stopSource_;
};

}  // namespace stepcompare::scheduler
