#ifndef SANESCAN_MAIN_WINDOW_H
#define SANESCAN_MAIN_WINDOW_H

#include <QtWidgets/QMainWindow>
#include <memory>

namespace sanescan {

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    std::unique_ptr<Ui::MainWindow> ui_;
};

} // namespace sanescan

#endif
