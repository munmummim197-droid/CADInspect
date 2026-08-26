#include <stepcompare/scheduler/progress_tracker.hpp>

#include <algorithm>

namespace stepcompare::scheduler {

void ProgressTracker::begin(const ProgressPhase phase,
                            const std::uint64_t total) noexcept {
    std::scoped_lock lock(mutex_);
    phase_ = phase;
    processed_ = 0;
    total_ = total;
    started_ = std::chrono::steady_clock::now();
    stopSource_ = std::stop_source{};
}

void ProgressTracker::advance(const std::uint64_t amount) noexcept {
    std::scoped_lock lock(mutex_);
    processed_ = std::min(total_, processed_ +
                                      std::min(amount, total_ - processed_));
}

void ProgressTracker::complete() noexcept {
    std::scoped_lock lock(mutex_);
    processed_ = total_;
    phase_ = ProgressPhase::Complete;
}

void ProgressTracker::requestCancel() noexcept {
    std::scoped_lock lock(mutex_);
    stopSource_.request_stop();
}

bool ProgressTracker::stopRequested() const noexcept {
    std::scoped_lock lock(mutex_);
    return stopSource_.stop_requested();
}

std::stop_token ProgressTracker::stopToken() const noexcept {
    std::scoped_lock lock(mutex_);
    return stopSource_.get_token();
}

ProgressSnapshot ProgressTracker::snapshot() const noexcept {
    std::scoped_lock lock(mutex_);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_);
    return {.phase = phase_,
            .processed = processed_,
            .total = total_,
            .elapsedMilliseconds = elapsed.count(),
            .cancellationRequested = stopSource_.stop_requested()};
}

}  // namespace stepcompare::scheduler
