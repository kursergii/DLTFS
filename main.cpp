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

// Note: The "main.moc" include is necessary for Qt's meta-object system to work properly
// when using signals and slots in the same file. It should be included at the end of the source file.