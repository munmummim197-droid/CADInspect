#include "step_preview_loader.hpp"

#include <stepcompare/import/occt_step_importer.hpp>

#include <QFuture>
#include <QPromise>
#include <QString>
#include <QtConcurrentRun>

#include <exception>
#include <utility>

namespace stepcompare::gui {
namespace {

void runImport(QPromise<PreviewJobResult>& promise,
               const stepcompare::viewer::ModelSide side,
               std::u8string sourcePathUtf8,
               const std::uint64_t generation) {
    promise.setProgressRange(0, 100);
    promise.setProgressValueAndText(5, QStringLiteral("Queued"));
    if (promise.isCanceled()) {
        return;
    }

    promise.setProgressValueAndText(15, QStringLiteral("Importing STEP with OCCT"));
    stepcompare::import::OcctStepImporter importer;
    auto importResult = importer.importStep({std::move(sourcePathUtf8)});
    if (promise.isCanceled()) {
        return;
    }

    promise.setProgressValueAndText(
        80, QStringLiteral("Building adaptive preview tessellation"));
    auto meshSummary = importResult.succeeded()
                           ? preparePreviewMeshes(importResult.model)
                           : PreviewMeshSummary{};
    if (promise.isCanceled()) {
        return;
    }

    promise.setProgressValueAndText(95, QStringLiteral("Preparing immutable 3D scene"));
    promise.addResult({.generation = generation,
                       .side = side,
                       .importResult = std::move(importResult),
                       .meshSummary = meshSummary});
    promise.setProgressValueAndText(100, QStringLiteral("Import job finished"));
}

std::string diagnosticMessage(
    const stepcompare::import::StepImportResult& importResult) {
    for (const auto& diagnostic : importResult.diagnostics) {
        if (diagnostic.severity ==
            stepcompare::import::ImportDiagnosticSeverity::Error) {
            return diagnostic.messageUtf8;
        }
    }
    return "STEP import did not produce a previewable model";
}

}  // namespace

StepPreviewLoader::StepPreviewLoader(StatusHandler statusHandler,
                                     ResultHandler resultHandler,
                                     QObject* parent)
    : QObject(parent),
      statusHandler_(std::move(statusHandler)),
      resultHandler_(std::move(resultHandler)) {
    connect(&watcher_, &QFutureWatcher<PreviewJobResult>::started, this, [this] {
        state_.markImporting(activeGeneration_);
        publishStatus();
    });
    connect(&watcher_,
            &QFutureWatcher<PreviewJobResult>::progressValueChanged,
            this,
            [this](const int percent) {
                const QByteArray text = watcher_.progressText().toUtf8();
                state_.updateProgress(activeGeneration_,
                                      percent,
                                      std::string(text.constData(), text.size()));
                publishStatus();
            });
    connect(&watcher_, &QFutureWatcher<PreviewJobResult>::finished, this, [this] {
        finishActiveJob();
    });
}

StepPreviewLoader::~StepPreviewLoader() {
    if (watcher_.isRunning()) {
        watcher_.cancel();
        watcher_.waitForFinished();
    }
}

bool StepPreviewLoader::start(const stepcompare::viewer::ModelSide side,
                              std::u8string sourcePathUtf8) {
    if (busy()) {
        return false;
    }
    activeGeneration_ = state_.begin(side, sourcePathUtf8);
    publishStatus();
    watcher_.setFuture(QtConcurrent::run(
        runImport, side, std::move(sourcePathUtf8), activeGeneration_));
    return true;
}

bool StepPreviewLoader::cancel() {
    if (!state_.requestCancel()) {
        return false;
    }
    publishStatus();
    watcher_.cancel();
    return true;
}

bool StepPreviewLoader::busy() const noexcept {
    return watcher_.isRunning();
}

void StepPreviewLoader::publishStatus() const {
    if (statusHandler_) {
        statusHandler_(state_.status());
    }
}

void StepPreviewLoader::finishActiveJob() {
    const QFuture<PreviewJobResult> future = watcher_.future();
    if (future.isCanceled() || future.resultCount() == 0) {
        state_.cancel(activeGeneration_);
        publishStatus();
        return;
    }

    PreviewJobResult result = future.result();
    if (!state_.accepts(result.generation)) {
        return;
    }
    if (!result.importResult.succeeded()) {
        state_.fail(result.generation, diagnosticMessage(result.importResult));
        publishStatus();
        return;
    }

    state_.markPreparingScene(result.generation);
    publishStatus();
    try {
        if (resultHandler_) {
            resultHandler_(std::move(result));
        }
        state_.complete(activeGeneration_, "Preview ready");
    } catch (const std::exception& failure) {
        state_.fail(activeGeneration_, failure.what());
    } catch (...) {
        state_.fail(activeGeneration_, "Unexpected 3D scene preparation failure");
    }
    publishStatus();
}

}  // namespace stepcompare::gui
