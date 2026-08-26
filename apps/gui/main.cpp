#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("StepCompare"));
    QCoreApplication::setOrganizationName(QStringLiteral("StepCompare"));

    stepcompare::gui::MainWindow window;
    window.show();
    return application.exec();
}
