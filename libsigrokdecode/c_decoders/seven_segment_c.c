#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_DIGIT = 0,
    NUM_ANN,
};

typedef struct {
    int polarity;
    int have_dp;
    int oldpins[8];
    uint64_t lastpos;
    int out_ann;
    int first_sample;
} seg7_state;

static struct srd_channel seg7_channels[] = {
    {"a", "A", "Segment A", 0, SRD_CHANNEL_SDATA, NULL},
    {"b", "B", "Segment B", 1, SRD_CHANNEL_SDATA, NULL},
    {"c", "C", "Segment C", 2, SRD_CHANNEL_SDATA, NULL},
    {"d", "D", "Segment D", 3, SRD_CHANNEL_SDATA, NULL},
    {"e", "E", "Segment E", 4, SRD_CHANNEL_SDATA, NULL},
    {"f", "F", "Segment F", 5, SRD_CHANNEL_SDATA, NULL},
    {"g", "G", "Segment G", 6, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_channel seg7_optional_channels[] = {
    {"dp", "DP", "Decimal point", 0, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option seg7_options[] = {
    {"polarity", NULL, "Expected polarity", NULL, NULL},
};

static const char *seg7_inputs[] = {"logic", NULL};
static const char *seg7_outputs[] = {NULL};
static const char *seg7_tags[] = {"Display", NULL};

static const char *seg7_ann_labels[][3] = {
    {"", "decoded-digit", "Decoded digit"},
};

static const int seg7_row_digit_classes[] = {ANN_DIGIT, -1};
static const struct srd_c_ann_row seg7_ann_rows[] = {
    {"decoded-digits", "Decoded digits", seg7_row_digit_classes, 1},
};

static char seg7_lookup(int a, int b, int c, int d, int e, int f, int g)
{
    int code = a | (b << 1) | (c << 2) | (d << 3) | (e << 4) | (f << 5) | (g << 6);
    switch (code) {
        case 0x00: return ' ';
        case 0x3F: return '0';
        case 0x06: return '1';
        case 0x5B: return '2';
        case 0x4F: return '3';
        case 0x66: return '4';
        case 0x6D: return '5';
        case 0x7D: return '6';
        case 0x07: return '7';
        case 0x7F: return '8';
        case 0x6F: return '9';
        case 0x77: return 'A';
        case 0x7C: return 'B';
        case 0x39: return 'C';
        case 0x5E: return 'D';
        case 0x79: return 'E';
        case 0x71: return 'F';
        default: return 0;
    }
}

static void seg7_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(seg7_state)));
    }
    seg7_state *s = (seg7_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(seg7_state));
    s->first_sample = 1;
}

static void seg7_start(struct srd_decoder_inst *di)
{
    seg7_state *s = (seg7_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "seven_segment");
    s->have_dp = c_decoder_has_channel(di, 7);
    const char *pol = c_decoder_get_option_string(di, "polarity", "common-cathode");
    s->polarity = (strcmp(pol, "common-anode") == 0) ? 1 : 0;
}

static void seg7_decode(struct srd_decoder_inst *di)
{
    seg7_state *s = (seg7_state *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 0);
        c_cond_or(cb);
        c_cond_edge(cb, 1);
        c_cond_or(cb);
        c_cond_edge(cb, 2);
        c_cond_or(cb);
        c_cond_edge(cb, 3);
        c_cond_or(cb);
        c_cond_edge(cb, 4);
        c_cond_or(cb);
        c_cond_edge(cb, 5);
        c_cond_or(cb);
        c_cond_edge(cb, 6);
        if (s->have_dp) {
            c_cond_or(cb);
            c_cond_edge(cb, 7);
        }
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int pins[8];
        for (int i = 0; i < 7; i++)
            pins[i] = c_decoder_get_pin(di, i, samplenum);
        if (s->have_dp)
            pins[7] = c_decoder_get_pin(di, 7, samplenum);
        else
            pins[7] = 0;

        if (s->first_sample) {
            s->first_sample = 0;
            memcpy(s->oldpins, pins, sizeof(s->oldpins));
            s->lastpos = samplenum;
            continue;
        }

        int old[8];
        memcpy(old, s->oldpins, sizeof(old));

        if (s->polarity) {
            if (s->have_dp) {
                for (int i = 0; i < 8; i++)
                    old[i] = 1 - old[i];
            } else {
                for (int i = 0; i < 7; i++)
                    old[i] = 1 - old[i];
            }
        }

        char digit = seg7_lookup(old[0], old[1], old[2], old[3], old[4], old[5], old[6]);
        if (digit != 0) {
            char str[4];
            if (s->have_dp && old[7] == 1)
                snprintf(str, sizeof(str), "%c.", digit);
            else
                snprintf(str, sizeof(str), "%c", digit);
            C_ANN_PUT(di, s->lastpos, samplenum, s->out_ann, ANN_DIGIT, str);
        }

        s->lastpos = samplenum;
        memcpy(s->oldpins, pins, sizeof(s->oldpins));
    }
}

static void seg7_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder segment_7_c_decoder = {
    .id = "seven_segment_c",
    .name = "Segment-7(C)",
    .longname = "7-segment display (C)",
    .desc = "7-segment display protocol. (C implementation)",
    .license = "gplv2+",
    .channels = seg7_channels,
    .num_channels = 7,
    .optional_channels = seg7_optional_channels,
    .num_optional_channels = 1,
    .options = seg7_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = seg7_ann_labels,
    .num_annotation_rows = 1,
    .annotation_rows = seg7_ann_rows,
    .inputs = seg7_inputs,
    .num_inputs = 1,
    .outputs = seg7_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = seg7_tags,
    .num_tags = 1,
    .reset = seg7_reset,
    .start = seg7_start,
    .decode = seg7_decode,
    .destroy = seg7_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    seg7_options[0].def = g_variant_new_string("common-cathode");
    GSList *pol_vals = NULL;
    pol_vals = g_slist_append(pol_vals, g_variant_new_string("common-cathode"));
    pol_vals = g_slist_append(pol_vals, g_variant_new_string("common-anode"));
    seg7_options[0].values = pol_vals;
    return &segment_7_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
