#pragma once

#include <QWidget>

#include <functional>

#include <stepcompare/viewer/preview_load_state.hpp>

class QLabel;
class QProgressBar;
class QPushButton;

namespace stepcompare::gui {

class PreviewStatusWidget final : public QWidget {
public:
    using CancelHandler = std::function<void()>;

    explicit PreviewStatusWidget(CancelHandler cancelHandler,
                                 QWidget* parent = nullptr);

    void setStatus(const stepcompare::viewer::PreviewLoadStatus& status);

private:
    QLabel* phaseLabel_{};
    QProgressBar* progress_{};
    QPushButton* cancel_{};
};

}  // namespace stepcompare::gui
