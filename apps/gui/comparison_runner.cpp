#include "comparison_runner.hpp"

#include <stepcompare/cache/file_identity.hpp>

#include <QFuture>
#include <QPromise>
#include <QString>
#include <QtConcurrentRun>

#include <filesystem>
#include <utility>

namespace stepcompare::gui {
namespace {

std::filesystem::path pathFromUtf8(const std::u8string& value) {
    return std::filesystem::path(value);
}

void runComparison(
    QPromise<stepcompare::application::ComparisonResult>& promise,
    stepcompare::application::ComparisonCoordinator* coordinator,
    stepcompare::application::ComparisonRequest request) {
    promise.setProgressRange(0, 100);
    request.identityA = stepcompare::cache::computeFileIdentity(
        pathFromUtf8(request.inputAUtf8));
    request.identityB = stepcompare::cache::computeFileIdentity(
        pathFromUtf8(request.inputBUtf8));
    request.progress = [&promise](
                           const stepcompare::application::ComparisonProgress& progress) {
        const auto percent = static_cast<int>(
            progress.totalStages == 0U
                ? 0U
                : (progress.completedStages * 100U) / progress.totalStages);
        promise.setProgressValue(percent);
    };
    if (promise.isCanceled()) {
        return;
    }
    auto result = coordinator->compare(request);
    promise.setProgressValue(100);
    promise.addResult(std::move(result));
}

}  // namespace

ComparisonRunner::ComparisonRunner(StatusHandler statusHandler,
                                   ResultHandler resultHandler,
                                   QObject* parent)
    : QObject(parent),
      statusHandler_(std::move(statusHandler)),
      resultHandler_(std::move(resultHandler)),
      coordinator_(importer_,
                   deepGeometry_,
                   &surfaceDeviation_,
                   &featureRecognition_) {
    connect(&watcher_,
            &QFutureWatcher<stepcompare::application::ComparisonResult>::started,
            this,
            [this] {
                if (statusHandler_) {
                    statusHandler_(0, "Comparison started");
                }
            });
    connect(&watcher_,
            &QFutureWatcher<stepcompare::application::ComparisonResult>::progressValueChanged,
            this,
            [this](const int percent) {
                if (statusHandler_) {
                    statusHandler_(percent, "Comparing canonical evidence");
                }
            });
    connect(&watcher_,
            &QFutureWatcher<stepcompare::application::ComparisonResult>::finished,
            this,
            [this] {
                const QFuture<stepcompare::application::ComparisonResult> future =
                    watcher_.future();
                if (future.resultCount() == 0) {
                    if (statusHandler_) {
                        statusHandler_(100, "Comparison cancelled");
                    }
                    return;
                }
                if (resultHandler_) {
                    resultHandler_(future.result());
                }
            });
}

ComparisonRunner::~ComparisonRunner() {
    if (watcher_.isRunning()) {
        stopSource_.request_stop();
        watcher_.cancel();
        watcher_.waitForFinished();
    }
}

bool ComparisonRunner::start(
    stepcompare::application::ComparisonRequest request) {
    if (busy()) {
        return false;
    }
    stopSource_ = std::stop_source{};
    request.cancellation = stopSource_.get_token();
    watcher_.setFuture(QtConcurrent::run(
        runComparison, &coordinator_, std::move(request)));
    return true;
}

bool ComparisonRunner::cancel() {
    if (!busy()) {
        return false;
    }
    const bool requested = stopSource_.request_stop();
    watcher_.cancel();
    return requested;
}

bool ComparisonRunner::busy() const noexcept {
    return watcher_.isRunning();
}

}  // namespace stepcompare::gui
