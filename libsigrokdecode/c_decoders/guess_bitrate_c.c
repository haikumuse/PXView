/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2021 Quard <2014500726@smail.xtu.edu.cn>
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

#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ANN_BITRATE = 0,
    NUM_ANN,
};

struct guess_bitrate_priv {
    uint64_t samplerate;
    uint64_t ss_edge;
    uint64_t bitwidth;
    int out_ann;
};

static struct srd_channel guess_bitrate_channels[] = {
    {"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, "dec_guess_bitrate_chan_data"},
};

static const char *guess_bitrate_ann_labels[][3] = {
    {"", "bitrate", "Bitrate / baudrate"},
};

static const int guess_bitrate_row_classes[] = {ANN_BITRATE};
static const struct srd_c_ann_row guess_bitrate_ann_rows[] = {
    {"bitrate", "Bitrate", guess_bitrate_row_classes, 1},
};

static const char *guess_bitrate_inputs[] = {"logic", NULL};
static const char *guess_bitrate_outputs[] = {NULL};
static const char *guess_bitrate_tags[] = {"Clock/timing", "Util", NULL};

static void guess_bitrate_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct guess_bitrate_priv)));
    }
    struct guess_bitrate_priv *s = (struct guess_bitrate_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct guess_bitrate_priv));
    s->out_ann = -1;
}

static void guess_bitrate_start(struct srd_decoder_inst *di)
{
    struct guess_bitrate_priv *s = (struct guess_bitrate_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "guess_bitrate");
    s->samplerate = c_decoder_get_samplerate(di);
}

static void guess_bitrate_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct guess_bitrate_priv *s = (struct guess_bitrate_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
    }
}

static void guess_bitrate_decode(struct srd_decoder_inst *di)
{
    struct guess_bitrate_priv *s = (struct guess_bitrate_priv *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    if (s->samplerate == 0)
        return;

    /* Get first edge on the data line */
    srd_cond_builder *cb = c_cond_new();
    c_cond_edge(cb, 0);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK)
        return;

    s->ss_edge = samplenum;

    while (1) {
        cb = c_cond_new();
        c_cond_edge(cb, 0);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        uint64_t b = samplenum - s->ss_edge;
        if (s->bitwidth == 0 || b < s->bitwidth) {
            s->bitwidth = b;
            uint64_t bitrate = s->samplerate / b;
            char buf[32];
            snprintf(buf, sizeof(buf), "%llu", (unsigned long long)bitrate);
            C_ANN_PUT(di, s->ss_edge, samplenum, s->out_ann, ANN_BITRATE, buf);
        }
        s->ss_edge = samplenum;
    }
}

static void guess_bitrate_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder guess_bitrate_c_decoder = {
    .id = "guess_bitrate_c",
    .name = "Guess bitrate(C)",
    .longname = "Guess bitrate/baudrate (C)",
    .desc = "Guess the bitrate/baudrate of a UART (or other) protocol. (C implementation)",
    .license = "gplv2+",
    .channels = guess_bitrate_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = guess_bitrate_ann_labels,
    .num_annotation_rows = 1,
    .annotation_rows = guess_bitrate_ann_rows,
    .inputs = guess_bitrate_inputs,
    .num_inputs = 1,
    .outputs = guess_bitrate_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = guess_bitrate_tags,
    .num_tags = 2,
    .reset = guess_bitrate_reset,
    .start = guess_bitrate_start,
    .decode = guess_bitrate_decode,
    .destroy = guess_bitrate_destroy,
    .metadata = guess_bitrate_metadata,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &guess_bitrate_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
