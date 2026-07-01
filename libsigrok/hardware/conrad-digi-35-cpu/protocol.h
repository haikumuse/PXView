/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Matthias Heidbrink <m-sigrok@heidbrink.biz>
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

#ifndef LIBSIGROK_HARDWARE_CONRAD_DIGI_35_CPU_PROTOCOL_H
#define LIBSIGROK_HARDWARE_CONRAD_DIGI_35_CPU_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "conrad-digi-35-cpu"

/*
 * PXView does not define several standard sigrok config keys that this
 * power supply driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30107 config range, and the
 * 50000-50007 acquisition range). These compat values live in reserved
 * gaps. The values mirror those used by the atten-pps3xxx, korad-kaxxxxp
 * and gwinstek-psp compat drivers so all power-supply drivers agree when
 * compiled together.
 */
#ifndef SR_CONF_POWER_SUPPLY
#define SR_CONF_POWER_SUPPLY 10008
#endif
#ifndef SR_CONF_VOLTAGE_TARGET
#define SR_CONF_VOLTAGE_TARGET 30221
#endif
#ifndef SR_CONF_CURRENT_LIMIT
#define SR_CONF_CURRENT_LIMIT 30223
#endif
#ifndef SR_CONF_OVER_CURRENT_PROTECTION_ENABLED
#define SR_CONF_OVER_CURRENT_PROTECTION_ENABLED 30226
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time. The Conrad DIGI 35 CPU driver only invokes
 * sr_sw_limits_init() (the limits field is initialized but never
 * queried, since this is a write-only power supply that uses
 * std_dummy_dev_acquisition_start/stop no-ops).
 */
struct sr_sw_limits {
	uint64_t limit_samples;
	uint64_t limit_msec;
	int64_t starttime_ms;
	uint64_t samples_read;
};

static inline void sr_sw_limits_init(struct sr_sw_limits *limits)
{
	if (!limits)
		return;
	memset(limits, 0, sizeof(*limits));
}

struct dev_context {
	struct sr_sw_limits limits;
};

/*
 * Local wrapper around PXView's 3-arg serial_timeout(). Standard sigrok's
 * serial_timeout() takes (serial, bytes) and derives the baudrate from the
 * serial port's internal state. PXView requires the baudrate explicitly, so
 * parse it from the serialcomm string (format like "9600/8n1"). Falls back
 * to a conservative default when parsing fails. Implemented in protocol.c.
 * Same pattern as atten-pps3xxx.
 */
SR_PRIV int conrad_serial_timeout(struct sr_serial_dev_inst *serial, int bytes);

SR_PRIV int send_msg1(const struct sr_dev_inst *sdi, char cmd, int param);

#endif
