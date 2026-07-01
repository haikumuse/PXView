/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2016 Alexandru Gagniuc <mr.nuke.me@gmail.com>
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

#ifndef LIBSIGROK_HARDWARE_HP_3457A_PROTOCOL_H
#define LIBSIGROK_HARDWARE_HP_3457A_PROTOCOL_H

#include <stdint.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "hp-3457a"

/*
 * PXView's libsigrok.h does not define several standard sigrok config keys
 * that this DMM driver needs. Provide them here with unique values that do
 * not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30098 config range, and the
 * 50000-50007 acquisition range). These compat values live in a reserved
 * gap (30160+) next to the DSO/sound compat keys in compat_config.h, and
 * match the approach used by other compat drivers (e.g. hp-59306a).
 */
#ifndef SR_CONF_MEASURED_QUANTITY
#define SR_CONF_MEASURED_QUANTITY 30160
#endif
#ifndef SR_CONF_ADC_POWERLINE_CYCLES
#define SR_CONF_ADC_POWERLINE_CYCLES 30161
#endif

/*
 * PXView's libsigrok.h mqflags enum ends at SR_MQFLAG_SPL_PCT_OVER_ALARM
 * (0x10000) and does not include SR_MQFLAG_FOUR_WIRE, which this driver
 * uses for 4-wire resistance measurements. Define it here with the standard
 * sigrok value (0x200000), guarded so it does not clash if PXView later
 * adds it upstream.
 */
#ifndef SR_MQFLAG_FOUR_WIRE
#define SR_MQFLAG_FOUR_WIRE 0x200000
#endif

/* Information about the rear card option currently installed. */
enum card_type {
	CARD_UNKNOWN,
	REAR_TERMINALS,
	HP_44491A,
	HP_44492A,
};

struct rear_card_info {
	unsigned int card_id;
	enum card_type type;
	const char *name;
	const char *cg_name;
	unsigned int num_channels;
};

/* Possible states in an acquisition. */
enum acquisition_state {
	ACQ_TRIGGERED_MEASUREMENT,
	ACQ_REQUESTED_HIRES,
	ACQ_REQUESTED_RANGE,
	ACQ_GOT_MEASUREMENT,
	ACQ_REQUESTED_CHANNEL_SYNC,
	ACQ_GOT_CHANNEL_SYNC,
};

/* Channel connector (front terminals, or rear card. */
enum channel_conn {
	CONN_FRONT,
	CONN_REAR,
};

struct dev_context {
	/* Information about rear card option, or NULL if unknown */
	const struct rear_card_info *rear_card;

	enum sr_mq measurement_mq;
	enum sr_mqflag measurement_mq_flags;
	enum sr_unit measurement_unit;
	uint64_t limit_samples;
	float nplc;
	GSList *active_channels;
	unsigned int num_active_channels;
	struct sr_channel *current_channel;

	enum acquisition_state acq_state;
	enum channel_conn input_loc;
	uint64_t num_samples;
	double base_measurement;
	double hires_register;
	double measurement_range;
	double last_channel_sync;
};

struct channel_context {
	enum channel_conn location;
	int index;
};

SR_PRIV const struct rear_card_info *hp_3457a_probe_rear_card(struct sr_scpi_dev_inst *scpi);
/*
 * PXView's sr_receive_data_callback_t is typed as
 *   int (*)(int fd, int revents, const struct sr_dev_inst *sdi)
 * (see libsigrok-internal.h:303). The original sigrok source used
 *   int (int fd, int revents, void *cb_data)
 * so the signature is adapted here: the opaque cb_data is replaced by a
 * typed const sdi, and the body no longer needs to dereference cb_data.
 */
SR_PRIV int hp_3457a_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);
SR_PRIV int hp_3457a_set_mq(const struct sr_dev_inst *sdi, enum sr_mq mq,
			    enum sr_mqflag mq_flags);
SR_PRIV int hp_3457a_set_nplc(const struct sr_dev_inst *sdi, float nplc);
SR_PRIV int hp_3457a_select_input(const struct sr_dev_inst *sdi,
				  enum channel_conn loc);
SR_PRIV int hp_3457a_send_scan_list(const struct sr_dev_inst *sdi,
				    unsigned int *channels, size_t len);

#endif
