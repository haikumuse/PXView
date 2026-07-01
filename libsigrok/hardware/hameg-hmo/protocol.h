/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 poljar (Damir Jelić) <poljarinho@gmail.com>
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

#ifndef LIBSIGROK_HARDWARE_HAMEG_HMO_PROTOCOL_H
#define LIBSIGROK_HARDWARE_HAMEG_HMO_PROTOCOL_H

#include <glib.h>
#include <stdint.h>
#include <string.h>
#include "hardware/compat/compat.h"

/* ===========================================================================
 * SCPI command enum — PXView's compat_scpi.h only defines SCPI_CMD_IDN/OPC
 * as macros. The hameg-hmo driver indexes a per-model scpi_dialect[] string
 * array by these enum values, so define the full enum locally (matches the
 * upstream libsigrok src/scpi.h ordering, starting at 1).
 * =========================================================================== */
#ifndef SCPI_CMD_GET_TIMEBASE
enum {
	SCPI_CMD_GET_TIMEBASE = 1,
	SCPI_CMD_SET_TIMEBASE,
	SCPI_CMD_GET_HORIZONTAL_DIV,
	SCPI_CMD_GET_VERTICAL_SCALE,
	SCPI_CMD_SET_VERTICAL_SCALE,
	SCPI_CMD_GET_TRIGGER_SOURCE,
	SCPI_CMD_SET_TRIGGER_SOURCE,
	SCPI_CMD_GET_TRIGGER_SLOPE,
	SCPI_CMD_SET_TRIGGER_SLOPE,
	SCPI_CMD_GET_TRIGGER_PATTERN,
	SCPI_CMD_SET_TRIGGER_PATTERN,
	SCPI_CMD_GET_HIGH_RESOLUTION,
	SCPI_CMD_SET_HIGH_RESOLUTION,
	SCPI_CMD_GET_PEAK_DETECTION,
	SCPI_CMD_SET_PEAK_DETECTION,
	SCPI_CMD_GET_COUPLING,
	SCPI_CMD_SET_COUPLING,
	SCPI_CMD_GET_HORIZ_TRIGGERPOS,
	SCPI_CMD_SET_HORIZ_TRIGGERPOS,
	SCPI_CMD_GET_ANALOG_CHAN_STATE,
	SCPI_CMD_SET_ANALOG_CHAN_STATE,
	SCPI_CMD_GET_DIG_CHAN_STATE,
	SCPI_CMD_SET_DIG_CHAN_STATE,
	SCPI_CMD_GET_VERTICAL_OFFSET,
	SCPI_CMD_GET_DIG_POD_STATE,
	SCPI_CMD_SET_DIG_POD_STATE,
	SCPI_CMD_GET_ANALOG_DATA,
	SCPI_CMD_GET_DIG_DATA,
	SCPI_CMD_GET_SAMPLE_RATE,
	SCPI_CMD_GET_PROBE_UNIT,
	SCPI_CMD_GET_DIG_POD_THRESHOLD,
	SCPI_CMD_SET_DIG_POD_THRESHOLD,
	SCPI_CMD_GET_DIG_POD_USER_THRESHOLD,
	SCPI_CMD_SET_DIG_POD_USER_THRESHOLD,
};
#endif

/* ===========================================================================
 * SR_CONF keys not defined in PXView's libsigrok.h. Use reserved compat
 * range 40600+ to avoid conflicts with PXView-native keys.
 * =========================================================================== */
#ifndef SR_CONF_TRIGGER_PATTERN
#define SR_CONF_TRIGGER_PATTERN        40600
#endif
#ifndef SR_CONF_HIGH_RESOLUTION
#define SR_CONF_HIGH_RESOLUTION        40601
#endif
#ifndef SR_CONF_PEAK_DETECTION
#define SR_CONF_PEAK_DETECTION        40602
#endif
#ifndef SR_CONF_LOGIC_THRESHOLD
#define SR_CONF_LOGIC_THRESHOLD        40603
#endif
#ifndef SR_CONF_LOGIC_THRESHOLD_CUSTOM
#define SR_CONF_LOGIC_THRESHOLD_CUSTOM 40604
#endif

#undef LOG_PREFIX
#define LOG_PREFIX "hameg-hmo"

#define DIGITAL_CHANNELS_PER_POD	8

#define MAX_INSTRUMENT_VERSIONS		10
#define MAX_COMMAND_SIZE		128
#define MAX_ANALOG_CHANNEL_COUNT	4
#define MAX_DIGITAL_CHANNEL_COUNT	16
#define MAX_DIGITAL_GROUP_COUNT		2

struct scope_config {
	const char *name[MAX_INSTRUMENT_VERSIONS];
	const uint8_t analog_channels;
	const uint8_t digital_channels;
	uint8_t digital_pods;

	const char *(*analog_names)[];
	const char *(*digital_names)[];

	const uint32_t (*devopts)[];
	const uint8_t num_devopts;

	const uint32_t (*devopts_cg_analog)[];
	const uint8_t num_devopts_cg_analog;

	const uint32_t (*devopts_cg_digital)[];
	const uint8_t num_devopts_cg_digital;

	const char *(*coupling_options)[];
	const uint8_t num_coupling_options;

	const char *(*logic_threshold)[];
	const uint8_t num_logic_threshold;
	const gboolean logic_threshold_for_pod;

	const char *(*trigger_sources)[];
	const uint8_t num_trigger_sources;

	const char *(*trigger_slopes)[];
	const uint8_t num_trigger_slopes;

	const uint64_t (*timebases)[][2];
	const uint8_t num_timebases;

	const uint64_t (*vdivs)[][2];
	const uint8_t num_vdivs;

	unsigned int num_xdivs;
	const unsigned int num_ydivs;

	const char *(*scpi_dialect)[];
};

struct analog_channel_state {
	int coupling;

	int vdiv;
	float vertical_offset;

	gboolean state;
	char probe_unit;
};

struct digital_pod_state {
	gboolean state;

	int threshold;
	float user_threshold;
};

struct scope_state {
	struct analog_channel_state *analog_channels;
	gboolean *digital_channels;
	struct digital_pod_state *digital_pods;

	int timebase;
	float horiz_triggerpos;

	int trigger_source;
	int trigger_slope;
	char trigger_pattern[MAX_ANALOG_CHANNEL_COUNT + MAX_DIGITAL_CHANNEL_COUNT + 1];

	gboolean high_resolution;
	gboolean peak_detection;

	uint64_t sample_rate;
};

struct dev_context {
	const void *model_config;
	void *model_state;

	struct sr_channel_group **analog_groups;
	struct sr_channel_group **digital_groups;

	GSList *enabled_channels;
	GSList *current_channel;
	uint64_t num_samples;
	uint64_t num_frames;

	uint64_t samples_limit;
	uint64_t frame_limit;

	size_t pod_count;
	GByteArray *logic_data;
};

SR_PRIV int hmo_init_device(struct sr_dev_inst *sdi);
SR_PRIV int hmo_request_data(const struct sr_dev_inst *sdi);
SR_PRIV int hmo_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);

SR_PRIV struct scope_state *hmo_scope_state_new(struct scope_config *config);
SR_PRIV void hmo_scope_state_free(struct scope_state *state);
SR_PRIV int hmo_scope_state_get(struct sr_dev_inst *sdi);
SR_PRIV int hmo_update_sample_rate(const struct sr_dev_inst *sdi);

/* std_session_send_df_frame_begin/end() are provided by compat_helpers.c. */

#endif
