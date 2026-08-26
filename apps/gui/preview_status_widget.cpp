#include "preview_status_widget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QString>

#include <utility>

namespace stepcompare::gui {

PreviewStatusWidget::PreviewStatusWidget(CancelHandler cancelHandler,
                                         QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    phaseLabel_ = new QLabel(tr("Ready"), this);
    progress_ = new QProgressBar(this);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(true);
    cancel_ = new QPushButton(tr("Cancel"), this);
    cancel_->setEnabled(false);
    layout->addWidget(phaseLabel_, 1);
    layout->addWidget(progress_);
    layout->addWidget(cancel_);
    connect(cancel_, &QPushButton::clicked, this, [handler = std::move(cancelHandler)] {
        if (handler) {
            handler();
        }
    });
}

void PreviewStatusWidget::setStatus(
    const stepcompare::viewer::PreviewLoadStatus& status) {
    const auto phase = stepcompare::viewer::toString(status.phase);
    const QString side = status.side == stepcompare::viewer::ModelSide::A
                             ? QStringLiteral("A")
                             : QStringLiteral("B");
    phaseLabel_->setText(
        tr("File %1 — %2 — %3")
            .arg(side,
                 QString::fromLatin1(phase.data(), static_cast<qsizetype>(phase.size())),
                 QString::fromUtf8(status.messageUtf8)));
    progress_->setValue(status.percent);
    cancel_->setEnabled(status.cancellable && !status.cancelRequested);
}

}  // namespace stepcompare::gui
