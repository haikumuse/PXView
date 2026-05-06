/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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

#include "searchpatterninput.h"

#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QApplication>
#include <QInputMethodEvent>

namespace pv {
namespace widgets {

SearchPatternInput::SearchPatternInput(QWidget *parent) :
    QWidget(parent),
    _channel_count(0),
    _cursor_pos(0),
    _has_focus(false)
{
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, false);
    setAttribute(Qt::WA_KeyCompression, false);
}

void SearchPatternInput::set_channel_count(int count)
{
    _channel_count = count;
    _cursor_pos = 0;
    _chars.clear();
    for (int i = 0; i < count; i++)
        _chars.push_back('X');
    setFixedSize(sizeHint());
    update();
}

int SearchPatternInput::channel_count() const
{
    return _channel_count;
}

std::map<uint16_t, QString> SearchPatternInput::get_pattern() const
{
    std::map<uint16_t, QString> pattern;
    for (int i = 0; i < _channel_count; i++) {
        uint16_t ch_index = _channel_count - 1 - i;
        pattern[ch_index] = QString(_chars[i]);
    }
    return pattern;
}

void SearchPatternInput::set_pattern(const std::map<uint16_t, QString> &pattern)
{
    for (int i = 0; i < _channel_count; i++) {
        uint16_t ch_index = _channel_count - 1 - i;
        auto it = pattern.find(ch_index);
        _chars[i] = (it != pattern.end()) ? it->second.at(0) : 'X';
    }
    update();
}

QSize SearchPatternInput::sizeHint() const
{
    return QSize(_channel_count * kCellWidth + kPadding * 2,
                 kLabelHeight + kCellHeight + kPadding * 2);
}

int SearchPatternInput::cellWidth() const
{
    if (_channel_count <= 0)
        return kCellWidth;
    return kCellWidth;
}

int SearchPatternInput::charIndexAt(int x) const
{
    int cw = cellWidth();
    int idx = (x - kPadding) / cw;
    if (idx < 0) idx = 0;
    if (idx >= _channel_count) idx = _channel_count - 1;
    return idx;
}

void SearchPatternInput::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    QColor bgColor = palette().color(QPalette::Base);
    QColor textColor = palette().color(QPalette::Text);
    QColor selBgColor(0x35, 0x87, 0xFE);
    QColor selTextColor(Qt::white);
    QColor labelColor = palette().color(QPalette::WindowText);

    int cw = cellWidth();
    int totalW = _channel_count * cw;
    int cellTop = kPadding + kLabelHeight;

    QFont labelFont("Source Code Pro");
    labelFont.setStyleHint(QFont::Monospace);
    labelFont.setFixedPitch(true);
    labelFont.setPixelSize(14);
    p.setFont(labelFont);
    p.setPen(labelColor);

    for (int i = 0; i < _channel_count; i++) {
        int x = kPadding + i * cw;
        uint16_t ch_index = _channel_count - 1 - i;
        p.drawText(QRect(x, kPadding, cw, kLabelHeight), Qt::AlignCenter, QString::number(ch_index));
    }

    p.setPen(Qt::NoPen);
    p.setBrush(bgColor);
    p.drawRoundedRect(kPadding, cellTop, totalW, kCellHeight, 4, 4);

    QFont charFont("Source Code Pro");
    charFont.setStyleHint(QFont::Monospace);
    charFont.setFixedPitch(true);
    charFont.setPixelSize(12);

    QFontMetrics charFm(charFont);
    int letterSpacing = 7;

    for (int i = 0; i < _channel_count; i++) {
        int x = kPadding + i * cw;

        if (_has_focus && i == _cursor_pos) {
            p.setPen(Qt::NoPen);
            p.setBrush(selBgColor);
            p.drawRect(x, cellTop, cw, kCellHeight);
        }

        QChar ch = (i < _chars.size()) ? _chars[i] : 'X';
        p.setFont(charFont);

        if (_has_focus && i == _cursor_pos)
            p.setPen(selTextColor);
        else
            p.setPen(textColor);

        int charX = x + (cw - charFm.boundingRect(ch).width() - letterSpacing) / 2 + letterSpacing / 2;
        int charY = cellTop + (kCellHeight + charFm.ascent() - charFm.descent()) / 2 - 1;
        p.drawText(charX, charY, QString(ch));
    }
}

void SearchPatternInput::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();

    if (key == Qt::Key_Left) {
        if (_cursor_pos > 0)
            _cursor_pos--;
        update();
        event->accept();
        return;
    }

    if (key == Qt::Key_Right) {
        if (_cursor_pos < _channel_count - 1)
            _cursor_pos++;
        update();
        event->accept();
        return;
    }

    if (key == Qt::Key_Backspace) {
        if (_cursor_pos > 0) {
            _cursor_pos--;
            _chars[_cursor_pos] = 'X';
            emit pattern_changed();
        }
        update();
        event->accept();
        return;
    }

    if (key == Qt::Key_Delete) {
        if (_cursor_pos < _channel_count) {
            _chars[_cursor_pos] = 'X';
            emit pattern_changed();
        }
        update();
        event->accept();
        return;
    }

    QChar ch;
    switch (key) {
    case Qt::Key_0: ch = '0'; break;
    case Qt::Key_1: ch = '1'; break;
    case Qt::Key_R: ch = 'R'; break;
    case Qt::Key_F: ch = 'F'; break;
    case Qt::Key_C: ch = 'C'; break;
    case Qt::Key_X: ch = 'X'; break;
    default:
        event->ignore();
        return;
    }

    if (_cursor_pos < _channel_count) {
        _chars[_cursor_pos] = ch;
        if (_cursor_pos < _channel_count - 1)
            _cursor_pos++;
        emit pattern_changed();
    }
    update();
    event->accept();
}

void SearchPatternInput::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int cellTop = kPadding + kLabelHeight;
        int y = event->y();
        if (y >= cellTop && y < cellTop + kCellHeight) {
            _cursor_pos = charIndexAt(event->x());
        }
        _has_focus = true;
        setFocus(Qt::MouseFocusReason);
        update();
        event->accept();
    }
}

void SearchPatternInput::focusInEvent(QFocusEvent *event)
{
    _has_focus = true;
    update();
    QWidget::focusInEvent(event);
}

void SearchPatternInput::focusOutEvent(QFocusEvent *event)
{
    _has_focus = false;
    update();
    QWidget::focusOutEvent(event);
}

} // namespace widgets
} // namespace pv
