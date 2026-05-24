/*
 * Copyright (C) 2022 Gerhard Sittig <gerhard.sittig@gmx.net>
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_SCS = 0,
    NUM_ANN,
};

enum scs_state {
    SCS_IDLE,
    SCS_HEADER1,
    SCS_ID,
    SCS_LENGTH,
    SCS_INSTRUCTION,
    SCS_PARAMS,
    SCS_CHECKSUM,
};

typedef struct {
    enum scs_state state;
    int telegram_idx;
    uint8_t id;
    uint8_t length;
    uint8_t instruction;
    uint8_t params[256];
    int param_count;
    uint8_t crc;
    int out_ann;
} scs_state;

static const char *scs_inputs[] = {"uart", NULL};
static const char *scs_tags[] = {"Embedded/industrial", "Networking", NULL};

static const char *scs_ann_labels[][3] = {
    {"", "scs", "SCS"},
};

static const int scs_row_scs_classes[] = {ANN_SCS};
static const struct srd_c_ann_row scs_ann_rows[] = {
    {"scs", "SCS", scs_row_scs_classes, 1},
};

static const char *scs_instruction_name(uint8_t instr)
{
    switch (instr) {
    case 0x01: return "PING";
    case 0x02: return "READ";
    case 0x03: return "WRITE";
    case 0x04: return "REG WRITE";
    case 0x05: return "ACTION";
    case 0x06: return "RESET";
    case 0x07: return "SYNC WRITE";
    case 0x08: return "BULK READ";
    default: return "UNKNOWN";
    }
}

static void scs_reset_state(scs_state *s)
{
    s->state = SCS_IDLE;
    s->telegram_idx = 0;
    s->id = 0;
    s->length = 0;
    s->instruction = 0;
    s->param_count = 0;
    s->crc = 0;
}

static void scs_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    scs_state *s = (scs_state *)c_decoder_get_private(di);
    if (!s)
        return;

    if (strcmp(cmd, "DATA") != 0)
        return;
    if (data_len < 1)
        return;

    uint8_t val = data[0];

    switch (s->state) {
    case SCS_IDLE:
        if (val == 0xFF) {
            s->state = SCS_HEADER1;
            s->crc = val;
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_SCS, "Header: 0xFF");
        }
        break;

    case SCS_HEADER1:
        if (val == 0xFF) {
            s->state = SCS_ID;
            s->crc ^= val;
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_SCS, "Header: 0xFF");
        } else {
            scs_reset_state(s);
        }
        break;

    case SCS_ID:
        s->id = val;
        s->crc ^= val;
        s->state = SCS_LENGTH;
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "ID: %d", val);
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_SCS, buf);
        }
        break;

    case SCS_LENGTH:
        s->length = val;
        s->crc ^= val;
        s->state = SCS_INSTRUCTION;
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "Length: %d", val);
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_SCS, buf);
        }
        break;

    case SCS_INSTRUCTION:
        s->instruction = val;
        s->crc ^= val;
        s->param_count = 0;
        if (s->length > 2)
            s->state = SCS_PARAMS;
        else
            s->state = SCS_CHECKSUM;
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "Instruction: %s (0x%02X)", scs_instruction_name(val), val);
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_SCS, buf);
        }
        break;

    case SCS_PARAMS:
        s->crc ^= val;
        if (s->param_count < (int)sizeof(s->params))
            s->params[s->param_count] = val;
        s->param_count++;
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "Param[%d]: 0x%02X", s->param_count - 1, val);
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_SCS, buf);
        }
        /* length includes instruction + params + checksum */
        if (s->param_count >= s->length - 2)
            s->state = SCS_CHECKSUM;
        break;

    case SCS_CHECKSUM:
        {
            uint8_t expected = ~s->crc;
            if (val == expected) {
                char buf[32];
                snprintf(buf, sizeof(buf), "Checksum OK: 0x%02X", val);
                C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_SCS, buf);
            } else {
                char buf[64];
                snprintf(buf, sizeof(buf), "Checksum ERROR: got 0x%02X, expected 0x%02X", val, expected);
                C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_SCS, buf);
            }
        }
        scs_reset_state(s);
        break;
    }
}

static void scs_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(scs_state)));
    }
    scs_state *s = (scs_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(scs_state));
    scs_reset_state(s);
}

static void scs_start(struct srd_decoder_inst *di)
{
    scs_state *s = (scs_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "scs");
}

static void scs_decode(struct srd_decoder_inst *di)
{
    (void)di;
}

static void scs_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder scs_c_decoder = {
    .id = "scs_c",
    .name = "SCS(C)",
    .longname = "Sistema Cablaggio Semplificato (C)",
    .desc = "Fieldbus network protocol for home automation (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = scs_ann_labels,
    .num_annotation_rows = 1,
    .annotation_rows = scs_ann_rows,
    .inputs = scs_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = scs_tags,
    .num_tags = 2,
    .reset = scs_reset,
    .start = scs_start,
    .decode = scs_decode,
    .destroy = scs_destroy,
    .recv_proto = scs_recv_proto,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &scs_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
