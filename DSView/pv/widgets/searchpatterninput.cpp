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
#include <QStyleOption>

namespace pv {
namespace widgets {

static const int kCellWidth = 22;
static const int kCellHeight = 26;
static const int kBorderRadius = 4;
static const int kPadding = 1;

SearchPatternInput::SearchPatternInput(QWidget *parent) :
    QWidget(parent),
    _channel_count(0),
    _cursor_pos(0),
    _has_focus(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);
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
    return QSize(_channel_count * kCellWidth + kPadding * 2, kCellHeight + kPadding * 2);
}

int SearchPatternInput::cellWidth() const
{
    if (_channel_count <= 0)
        return kCellWidth;
    int inner_width = width() - kPadding * 2;
    return inner_width / _channel_count;
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
    p.setRenderHint(QPainter::Antialiasing);

    QStyleOption opt;
    opt.initFrom(this);

    QColor bgColor = palette().color(QPalette::Base);
    QColor borderColor = palette().color(QPalette::Mid);
    QColor textColor = palette().color(QPalette::Text);
    QColor selBgColor = palette().color(QPalette::Highlight);
    QColor selTextColor = palette().color(QPalette::HighlightedText);
    QColor separatorColor = palette().color(QPalette::Mid);

    int cw = cellWidth();
    int totalW = _channel_count * cw;

    p.setPen(Qt::NoPen);
    p.setBrush(bgColor);
    p.drawRoundedRect(kPadding, kPadding, totalW, kCellHeight, kBorderRadius, kBorderRadius);

    for (int i = 0; i < _channel_count; i++) {
        int x = kPadding + i * cw;

        if (_has_focus && i == _cursor_pos) {
            p.setPen(Qt::NoPen);
            p.setBrush(selBgColor);
            p.drawRect(x, kPadding, cw, kCellHeight);
        }

        if (i > 0) {
            p.setPen(QPen(separatorColor, 1));
            p.drawLine(x, kPadding + 2, x, kPadding + kCellHeight - 2);
        }

        QChar ch = (i < _chars.size()) ? _chars[i] : 'X';
        QFont font("Source Code Pro");
        font.setStyleHint(QFont::Monospace);
        font.setFixedPitch(true);
        font.setPointSizeF(10);
        p.setFont(font);

        if (_has_focus && i == _cursor_pos)
            p.setPen(selTextColor);
        else
            p.setPen(textColor);

        p.drawText(QRect(x, kPadding, cw, kCellHeight), Qt::AlignCenter, QString(ch));
    }

    p.setPen(QPen(borderColor, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(kPadding, kPadding, totalW, kCellHeight, kBorderRadius, kBorderRadius);
}

void SearchPatternInput::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();

    if (key == Qt::Key_Left) {
        if (_cursor_pos > 0)
            _cursor_pos--;
        update();
        return;
    }

    if (key == Qt::Key_Right) {
        if (_cursor_pos < _channel_count - 1)
            _cursor_pos++;
        update();
        return;
    }

    if (key == Qt::Key_Backspace) {
        if (_cursor_pos > 0) {
            _cursor_pos--;
            _chars[_cursor_pos] = 'X';
            emit pattern_changed();
        }
        update();
        return;
    }

    if (key == Qt::Key_Delete) {
        if (_cursor_pos < _channel_count) {
            _chars[_cursor_pos] = 'X';
            emit pattern_changed();
        }
        update();
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
        QWidget::keyPressEvent(event);
        return;
    }

    if (_cursor_pos < _channel_count) {
        _chars[_cursor_pos] = ch;
        if (_cursor_pos < _channel_count - 1)
            _cursor_pos++;
        emit pattern_changed();
    }
    update();
}

void SearchPatternInput::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        _cursor_pos = charIndexAt(event->x());
        _has_focus = true;
        update();
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
