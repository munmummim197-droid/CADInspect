#pragma once

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stop_token>

namespace stepcompare::scheduler {

class BoundedTaskScheduler final {
public:
    using Task = std::function<void(std::stop_token)>;

    BoundedTaskScheduler(std::size_t workerCount, std::size_t queueCapacity);
    ~BoundedTaskScheduler();

    BoundedTaskScheduler(const BoundedTaskScheduler&) = delete;
    BoundedTaskScheduler& operator=(const BoundedTaskScheduler&) = delete;
    BoundedTaskScheduler(BoundedTaskScheduler&&) = delete;
    BoundedTaskScheduler& operator=(BoundedTaskScheduler&&) = delete;

    [[nodiscard]] std::optional<std::future<void>> trySubmit(Task task);
    void requestCancel() noexcept;

    [[nodiscard]] std::size_t workerCount() const noexcept;
    [[nodiscard]] std::size_t queueCapacity() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace stepcompare::scheduler

