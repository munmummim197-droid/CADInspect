#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QImageReader>
#include <QSet>
#include <QSize>

#include <array>
#include <cstdlib>

#ifdef Q_OS_WIN
#include <shobjidl_core.h>
#endif

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    // Give every CADInspect top-level window one stable Windows taskbar
    // identity. This must be set before QApplication creates native windows.
    static_cast<void>(SetCurrentProcessExplicitAppUserModelID(
        L"CADInspect.Project.Desktop"));
#endif

    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("CADInspect"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("CADInspect"));
    QCoreApplication::setOrganizationName(QStringLiteral("StepCompare"));

    QImageReader iconReader(QStringLiteral(":/icons/StepCompare.ico"), "ico");
    QSet<QSize> availableIconSizes;
    for (int imageIndex = 0; imageIndex < iconReader.imageCount(); ++imageIndex) {
        if (iconReader.jumpToImage(imageIndex)) {
            availableIconSizes.insert(iconReader.size());
        }
    }
    constexpr std::array requiredIconSizes{16, 32, 48, 256};
    for (const int size : requiredIconSizes) {
        if (!availableIconSizes.contains(QSize(size, size))) {
            qCritical() << "StepCompare application icon is missing required size" << size;
            return EXIT_FAILURE;
        }
    }

    const QIcon applicationIcon(QStringLiteral(":/icons/StepCompare.ico"));
    if (applicationIcon.isNull()) {
        qCritical() << "StepCompare application icon resource is unavailable.";
        return EXIT_FAILURE;
    }
    application.setWindowIcon(applicationIcon);

    stepcompare::gui::MainWindow window;
    window.setWindowIcon(applicationIcon);
    window.show();
    return application.exec();
}
