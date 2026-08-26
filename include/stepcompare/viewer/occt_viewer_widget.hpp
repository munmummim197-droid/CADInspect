#pragma once

#include <QWidget>

#include <functional>
#include <memory>
#include <span>
#include <string>

#include <stepcompare/viewer/deviation_coloring.hpp>
#include <stepcompare/viewer/viewer_state.hpp>

class TopLoc_Location;
class TopoDS_Shape;

namespace stepcompare::viewer {

struct DeviationColorAssignment final {
    StableSelectionId stableId;
    double maximumMm{};
};

// Qt/OCCT adapter boundary. TopoDS_Shape does not escape into core or application APIs.
class OcctViewerWidget final : public QWidget {
public:
    explicit OcctViewerWidget(QWidget* parent = nullptr);
    ~OcctViewerWidget() override;

    OcctViewerWidget(const OcctViewerWidget&) = delete;
    OcctViewerWidget& operator=(const OcctViewerWidget&) = delete;

    void displayShape(const TopoDS_Shape& shape,
                      ModelSide side,
                      StableSelectionId stableId,
                      bool differs = false);
    void removeShape(const StableSelectionId& stableId);
    void clearShapes(ModelSide side);
    void clearShapes();
    void setDifferenceState(const StableSelectionId& stableId, bool differs);
    void setDifferenceStates(std::span<const StableSelectionId> changedStableIds);
    void clearDifferenceStates();

    // Applies aggregate component deviation values to existing
    // presentations. The batch is atomic: invalid/non-finite input leaves the
    // previous heatmap untouched. This is a GUI-thread presentation API and
    // performs no geometry or OCCT analysis.
    [[nodiscard]] bool setDeviationColors(
        std::span<const DeviationColorAssignment> assignments,
        const DeviationColorScale& scale);
    void clearDeviationColors();
    void setDeviationColoringEnabled(bool enabled);
    [[nodiscard]] bool deviationColoringEnabled() const noexcept;

    // BToA is presentation-only and is used only in CoordinateMode::Aligned.
    void setAlignedLocation(const StableSelectionId& stableId,
                            const TopLoc_Location& bToA);
    void clearAlignedLocation(const StableSelectionId& stableId);

    void applyState(const ViewerStateModel& state);
    void selectStableId(const StableSelectionId& stableId, bool fitSelection);
    void clearSelection();
    void setSelectionChangedHandler(
        std::function<void(std::string)> handler);

    void fitAll();
    void resetView();
    void setCameraOrientation(CameraOrientation orientation);

protected:
    QPaintEngine* paintEngine() const override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace stepcompare::viewer
