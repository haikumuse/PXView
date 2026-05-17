#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

struct i2s_priv {
    int bit_depth;
    int msb_first;
    int ws_polarity_left_high;
    int clk_rising_edge;
    int bit_shift;
    int bit_align_left;
    int oldws;
    int bitcount;
    uint32_t data;
    uint32_t last;
    int samplesreceived;
    uint64_t ss_block;
    int wrote_wav_header;
    int out_ann;
    int out_python;
    int out_binary;
};

#define ANN_LEFT  0
#define ANN_RIGHT 1
#define ANN_WARN  2
#define NUM_ANN   3

static struct srd_channel i2s_channels[] = {
    {"sck", "SCK", "Bit clock line", 0, SRD_CHANNEL_SCLK, NULL},
    {"ws", "WS", "Word select line", 1, SRD_CHANNEL_COMMON, NULL},
    {"sd", "SD", "Serial data line", 2, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option i2s_options[] = {
    {"ws_polarity", NULL, "WS polarity", NULL, NULL},
    {"clk_edge", NULL, "SCK active edge", NULL, NULL},
    {"bit_shift", NULL, "Bit shift", NULL, NULL},
    {"bit_align", NULL, "Bit align", NULL, NULL},
    {"bitorder", NULL, "Bit order", NULL, NULL},
    {"wordsize", NULL, "Word size", NULL, NULL},
};

static const char *i2s_ann_labels[][3] = {
    {"", "left", "Left channel"},
    {"", "right", "Right channel"},
    {"", "warnings", "Warnings"},
};

static const int i2s_row_left_classes[] = {ANN_LEFT, -1};
static const int i2s_row_right_classes[] = {ANN_RIGHT, -1};
static const int i2s_row_warnings_classes[] = {ANN_WARN, -1};
static const struct srd_c_ann_row i2s_ann_rows[] = {
    {"left", "Left channel", i2s_row_left_classes, 1},
    {"right", "Right channel", i2s_row_right_classes, 1},
    {"warnings", "Warnings", i2s_row_warnings_classes, 1},
};

static const char *i2s_inputs[] = {"logic", NULL};
static const char *i2s_outputs[] = {"i2s", NULL};
static const char *i2s_tags[] = {"Audio", "PC", NULL};

static const struct srd_decoder_binary i2s_binary[] = {
    {0, "wav", "WAV file"},
};

static void i2s_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct i2s_priv)));
    }
    struct i2s_priv *s = (struct i2s_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct i2s_priv));
    s->bit_depth = 16;
    s->msb_first = 1;
    s->ws_polarity_left_high = 1;
    s->clk_rising_edge = 1;
    s->bit_shift = 0;
    s->bit_align_left = 1;
    s->oldws = 1;
    s->bitcount = 0;
    s->data = 0;
    s->samplesreceived = 0;
    s->ss_block = 0;
    s->wrote_wav_header = 0;
}

static void i2s_start(struct srd_decoder_inst *di)
{
    struct i2s_priv *s = (struct i2s_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "i2s");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "i2s");
    s->out_binary = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "i2s");

    const char *ws_pol = c_decoder_get_option_string(di, "ws_polarity", "left-high");
    s->ws_polarity_left_high = (strcmp(ws_pol, "left-high") == 0) ? 1 : 0;

    const char *clk_edge = c_decoder_get_option_string(di, "clk_edge", "rising-edge");
    s->clk_rising_edge = (strcmp(clk_edge, "rising-edge") == 0) ? 1 : 0;

    const char *bit_shift = c_decoder_get_option_string(di, "bit_shift", "none");
    s->bit_shift = (strcmp(bit_shift, "right-shifted by one") == 0) ? 1 : 0;

    const char *bit_align = c_decoder_get_option_string(di, "bit_align", "left-aligned");
    s->bit_align_left = (strcmp(bit_align, "left-aligned") == 0) ? 1 : 0;

    s->bit_depth = (int)c_decoder_get_option_int(di, "wordsize", 16);
    const char *msb = c_decoder_get_option_string(di, "bitorder", "msb-first");
    s->msb_first = (strcmp(msb, "msb-first") == 0) ? 1 : 0;
}

static void i2s_decode(struct srd_decoder_inst *di)
{
    struct i2s_priv *s = (struct i2s_priv *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;
    int left_high = s->ws_polarity_left_high;
    int active_rising = s->clk_rising_edge;
    int right_shifted = s->bit_shift;
    int left_aligned = s->bit_align_left;
    int msb = s->msb_first;
    int wordlength = s->bit_depth;

    {
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 1);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;
        s->ss_block = samplenum;
        s->oldws = c_decoder_get_pin(di, 1, samplenum);
        if (right_shifted) {
            srd_cond_builder *cb2 = c_cond_new();
            if (active_rising)
                c_cond_rise(cb2, 0);
            else
                c_cond_fall(cb2, 0);
            ret = c_cond_wait(cb2, di, &samplenum, &matched);
            c_cond_free(cb2);
            if (ret != SRD_OK)
                return;
        }
    }

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        if (active_rising)
            c_cond_rise(cb, 0);
        else
            c_cond_fall(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int ws = c_decoder_get_pin(di, 1, samplenum);
        int sd = c_decoder_get_pin(di, 2, samplenum);

        if (!right_shifted && ws != s->oldws) {
            s->last = sd;
        } else {
            if (msb)
                s->data = (s->data << 1) | sd;
            else
                s->data = s->data | ((uint32_t)sd << s->bitcount);
            s->bitcount++;
        }

        if (ws == s->oldws)
            continue;

        s->samplesreceived++;

        if (right_shifted) {
            srd_cond_builder *cb2 = c_cond_new();
            if (active_rising)
                c_cond_fall(cb2, 0);
            else
                c_cond_rise(cb2, 0);
            ret = c_cond_wait(cb2, di, &samplenum, &matched);
            c_cond_free(cb2);
            if (ret != SRD_OK)
                return;
        }

        if (wordlength > s->bitcount) {
            char warn_str[128];
            snprintf(warn_str, sizeof(warn_str),
                     "Received %d-bit word, expected %d-bit word",
                     s->bitcount, wordlength);
            C_ANN_PUT(di, s->ss_block, samplenum, s->out_ann, ANN_WARN, warn_str);
        } else {
            uint32_t val = s->data;
            if ((left_aligned && msb) || (!left_aligned && !msb))
                val = val >> (s->bitcount - wordlength);
            else
                val = val & ((1u << wordlength) - 1);

            int oldws_for_channel = left_high ? s->oldws : !s->oldws;
            int idx = oldws_for_channel ? 0 : 1;
            const char *c1 = oldws_for_channel ? "Left channel" : "Right channel";
            const char *c2 = oldws_for_channel ? "Left" : "Right";
            const char *c3 = oldws_for_channel ? "L" : "R";

            char v_str[16];
            snprintf(v_str, sizeof(v_str), "%08x", val);

            char ann1[64], ann2[64], ann3[64];
            snprintf(ann1, sizeof(ann1), "%s: %s", c1, v_str);
            snprintf(ann2, sizeof(ann2), "%s: %s", c2, v_str);
            snprintf(ann3, sizeof(ann3), "%s: %s", c3, v_str);
            C_ANN_PUT(di, s->ss_block, samplenum, s->out_ann, idx, ann1, ann2, ann3, c3);

            unsigned char py_data[5];
            py_data[0] = c3[0];
            py_data[1] = val & 0xFF;
            py_data[2] = (val >> 8) & 0xFF;
            py_data[3] = (val >> 16) & 0xFF;
            py_data[4] = (val >> 24) & 0xFF;
            c_decoder_put_python(di, s->ss_block, samplenum, s->out_python, "DATA", py_data, sizeof(py_data));

            unsigned char bin_data[4];
            bin_data[0] = val & 0xFF;
            bin_data[1] = (val >> 8) & 0xFF;
            bin_data[2] = (val >> 16) & 0xFF;
            bin_data[3] = (val >> 24) & 0xFF;
            c_decoder_put_binary(di, s->ss_block, samplenum, s->out_binary, 0, sizeof(bin_data), bin_data);
        }

        s->data = right_shifted ? 0 : s->last;
        s->bitcount = right_shifted ? 0 : 1;
        s->ss_block = samplenum;
        s->oldws = ws;
    }
}

static void i2s_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder i2s_c_decoder = {
    .id = "i2s_c",
    .name = "I²S(C)",
    .longname = "Inter-IC Sound (C)",
    .desc = "I2S protocol decoder (C implementation)",
    .license = "gplv2+",
    .channels = i2s_channels,
    .num_channels = 3,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = i2s_options,
    .num_options = 6,
    .num_annotations = NUM_ANN,
    .ann_labels = i2s_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = i2s_ann_rows,
    .inputs = i2s_inputs,
    .num_inputs = 1,
    .outputs = i2s_outputs,
    .num_outputs = 1,
    .binary = i2s_binary,
    .num_binary = 1,
    .tags = i2s_tags,
    .num_tags = 2,
    .reset = i2s_reset,
    .start = i2s_start,
    .decode = i2s_decode,
    .destroy = i2s_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    i2s_options[0].def = g_variant_new_string("left-high");
    i2s_options[1].def = g_variant_new_string("rising-edge");
    i2s_options[2].def = g_variant_new_string("none");
    i2s_options[3].def = g_variant_new_string("left-aligned");
    i2s_options[4].def = g_variant_new_string("msb-first");
    i2s_options[5].def = g_variant_new_int64(16);
    return &i2s_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
