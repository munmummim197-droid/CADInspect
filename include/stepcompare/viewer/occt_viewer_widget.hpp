#pragma once

#include <QWidget>

#include <functional>
#include <memory>
#include <string>

#include <stepcompare/viewer/viewer_state.hpp>

class TopLoc_Location;
class TopoDS_Shape;

namespace stepcompare::viewer {

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
    void clearShapes();

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
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace stepcompare::viewer
