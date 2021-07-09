#include "main_window.h"
#include "ui_main_window.h"

namespace sanescan {

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui_{std::make_unique<Ui::MainWindow>()}
{
    ui_->setupUi(this);
}

MainWindow::~MainWindow() = default;

} // namespace sanescan
