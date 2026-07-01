/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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

#ifndef LIBSIGROK_HARDWARE_OPENBENCH_LOGIC_SNIFFER_PROTOCOL_H
#define LIBSIGROK_HARDWARE_OPENBENCH_LOGIC_SNIFFER_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "openbench-logic-sniffer"

#define NUM_BASIC_TRIGGER_STAGES     4
#define CLOCK_RATE                   SR_MHZ(100)
#define MIN_NUM_SAMPLES              4
#define DEFAULT_SAMPLERATE           SR_KHZ(200)

/* Command opcodes */
#define CMD_RESET                     0x00
#define CMD_ARM_BASIC_TRIGGER         0x01
#define CMD_ID                        0x02
#define CMD_METADATA                  0x04
#define CMD_SET_DIVIDER               0x80
#define CMD_CAPTURE_SIZE              0x81
#define CMD_SET_FLAGS                 0x82
#define CMD_CAPTURE_DELAYCOUNT        0x83
#define CMD_CAPTURE_READCOUNT         0x84
#define CMD_SET_BASIC_TRIGGER_MASK0   0xC0
#define CMD_SET_BASIC_TRIGGER_VALUE0  0xC1
#define CMD_SET_BASIC_TRIGGER_CONFIG0 0xC2

/* Metadata tokens */
#define METADATA_TOKEN_END                    0x0
#define METADATA_TOKEN_DEVICE_NAME            0x1
#define METADATA_TOKEN_FPGA_VERSION           0x2
#define METADATA_TOKEN_ANCILLARY_VERSION      0x3
#define METADATA_TOKEN_NUM_PROBES_LONG        0x20
#define METADATA_TOKEN_SAMPLE_MEMORY_BYTES    0x21
#define METADATA_TOKEN_MAX_SAMPLE_RATE_HZ     0x23
#define METADATA_TOKEN_PROTOCOL_VERSION_LONG  0x24
#define METADATA_TOKEN_NUM_PROBES_SHORT       0x40
#define METADATA_TOKEN_PROTOCOL_VERSION_SHORT 0x41

/* Basic Trigger Config */
#define TRIGGER_START              (1 << 3)

/* Capture flags */
#define CAPTURE_FLAG_INTERNAL_TEST_MODE  (1 << 11)
#define CAPTURE_FLAG_EXTERNAL_TEST_MODE  (1 << 10)
#define CAPTURE_FLAG_SWAP_CHANNELS       (1 << 9)
#define CAPTURE_FLAG_RLE                 (1 << 8)
#define CAPTURE_FLAG_INVERT_EXT_CLOCK    (1 << 7)
#define CAPTURE_FLAG_CLOCK_EXTERNAL      (1 << 6)
#define CAPTURE_FLAG_NOISE_FILTER        (1 << 1)
#define CAPTURE_FLAG_DEMUX               (1 << 0)

#define OLS_NO_TRIGGER (-1)

struct dev_context {
	char **channel_names;
	size_t max_channels;
	uint32_t max_samples;
	uint32_t max_samplerate;
	uint32_t protocol_version;
	uint64_t cur_samplerate;
	uint32_t cur_samplerate_divider;
	uint64_t limit_samples;
	uint64_t capture_ratio;
	int trigger_at_smpl;
	uint16_t capture_flags;
	unsigned int num_transfers;
	unsigned int num_samples;
	int num_bytes;
	int cnt_bytes;
	int cnt_samples;
	int cnt_samples_rle;
	unsigned int rle_count;
	unsigned char sample[4];
	unsigned char *raw_sample_buf;
	uint16_t unitsize;
};

SR_PRIV extern const char *ols_channel_names[];

SR_PRIV int send_shortcommand(struct sr_serial_dev_inst *serial, uint8_t command);
SR_PRIV int send_longcommand(struct sr_serial_dev_inst *serial, uint8_t command, uint8_t *data);
SR_PRIV int ols_send_reset(struct sr_serial_dev_inst *serial);
SR_PRIV int ols_prepare_acquisition(const struct sr_dev_inst *sdi);
SR_PRIV uint32_t ols_channel_mask(const struct sr_dev_inst *sdi);
SR_PRIV int ols_get_metadata(struct sr_dev_inst *sdi);
SR_PRIV int ols_set_samplerate(const struct sr_dev_inst *sdi, uint64_t samplerate);
SR_PRIV void abort_acquisition(const struct sr_dev_inst *sdi);
SR_PRIV int ols_receive_data(int fd, int revents, void *cb_data);

#endif