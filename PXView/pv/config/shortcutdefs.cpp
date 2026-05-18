/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
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

#include "shortcutdefs.h"
#include "../ui/langresource.h"

namespace pv {

static const ShortcutActionInfo g_shortcutActionInfos[] = {
    { SHORTCUT_RUN_STOP,       "S",      S_ID(IDS_DLG_SC_RUN_STOP)       },
    { SHORTCUT_INSTANT,        "I",      S_ID(IDS_DLG_SC_INSTANT)        },
    { SHORTCUT_TRIGGER,        "T",      S_ID(IDS_DLG_SC_TRIGGER)        },
    { SHORTCUT_DECODE,         "D",      S_ID(IDS_DLG_SC_DECODE)         },
    { SHORTCUT_MEASURE,        "M",      S_ID(IDS_DLG_SC_MEASURE)        },
    { SHORTCUT_SEARCH,         "R",      S_ID(IDS_DLG_SC_SEARCH)         },
    { SHORTCUT_OPTIONS,        "O",      S_ID(IDS_DLG_SC_OPTIONS)        },
    { SHORTCUT_DEVICE_SELECT,  "E",      S_ID(IDS_DLG_SC_DEVICE_SELECT)  },
    { SHORTCUT_PAGE_UP,        "PgUp",   S_ID(IDS_DLG_SC_PAGE_UP)        },
    { SHORTCUT_PAGE_DOWN,      "PgDown", S_ID(IDS_DLG_SC_PAGE_DOWN)      },
    { SHORTCUT_ZOOM_IN,        "Left",   S_ID(IDS_DLG_SC_ZOOM_IN)        },
    { SHORTCUT_ZOOM_OUT,       "Right",  S_ID(IDS_DLG_SC_ZOOM_OUT)       },
    { SHORTCUT_DSO_CH0,        "0",      S_ID(IDS_DLG_SC_DSO_CH0)        },
    { SHORTCUT_DSO_CH1,        "1",      S_ID(IDS_DLG_SC_DSO_CH1)        },
    { SHORTCUT_DSO_VUP,        "Up",     S_ID(IDS_DLG_SC_DSO_VUP)        },
    { SHORTCUT_DSO_VDOWN,      "Down",   S_ID(IDS_DLG_SC_DSO_VDOWN)      }
};

const ShortcutActionInfo* GetShortcutActionInfos(int* count)
{
    if (count) {
        *count = sizeof(g_shortcutActionInfos) / sizeof(g_shortcutActionInfos[0]);
    }
    return g_shortcutActionInfos;
}

} // namespace pv
