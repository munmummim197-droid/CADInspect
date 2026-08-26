#pragma once

#include <QWidget>

#include <functional>

#include <stepcompare/viewer/preview_load_state.hpp>

class QLabel;
class QProgressBar;
class QPushButton;
class QString;

namespace stepcompare::gui {

class PreviewStatusWidget final : public QWidget {
public:
    using CancelHandler = std::function<void()>;

    explicit PreviewStatusWidget(CancelHandler cancelHandler,
                                 QWidget* parent = nullptr);

    void setStatus(const stepcompare::viewer::PreviewLoadStatus& status);
    void setOperationStatus(QString message, int percent, bool cancellable);

private:
    QLabel* phaseLabel_{};
    QProgressBar* progress_{};
    QPushButton* cancel_{};
};

}  // namespace stepcompare::gui
