#include "mw.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // Force X11 platform to avoid Wayland plugin issues
    qputenv("QT_QPA_PLATFORM", "xcb");

    QApplication a(argc, argv);
    MW w;
    w.show();
    return a.exec();
}
