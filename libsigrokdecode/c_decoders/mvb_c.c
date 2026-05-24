/*
 * This file is part of the PXView project.
 *
 * Copyright (C) 2024 DreamSourceLab <info@dreamsourcelab.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

#define PREAMBLE_MASTER 0x16465   /* 0b101100011100010101 */
#define PREAMBLE_SLAVE  0x15463   /* 0b101010100011100011 */
#define PREAMBLE_LENGTH 18
#define PREAMBLE_MASK   0x3FFFF   /* 18-bit mask */
#define MVB_CLOCK_RATE  3000000ULL /* 3 MHz */

enum mvb_ann {
    ANN_MASTER_PREAMBLE = 0,
    ANN_SLAVE_PREAMBLE,
    ANN_MASTER_DATA,
    ANN_F_CODE,
    ANN_SLAVE_DATA,
    ANN_CRC,
    ANN_CRC_ERROR,
    ANN_BIT,
    ANN_ADDR,
    NUM_ANN,
};

enum mvb_state {
    STATE_FIND_START,
    STATE_DECODING,
};

static const char *F_codes[] = {
    "PD 2B", "PD 4B", "PD 8B", "PD 16B", "PD 32B",
    "reserved", "reserved", "reserved",
    "Master transfer", "General event",
    "reserved", "reserved",
    "MD", "Group event", "Single event", "Device status"
};

typedef struct {
    int state;
    uint64_t matching_header_ticks;
    int received_master_header;
    int received_slave_header;
    int last_tick;
    int is_even_tick;
    uint8_t decoded_buffer[512];
    int decoded_len;
    uint64_t frame_data_begin;
    uint64_t mvb_samples_per_bit;
    uint64_t samples_per_tick;
    uint64_t sample_begin;
    uint64_t sample_end;
    uint64_t samplerate;
    int out_ann;
} mvb_priv;

static struct srd_channel mvb_channels[] = {
    { "mvb", "MVB", "TTL from RS485", 0, SRD_CHANNEL_SDATA, NULL },
};

static const char *mvb_ann_labels[][3] = {
    { "", "master_preamble", "Master preamble" },
    { "", "slave_preamble", "Slave preamble" },
    { "", "master_data", "Master data" },
    { "", "f_code", "Function code" },
    { "", "slave_data", "Slave data" },
    { "", "crc", "CRC" },
    { "", "crc_error", "CRC Error" },
    { "", "bit", "Bit" },
    { "", "addr", "Address" },
};

static const int mvb_row_bits_classes[] = { ANN_MASTER_PREAMBLE, ANN_SLAVE_PREAMBLE, ANN_BIT, -1 };
static const int mvb_row_crcs_classes[] = { ANN_CRC, -1 };
static const int mvb_row_data_classes[] = { ANN_MASTER_DATA, ANN_SLAVE_DATA, -1 };
static const int mvb_row_fcodes_classes[] = { ANN_F_CODE, ANN_ADDR, -1 };
static const int mvb_row_errors_classes[] = { ANN_CRC_ERROR, -1 };

static const struct srd_c_ann_row mvb_ann_rows[] = {
    { "bits", "Bits", mvb_row_bits_classes, 3 },
    { "crcs", "Check sequence", mvb_row_crcs_classes, 1 },
    { "ma-sl-data", "Data", mvb_row_data_classes, 2 },
    { "f_codes", "Function code", mvb_row_fcodes_classes, 2 },
    { "errors", "Decoding errors", mvb_row_errors_classes, 1 },
};

static const char *mvb_inputs[] = { "logic" };
static const char *mvb_tags[] = { "Frame" };

static uint8_t mvb_crc8(const uint8_t *data, int bit_len)
{
    uint8_t crc = 0;
    for (int i = 0; i < bit_len; i++) {
        uint8_t msb = (crc >> 7) & 1;
        crc = (crc << 1) & 0xFF;
        if (msb ^ data[i])
            crc ^= 0xE5;
    }
    return crc;
}

static int check_check_sequence(const uint8_t *frame_bits, int total_bits)
{
    int parity = 0;
    for (int i = 0; i < total_bits - 8; i++)
        parity ^= frame_bits[i];

    uint8_t calc_crc = mvb_crc8(frame_bits, total_bits - 8);
    uint8_t check = calc_crc ^ 0xFF;
    check ^= parity;

    uint8_t received = 0;
    for (int i = 0; i < 8; i++)
        received = (received << 1) | frame_bits[total_bits - 8 + i];

    return check == received;
}

static void reset_frame(mvb_priv *s)
{
    s->received_master_header = 0;
    s->received_slave_header = 0;
    s->decoded_len = 0;
    s->matching_header_ticks = 0;
    s->is_even_tick = 0;
    s->last_tick = 0;
    s->state = STATE_FIND_START;
}

static void process_master_frame(struct srd_decoder_inst *di, mvb_priv *s)
{
    if (s->decoded_len < 24) return;

    /* Master frame: 4-bit flag + 12-bit address + 8-bit CRC = 24 bits */
    int crc_ok = check_check_sequence(s->decoded_buffer, s->decoded_len);

    /* Address: bits 4-15 (12 bits, MSB first) */
    uint16_t addr = 0;
    for (int i = 4; i < 16 && i < s->decoded_len; i++)
        addr = (addr << 1) | s->decoded_buffer[i];

    /* F-code: bits 0-3 (4 bits, MSB first) */
    int fcode = 0;
    for (int i = 0; i < 4 && i < s->decoded_len; i++)
        fcode = (fcode << 1) | s->decoded_buffer[i];

    /* Master data annotation */
    char data_str[64];
    snprintf(data_str, sizeof(data_str), "Master: F=%s Addr=%d",
             (fcode < 16) ? F_codes[fcode] : "?", addr);
    C_ANN_PUT(di, s->frame_data_begin, s->sample_end, s->out_ann, ANN_MASTER_DATA, data_str);

    /* F-code annotation */
    if (fcode < 16) {
        char fc_str[64];
        snprintf(fc_str, sizeof(fc_str), "F-code: %s (%d)", F_codes[fcode], fcode);
        C_ANN_PUT(di, s->frame_data_begin, s->sample_end, s->out_ann, ANN_F_CODE, fc_str);
    }

    /* Address annotation */
    char addr_str[32];
    snprintf(addr_str, sizeof(addr_str), "Address: %d (0x%03X)", addr, addr);
    C_ANN_PUT(di, s->frame_data_begin, s->sample_end, s->out_ann, ANN_ADDR, addr_str);

    /* CRC annotation */
    char crc_str[32];
    snprintf(crc_str, sizeof(crc_str), "CRC: %s", crc_ok ? "OK" : "ERROR");
    C_ANN_PUT(di, s->frame_data_begin, s->sample_end, s->out_ann, ANN_CRC, crc_str);

    if (!crc_ok)
        C_ANN_PUT(di, s->frame_data_begin, s->sample_end, s->out_ann, ANN_CRC_ERROR, "CRC Error");
}

static void process_slave_frame(struct srd_decoder_inst *di, mvb_priv *s)
{
    if (s->decoded_len < 24) return;

    /* Slave frame: 16/32/64-bit data + 8-bit CRC */
    /* Determine data length by dividing into 72-bit segments (64 data + 8 CRC) */
    int data_bits = s->decoded_len - 8;
    if (data_bits <= 0) return;

    int crc_ok = check_check_sequence(s->decoded_buffer, s->decoded_len);

    /* Extract data bytes */
    int num_bytes = data_bits / 8;
    char data_hex[256];
    int pos = 0;
    for (int byte_idx = 0; byte_idx < num_bytes && pos < (int)sizeof(data_hex) - 4; byte_idx++) {
        uint8_t byte_val = 0;
        for (int bit = 0; bit < 8; bit++) {
            int idx = byte_idx * 8 + bit;
            if (idx < data_bits)
                byte_val = (byte_val << 1) | s->decoded_buffer[idx];
        }
        pos += snprintf(data_hex + pos, sizeof(data_hex) - pos, "%s%02X",
                        (byte_idx > 0) ? " " : "", byte_val);
    }

    char slave_str[256];
    snprintf(slave_str, sizeof(slave_str), "Slave: %s", data_hex);
    C_ANN_PUT(di, s->frame_data_begin, s->sample_end, s->out_ann, ANN_SLAVE_DATA, slave_str);

    char crc_str[32];
    snprintf(crc_str, sizeof(crc_str), "CRC: %s", crc_ok ? "OK" : "ERROR");
    C_ANN_PUT(di, s->frame_data_begin, s->sample_end, s->out_ann, ANN_CRC, crc_str);

    if (!crc_ok)
        C_ANN_PUT(di, s->frame_data_begin, s->sample_end, s->out_ann, ANN_CRC_ERROR, "CRC Error");
}

static int process_tick(struct srd_decoder_inst *di, mvb_priv *s, int tick_value, uint64_t samplenum)
{
    if (!s->received_master_header && !s->received_slave_header) {
        s->matching_header_ticks = ((s->matching_header_ticks << 1) | tick_value) & PREAMBLE_MASK;
        if (s->matching_header_ticks == PREAMBLE_MASTER) {
            s->received_master_header = 1;
            s->matching_header_ticks = 0;
            C_ANN_PUT(di, samplenum - (PREAMBLE_LENGTH * s->mvb_samples_per_bit / 2),
                      s->sample_end, s->out_ann, ANN_MASTER_PREAMBLE, "Master preamble", "MP");
            s->frame_data_begin = s->sample_end;
        }
        if (s->matching_header_ticks == PREAMBLE_SLAVE) {
            s->received_slave_header = 1;
            s->matching_header_ticks = 0;
            C_ANN_PUT(di, samplenum - (PREAMBLE_LENGTH * s->mvb_samples_per_bit / 2),
                      s->sample_end, s->out_ann, ANN_SLAVE_PREAMBLE, "Slave preamble", "SP");
            s->frame_data_begin = s->sample_end;
        }
        return 1;
    }

    /* Manchester decoding */
    if (!s->is_even_tick) {
        uint64_t bit_begin = s->sample_end - s->mvb_samples_per_bit;
        if (s->last_tick == 0 && tick_value == 1) {
            if (s->decoded_len < 512)
                s->decoded_buffer[s->decoded_len++] = 0;
            C_ANN_PUT(di, bit_begin, s->sample_end, s->out_ann, ANN_BIT, "0");
        } else if (s->last_tick == 1 && tick_value == 0) {
            if (s->decoded_len < 512)
                s->decoded_buffer[s->decoded_len++] = 1;
            C_ANN_PUT(di, bit_begin, s->sample_end, s->out_ann, ANN_BIT, "1");
        } else {
            /* Same bit twice = transition error = frame boundary */
            if (s->received_master_header)
                process_master_frame(di, s);
            if (s->received_slave_header)
                process_slave_frame(di, s);
            reset_frame(s);
            return 0;
        }
    }
    s->is_even_tick = !s->is_even_tick;
    s->last_tick = tick_value;
    return 1;
}

static void mvb_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(mvb_priv)));
    mvb_priv *s = (mvb_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(mvb_priv));
    s->state = STATE_FIND_START;
    s->out_ann = -1;
}

static void mvb_start(struct srd_decoder_inst *di)
{
    mvb_priv *s = (mvb_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "mvb");
}

static void mvb_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    mvb_priv *s = (mvb_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
        if (value > 0) {
            s->samples_per_tick = value / MVB_CLOCK_RATE;
            s->mvb_samples_per_bit = 2 * s->samples_per_tick;
        }
    }
}

static void mvb_decode(struct srd_decoder_inst *di)
{
    mvb_priv *s = (mvb_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched = 0;

    if (s->samplerate == 0)
        return;

    /* Wait for first falling edge */
    srd_cond_builder *cb = c_cond_new();
    c_cond_fall(cb, 0);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    s->sample_begin = samplenum;
    s->sample_end = samplenum;

    while (1) {
        cb = c_cond_new();
        c_cond_edge(cb, 0);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;

        uint64_t notch = samplenum - s->sample_end;
        s->sample_begin = s->sample_end;
        s->sample_end = samplenum;

        /* Convert notch length to tick count */
        int num_ticks = 0;
        if (s->samples_per_tick > 0) {
            num_ticks = (int)((notch + s->samples_per_tick / 2) / s->samples_per_tick);
            if (num_ticks < 1) num_ticks = 1;
            if (num_ticks > 4) num_ticks = 4;
        }

        /* Process each tick */
        int pin = c_decoder_get_pin(di, 0, samplenum);
        for (int t = 0; t < num_ticks; t++) {
            int tick_value = (t == num_ticks - 1) ? pin : (1 - pin);
            int cont = process_tick(di, s, tick_value, samplenum);
            if (!cont) break;
        }
    }
}

static void mvb_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder mvb_c_decoder = {
    .id = "mvb_c",
    .name = "MVB(C)",
    .longname = "Multifunction Vehicle Bus (C)",
    .desc = "Multifunction Vehicle Bus Manchester II with custom preamble. (C implementation)",
    .license = "gplv2+",
    .channels = mvb_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = mvb_ann_labels,
    .num_annotation_rows = 5,
    .annotation_rows = mvb_ann_rows,
    .inputs = mvb_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .tags = mvb_tags,
    .num_tags = 1,
    .binary = NULL,
    .num_binary = 0,
    .reset = mvb_reset,
    .start = mvb_start,
    .decode = mvb_decode,
    .destroy = mvb_destroy,
    .metadata = mvb_metadata,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &mvb_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
