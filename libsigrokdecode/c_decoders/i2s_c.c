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
    uint32_t left_data;
    uint32_t right_data;
    int left_bits;
    int right_bits;
    int last_ws;
    uint64_t ss_left;
    uint64_t ss_right;
    uint64_t ss_frame;
    int out_ann;
};

#define ANN_LEFT  0
#define ANN_RIGHT 1
#define ANN_WARN  2
#define NUM_ANN   3

static struct srd_channel i2s_channels[] = {
    {"sclk", "SCLK", "Serial clock", 0, SRD_CHANNEL_SCLK, NULL},
    {"ws", "WS", "Word select", 1, SRD_CHANNEL_COMMON, NULL},
    {"sd", "SD", "Serial data", 2, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option i2s_options[] = {
    {"ws_polarity", NULL, "WS polarity", NULL, NULL},
    {"clk_edge", NULL, "SCK active edge", NULL, NULL},
    {"bit_shift", NULL, "Bit shift", NULL, NULL},
    {"bit_align", NULL, "Bit align", NULL, NULL},
    {"bit_depth", NULL, "Bit depth", NULL, NULL},
    {"msb_first", NULL, "MSB first", NULL, NULL},
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
    {"left-words", "Left", i2s_row_left_classes, 1},
    {"right-words", "Right", i2s_row_right_classes, 1},
    {"warnings", "Warnings", i2s_row_warnings_classes, 1},
};

static const char *i2s_inputs[] = {"logic", NULL};
static const char *i2s_outputs[] = {"i2s", NULL};
static const char *i2s_tags[] = {"Audio", NULL};

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
    s->last_ws = -1;
}

static void i2s_start(struct srd_decoder_inst *di)
{
    struct i2s_priv *s = (struct i2s_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "i2s");

    const char *ws_pol = c_decoder_get_option_string(di, "ws_polarity", "left-high");
    s->ws_polarity_left_high = (strcmp(ws_pol, "left-high") == 0) ? 1 : 0;

    const char *clk_edge = c_decoder_get_option_string(di, "clk_edge", "rising-edge");
    s->clk_rising_edge = (strcmp(clk_edge, "rising-edge") == 0) ? 1 : 0;

    const char *bit_shift = c_decoder_get_option_string(di, "bit_shift", "none");
    s->bit_shift = (strcmp(bit_shift, "right-shifted by one") == 0) ? 1 : 0;

    const char *bit_align = c_decoder_get_option_string(di, "bit_align", "left-aligned");
    s->bit_align_left = (strcmp(bit_align, "left-aligned") == 0) ? 1 : 0;

    s->bit_depth = (int)c_decoder_get_option_int(di, "bit_depth", 16);
    const char *msb = c_decoder_get_option_string(di, "msb_first", "yes");
    s->msb_first = (strcmp(msb, "yes") == 0) ? 1 : 0;
}

static void i2s_decode(struct srd_decoder_inst *di)
{
    struct i2s_priv *s = (struct i2s_priv *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        if (s->clk_rising_edge)
            c_cond_rise(cb, 0);
        else
            c_cond_fall(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int ws = c_decoder_get_pin(di, 1, samplenum);
        int sd = c_decoder_get_pin(di, 2, samplenum);

        int ws_is_left = s->ws_polarity_left_high ? (ws == 0) : (ws == 1);

        if (s->last_ws == -1) {
            s->last_ws = ws;
            s->ss_frame = samplenum;
            if (ws_is_left)
                s->ss_left = samplenum;
            else
                s->ss_right = samplenum;
        }

        if (ws != s->last_ws) {
            int prev_was_left = s->ws_polarity_left_high ? (s->last_ws == 0) : (s->last_ws == 1);

            if (prev_was_left && s->left_bits > 0) {
                uint32_t val = s->left_data;
                if (s->bit_align_left && s->left_bits < s->bit_depth)
                    val = val >> (s->bit_depth - s->left_bits);
                char word_str[64];
                snprintf(word_str, sizeof(word_str), "L: 0x%0*X",
                         (s->bit_depth + 3) / 4, val);
                C_ANN_PUT(di, s->ss_left, samplenum, s->out_ann, ANN_LEFT, word_str);
            } else if (!prev_was_left && s->right_bits > 0) {
                uint32_t val = s->right_data;
                if (s->bit_align_left && s->right_bits < s->bit_depth)
                    val = val >> (s->bit_depth - s->right_bits);
                char word_str[64];
                snprintf(word_str, sizeof(word_str), "R: 0x%0*X",
                         (s->bit_depth + 3) / 4, val);
                C_ANN_PUT(di, s->ss_right, samplenum, s->out_ann, ANN_RIGHT, word_str);
            }

            s->left_data = 0;
            s->right_data = 0;
            s->left_bits = 0;
            s->right_bits = 0;
            s->last_ws = ws;
        }

        if (ws_is_left) {
            if (s->left_bits == 0)
                s->ss_left = samplenum;
            if (s->left_bits < s->bit_depth) {
                if (s->msb_first)
                    s->left_data = (s->left_data << 1) | sd;
                else
                    s->left_data |= ((uint32_t)sd << s->left_bits);
                s->left_bits++;
            } else {
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_WARN,
                           "Left channel overflow");
            }
        } else {
            if (s->right_bits == 0)
                s->ss_right = samplenum;
            if (s->right_bits < s->bit_depth) {
                if (s->msb_first)
                    s->right_data = (s->right_data << 1) | sd;
                else
                    s->right_data |= ((uint32_t)sd << s->right_bits);
                s->right_bits++;
            } else {
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_WARN,
                           "Right channel overflow");
            }
        }
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
    .binary = NULL,
    .num_binary = 0,
    .tags = i2s_tags,
    .num_tags = 1,
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
    i2s_options[4].def = g_variant_new_int64(16);
    i2s_options[5].def = g_variant_new_string("yes");
    return &i2s_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
