/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2016 Vlad Ivanov <vlad.ivanov@lab-systems.ru>
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

#ifndef LIBSIGROK_HARDWARE_ROHDE_SCHWARZ_SME_0X_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ROHDE_SCHWARZ_SME_0X_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#define LOG_PREFIX "rohde-schwarz-sme-0x"

/*
 * PXView does not define several standard sigrok config keys that this
 * driver needs. Provide them here with unique values that do not collide
 * with PXView's existing SR_CONF_* keys (which occupy the 10000-10006
 * device-type range, the 30000-30098 config range, and the 50000-50007
 * acquisition range). These compat values live in reserved gaps and
 * match the values used by the rigol-dg compat driver.
 */
#ifndef SR_CONF_SIGNAL_GENERATOR
#define SR_CONF_SIGNAL_GENERATOR   10020
#endif
#ifndef SR_CONF_OUTPUT_FREQUENCY
#define SR_CONF_OUTPUT_FREQUENCY   30201
#endif
#ifndef SR_CONF_AMPLITUDE
#define SR_CONF_AMPLITUDE          30202
#endif

struct rs_sme0x_info {
	struct sr_dev_driver di;
	char *vendor;
	char *device;
};

struct rs_device_model {
	const char *model_str;
	double freq_max;
	double freq_min;
	double power_max;
	double power_min;
};

struct dev_context {
	const struct rs_device_model *model_config;
};

SR_PRIV int rs_sme0x_mode_remote(struct sr_scpi_dev_inst *scpi);
SR_PRIV int rs_sme0x_get_freq(const struct sr_dev_inst *sdi, double *freq);
SR_PRIV int rs_sme0x_get_power(const struct sr_dev_inst *sdi, double *power);
SR_PRIV int rs_sme0x_set_freq(const struct sr_dev_inst *sdi, double freq);
SR_PRIV int rs_sme0x_set_power(const struct sr_dev_inst *sdi, double power);

#endif
