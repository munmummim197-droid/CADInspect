#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace stepcompare::scheduler {

struct WorkerBudgetInput final {
    std::size_t logicalProcessors{};
    std::uint64_t availableBytes{};
    std::uint64_t reserveBytes{};
    std::uint64_t estimatedBytesPerWorker{};
    std::size_t configuredMaximum{};
};

// Selects a bounded worker count from CPU and current memory headroom. One
// worker remains available under pressure so the coordinator can continue in
// serialized batches rather than allocating an unbounded task set.
[[nodiscard]] inline std::size_t chooseWorkerCount(
    const WorkerBudgetInput& input) {
    if (input.logicalProcessors == 0 || input.estimatedBytesPerWorker == 0 ||
        input.configuredMaximum == 0) {
        throw std::invalid_argument("Worker budget bounds must be positive");
    }
    const auto usableBytes = input.availableBytes > input.reserveBytes
                                 ? input.availableBytes - input.reserveBytes
                                 : 0;
    const auto byMemory = std::max<std::uint64_t>(
        1, usableBytes / input.estimatedBytesPerWorker);
    return std::min({input.logicalProcessors,
                     input.configuredMaximum,
                     static_cast<std::size_t>(std::min<std::uint64_t>(
                         byMemory,
                         static_cast<std::uint64_t>(
                             std::numeric_limits<std::size_t>::max()))) });
}

}  // namespace stepcompare::scheduler
