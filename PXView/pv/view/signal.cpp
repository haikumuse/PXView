/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

  
#include <math.h> 
#include "signal.h"
#include "view.h"
#include "../dsvdef.h"
#include "../appcontrol.h"
#include "../sigsession.h"
#include "../data/signalmodel.h"

namespace pv {
namespace view {

Signal::Signal(sr_channel *probe) :
    Trace(probe->name, probe->index, probe->type),
    _probe(probe)
{
    session = AppControl::Instance()->GetSession();
}

Signal::Signal(const Signal &s, sr_channel *probe) :
    Trace((const Trace &)s), 
    _probe(probe),
    _local_enabled(s._local_enabled)
{   
    session = AppControl::Instance()->GetSession();
}

bool Signal::enabled()
{
    return _local_enabled;
}

void Signal::set_enabled(bool en)
{
    _local_enabled = en;
    // R2: 实时写回 Core (sr_channel->enabled)，让 SigSession::reload() 重建
    // SignalModel 时能读到正确的 enabled 状态。
    // 不在此处广播 DSV_MSG_DEVICE_OPTIONS_UPDATED: MainWindow::OnMessage 收到该
    // 消息会调 rebuild_signals() -> apply_model_properties() -> set_enabled()，
    // 形成无限循环。广播由用户交互入口负责（如 DeviceOptionsDock 已有广播）。
    if (_probe)
        _probe->enabled = en;
    // Task 6.1: 同步写回 Core SignalModel->enabled，保证 headless API 读取到最新状态。
    // 不广播：由调用方（用户交互入口）负责广播，避免 rebuild 循环。
    if (_probe && session) {
        auto model = session->get_signal_by_index(_probe->index);
        if (model)
            model->set_enabled(en);
    }
}

void Signal::set_name(QString name)
{
    Trace::set_name(name);
    g_free(_probe->name);
    _probe->name = g_strdup(name.toUtf8().data());
}
} // namespace view
} // namespace pv
