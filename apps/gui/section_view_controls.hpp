#pragma once

#include <QFrame>

#include <functional>

#include <stepcompare/viewer/viewer_state.hpp>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

namespace stepcompare::gui {

class SectionViewControls final : public QFrame {
public:
    using ChangeHandler =
        std::function<void(stepcompare::viewer::SectionSettings)>;

    explicit SectionViewControls(ChangeHandler handler,
                                 QWidget* parent = nullptr);

    void setSectionModeActive(bool active);
    [[nodiscard]] stepcompare::viewer::SectionSettings settings() const noexcept;

private:
    void publish();
    void reset();
    void refreshOffsetLabel();

    ChangeHandler handler_;
    stepcompare::viewer::SectionSettings settings_{};
    QComboBox* directionCombo_{};
    QSlider* offsetSlider_{};
    QLabel* offsetLabel_{};
    QCheckBox* flipCheck_{};
    QComboBox* targetCombo_{};
    QPushButton* resetButton_{};
};

}  // namespace stepcompare::gui
