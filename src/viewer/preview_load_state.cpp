#include <stepcompare/viewer/preview_load_state.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stepcompare::viewer {

std::uint64_t PreviewLoadStateModel::begin(const ModelSide side,
                                           std::u8string sourcePathUtf8) {
    if (sourcePathUtf8.empty()) {
        throw std::invalid_argument("Preview source path cannot be empty");
    }
    ++nextGeneration_;
    status_ = {
        .phase = PreviewLoadPhase::Queued,
        .side = side,
        .sourcePathUtf8 = std::move(sourcePathUtf8),
        .messageUtf8 = "Queued",
        .percent = 0,
        .cancellable = true,
        .cancelRequested = false,
        .generation = nextGeneration_,
    };
    return nextGeneration_;
}

void PreviewLoadStateModel::markImporting(const std::uint64_t generation) {
    if (!accepts(generation) || !active()) {
        return;
    }
    status_.phase = PreviewLoadPhase::Importing;
    status_.messageUtf8 = "Importing STEP";
}

void PreviewLoadStateModel::updateProgress(const std::uint64_t generation,
                                           const int percent,
                                           std::string messageUtf8) {
    if (!accepts(generation) || !active() || percent < status_.percent ||
        percent < 0 || percent > 100) {
        return;
    }
    status_.percent = percent;
    status_.messageUtf8 = std::move(messageUtf8);
}

void PreviewLoadStateModel::markPreparingScene(const std::uint64_t generation) {
    if (!accepts(generation) || !active()) {
        return;
    }
    status_.phase = PreviewLoadPhase::PreparingScene;
    status_.percent = std::max(status_.percent, 80);
    status_.messageUtf8 = "Preparing 3D scene";
}

void PreviewLoadStateModel::complete(const std::uint64_t generation,
                                     std::string messageUtf8) {
    if (!accepts(generation) || !active()) {
        return;
    }
    status_.phase = PreviewLoadPhase::Completed;
    status_.percent = 100;
    status_.messageUtf8 = std::move(messageUtf8);
    status_.cancellable = false;
    status_.cancelRequested = false;
}

void PreviewLoadStateModel::fail(const std::uint64_t generation,
                                 std::string messageUtf8) {
    if (!accepts(generation) || !active()) {
        return;
    }
    status_.phase = PreviewLoadPhase::Failed;
    status_.messageUtf8 = std::move(messageUtf8);
    status_.cancellable = false;
    status_.cancelRequested = false;
}

bool PreviewLoadStateModel::requestCancel() noexcept {
    if (!active() || status_.cancelRequested) {
        return false;
    }
    status_.cancelRequested = true;
    status_.messageUtf8 = "Cancelling after current OCCT checkpoint";
    return true;
}

void PreviewLoadStateModel::cancel(const std::uint64_t generation) noexcept {
    if (!accepts(generation) || !active()) {
        return;
    }
    status_.phase = PreviewLoadPhase::Cancelled;
    status_.messageUtf8 = "Cancelled";
    status_.cancellable = false;
    status_.cancelRequested = false;
}

const PreviewLoadStatus& PreviewLoadStateModel::status() const noexcept {
    return status_;
}

bool PreviewLoadStateModel::accepts(const std::uint64_t generation) const noexcept {
    return generation != 0 && generation == status_.generation;
}

bool PreviewLoadStateModel::active() const noexcept {
    return status_.phase == PreviewLoadPhase::Queued ||
           status_.phase == PreviewLoadPhase::Importing ||
           status_.phase == PreviewLoadPhase::PreparingScene;
}

std::string_view toString(const PreviewLoadPhase phase) noexcept {
    switch (phase) {
        case PreviewLoadPhase::Idle:
            return "IDLE";
        case PreviewLoadPhase::Queued:
            return "QUEUED";
        case PreviewLoadPhase::Importing:
            return "IMPORTING";
        case PreviewLoadPhase::PreparingScene:
            return "PREPARING_SCENE";
        case PreviewLoadPhase::Completed:
            return "COMPLETED";
        case PreviewLoadPhase::Failed:
            return "FAILED";
        case PreviewLoadPhase::Cancelled:
            return "CANCELLED";
    }
    return "UNKNOWN";
}

}  // namespace stepcompare::viewer
