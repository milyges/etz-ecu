#include "wndmain.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    WndMain w;
    w.show();

    return a.exec();
}
