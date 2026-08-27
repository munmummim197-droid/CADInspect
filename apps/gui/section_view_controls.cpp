#include "section_view_controls.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>

#include <array>
#include <utility>

namespace stepcompare::gui {

SectionViewControls::SectionViewControls(ChangeHandler handler, QWidget* parent)
    : QFrame(parent), handler_(std::move(handler)) {
    setObjectName(QStringLiteral("sectionViewControls"));
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(QStringLiteral(
        "QFrame#sectionViewControls { background:#edf3f8; border-bottom:1px solid #b9c8d4; } "
        "QLabel#sectionTitle { font-weight:700; color:#17324a; }"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 5, 8, 5);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("SECTION VIEW"), this);
    title->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(title);
    layout->addWidget(new QLabel(tr("Hướng cắt:"), this));

    directionCombo_ = new QComboBox(this);
    directionCombo_->setObjectName(QStringLiteral("sectionDirectionCombo"));
    const std::array directions{
        std::pair{tr("XY"), stepcompare::viewer::SectionDirection::XY},
        std::pair{tr("YZ"), stepcompare::viewer::SectionDirection::YZ},
        std::pair{tr("ZX"), stepcompare::viewer::SectionDirection::ZX},
        std::pair{tr("Front"), stepcompare::viewer::SectionDirection::Front},
        std::pair{tr("Top"), stepcompare::viewer::SectionDirection::Top},
        std::pair{tr("Right"), stepcompare::viewer::SectionDirection::Right},
        std::pair{tr("Theo camera"), stepcompare::viewer::SectionDirection::Camera},
    };
    for (const auto& [label, direction] : directions) {
        directionCombo_->addItem(label, static_cast<int>(direction));
    }
    directionCombo_->setCurrentIndex(directionCombo_->count() - 1);
    layout->addWidget(directionCombo_);

    offsetLabel_ = new QLabel(this);
    offsetLabel_->setObjectName(QStringLiteral("sectionOffsetLabel"));
    offsetLabel_->setMinimumWidth(90);
    layout->addWidget(offsetLabel_);
    offsetSlider_ = new QSlider(Qt::Horizontal, this);
    offsetSlider_->setObjectName(QStringLiteral("sectionOffsetSlider"));
    offsetSlider_->setRange(-100, 100);
    offsetSlider_->setSingleStep(1);
    offsetSlider_->setPageStep(10);
    offsetSlider_->setValue(0);
    offsetSlider_->setMinimumWidth(180);
    offsetSlider_->setToolTip(
        tr("Kéo mặt cắt trong phạm vi hình học đang áp dụng"));
    layout->addWidget(offsetSlider_, 1);

    flipCheck_ = new QCheckBox(tr("Đảo hướng"), this);
    flipCheck_->setObjectName(QStringLiteral("sectionFlipDirection"));
    layout->addWidget(flipCheck_);

    layout->addWidget(new QLabel(tr("Áp dụng:"), this));
    targetCombo_ = new QComboBox(this);
    targetCombo_->setObjectName(QStringLiteral("sectionTargetCombo"));
    targetCombo_->addItem(QStringLiteral("A"),
                          static_cast<int>(stepcompare::viewer::SectionTarget::A));
    targetCombo_->addItem(QStringLiteral("B"),
                          static_cast<int>(stepcompare::viewer::SectionTarget::B));
    targetCombo_->addItem(QStringLiteral("A+B"),
                          static_cast<int>(stepcompare::viewer::SectionTarget::Both));
    targetCombo_->setCurrentIndex(2);
    layout->addWidget(targetCombo_);

    resetButton_ = new QPushButton(tr("Reset"), this);
    resetButton_->setObjectName(QStringLiteral("sectionResetButton"));
    layout->addWidget(resetButton_);

    connect(directionCombo_, &QComboBox::currentIndexChanged, this,
            [this](const int index) {
                settings_.direction =
                    static_cast<stepcompare::viewer::SectionDirection>(
                        directionCombo_->itemData(index).toInt());
                publish();
            });
    connect(offsetSlider_, &QSlider::valueChanged, this, [this](const int value) {
        settings_.normalizedOffset = static_cast<double>(value) / 100.0;
        refreshOffsetLabel();
        publish();
    });
    connect(flipCheck_, &QCheckBox::toggled, this, [this](const bool checked) {
        settings_.flipped = checked;
        publish();
    });
    connect(targetCombo_, &QComboBox::currentIndexChanged, this,
            [this](const int index) {
                settings_.target = static_cast<stepcompare::viewer::SectionTarget>(
                    targetCombo_->itemData(index).toInt());
                publish();
            });
    connect(resetButton_, &QPushButton::clicked, this, [this] { reset(); });

    refreshOffsetLabel();
    setSectionModeActive(false);
}

void SectionViewControls::setSectionModeActive(const bool active) {
    setVisible(active);
}

stepcompare::viewer::SectionSettings SectionViewControls::settings() const noexcept {
    return settings_;
}

void SectionViewControls::publish() {
    if (handler_) {
        handler_(settings_);
    }
}

void SectionViewControls::reset() {
    settings_ = {};
    const QSignalBlocker directionBlocker(directionCombo_);
    const QSignalBlocker offsetBlocker(offsetSlider_);
    const QSignalBlocker flipBlocker(flipCheck_);
    const QSignalBlocker targetBlocker(targetCombo_);
    directionCombo_->setCurrentIndex(directionCombo_->count() - 1);
    offsetSlider_->setValue(0);
    flipCheck_->setChecked(false);
    targetCombo_->setCurrentIndex(2);
    refreshOffsetLabel();
    publish();
}

void SectionViewControls::refreshOffsetLabel() {
    const int percent = offsetSlider_->value();
    offsetLabel_->setText(tr("Offset: %1%")
                              .arg(percent > 0 ? QStringLiteral("+") +
                                                     QString::number(percent)
                                               : QString::number(percent)));
}

}  // namespace stepcompare::gui
