#include "mainwindow.h"

#include <QApplication>
#include <QProcessEnvironment>

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // 4096MB
    //qputenv("QT_IMAGEIO_MAXALLOC", QByteArray("4096"));
    // あるいは制限なしなら
    qputenv("QT_IMAGEIO_MAXALLOC", QByteArray("0"));
#endif

    QApplication a(argc, argv);
    MainWindow w;
    w.resize(1300,800);
    w.show();
    return a.exec();
}
