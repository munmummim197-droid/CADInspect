#pragma once

#include <QFrame>

#include <functional>

#include <stepcompare/viewer/viewer_state.hpp>

class QAction;

namespace stepcompare::gui {

enum class IsolationCommand {
    ShowOnlyA,
    ShowOnlyB,
    ShowOnlyPair,
    RestoreAssembly,
};

class ViewerControlBar final : public QFrame {
public:
    using PresentationHandler =
        std::function<void(stepcompare::viewer::PresentationMode)>;
    using LayerHandler = std::function<void(stepcompare::viewer::SceneLayer)>;
    using HeatmapHandler = std::function<void(bool)>;
    using IsolationHandler = std::function<void(IsolationCommand)>;
    using CommandHandler = std::function<void()>;

    ViewerControlBar(PresentationHandler presentationHandler,
                     LayerHandler layerHandler,
                     HeatmapHandler heatmapHandler,
                     IsolationHandler isolationHandler,
                     CommandHandler fitAllHandler,
                     QWidget* parent = nullptr);

    void setPresentationMode(stepcompare::viewer::PresentationMode mode);
    void setLayer(stepcompare::viewer::SceneLayer layer);
    void setHeatmapEnabled(bool enabled);
    void setIsolationActive(bool active);

private:
    QAction* shadedAction_{};
    QAction* shadedEdgesAction_{};
    QAction* wireframeAction_{};
    QAction* transparentAction_{};
    QAction* sectionAction_{};
    QAction* differenceAction_{};
    QAction* heatmapAction_{};
    QAction* restoreAction_{};
};

}  // namespace stepcompare::gui
