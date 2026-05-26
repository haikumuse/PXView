/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2023 Maciej Grela <enki@fsck.pl>
 * Copyright (C) 2023 ALIENTEK(正点原子) <39035605@qq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
    ANN_START_BIT = 0,
    ANN_BIT,
    ANN_PARITY,
    ANN_ACK,
    ANN_BROADCAST,
    ANN_MADDR,
    ANN_SADDR,
    ANN_CONTROL,
    ANN_DATALEN,
    ANN_BYTE,
    ANN_WARNING,
    NUM_ANN,
};

enum {
    CMD_READ_STATUS = 0x00,
    CMD_READ_DATA_LOCK = 0x03,
    CMD_READ_LOCK_ADDR_LO = 0x04,
    CMD_READ_LOCK_ADDR_HI = 0x05,
    CMD_READ_STATUS_UNLOCK = 0x06,
    CMD_READ_DATA = 0x07,
    CMD_WRITE_CMD_LOCK = 0x0a,
    CMD_WRITE_DATA_LOCK = 0x0b,
    CMD_WRITE_CMD = 0x0e,
    CMD_WRITE_DATA = 0x0f,
};

#define CH_BUS 0

typedef struct {
    uint64_t samplerate;
    int bus_polarity;    /* 0=idle-low, 1=idle-high */
    int ignore_nak;      /* 0=Disabled, 1=Enabled */
    int broadcast_bit;

    uint64_t bits_begin;
    uint64_t bits_end;

    int out_ann;
    int out_python;
} iebus_state;

static const char *cmd_names[] = {
    "READ_STATUS",         /* 0x00 */
    NULL, NULL,
    "READ_DATA_LOCK",      /* 0x03 */
    "READ_LOCK_ADDR_LO",   /* 0x04 */
    "READ_LOCK_ADDR_HI",   /* 0x05 */
    "READ_STATUS_UNLOCK",  /* 0x06 */
    "READ_DATA",           /* 0x07 */
    NULL, NULL,
    "WRITE_CMD_LOCK",      /* 0x0a */
    "WRITE_DATA_LOCK",     /* 0x0b */
    NULL, NULL,
    "WRITE_CMD",           /* 0x0e */
    "WRITE_DATA",          /* 0x0f */
};

static int popcount32(uint32_t v)
{
    int count = 0;
    while (v) {
        count++;
        v &= v - 1;
    }
    return count;
}

static void iebus_put(iebus_state *s, struct srd_decoder_inst *di,
                      uint64_t ss, uint64_t es, int ann_class, const char **txts)
{
    struct srd_c_annotation ann;
    ann.ann_class = ann_class;
    ann.ann_type = 0;
    ann.ann_text = (char **)txts;
    c_decoder_put(di, ss, es, s->out_ann, &ann);
}

static void iebus_put_python(iebus_state *s, struct srd_decoder_inst *di,
                             uint64_t ss, uint64_t es, const char *text)
{
    if (s->out_python < 0)
        return;
    c_decoder_put_python(di, ss, es, s->out_python, text, NULL, 0);
}

/* Read a single bit from the bus */
static int read_bit(iebus_state *s, struct srd_decoder_inst *di)
{
    uint64_t samplenum = 0;
    uint64_t matched = 0;

    /* Wait for sync edge */
    srd_cond_builder *cb = c_cond_new();
    if (s->bus_polarity == 0)
        c_cond_rise(cb, CH_BUS);
    else
        c_cond_fall(cb, CH_BUS);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK)
        return -1;

    uint64_t bit_start = samplenum;

    /* Sample 27us after sync edge */
    uint64_t skip_count = (uint64_t)(27e-6 * (double)s->samplerate);
    cb = c_cond_new();
    c_cond_skip(cb, skip_count);
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK)
        return -1;

    uint8_t pin = c_decoder_get_pin(di, CH_BUS, samplenum);
    int bit = (pin + 1) % 2;

    /* Invert for idle-high */
    if (s->bus_polarity == 1)
        bit = (bit + 1) % 2;

    /* Assume 33us bit length */
    uint64_t bit_end = bit_start + (uint64_t)(33e-6 * (double)s->samplerate);

    char bit_str[4];
    snprintf(bit_str, sizeof(bit_str), "%d", bit);
    const char *bit_txts[] = {bit_str, NULL};
    iebus_put(s, di, bit_start, bit_end, ANN_BIT, bit_txts);

    s->bits_begin = bit_start;
    s->bits_end = bit_end;

    return bit;
}

/* Read n bits from the bus, return value MSB first */
static int read_bits(iebus_state *s, struct srd_decoder_inst *di, int n, uint16_t *value)
{
    uint16_t v = 0;
    s->bits_begin = 0;
    s->bits_end = 0;

    for (int i = 0; i < n; i++) {
        int bit = read_bit(s, di);
        if (bit < 0)
            return -1;
        v = (v << 1) | bit;
    }

    if (value)
        *value = v;
    return 0;
}

/* Read value and return with ss/es */
static int read_value(iebus_state *s, struct srd_decoder_inst *di,
                      int num_bits, uint16_t *value, uint64_t *ss, uint64_t *es)
{
    if (read_bits(s, di, num_bits, value) < 0)
        return -1;
    if (ss) *ss = s->bits_begin;
    if (es) *es = s->bits_end;
    return 0;
}

/* Read broadcast bit */
static int read_broadcast_bit(iebus_state *s, struct srd_decoder_inst *di)
{
    int broadcast_bit = read_bit(s, di);
    if (broadcast_bit < 0)
        return -1;

    static const char *unicast_txts[] = {"Unicast", "Uni", "U", NULL};
    static const char *broadcast_txts[] = {"Broadcast", "Bro", "B", NULL};

    const char **anno;
    if (broadcast_bit == 1)
        anno = unicast_txts;
    else
        anno = broadcast_txts;

    iebus_put(s, di, s->bits_begin, s->bits_end, ANN_BROADCAST, anno);
    return broadcast_bit;
}

/* Read ACK/NAK bit */
static int read_ack_bit(iebus_state *s, struct srd_decoder_inst *di)
{
    int ack_bit = read_bit(s, di);
    if (ack_bit < 0)
        return -1;

    if (s->broadcast_bit == 1) {
        if (s->ignore_nak == 1)
            ack_bit = 0;

        if (ack_bit == 0) {
            const char *ack_txts[] = {"ACK", "A", NULL};
            iebus_put(s, di, s->bits_begin, s->bits_end, ANN_ACK, ack_txts);
        } else if (ack_bit == 1) {
            const char *nak_txts[] = {"NAK", "N", NULL};
            iebus_put(s, di, s->bits_begin, s->bits_end, ANN_ACK, nak_txts);
        }
    }

    return ack_bit;
}

/* Read parity bit and check */
static int read_parity_bit(iebus_state *s, struct srd_decoder_inst *di, int value)
{
    int parity_bit = read_bit(s, di);
    if (parity_bit < 0)
        return -1;

    const char *par_txts[] = {"Parity", "Par", "P", NULL};
    iebus_put(s, di, s->bits_begin, s->bits_end, ANN_PARITY, par_txts);

    int expected_parity = popcount32((uint32_t)value) % 2;
    if (expected_parity != parity_bit) {
        const char *warn_txts[] = {"Parity error", NULL};
        iebus_put(s, di, s->bits_begin, s->bits_end, ANN_WARNING, warn_txts);
    }

    return parity_bit;
}

/* Read header (start bit + broadcast bit) */
static int read_header(iebus_state *s, struct srd_decoder_inst *di,
                       int *start_bit, int *broadcast_bit_out,
                       uint64_t *ss, uint64_t *es)
{
    uint64_t samplenum = 0;
    uint64_t matched = 0;

    /* Wait for start edge */
    srd_cond_builder *cb = c_cond_new();
    if (s->bus_polarity == 0)
        c_cond_rise(cb, CH_BUS);
    else
        c_cond_fall(cb, CH_BUS);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK)
        return -1;

    uint64_t start_ss = samplenum;

    /* Wait for opposite edge */
    cb = c_cond_new();
    if (s->bus_polarity == 0)
        c_cond_fall(cb, CH_BUS);
    else
        c_cond_rise(cb, CH_BUS);
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK)
        return -1;

    uint64_t start_es = samplenum;

    /* Check start bit width >= 100us */
    double duration = (double)(start_es - start_ss) / (double)s->samplerate;
    if (duration < 100e-6) {
        const char *warn_txts[] = {"Startbit too short", "Too short", NULL};
        iebus_put(s, di, start_ss, start_es, ANN_WARNING, warn_txts);
        if (start_bit) *start_bit = 0;
        if (ss) *ss = start_ss;
        if (es) *es = start_es;
        return 0;
    }

    const char *start_txts[] = {"Start bit", "Start", "S", NULL};
    iebus_put(s, di, start_ss, start_es, ANN_START_BIT, start_txts);

    /* Read broadcast bit */
    int bb = read_broadcast_bit(s, di);
    if (bb < 0)
        return -1;

    if (start_bit) *start_bit = 1;
    if (broadcast_bit_out) *broadcast_bit_out = bb;
    if (ss) *ss = start_ss;
    if (es) *es = s->bits_end;
    return 0;
}

/* Handle data bytes */
static int handle_data_bytes(iebus_state *s, struct srd_decoder_inst *di, int data_len)
{
    while (data_len > 0) {
        uint16_t b;
        uint64_t ss, es;
        if (read_value(s, di, 8, &b, &ss, &es) < 0)
            return -1;

        char db_str[32];
        snprintf(db_str, sizeof(db_str), "Data: 0x%02x", b);
        char db_short[16];
        snprintf(db_short, sizeof(db_short), "0x%02x", b);
        const char *db_txts[] = {db_str, db_short, NULL};
        iebus_put(s, di, ss, es, ANN_BYTE, db_txts);

        int parity_bit = read_parity_bit(s, di, b);
        if (parity_bit < 0)
            return -1;

        int ack_bit = read_ack_bit(s, di);
        if (ack_bit < 0)
            return -1;

        /* Output PYTHON data */
        char py_str[128];
        snprintf(py_str, sizeof(py_str), "DATA_BYTE,%d,%d,%d,%llu,%llu",
                 b, parity_bit, ack_bit,
                 (unsigned long long)ss, (unsigned long long)es);
        iebus_put_python(s, di, ss, es, py_str);

        data_len--;

        /* NAK condition */
        if (s->broadcast_bit == 1 && ack_bit == 1)
            break;
    }
    return 0;
}

static struct srd_channel iebus_channels[] = {
    {"bus", "BUS", "Bus input", 0, SRD_CHANNEL_SDATA, "dec_iebus_chan_bus"},
};

static struct srd_decoder_option iebus_options[] = {
    {"mode", NULL, "Mode", NULL, NULL},
    {"bus_polarity", NULL, "Bus polarity", NULL, NULL},
    {"ignore_nak", NULL, "Ignore NAK condition", NULL, NULL},
};

static const char *iebus_ann_labels[][3] = {
    {"", "start-bit", "Start bit"},
    {"", "bit", "Bit"},
    {"", "parity", "Parity"},
    {"", "ack", "Acknowledge"},
    {"", "broadcast", "Broadcast flag"},
    {"", "maddr", "Master address"},
    {"", "saddr", "Slave address"},
    {"", "control", "Control"},
    {"", "datalen", "Data Length"},
    {"", "byte", "Data Byte"},
    {"", "warning", "Warning"},
};

static const int iebus_row_bits_classes[] = {ANN_START_BIT, ANN_BIT, ANN_PARITY, ANN_ACK};
static const int iebus_row_fields_classes[] = {ANN_BROADCAST, ANN_MADDR, ANN_SADDR, ANN_CONTROL, ANN_DATALEN, ANN_BYTE};
static const int iebus_row_warnings_classes[] = {ANN_WARNING};
static const struct srd_c_ann_row iebus_ann_rows[] = {
    {"bits", "Bits", iebus_row_bits_classes, 4},
    {"fields", "Raw Fields", iebus_row_fields_classes, 6},
    {"warnings", "Warnings", iebus_row_warnings_classes, 1},
};

static const char *iebus_inputs[] = {"logic"};
static const char *iebus_outputs[] = {"iebus"};
static const char *iebus_tags[] = {"Automotive"};

static void iebus_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(iebus_state)));
    iebus_state *s = (iebus_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(iebus_state));
    s->out_ann = 0;
    s->out_python = -1;
}

static void iebus_start(struct srd_decoder_inst *di)
{
    iebus_state *s = (iebus_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "iebus");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "iebus");

    const char *pol = c_decoder_get_option_string(di, "bus_polarity", "idle-low");
    s->bus_polarity = (strcmp(pol, "idle-high") == 0) ? 1 : 0;

    const char *nak = c_decoder_get_option_string(di, "ignore_nak", "Disabled");
    s->ignore_nak = (strcmp(nak, "Enabled") == 0) ? 1 : 0;

    s->samplerate = c_decoder_get_samplerate(di);
}

static void iebus_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    iebus_state *s = (iebus_state *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE)
        s->samplerate = value;
}

static void iebus_decode(struct srd_decoder_inst *di)
{
    iebus_state *s = (iebus_state *)c_decoder_get_private(di);

    /* Fallback samplerate */
    if (s->samplerate == 0)
        s->samplerate = c_decoder_get_samplerate(di);
    if (s->samplerate == 0)
        return;

    while (1) {
        int start_bit, broadcast_bit;
        uint64_t ss, es;

        if (read_header(s, di, &start_bit, &broadcast_bit, &ss, &es) < 0)
            return;

        if (!start_bit)
            continue;

        s->broadcast_bit = broadcast_bit;

        /* Output HEADER */
        char hdr_str[32];
        snprintf(hdr_str, sizeof(hdr_str), "HEADER,%d", broadcast_bit);
        iebus_put_python(s, di, ss, es, hdr_str);

        /* Master address (12 bits) */
        uint16_t master_addr;
        if (read_value(s, di, 12, &master_addr, &ss, &es) < 0)
            return;

        char ma_str[48], ma_short[16];
        snprintf(ma_str, sizeof(ma_str), "Master: 0x%03x", master_addr);
        snprintf(ma_short, sizeof(ma_short), "0x%03x", master_addr);
        const char *ma_txts[] = {ma_str, ma_short, NULL};
        iebus_put(s, di, ss, es, ANN_MADDR, ma_txts);

        int parity_bit = read_parity_bit(s, di, master_addr);
        if (parity_bit < 0)
            return;

        char ma_py[64];
        snprintf(ma_py, sizeof(ma_py), "MASTER ADDRESS,%d,%d", master_addr, parity_bit);
        iebus_put_python(s, di, ss, es, ma_py);

        /* Slave address (12 bits) */
        uint16_t slave_addr;
        if (read_value(s, di, 12, &slave_addr, &ss, &es) < 0)
            return;

        char sa_str[48], sa_short[16];
        snprintf(sa_str, sizeof(sa_str), "Slave: 0x%03x", slave_addr);
        snprintf(sa_short, sizeof(sa_short), "0x%03x", slave_addr);
        const char *sa_txts[] = {sa_str, sa_short, NULL};
        iebus_put(s, di, ss, es, ANN_SADDR, sa_txts);

        parity_bit = read_parity_bit(s, di, slave_addr);
        if (parity_bit < 0)
            return;

        int ack_bit = read_ack_bit(s, di);
        if (ack_bit < 0)
            return;

        char sa_py[64];
        snprintf(sa_py, sizeof(sa_py), "SLAVE ADDRESS,%d,%d,%d", slave_addr, parity_bit, ack_bit);
        iebus_put_python(s, di, ss, es, sa_py);

        if (s->broadcast_bit == 1 && ack_bit == 1) {
            iebus_put_python(s, di, s->bits_begin, s->bits_end, "NAK");
            continue;
        }

        /* Control bits (4 bits) */
        uint16_t control;
        if (read_value(s, di, 4, &control, &ss, &es) < 0)
            return;

        parity_bit = read_parity_bit(s, di, control);
        if (parity_bit < 0)
            return;

        ack_bit = read_ack_bit(s, di);
        if (ack_bit < 0)
            return;

        const char *ctrl_name = NULL;
        if (control <= 0x0f && cmd_names[control])
            ctrl_name = cmd_names[control];

        if (ctrl_name) {
            char ctrl_str[64], ctrl_short[32];
            snprintf(ctrl_str, sizeof(ctrl_str), "Control: %s", ctrl_name);
            snprintf(ctrl_short, sizeof(ctrl_short), "%s", ctrl_name);
            const char *ctrl_txts[] = {ctrl_str, ctrl_short, NULL};
            iebus_put(s, di, ss, es, ANN_CONTROL, ctrl_txts);

            char ctrl_py[64];
            snprintf(ctrl_py, sizeof(ctrl_py), "CONTROL,%s,%d,%d", ctrl_name, parity_bit, ack_bit);
            iebus_put_python(s, di, ss, es, ctrl_py);
        } else {
            char ctrl_str[32], ctrl_short[16];
            snprintf(ctrl_str, sizeof(ctrl_str), "Control: 0x%02x", control);
            snprintf(ctrl_short, sizeof(ctrl_short), "0x%02x", control);
            const char *ctrl_txts[] = {ctrl_str, ctrl_short, NULL};
            iebus_put(s, di, ss, es, ANN_CONTROL, ctrl_txts);

            char ctrl_py[64];
            snprintf(ctrl_py, sizeof(ctrl_py), "CONTROL,%d,%d,%d", control, parity_bit, ack_bit);
            iebus_put_python(s, di, ss, es, ctrl_py);
        }

        if (s->broadcast_bit == 1 && ack_bit == 1) {
            iebus_put_python(s, di, s->bits_begin, s->bits_end, "NAK");
            continue;
        }

        /* Data length (8 bits) */
        uint16_t data_len;
        if (read_value(s, di, 8, &data_len, &ss, &es) < 0)
            return;

        parity_bit = read_parity_bit(s, di, data_len);
        if (parity_bit < 0)
            return;

        if (data_len == 0)
            data_len = 256;

        char dl_str[32], dl_short[16], dl_tiny[8];
        snprintf(dl_str, sizeof(dl_str), "Data Length: %d", data_len);
        snprintf(dl_short, sizeof(dl_short), "%d", data_len);
        snprintf(dl_tiny, sizeof(dl_tiny), "Len");
        const char *dl_txts[] = {dl_str, dl_short, dl_tiny, NULL};
        iebus_put(s, di, ss, es, ANN_DATALEN, dl_txts);

        if (data_len > 128) {
            const char *warn_txts[] = {
                "Message too long, mode 2 allows only for 128 bytes maximum",
                "Message too long", "Too long", NULL};
            iebus_put(s, di, ss, es, ANN_WARNING, warn_txts);
        }

        ack_bit = read_ack_bit(s, di);
        if (ack_bit < 0)
            return;

        char dl_py[64];
        snprintf(dl_py, sizeof(dl_py), "DATA LENGTH,%d,%d,%d", data_len, parity_bit, ack_bit);
        iebus_put_python(s, di, ss, es, dl_py);

        if (s->broadcast_bit == 1 && ack_bit == 1) {
            iebus_put_python(s, di, s->bits_begin, s->bits_end, "NAK");
            continue;
        }

        /* Data bytes */
        handle_data_bytes(s, di, data_len);
    }
}

static void iebus_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder iebus_c_decoder = {
    .id = "iebus_c",
    .name = "IEBus(C)",
    .longname = "Inter-Equipment Bus (C)",
    .desc = "Inter-Equipment Bus is an automotive communication bus used in Toyota and Honda vehicles (C implementation)",
    .license = "gplv3+",
    .channels = iebus_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = iebus_options,
    .num_options = 3,
    .num_annotations = NUM_ANN,
    .ann_labels = iebus_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = iebus_ann_rows,
    .reset = iebus_reset,
    .start = iebus_start,
    .decode = iebus_decode,
    .metadata = iebus_metadata,
    .destroy = iebus_destroy,
    .inputs = iebus_inputs,
    .num_inputs = 1,
    .outputs = iebus_outputs,
    .num_outputs = 1,
    .tags = iebus_tags,
    .num_tags = 1,
    .binary = NULL,
    .num_binary = 0,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    iebus_options[0].def = g_variant_new_string("Mode 2");
    {
        GVariant *v0 = g_variant_new_string("Mode 2");
        GSList *vals = g_slist_append(NULL, v0);
        iebus_options[0].values = vals;
    }
    iebus_options[1].def = g_variant_new_string("idle-low");
    {
        GVariant *v0 = g_variant_new_string("idle-low");
        GVariant *v1 = g_variant_new_string("idle-high");
        GSList *vals = g_slist_append(NULL, v0);
        vals = g_slist_append(vals, v1);
        iebus_options[1].values = vals;
    }
    iebus_options[2].def = g_variant_new_string("Disabled");
    {
        GVariant *v0 = g_variant_new_string("Disabled");
        GVariant *v1 = g_variant_new_string("Enabled");
        GSList *vals = g_slist_append(NULL, v0);
        vals = g_slist_append(vals, v1);
        iebus_options[2].values = vals;
    }
    return &iebus_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
