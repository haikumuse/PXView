#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <cstdlib>

int main(int argc, char *argv[]) {
    bool useFreetype = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "freetype") {
            useFreetype = true;
        }
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
    QImage img(1000, 4500, QImage::Format_ARGB32);
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
                    p.setFont(f);
                    p.drawText(350, y, text);
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
