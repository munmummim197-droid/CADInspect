#pragma once

#include <QFutureWatcher>
#include <QObject>

#include <cstdint>
#include <functional>
#include <string>

#include <stepcompare/import/step_import_port.hpp>
#include <stepcompare/viewer/preview_load_state.hpp>

#include "preview_quality.hpp"

namespace stepcompare::gui {

struct PreviewJobResult final {
    std::uint64_t generation{};
    stepcompare::viewer::ModelSide side{stepcompare::viewer::ModelSide::A};
    stepcompare::import::StepImportResult importResult;
    PreviewMeshSummary meshSummary{};
};

class StepPreviewLoader final : public QObject {
public:
    using StatusHandler =
        std::function<void(const stepcompare::viewer::PreviewLoadStatus&)>;
    using ResultHandler = std::function<void(PreviewJobResult)>;

    StepPreviewLoader(StatusHandler statusHandler,
                      ResultHandler resultHandler,
                      QObject* parent = nullptr);
    ~StepPreviewLoader() override;

    [[nodiscard]] bool start(stepcompare::viewer::ModelSide side,
                             std::u8string sourcePathUtf8);
    [[nodiscard]] bool cancel();
    [[nodiscard]] bool busy() const noexcept;

private:
    void publishStatus() const;
    void finishActiveJob();

    QFutureWatcher<PreviewJobResult> watcher_;
    stepcompare::viewer::PreviewLoadStateModel state_;
    StatusHandler statusHandler_;
    ResultHandler resultHandler_;
    std::uint64_t activeGeneration_{};
};

}  // namespace stepcompare::gui
