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

QString progressText(
    const stepcompare::application::ComparisonPhase phase) {
    using stepcompare::application::ComparisonPhase;
    switch (phase) {
    case ComparisonPhase::ImportA:
        return QStringLiteral("Đang nhập File A");
    case ComparisonPhase::ImportB:
        return QStringLiteral("Đang nhập File B");
    case ComparisonPhase::AssemblyIndex:
        return QStringLiteral("Đang lập chỉ mục Assembly");
    case ComparisonPhase::Matching:
        return QStringLiteral("Đang đối sánh và kiểm tra hình học");
    case ComparisonPhase::FeatureEvidence:
        return QStringLiteral("Đang phân tích Feature");
    case ComparisonPhase::Complete:
        return QStringLiteral("So sánh hoàn tất");
    }
    return QStringLiteral("Đang so sánh");
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
        promise.setProgressValueAndText(percent, progressText(progress.phase));
    };
    if (promise.isCanceled()) {
        return;
    }
    auto result = coordinator->compare(request);
    promise.setProgressValue(100);
    promise.addResult(std::move(result));
}

void runFeaturePair(
    QPromise<stepcompare::application::FeaturePairComparisonResult>& promise,
    stepcompare::application::ComparisonCoordinator* coordinator,
    stepcompare::application::FeaturePairComparisonRequest request) {
    promise.setProgressRange(0, 100);
    promise.setProgressValueAndText(5, QStringLiteral("Đang mở đúng cặp Part"));
    if (promise.isCanceled()) {
        return;
    }
    promise.setProgressValueAndText(
        35, QStringLiteral("Đang nhận diện và so sánh Feature của pair"));
    auto result = coordinator->compareFeaturePair(request);
    promise.setProgressValue(100);
    promise.addResult(std::move(result));
}

}  // namespace

ComparisonRunner::ComparisonRunner(StatusHandler statusHandler,
                                   ResultHandler resultHandler,
                                   FeaturePairResultHandler featurePairResultHandler,
                                   QObject* parent)
    : QObject(parent),
      statusHandler_(std::move(statusHandler)),
      resultHandler_(std::move(resultHandler)),
      featurePairResultHandler_(std::move(featurePairResultHandler)),
      coordinator_(importer_,
                   deepGeometry_,
                   &surfaceDeviation_,
                   &featureRecognition_) {
    connect(&watcher_,
            &QFutureWatcher<stepcompare::application::ComparisonResult>::started,
            this,
            [this] {
                if (statusHandler_) {
                    statusHandler_(0, "Bắt đầu so sánh");
                }
            });
    connect(&watcher_,
            &QFutureWatcher<stepcompare::application::ComparisonResult>::progressValueChanged,
            this,
            [this](const int percent) {
                if (statusHandler_) {
                    const QByteArray text = watcher_.progressText().toUtf8();
                    statusHandler_(
                        percent,
                        text.isEmpty()
                            ? std::string("Đang so sánh canonical evidence")
                            : text.toStdString());
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
                        statusHandler_(100, "Đã hủy so sánh");
                    }
                    return;
                }
                if (resultHandler_) {
                    resultHandler_(future.result());
                }
            });
    connect(&featurePairWatcher_,
            &QFutureWatcher<stepcompare::application::FeaturePairComparisonResult>::started,
            this,
            [this] {
                if (statusHandler_) {
                    statusHandler_(0, "Bắt đầu so sánh Feature của pair");
                }
            });
    connect(&featurePairWatcher_,
            &QFutureWatcher<stepcompare::application::FeaturePairComparisonResult>::progressValueChanged,
            this,
            [this](const int percent) {
                if (statusHandler_) {
                    const QByteArray text = featurePairWatcher_.progressText().toUtf8();
                    statusHandler_(
                        percent,
                        text.isEmpty()
                            ? std::string("Đang so sánh Feature của pair")
                            : text.toStdString());
                }
            });
    connect(&featurePairWatcher_,
            &QFutureWatcher<stepcompare::application::FeaturePairComparisonResult>::finished,
            this,
            [this] {
                const auto future = featurePairWatcher_.future();
                if (future.resultCount() == 0) {
                    if (statusHandler_) {
                        statusHandler_(100, "Đã hủy so sánh Feature của pair");
                    }
                    return;
                }
                if (featurePairResultHandler_) {
                    featurePairResultHandler_(future.result());
                }
            });
}

ComparisonRunner::~ComparisonRunner() {
    if (watcher_.isRunning()) {
        stopSource_.request_stop();
        watcher_.cancel();
        watcher_.waitForFinished();
    }
    if (featurePairWatcher_.isRunning()) {
        stopSource_.request_stop();
        featurePairWatcher_.cancel();
        featurePairWatcher_.waitForFinished();
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
    if (watcher_.isRunning()) {
        watcher_.cancel();
    }
    if (featurePairWatcher_.isRunning()) {
        featurePairWatcher_.cancel();
    }
    return requested;
}

bool ComparisonRunner::busy() const noexcept {
    return watcher_.isRunning() || featurePairWatcher_.isRunning();
}

bool ComparisonRunner::startFeaturePair(
    stepcompare::application::FeaturePairComparisonRequest request) {
    if (busy()) {
        return false;
    }
    stopSource_ = std::stop_source{};
    request.cancellation = stopSource_.get_token();
    featurePairWatcher_.setFuture(QtConcurrent::run(
        runFeaturePair, &coordinator_, std::move(request)));
    return true;
}

}  // namespace stepcompare::gui
