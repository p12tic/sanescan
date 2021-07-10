
#include "main_window.h"
#include "version.h"
#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("sanescan");
    QCoreApplication::setApplicationVersion(SANESCAN_VERSION);

    sanescan::MainWindow main_window;
    main_window.show();

    return app.exec();
}
