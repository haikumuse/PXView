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
    ANN_HEADER = 0,
    ANN_PROPORTIONAL,
    ANN_DIGITAL,
    ANN_FRAME_LOST,
    ANN_FAILSAFE,
    ANN_FOOTER,
    ANN_WARN,
    NUM_ANN,
};

typedef struct {
    uint8_t bit_vals[256];
    uint64_t bit_ss[256];
    uint64_t bit_es[256];
    int num_bits;

    int sent_fields;
    int msg_complete;
    int failed;
    char fail_text[64];

    int prop_val_min;
    int prop_val_max;

    int out_ann;
    int out_python;
} sbus_state;

static const char *sbus_inputs[] = {"uart", NULL};
static const char *sbus_outputs[] = {"sbus_futaba", NULL};
static const char *sbus_tags[] = {"Remote Control", NULL};

static struct srd_decoder_option sbus_options[] = {
    {"prop_val_min", "dec_sbus_futaba_opt_prop_val_min", "Proportional value lower boundary", NULL, NULL},
    {"prop_val_max", "dec_sbus_futaba_opt_prop_val_max", "Proportional value upper boundary", NULL, NULL},
};

static const char *sbus_ann_labels[][3] = {
    {"", "header", "Header"},
    {"", "proportional", "Proportional"},
    {"", "digital", "Digital"},
    {"", "framelost", "Frame Lost"},
    {"", "failsafe", "Failsafe"},
    {"", "footer", "Footer"},
    {"", "warning", "Warning"},
};

static const int sbus_row_framing_classes[] = {ANN_HEADER, ANN_FOOTER, ANN_FRAME_LOST, ANN_FAILSAFE};
static const int sbus_row_channels_classes[] = {ANN_PROPORTIONAL, ANN_DIGITAL};
static const int sbus_row_warnings_classes[] = {ANN_WARN};
static const struct srd_c_ann_row sbus_ann_rows[] = {
    {"framing", "Framing", sbus_row_framing_classes, 4},
    {"channels", "Channels", sbus_row_channels_classes, 2},
    {"warnings", "Warnings", sbus_row_warnings_classes, 1},
};

static uint32_t bitpack_lsb(uint8_t *bits, int count)
{
    uint32_t val = 0;
    for (int i = 0; i < count && i < 32; i++)
        val |= ((uint32_t)(bits[i] & 1)) << i;
    return val;
}

static void sbus_reset_state(sbus_state *s)
{
    s->num_bits = 0;
    s->sent_fields = 0;
    s->msg_complete = 0;
    s->failed = 0;
    s->fail_text[0] = '\0';
}

static void sbus_get_ss_es_bits(sbus_state *s, int bitcount,
    uint64_t *out_ss, uint64_t *out_es, uint8_t *out_bits)
{
    if (s->num_bits < bitcount) {
        *out_ss = 0;
        *out_es = 0;
        return;
    }
    *out_ss = s->bit_ss[0];
    *out_es = s->bit_es[bitcount - 1];
    for (int i = 0; i < bitcount; i++)
        out_bits[i] = s->bit_vals[i];
    /* shift remaining bits */
    int remaining = s->num_bits - bitcount;
    if (remaining > 0) {
        memmove(s->bit_vals, s->bit_vals + bitcount, remaining);
        memmove(s->bit_ss, s->bit_ss + bitcount, remaining * sizeof(uint64_t));
        memmove(s->bit_es, s->bit_es + bitcount, remaining * sizeof(uint64_t));
    }
    s->num_bits = remaining;
}

static void sbus_flush_accum_bits(struct srd_decoder_inst *di, sbus_state *s)
{
    uint64_t ss, es;
    uint8_t bits[32];
    char buf[64];

    if (s->failed)
        return;

    /* Header byte (8 bits) */
    int upto = 1;
    while (s->sent_fields < upto) {
        if (s->num_bits < 8)
            return;
        sbus_get_ss_es_bits(s, 8, &ss, &es, bits);
        uint32_t value = bitpack_lsb(bits, 8);
        snprintf(buf, sizeof(buf), "0x%02x", value);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_HEADER, buf);
        if (value != 0x0f) {
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, "Unexpected header", "Header");
        }
        s->sent_fields++;
    }

    /* 16 proportional channels (11 bits each) */
    upto += 16;
    while (s->sent_fields < upto) {
        if (s->num_bits < 11)
            return;
        sbus_get_ss_es_bits(s, 11, &ss, &es, bits);
        uint32_t value = bitpack_lsb(bits, 11);
        snprintf(buf, sizeof(buf), "%d", value);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_PROPORTIONAL, buf);
        if ((int)value < s->prop_val_min) {
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, "Low proportional value", "Low value", "Low");
        }
        if ((int)value > s->prop_val_max) {
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, "High proportional value", "High value", "High");
        }
        s->sent_fields++;
    }

    /* 2 digital channels (1 bit each) */
    upto += 2;
    while (s->sent_fields < upto) {
        if (s->num_bits < 1)
            return;
        sbus_get_ss_es_bits(s, 1, &ss, &es, bits);
        uint32_t value = bitpack_lsb(bits, 1);
        snprintf(buf, sizeof(buf), "%d", value);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_DIGITAL, buf);
        s->sent_fields++;
    }

    /* Flags: framelost(1b), failsafe(1b) */
    upto += 2;
    while (s->sent_fields < upto) {
        if (s->num_bits < 1)
            return;
        sbus_get_ss_es_bits(s, 1, &ss, &es, bits);
        uint32_t value = bitpack_lsb(bits, 1);
        snprintf(buf, sizeof(buf), "%d", value);
        int idx = s->sent_fields - (upto - 2);
        int cls = ANN_FRAME_LOST + idx;
        C_ANN_PUT(di, ss, es, s->out_ann, cls, buf);
        s->sent_fields++;
    }

    /* MSB padding flags (4 bits), expect 0 */
    upto += 1;
    while (s->sent_fields < upto) {
        if (s->num_bits < 4)
            return;
        sbus_get_ss_es_bits(s, 4, &ss, &es, bits);
        uint32_t value = bitpack_lsb(bits, 4);
        if (value != 0x0) {
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, "Unexpected MSB flags", "Flags");
        }
        s->sent_fields++;
    }

    /* Footer byte (8 bits) */
    upto += 1;
    while (s->sent_fields < upto) {
        if (s->num_bits < 8)
            return;
        sbus_get_ss_es_bits(s, 8, &ss, &es, bits);
        uint32_t value = bitpack_lsb(bits, 8);
        snprintf(buf, sizeof(buf), "0x%02x", value);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_FOOTER, buf);
        if (value != 0x00) {
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, "Unexpected footer", "Footer");
        }
        s->sent_fields++;
    }

    /* Check for completion */
    if (s->sent_fields >= upto)
        s->msg_complete = 1;

    if (s->msg_complete && s->num_bits > 0) {
        s->failed = 1;
        snprintf(s->fail_text, sizeof(s->fail_text), "Excess data bits");
    }
}

static void sbus_handle_idle(struct srd_decoder_inst *di, sbus_state *s, uint64_t ss, uint64_t es)
{
    if (s->num_bits > 0 && !s->failed) {
        s->failed = 1;
        snprintf(s->fail_text, sizeof(s->fail_text), "Unprocessed data bits");
    }
    if (s->num_bits > 0 && s->failed) {
        uint64_t warn_ss = s->bit_ss[0];
        uint64_t warn_es = s->bit_es[s->num_bits - 1];
        C_ANN_PUT(di, warn_ss, warn_es, s->out_ann, ANN_WARN, s->fail_text);
    }
    sbus_reset_state(s);
}

static void sbus_handle_break(struct srd_decoder_inst *di, sbus_state *s, uint64_t ss, uint64_t es)
{
    if (!s->failed) {
        s->failed = 1;
        snprintf(s->fail_text, sizeof(s->fail_text), "BREAK condition");
    }
    /* Re-use idle logic for annotated bits warning */
    sbus_handle_idle(di, s, 0, 0);
    /* Annotate BREAK as warning */
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, "BREAK condition", "Break");
    sbus_reset_state(s);
}

static void sbus_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    sbus_state *s = (sbus_state *)c_decoder_get_private(di);
    if (!s)
        return;

    if (strcmp(cmd, "DATA") == 0) {
        if (data_len < 1) return;
        uint8_t byte_val = data[0];
        /* Expand byte to 8 bits LSB-first */
        for (int i = 0; i < 8; i++) {
            if (s->num_bits < 256) {
                s->bit_vals[s->num_bits] = (byte_val >> i) & 1;
                s->bit_ss[s->num_bits] = start_sample;
                s->bit_es[s->num_bits] = end_sample;
                s->num_bits++;
            }
        }
    } else if (strcmp(cmd, "FRAME") == 0) {
        if (data_len >= 3 && data[2] == 0) {
            s->failed = 1;
            snprintf(s->fail_text, sizeof(s->fail_text), "Invalid data");
        }
        sbus_flush_accum_bits(di, s);
    } else if (strcmp(cmd, "IDLE") == 0) {
        sbus_handle_idle(di, s, start_sample, end_sample);
    } else if (strcmp(cmd, "BREAK") == 0) {
        sbus_handle_break(di, s, start_sample, end_sample);
    }
}

static void sbus_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(sbus_state)));
    }
    sbus_state *s = (sbus_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(sbus_state));
    s->prop_val_min = 0;
    s->prop_val_max = 2047;
    sbus_reset_state(s);
}

static void sbus_start(struct srd_decoder_inst *di)
{
    sbus_state *s = (sbus_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "sbus_futaba");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "sbus_futaba");

    s->prop_val_min = (int)c_decoder_get_option_int(di, "prop_val_min", 0);
    s->prop_val_max = (int)c_decoder_get_option_int(di, "prop_val_max", 2047);
}

static void sbus_decode(struct srd_decoder_inst *di)
{
    (void)di;
}

static void sbus_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder sbus_futaba_c_decoder = {
    .id = "sbus_futaba_c",
    .name = "SBUS(C)",
    .longname = "Futaba SBUS Serial Bus (C)",
    .desc = "Serial bus for hobby remote control by Futaba (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = sbus_options,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = sbus_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = sbus_ann_rows,
    .inputs = sbus_inputs,
    .num_inputs = 1,
    .outputs = sbus_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = sbus_tags,
    .num_tags = 1,
    .reset = sbus_reset,
    .start = sbus_start,
    .decode = sbus_decode,
    .destroy = sbus_destroy,
    .recv_proto = sbus_recv_proto,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    sbus_options[0].def = g_variant_new_int64(0);
    sbus_options[1].def = g_variant_new_int64(2047);

    return &sbus_futaba_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
