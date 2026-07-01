/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2023 Gerhard Sittig <gerhard.sittig@gmx.net>
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

/*
 * PXView port of the libsigrok juntek-jds6600 driver.
 *
 * This is a DDS signal generator (function generator) that communicates via
 * USB CDC (virtual COM port). It is a pure control device -- applications
 * send configuration requests and optionally read back state, but the device
 * does not stream sample data. Acquisition start/stop therefore are local
 * no-ops (std_dummy_dev_acquisition_start/stop), matching the conrad-digi-35-cpu
 * and hp-59306a compat driver pattern.
 *
 * Migration notes (see AGENTS.md / task spec):
 * - Includes <libsigrok/libsigrok.h> and "libsigrok-internal.h" were replaced
 *   by "hardware/compat/compat.h" (Rule 1).
 * - dev_context has no enum sr_mq/sr_unit/sr_mqflag fields, so no enum-tag-to-
 *   int conversion was needed (Rule 14 / driver-specific point 4 N/A).
 * - The driver has no analog datafeed, no sr_analog_init / feed_queue_analog
 *   (Rule 14 / driver-specific point 5).
 */

#ifndef LIBSIGROK_HARDWARE_JUNTEK_JDS6600_PROTOCOL_H
#define LIBSIGROK_HARDWARE_JUNTEK_JDS6600_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include "hardware/compat/compat.h"

/*
 * PXView does not define the signal-generator config keys this driver needs.
 * Provide them locally with values matching rigol-dg (30200-30205 range).
 */
#ifndef SR_CONF_SIGNAL_GENERATOR
#define SR_CONF_SIGNAL_GENERATOR   10020
#endif
#ifndef SR_CONF_ENABLED
#define SR_CONF_ENABLED            30200
#endif
#ifndef SR_CONF_OUTPUT_FREQUENCY
#define SR_CONF_OUTPUT_FREQUENCY   30201
#endif
#ifndef SR_CONF_AMPLITUDE
#define SR_CONF_AMPLITUDE          30202
#endif
#ifndef SR_CONF_OFFSET
#define SR_CONF_OFFSET             30203
#endif
#ifndef SR_CONF_PHASE
#define SR_CONF_PHASE              30204
#endif
#ifndef SR_CONF_DUTY_CYCLE
#define SR_CONF_DUTY_CYCLE         30205
#endif

/*
 * ATTR_FMT_PRINTF is not defined in PXView's libsigrok-internal.h (standard
 * sigrok provides it there). Define locally with a guard so the printf format
 * attribute annotations on the va_list helpers in protocol.c compile cleanly.
 */
#ifndef ATTR_FMT_PRINTF
#define ATTR_FMT_PRINTF(fmt, var) __attribute__((format(printf, fmt, var)))
#endif

#undef LOG_PREFIX
#define LOG_PREFIX "juntek-jds6600"

#define MAX_GEN_CHANNELS	2

struct dev_context {
	struct devc_dev {
		unsigned int device_type;
		char *serial_number;
		uint64_t max_output_frequency;
		size_t channel_count_gen;
	} device;
	struct devc_wave {
		size_t builtin_count;
		size_t arbitrary_count;
		size_t names_count;
		const char **names;
		uint32_t *fw_codes;
	} waveforms;
	struct devc_chan {
		gboolean enabled;
		uint32_t waveform_code;
		size_t waveform_index;
		double output_frequency;
		double amplitude;
		double offset;
		double dutycycle;
	} channel_config[MAX_GEN_CHANNELS];
	double channels_phase;
	GString *quick_req;
};

SR_PRIV int jds6600_identify(struct sr_dev_inst *sdi);
SR_PRIV int jds6600_setup_devc(struct sr_dev_inst *sdi);

SR_PRIV int jds6600_get_chans_enable(const struct sr_dev_inst *sdi);
SR_PRIV int jds6600_get_waveform(const struct sr_dev_inst *sdi, size_t ch_idx);
SR_PRIV int jds6600_get_frequency(const struct sr_dev_inst *sdi, size_t ch_idx);
SR_PRIV int jds6600_get_amplitude(const struct sr_dev_inst *sdi, size_t ch_idx);
SR_PRIV int jds6600_get_offset(const struct sr_dev_inst *sdi, size_t ch_idx);
SR_PRIV int jds6600_get_dutycycle(const struct sr_dev_inst *sdi, size_t ch_idx);
SR_PRIV int jds6600_get_phase_chans(const struct sr_dev_inst *sdi);

SR_PRIV int jds6600_set_chans_enable(const struct sr_dev_inst *sdi);
SR_PRIV int jds6600_set_waveform(const struct sr_dev_inst *sdi, size_t ch_idx);
SR_PRIV int jds6600_set_frequency(const struct sr_dev_inst *sdi, size_t ch_idx);
SR_PRIV int jds6600_set_amplitude(const struct sr_dev_inst *sdi, size_t ch_idx);
SR_PRIV int jds6600_set_offset(const struct sr_dev_inst *sdi, size_t ch_idx);
SR_PRIV int jds6600_set_dutycycle(const struct sr_dev_inst *sdi, size_t ch_idx);
SR_PRIV int jds6600_set_phase_chans(const struct sr_dev_inst *sdi);

/*
 * Local no-op acquisition start/stop helpers (Rule 14 / driver-specific
 * point 1). The JDS6600 is a signal generator that does not stream sample
 * data, so these return SR_OK without registering any data source. Matches
 * the conrad-digi-35-cpu and hp-59306a compat pattern.
 */
SR_PRIV int std_dummy_dev_acquisition_start(struct sr_dev_inst *sdi);
SR_PRIV int std_dummy_dev_acquisition_stop(struct sr_dev_inst *sdi);

/*
 * Local dev_clear with callback (driver-specific point 2). PXView's compat
 * layer does not provide std_dev_clear_with_callback(). This implementation
 * iterates the driver's instance list, calls clear_helper on each devc to
 * release waveform names/codes/quick_req buffers, then delegates to
 * std_dev_clear_compat() to free the sdi list. Matches the atorch pattern.
 */
SR_PRIV int jds6600_dev_clear(const struct sr_dev_driver *di);

#endif
