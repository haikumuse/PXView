/*
 * Copyright (C) 2023 DreamSourceLab <support@dreamsourcelab.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

#define J1708_BAUD 9600
#define MIN_BUS_ACCESS_BIT_TIMES 12
#define MAX_MSG_LEN 256

enum {
    ANN_DATUM = 0,
    ANN_INFO,
    ANN_ERROR,
    ANN_INLINE_ERROR,
    ANN_DELAY,
    ANN_BUS_ACCESS,
    NUM_ANN,
};

enum {
    BIN_MID = 0,
    BIN_PAYLOAD,
    BIN_CRC,
    NUM_BIN,
};

enum j1708_state {
    J1708_IDLE,
    J1708_IN_MESSAGE,
};

typedef struct {
    enum j1708_state state;
    uint8_t data[MAX_MSG_LEN];
    int data_len;
    uint64_t first_startbit_ss;
    uint64_t prev_stopbit_es;
    uint64_t last_valid_msg_stopbit_es;
    double bit_width;
    int message_break;
    int out_ann;
    int out_bin;
} j1708_state;

static const char *j1708_inputs[] = {"uart", NULL};
static const char *j1708_tags[] = {"Automotive", NULL};

static struct srd_decoder_option j1708_options[] = {
    {"message_break", "dec_j1708_opt_message_break", "Delay (in bit times) for message break", NULL, NULL},
};

static const char *j1708_ann_labels[][3] = {
    {"", "datum", "A J1708 message"},
    {"", "info", "Protocol info"},
    {"", "error", "Protocol violation or error"},
    {"", "inline_error", "Protocol violation or error"},
    {"", "delay", "Inter-message Delay [bit times]"},
    {"", "bus_access", "Bus Access time violation [bit times]"},
};

static const int j1708_row_fields_classes[] = {ANN_INFO, -1};
static const int j1708_row_data_classes[] = {ANN_DATUM, ANN_INLINE_ERROR, -1};
static const int j1708_row_errors_classes[] = {ANN_ERROR, ANN_BUS_ACCESS, -1};
static const int j1708_row_delays_classes[] = {ANN_DELAY, -1};

static const struct srd_c_ann_row j1708_ann_rows[] = {
    {"fields", "RX Fields", j1708_row_fields_classes, 1},
    {"data", "RX Data", j1708_row_data_classes, 2},
    {"errors", "RX Errors", j1708_row_errors_classes, 2},
    {"delays", "RX Message Delays", j1708_row_delays_classes, 1},
};

static const struct srd_decoder_binary j1708_binary[] = {
    {BIN_MID, "mid", "J1708 MID"},
    {BIN_PAYLOAD, "payload", "J1708 Payload"},
    {BIN_CRC, "crc", "J1708 Checksum"},
};

static uint8_t j1708_checksum(uint8_t *msg, int len)
{
    uint16_t sum = 0;
    for (int i = 0; i < len; i++)
        sum = (sum + msg[i]) & 0xFF;
    return (~sum + 1) & 0xFF;
}

static void j1708_flush_message(struct srd_decoder_inst *di, j1708_state *s)
{
    if (s->data_len == 0)
        return;

    s->last_valid_msg_stopbit_es = s->prev_stopbit_es;

    if (s->data_len < 2) {
        /* Too short for checksum validation */
        char buf[256];
        int pos = 0;
        for (int i = 0; i < s->data_len && pos < 200; i++)
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%02x", s->data[i]);
        C_ANN_PUT(di, s->first_startbit_ss, s->prev_stopbit_es, s->out_ann, ANN_INLINE_ERROR, buf);
        C_ANN_PUT(di, s->first_startbit_ss, s->prev_stopbit_es, s->out_ann, ANN_ERROR, "Message too short");
        s->data_len = 0;
        return;
    }

    /* Validate checksum */
    uint8_t calc_crc = j1708_checksum(s->data, s->data_len - 1);
    uint8_t recv_crc = s->data[s->data_len - 1];

    if (calc_crc != recv_crc) {
        /* Checksum error */
        char buf[256];
        int pos = 0;
        for (int i = 0; i < s->data_len - 1 && pos < 200; i++)
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%02x", s->data[i]);
        pos += snprintf(buf + pos, sizeof(buf) - pos, "(%02x)", recv_crc);
        C_ANN_PUT(di, s->first_startbit_ss, s->prev_stopbit_es, s->out_ann, ANN_INLINE_ERROR, buf);

        uint64_t crc_ss = (uint64_t)(s->prev_stopbit_es - s->bit_width * 10);
        C_ANN_PUT(di, crc_ss, s->prev_stopbit_es, s->out_ann, ANN_ERROR, "Checksum", "CRC");
    } else {
        /* Valid message */
        char buf[256];
        int pos = 0;
        for (int i = 0; i < s->data_len - 1 && pos < 200; i++)
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%02x", s->data[i]);
        C_ANN_PUT(di, s->first_startbit_ss, s->prev_stopbit_es, s->out_ann, ANN_DATUM, buf);

        /* MID field */
        char mid_buf[32];
        snprintf(mid_buf, sizeof(mid_buf), "MID: 0x%02x", s->data[0]);
        uint64_t mid_es = (uint64_t)(s->first_startbit_ss + s->bit_width * 10);
        C_ANN_PUT(di, s->first_startbit_ss, mid_es, s->out_ann, ANN_INFO, mid_buf, "MID");
        c_decoder_put_binary(di, s->first_startbit_ss, mid_es, s->out_bin, BIN_MID, 1, &s->data[0]);

        /* Payload field */
        if (s->data_len > 2) {
            char payload_buf[256];
            int ppos = 0;
            for (int i = 1; i < s->data_len - 1 && ppos < 200; i++)
                ppos += snprintf(payload_buf + ppos, sizeof(payload_buf) - ppos, "%02x", s->data[i]);
            uint64_t payload_es = (uint64_t)(s->prev_stopbit_es - s->bit_width * 10);
            C_ANN_PUT(di, mid_es, payload_es, s->out_ann, ANN_INFO, "Payload", payload_buf);
            c_decoder_put_binary(di, mid_es, payload_es, s->out_bin, BIN_PAYLOAD,
                                 s->data_len - 2, &s->data[1]);
        }

        /* CRC field */
        char crc_buf[32];
        snprintf(crc_buf, sizeof(crc_buf), "CRC: %02x", recv_crc);
        uint64_t crc_ss = (uint64_t)(s->prev_stopbit_es - s->bit_width * 10);
        C_ANN_PUT(di, crc_ss, s->prev_stopbit_es, s->out_ann, ANN_INFO, crc_buf, "CRC");
        c_decoder_put_binary(di, crc_ss, s->prev_stopbit_es, s->out_bin, BIN_CRC, 1, &recv_crc);
    }

    s->data_len = 0;
}

static void j1708_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    j1708_state *s = (j1708_state *)c_decoder_get_private(di);
    if (!s)
        return;

    /* Get bit_width from STARTBIT/STOPBIT */
    if (s->bit_width == 0) {
        if (strcmp(cmd, "STARTBIT") == 0 || strcmp(cmd, "STOPBIT") == 0) {
            s->bit_width = (double)(end_sample - start_sample);
            return;
        } else if (strcmp(cmd, "DATA") != 0) {
            return;
        }
    }

    /* Only process DATA, only RX */
    if (strcmp(cmd, "DATA") != 0)
        return;
    if (data_len < 2)
        return;

    uint8_t byte_val = data[0];
    uint8_t rxtx = data[1];
    if (rxtx != 0)
        return; /* J1708 is RX only */

    /* Check message break interval */
    if (s->prev_stopbit_es > 0 && s->bit_width > 0) {
        double delay_bits = (double)(start_sample - s->prev_stopbit_es) / s->bit_width;
        if ((int)delay_bits > s->message_break) {
            j1708_flush_message(di, s);
        }
        /* Output delay info */
        if (s->last_valid_msg_stopbit_es > 0) {
            double inter_delay = (double)(start_sample - s->last_valid_msg_stopbit_es) / s->bit_width;
            char buf[32];
            snprintf(buf, sizeof(buf), "%05.1f", inter_delay);
            C_ANN_PUT(di, s->last_valid_msg_stopbit_es, start_sample, s->out_ann, ANN_DELAY, buf);
            if (inter_delay < MIN_BUS_ACCESS_BIT_TIMES) {
                C_ANN_PUT(di, s->last_valid_msg_stopbit_es, start_sample, s->out_ann, ANN_BUS_ACCESS, buf);
            }
        }
    }

    /* Record first byte position */
    if (s->data_len == 0) {
        s->first_startbit_ss = start_sample;
    }

    if (s->data_len < MAX_MSG_LEN) {
        s->data[s->data_len++] = byte_val;
    }
    s->prev_stopbit_es = end_sample;
}

static void j1708_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(j1708_state)));
    }
    j1708_state *s = (j1708_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(j1708_state));
    s->state = J1708_IDLE;
    s->message_break = 2;
}

static void j1708_start(struct srd_decoder_inst *di)
{
    j1708_state *s = (j1708_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "j1708");
    s->out_bin = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "j1708");
    s->message_break = (int)c_decoder_get_option_int(di, "message_break", 2);
}

static void j1708_decode(struct srd_decoder_inst *di)
{
    (void)di;
}

static void j1708_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder j1708_c_decoder = {
    .id = "j1708_c",
    .name = "J1708(C)",
    .longname = "J1708 (C)",
    .desc = "J1708 truck/bus serial communication protocol. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = j1708_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = j1708_ann_labels,
    .num_annotation_rows = 4,
    .annotation_rows = j1708_ann_rows,
    .inputs = j1708_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = j1708_binary,
    .num_binary = 3,
    .tags = j1708_tags,
    .num_tags = 1,
    .reset = j1708_reset,
    .start = j1708_start,
    .decode = j1708_decode,
    .destroy = j1708_destroy,
    .recv_proto = j1708_recv_proto,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    j1708_options[0].def = g_variant_new_int64(2);
    GSList *vals = NULL;
    vals = g_slist_append(vals, g_variant_new_int64(2));
    vals = g_slist_append(vals, g_variant_new_int64(10));
    vals = g_slist_append(vals, g_variant_new_int64(12));
    j1708_options[0].values = vals;

    return &j1708_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
