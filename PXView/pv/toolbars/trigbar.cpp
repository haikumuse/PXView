/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "trigbar.h"

#include <QBitmap>
#include <QPainter>
#include <QEvent>

#include "../sigsession.h"
#include "../dialogs/fftoptions.h"
#include "../dialogs/lissajousoptions.h"
#include "../dialogs/mathoptions.h"
#include "../view/trace.h"
#include "../dialogs/applicationpardlg.h"
#include "../ui/langresource.h"
#include "../config/appconfig.h"
#include "../ui/fn.h"
#include "../ui/iconcache.h"

namespace pv {
namespace toolbars {

TrigBar::TrigBar(SigSession *session, QWidget *parent) :
    QToolBar("Trig Bar", parent),
    _session(session)
{
    _enable = true;

    setMovable(false);
    setContentsMargins(0,0,0,0);

    _action_fft = new QAction(this);
    _action_fft->setObjectName(QString::fromUtf8("actionFft"));

    _action_math = new QAction(this);
    _action_math->setObjectName(QString::fromUtf8("actionMath"));

    _function_menu = new QMenu(this);
    _function_menu->setContentsMargins(0,0,0,0);
    _function_menu->addAction(_action_fft);
    _function_menu->addAction(_action_math);

    _action_lissajous = new QAction(this);
    _action_lissajous->setObjectName(QString::fromUtf8("actionLissajous"));

    _dark_style = new QAction(this);
    _dark_style->setObjectName(QString::fromUtf8("actionDark"));

    _light_style = new QAction(this);
    _light_style->setObjectName(QString::fromUtf8("actionLight"));

    _themes = new QMenu(this);
    _themes->setObjectName(QString::fromUtf8("menuThemes"));
    _themes->addAction(_light_style);
    _themes->addAction(_dark_style);

    _action_dispalyOptions = new QAction(this);

    _display_menu = new QMenu(this);
    _display_menu->setContentsMargins(0,0,0,0);

    _display_menu->addAction(_action_lissajous);
    _display_menu->addMenu(_themes);
    _display_menu->addAction(_action_dispalyOptions);

    connect(_action_fft, SIGNAL(triggered()), this, SLOT(on_actionFft_triggered()));
    connect(_action_math, SIGNAL(triggered()), this, SLOT(on_actionMath_triggered()));
    connect(_action_lissajous, SIGNAL(triggered()), this, SLOT(on_actionLissajous_triggered()));
    connect(_dark_style, SIGNAL(triggered()), this, SLOT(on_actionDark_triggered()));
    connect(_light_style, SIGNAL(triggered()), this, SLOT(on_actionLight_triggered()));
    connect(_action_dispalyOptions, SIGNAL(triggered()), this, SLOT(on_display_setting()));

    ADD_UI(this);
}

TrigBar::~TrigBar()
{
    REMOVE_UI(this);
}

void TrigBar::retranslateUi()
{
    _themes->setTitle(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DISPLAY_THEMES), "Themes"));
    _action_lissajous->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DISPLAY_LISSAJOUS), "Lissajous"));

    _dark_style->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DISPLAY_THEMES_DARK), "Dark"));
    _light_style->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DISPLAY_THEMES_LIGHT), "Light"));

    _action_fft->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_FUNCTION_FFT), "FFT"));
    _action_math->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_FUNCTION_MATH), "Math"));

    _action_dispalyOptions->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DISPLAY_OPTIONS), "Options"));
}

void TrigBar::reStyle()
{
    QString iconPath = GetIconPath();

    _action_fft->setIcon(IconCache::Instance().icon(iconPath+"/fft.svg"));
    _action_math->setIcon(IconCache::Instance().icon(iconPath+"/math.svg"));
    _action_lissajous->setIcon(IconCache::Instance().icon(iconPath+"/lissajous.svg"));
    _dark_style->setIcon(IconCache::Instance().icon(iconPath+"/dark.svg"));
    _light_style->setIcon(IconCache::Instance().icon(iconPath+"/light.svg"));

    _action_dispalyOptions->setIcon(IconCache::Instance().icon(iconPath+"/gear.svg"));

    AppConfig &app = AppConfig::Instance();
    QString icon_fname = iconPath +"/"+ app.frameOptions.style +".svg";
    _themes->setIcon(QIcon(icon_fname));
}

void TrigBar::reload()
{
    update_view_status();
    update();
}

void TrigBar::on_actionFft_triggered()
{
    pv::dialogs::FftOptions fft_dlg(this, _session);
    fft_dlg.exec();
}

void TrigBar::on_actionMath_triggered()
{
    pv::dialogs::MathOptions math_dlg(_session, this);
    if (math_dlg.exec() == QDialog::Accepted)
    {
        math_dlg.Apply();
    }
}

void TrigBar::on_actionDark_triggered()
{
    sig_setTheme(THEME_STYLE_DARK);
    QString icon = GetIconPath() + "/" + THEME_STYLE_DARK + ".svg";
    _themes->setIcon(QIcon(icon));
}

void TrigBar::on_actionLight_triggered()
{
    sig_setTheme(THEME_STYLE_LIGHT);
    QString icon = GetIconPath() + "/" + THEME_STYLE_LIGHT +".svg";
    _themes->setIcon(QIcon(icon));
}

void TrigBar::on_actionLissajous_triggered()
{
    pv::dialogs::LissajousOptions lissajous_dlg(_session, this);
    lissajous_dlg.exec();
}

 void TrigBar::on_display_setting()
 {
    pv::dialogs::ApplicationParamDlg dlg;
    dlg.ShowDlg(this);
 }

 void TrigBar::update_view_status()
 {
 }

void TrigBar::UpdateLanguage()
{
    retranslateUi();
}

void TrigBar::UpdateTheme()
{
    reStyle();
}

void TrigBar::UpdateFont()
{
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    ui::set_toolbar_font(this, font);
}

} // namespace toolbars
} // namespace pv
