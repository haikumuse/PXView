/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021-2023 Frank Stettner <frank-stettner@gmx.net>
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

#ifndef LIBSIGROK_HARDWARE_ICSTATION_USBRELAY_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ICSTATION_USBRELAY_PROTOCOL_H

#include <glib.h>
#include <stdint.h>

#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "icstation-usbrelay"

/*
 * PXView does not define several standard sigrok config keys that this
 * driver needs. Provide them here with unique values that do not collide
 * with PXView's existing SR_CONF_* keys (which occupy the 10000-10006
 * device-type range, the 30000-30098 config range, and the 50000-50007
 * acquisition range). These compat values live in reserved gaps and
 * match the values used by other compat drivers (e.g. hp-59306a for
 * SR_CONF_MULTIPLEXER, scpi-pps for SR_CONF_ENABLED).
 */
#ifndef SR_CONF_MULTIPLEXER
#define SR_CONF_MULTIPLEXER 10010
#endif
#ifndef SR_CONF_ENABLED
#define SR_CONF_ENABLED 30200
#endif

/* Known models. */
enum icstation_model {
	ICSE012A = 1,
	ICSE013A,
	ICSE014A,
};

/* Supported device profiles */
struct ics_usbrelay_profile {
	enum icstation_model model;
	uint8_t id;
	const char *modelname;
	size_t nb_channels;
};

struct dev_context {
	size_t relay_count;
	uint8_t relay_mask;
	uint8_t relay_state;
};

struct channel_group_context {
	size_t index;
};

SR_PRIV int icstation_usbrelay_identify(struct sr_serial_dev_inst *serial,
	uint8_t *id);
SR_PRIV int icstation_usbrelay_start(const struct sr_dev_inst *sdi);
SR_PRIV int icstation_usbrelay_switch_cg(const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg, gboolean on);

#endif
