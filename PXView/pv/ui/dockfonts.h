#ifndef DOCKFONTS_H
#define DOCKFONTS_H

#include <QFont>
#include <QApplication>

namespace DockFontSizes
{
    constexpr int MainTitle = 18;
    constexpr int SectionTitle = 16;
    constexpr int Label = 14;
    constexpr int Content = 12;
}

inline QFont dock_font_main_title()
{
    QFont font = QApplication::font();
    font.setPixelSize(DockFontSizes::MainTitle);
    font.setBold(true);
    return font;
}

inline QFont dock_font_section_title()
{
    QFont font = QApplication::font();
    font.setPixelSize(DockFontSizes::SectionTitle);
    font.setBold(true);
    return font;
}

inline QFont dock_font_label()
{
    QFont font = QApplication::font();
    font.setPixelSize(DockFontSizes::Label);
    return font;
}

inline QFont dock_font_content()
{
    QFont font = QApplication::font();
    font.setPixelSize(DockFontSizes::Content);
    return font;
}

#endif
