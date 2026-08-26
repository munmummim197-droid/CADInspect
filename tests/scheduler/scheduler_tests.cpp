#include <stepcompare/scheduler/bounded_task_scheduler.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void constructorRejectsInvalidBounds() {
    try {
        stepcompare::scheduler::BoundedTaskScheduler invalid(0, 1);
        expect(false, "zero workers must be rejected");
    } catch (const std::invalid_argument&) {
    }
}

void queueIsBounded() {
    stepcompare::scheduler::BoundedTaskScheduler scheduler(1, 1);
    std::promise<void> release;
    auto gate = release.get_future().share();
    std::promise<void> started;
    auto startedFuture = started.get_future();

    auto active = scheduler.trySubmit([&](std::stop_token) {
        started.set_value();
        gate.wait();
    });
    expect(active.has_value(), "first task must be accepted");
    if (!active) {
        return;
    }
    if (startedFuture.wait_for(2s) != std::future_status::ready) {
        expect(false, "first task must begin within the bounded timeout");
        release.set_value();
        return;
    }

    auto queued = scheduler.trySubmit([](std::stop_token) {});
    auto rejected = scheduler.trySubmit([](std::stop_token) {});
    expect(queued.has_value(), "one queued task must fit capacity");
    expect(!rejected.has_value(), "task beyond queue capacity must be rejected");

    release.set_value();
    active->get();
    queued->get();
}

void workerConcurrencyIsBounded() {
    stepcompare::scheduler::BoundedTaskScheduler scheduler(2, 8);
    std::atomic<int> running{0};
    std::atomic<int> peak{0};
    std::vector<std::future<void>> futures;

    for (int index = 0; index < 8; ++index) {
        auto submitted = scheduler.trySubmit([&](std::stop_token) {
            const auto current = running.fetch_add(1) + 1;
            auto observed = peak.load();
            while (current > observed &&
                   !peak.compare_exchange_weak(observed, current)) {
            }
            std::this_thread::sleep_for(20ms);
            running.fetch_sub(1);
        });
        expect(submitted.has_value(), "task within queue bound must be accepted");
        if (submitted) {
            futures.push_back(std::move(*submitted));
        }
    }
    for (auto& future : futures) {
        future.get();
    }
    expect(peak.load() <= 2, "active tasks must never exceed worker count");
}

void cancellationIsObservable() {
    stepcompare::scheduler::BoundedTaskScheduler scheduler(1, 2);
    std::promise<void> entered;
    std::atomic<bool> observed{false};

    auto future = scheduler.trySubmit([&](std::stop_token stopToken) {
        entered.set_value();
        while (!stopToken.stop_requested()) {
            std::this_thread::yield();
        }
        observed.store(true);
    });
    expect(future.has_value(), "cancellation probe task must be accepted");
    if (!future) {
        return;
    }
    if (entered.get_future().wait_for(2s) != std::future_status::ready) {
        expect(false, "cancellation probe must begin within bounded timeout");
        return;
    }
    scheduler.requestCancel();
    future->get();
    expect(observed.load(), "running task must observe cooperative cancellation");
}

}  // namespace

int main() {
    constructorRejectsInvalidBounds();
    queueIsBounded();
    workerConcurrencyIsBounded();
    cancellationIsObservable();

    if (failures != 0) {
        std::cerr << failures << " scheduler assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All scheduler tests passed\n";
    return EXIT_SUCCESS;
}
