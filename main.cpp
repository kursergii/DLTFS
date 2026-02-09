#include "mw.h"

#include <QApplication>

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    // Force X11 platform to avoid Wayland plugin issues
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    QApplication a(argc, argv);
    MW w;
    w.show();
    return a.exec();
}
