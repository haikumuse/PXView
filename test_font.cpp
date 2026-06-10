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
    // 强制禁用高DPI缩放，确保字体的 setPixelSize 是绝对准确的物理像素
    // 否则 Windows 缩放会导致 10px 变成 12.5px，破坏点阵和 Hinting 测试
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
    qputenv("QT_SCALE_FACTOR", "1");

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

    // 加载字体
    QStringList fontPaths = {
        "PXView/fonts/OPPOSans-M.ttf",
        "PXView/fonts/SourceHanSansCN-Regular.otf",
        "PXView/fonts/SourceCodePro-Medium.ttf"
    };
    
    for (const QString& path : fontPaths) {
        if (QFontDatabase::addApplicationFont(path) == -1) {
            qWarning() << "Failed to load font:" << path;
        }
    }

    // 【核心修复】必须使用 QPixmap，它附带设备上下文，能让 DirectWrite 知道屏幕像素排列，从而激活红蓝彩边！
    // 高度扩展为 8000，防止内容因过长被截断
    QPixmap img(1000, 8000);
    img.fill(Qt::white); // 必须填充纯色背景，否则带透明度的底色依然会关闭 ClearType
    QPainter p(&img);

    QString text = "0 9 (0123456789 ABCDEF)";

    QStringList families = {"Microsoft YaHei", "Source Han Sans CN", "OPPOSans M", "Source Code Pro", "Tahoma", "Arial", "Segoe UI", "Verdana", "Consolas"};
    int sizes[] = {10, 11, 12};
    
    // Hinting 偏好
    QFont::HintingPreference hintings[] = {
        QFont::PreferDefaultHinting,
        QFont::PreferNoHinting,
        QFont::PreferVerticalHinting,
        QFont::PreferFullHinting
    };
    QStringList hintNames = {"DefHint", "NoHint", "VertHint", "FullHint"};

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
                    
                    // 用默认字体画描述文本
                    QFont descFont("Arial", 9);
                    descFont.setStyleStrategy(QFont::PreferDefault);
                    p.setFont(descFont);
                    p.drawText(10, y, desc);

                    // 绘制测试文本
                    if (aaMode == 1) {
                        // 【保留您的神级Hack】: 强制创建一个带 Alpha 通道的 QImage，
                        // 故意破坏 DirectWrite 的 ClearType 机制，强行生成纯正的灰度抗锯齿作对照！
                        QImage tmp(600, 30, QImage::Format_ARGB32);
                        tmp.fill(Qt::transparent);
                        QPainter tp(&tmp);
                        tp.setRenderHint(QPainter::TextAntialiasing, true);
                        tp.setFont(f);
                        tp.setPen(Qt::black);
                        tp.drawText(0, 20, text);
                        p.drawImage(350, y - 20, tmp);
                    } else {
                        // 在 QPixmap 上直接画，尽情触发极致红蓝彩边 / 锐利1-bit点阵
                        p.setFont(f);
                        p.drawText(350, y, text);
                    }
                    y += 20;
                }
            }
            // 画分隔线
            p.setPen(QColor(220, 220, 220));
            p.drawLine(10, y-10, 990, y-10);
            p.setPen(Qt::black);
            y += 10;
        }
        p.setPen(QColor(100, 100, 100));
        p.drawLine(10, y, 990, y);
        p.setPen(Qt::black);
        y += 20;
    }

    // 根据实际绘制高度裁剪图片，丢掉底部的多余空白
    QPixmap finalImg = img.copy(0, 0, 1000, y);

    QString filename = useFreetype ? "font_test_freetype.png" : "font_test_directwrite.png";
    finalImg.save(filename);
    qDebug() << "Saved to" << filename << "- Content Height:" << y;
    return 0;
}