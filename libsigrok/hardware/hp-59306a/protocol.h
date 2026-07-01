/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021 Frank Stettner <frank-stettner@gmx.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_HARDWARE_HP_59306A_PROTOCOL_H
#define LIBSIGROK_HARDWARE_HP_59306A_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "hp-59306a"

/*
 * PXView does not define several standard sigrok config keys that this
 * driver needs. Provide them here with unique values that do not collide
 * with PXView's existing SR_CONF_* keys (which occupy the 10000-10006
 * device-type range, the 30000-30098 config range, and the 50000-50007
 * acquisition range). These compat values live in reserved gaps and
 * match the values used by other compat drivers (e.g. scpi-pps for
 * SR_CONF_ENABLED).
 */
#ifndef SR_CONF_MULTIPLEXER
#define SR_CONF_MULTIPLEXER 10010
#endif
#ifndef SR_CONF_ENABLED
#define SR_CONF_ENABLED 30200
#endif

struct dev_context {
	size_t channel_count;
};

struct channel_group_context {
	/** The number of the channel group, as labeled on the device. */
	size_t number;
};

SR_PRIV int hp_59306a_switch_cg(const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg, gboolean enabled);

#endif
