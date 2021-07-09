
#include "main_window.h"
#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    sanescan::MainWindow main_window;
    main_window.show();

    return app.exec();
}
