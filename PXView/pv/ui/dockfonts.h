#ifndef DOCKFONTS_H
#define DOCKFONTS_H

#include <QFont>
#include <QApplication>
#include "../config/appconfig.h"

namespace DockFontSizes
{
    constexpr int MainTitle = 18;
    constexpr int SectionTitle = 16;
    constexpr int Label = 14;
    constexpr int Content = 12;
}

inline int get_dock_font_size(const QString& token, int defaultSize) {
    int sz = AppConfig::Instance().GetThemeTokenValue(token).toInt();
    return sz > 0 ? sz : defaultSize;
}

inline QFont dock_font_main_title()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@dock-font-main-title", DockFontSizes::MainTitle));
    font.setBold(true);
    return font;
}

inline QFont dock_font_section_title()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@dock-font-section-title", DockFontSizes::SectionTitle));
    font.setBold(true);
    return font;
}

inline QFont dock_font_label()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@dock-font-label", DockFontSizes::Label));
    return font;
}

inline QFont dock_font_content()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@dock-font-content", DockFontSizes::Content));
    return font;
}

inline int floating_panel_font_label_size()
{
    return get_dock_font_size("@floating-panel-font-label", 10);
}

inline int floating_panel_font_value_size()
{
    return get_dock_font_size("@floating-panel-font-value", 16);
}

inline QFont theme_font_titlebar()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@titlebar-font-size", 13));
    return font;
}

inline QFont theme_font_toolbar()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@toolbar-font-size", 12));
    return font;
}

inline QFont theme_font_sidebar()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@sidebar-font-size", 12));
    return font;
}

inline QFont theme_font_dialog()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@dialog-font-size", 12));
    return font;
}

inline QFont theme_font_trace_label()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@trace-label-font-size", 10));
    return font;
}

inline QFont theme_font_ruler()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@ruler-font-size", 10));
    return font;
}

inline QFont theme_font_cursor()
{
    QFont font = QApplication::font();
    font.setPixelSize(get_dock_font_size("@cursor-font-size", 10));
    return font;
}

#endif
