#include <stepcompare/scheduler/bounded_task_scheduler.hpp>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace stepcompare::scheduler {

struct BoundedTaskScheduler::Impl final {
    std::size_t workers{};
    std::size_t capacity{};
    std::mutex mutex{};
    std::condition_variable_any available{};
    std::deque<std::shared_ptr<std::packaged_task<void(std::stop_token)>>> queue{};
    bool accepting{true};
    std::vector<std::jthread> threads{};
};

BoundedTaskScheduler::BoundedTaskScheduler(std::size_t workerCount,
                                           std::size_t queueCapacity)
    : impl_(std::make_unique<Impl>()) {
    if (workerCount == 0 || queueCapacity == 0) {
        throw std::invalid_argument("Scheduler bounds must be positive");
    }
    impl_->workers = workerCount;
    impl_->capacity = queueCapacity;
    impl_->threads.reserve(workerCount);
    for (std::size_t index = 0; index < workerCount; ++index) {
        impl_->threads.emplace_back([state = impl_.get()](std::stop_token stopToken) {
            for (;;) {
                std::shared_ptr<std::packaged_task<void(std::stop_token)>> task;
                {
                    std::unique_lock lock(state->mutex);
                    state->available.wait(lock, stopToken, [state] {
                        return !state->queue.empty();
                    });
                    if (state->queue.empty()) {
                        if (stopToken.stop_requested()) {
                            return;
                        }
                        continue;
                    }
                    task = std::move(state->queue.front());
                    state->queue.pop_front();
                }
                (*task)(stopToken);
            }
        });
    }
}

BoundedTaskScheduler::~BoundedTaskScheduler() {
    requestCancel();
    impl_->threads.clear();
}

std::optional<std::future<void>> BoundedTaskScheduler::trySubmit(Task task) {
    if (!task) {
        return std::nullopt;
    }
    auto packaged =
        std::make_shared<std::packaged_task<void(std::stop_token)>>(std::move(task));
    auto future = packaged->get_future();
    {
        std::lock_guard lock(impl_->mutex);
        if (!impl_->accepting || impl_->queue.size() >= impl_->capacity) {
            return std::nullopt;
        }
        impl_->queue.push_back(std::move(packaged));
    }
    impl_->available.notify_one();
    return future;
}

void BoundedTaskScheduler::requestCancel() noexcept {
    {
        std::lock_guard lock(impl_->mutex);
        impl_->accepting = false;
    }
    for (auto& worker : impl_->threads) {
        worker.request_stop();
    }
    impl_->available.notify_all();
}

std::size_t BoundedTaskScheduler::workerCount() const noexcept {
    return impl_->workers;
}

std::size_t BoundedTaskScheduler::queueCapacity() const noexcept {
    return impl_->capacity;
}

}  // namespace stepcompare::scheduler
