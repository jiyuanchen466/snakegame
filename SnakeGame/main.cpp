#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    MainWindow w; // 创建带堆栈页面的主窗口
    w.show();
    return a.exec();
}
