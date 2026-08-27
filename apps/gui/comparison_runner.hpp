#pragma once

#include <QFutureWatcher>
#include <QObject>

#include <functional>
#include <stop_token>

#include <stepcompare/application/comparison_coordinator.hpp>
#include <stepcompare/deep/occt_deep_geometry_engine.hpp>
#include <stepcompare/deviation/occt_surface_deviation_engine.hpp>
#include <stepcompare/import/occt_step_importer.hpp>
#include <stepcompare/feature/occt_feature_recognizer.hpp>

namespace stepcompare::gui {

class ComparisonRunner final : public QObject {
public:
    using ResultHandler =
        std::function<void(stepcompare::application::ComparisonResult)>;
    using StatusHandler = std::function<void(int, std::string)>;

    ComparisonRunner(StatusHandler statusHandler,
                     ResultHandler resultHandler,
                     QObject* parent = nullptr);
    ~ComparisonRunner() override;

    [[nodiscard]] bool start(
        stepcompare::application::ComparisonRequest request);
    [[nodiscard]] bool cancel();
    [[nodiscard]] bool busy() const noexcept;

private:
    QFutureWatcher<stepcompare::application::ComparisonResult> watcher_;
    StatusHandler statusHandler_;
    ResultHandler resultHandler_;
    std::stop_source stopSource_;
    stepcompare::import::OcctStepImporter importer_;
    stepcompare::deep::OcctDeepGeometryEngine deepGeometry_;
    stepcompare::deviation::OcctSurfaceDeviationEngine surfaceDeviation_;
    stepcompare::feature::OcctFeatureRecognizer featureRecognition_;
    stepcompare::application::ComparisonCoordinator coordinator_;
};

}  // namespace stepcompare::gui
