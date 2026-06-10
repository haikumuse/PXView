#include <QApplication>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QProcess>
#include <cstdlib>

int main(int argc, char *argv[]) {
    bool useFreetype = false;
    bool isChild = false;

    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "freetype") {
            useFreetype = true;
            isChild = true;
        } else if (QString(argv[i]) == "directwrite") {
            useFreetype = false;
            isChild = true;
        }
    }

    if (!isChild) {
        qDebug() << "Spawning directwrite and freetype processes...";
        QProcess p1, p2;
        p1.start(argv[0], QStringList() << "directwrite");
        p1.waitForFinished();
        p2.start(argv[0], QStringList() << "freetype");
        p2.waitForFinished();
        qDebug() << "Done generating both images.";
        return 0;
    }

    if (useFreetype) {
        qputenv("QT_QPA_PLATFORM", "windows:fontengine=freetype");
    }

    QApplication app(argc, argv);

    // Load fonts
    QFontDatabase::addApplicationFont("PXView/fonts/OPPOSans-M.ttf");
    QFontDatabase::addApplicationFont("PXView/fonts/SourceHanSansCN-Regular.otf");
    QFontDatabase::addApplicationFont("PXView/fonts/SourceCodePro-Medium.ttf");

    // We'll generate a very tall image to fit all combinations
    QPixmap img(1000, 4500);
    img.fill(Qt::white);
    QPainter p(&img);

    QString text = "0 9 (0123456789 ABCDEF)";

    QStringList families = {"Microsoft YaHei", "Source Han Sans CN", "OPPOSans M", "Source Code Pro", "Tahoma", "Arial", "Segoe UI", "Verdana", "Consolas"};
    int sizes[] = {10, 11, 12};
    
    // Hinting
    QFont::HintingPreference hintings[] = {
        QFont::PreferDefaultHinting,
        QFont::PreferNoHinting,
        QFont::PreferVerticalHinting,
        QFont::PreferFullHinting
    };
    QStringList hintNames = {"DefHint", "NoHint", "VertHint", "FullHint"};

    // Antialiasing strategies
    // 0: NoAntialias (1-bit)
    // 1: PreferAntialias + NoSubpixelAntialias (Grayscale AA)
    // 2: PreferDefault (Subpixel / ClearType AA)
    
    int y = 30;
    p.setPen(Qt::black);

    for (const QString &family : families) {
        for (int size : sizes) {
            for (int aaMode = 0; aaMode <= 2; aaMode++) {
                for (int h = 0; h < 4; h++) {
                    QFont f(family);
                    f.setPixelSize(size);
                    f.setHintingPreference(hintings[h]);

                    if (aaMode == 0) {
                        f.setStyleStrategy(QFont::NoAntialias);
                        p.setRenderHint(QPainter::TextAntialiasing, false);
                    } else if (aaMode == 1) {
                        f.setStyleStrategy((QFont::StyleStrategy)(QFont::PreferAntialias | QFont::NoSubpixelAntialias));
                        p.setRenderHint(QPainter::TextAntialiasing, true);
                    } else {
                        f.setStyleStrategy(QFont::PreferDefault);
                        p.setRenderHint(QPainter::TextAntialiasing, true);
                    }

                    p.setFont(f);
                    
                    QString aaName;
                    if (aaMode == 0) aaName = "NoAA(1-bit)";
                    else if (aaMode == 1) aaName = "GrayAA";
                    else aaName = "SubpixelAA";

                    QString desc = QString("%1 %2px | %3 | %4")
                                   .arg(family).arg(size, 2).arg(aaName, 12).arg(hintNames[h]);
                    
                    // Draw description with default font so it's readable
                    QFont descFont("Arial", 9);
                    p.setFont(descFont);
                    p.drawText(10, y, desc);

                    // Draw actual text with target font
                    if (aaMode == 1) {
                        // To force true Grayscale AA on Windows, we draw to an ARGB32 QImage first,
                        // which forces Qt's raster engine (no subpixel AA).
                        QImage tmp(600, 30, QImage::Format_ARGB32);
                        tmp.fill(Qt::transparent);
                        QPainter tp(&tmp);
                        tp.setRenderHint(QPainter::TextAntialiasing, true);
                        tp.setFont(f);
                        tp.setPen(Qt::black);
                        tp.drawText(0, 20, text);
                        p.drawImage(350, y - 20, tmp);
                    } else {
                        p.setFont(f);
                        p.drawText(350, y, text);
                    }
                    y += 20;
                }
            }
            // Draw a separator line
            p.setPen(QColor(200, 200, 200));
            p.drawLine(10, y-10, 990, y-10);
            p.setPen(Qt::black);
            y += 10;
        }
        p.setPen(QColor(100, 100, 100));
        p.drawLine(10, y, 990, y);
        p.setPen(Qt::black);
        y += 20;
    }

    QString filename = useFreetype ? "font_test_freetype.png" : "font_test_directwrite.png";
    img.save(filename);
    qDebug() << "Saved to" << filename;
    return 0;
}
