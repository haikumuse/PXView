#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <QElapsedTimer>

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    QFile file("test_resize.dat");
    if (file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        QElapsedTimer timer;
        timer.start();
        qint64 size = 16LL * 1024 * 1024 * 1024; // 16GB
        file.resize(size);
        qDebug() << "Resize 16GB took:" << timer.elapsed() << "ms";
        uchar* ptr = file.map(0, size);
        if (ptr) {
            qDebug() << "Map 16GB successful!";
            file.unmap(ptr);
        } else {
            qDebug() << "Map 16GB failed:" << file.errorString();
        }
        file.close();
    }
    return 0;
}
