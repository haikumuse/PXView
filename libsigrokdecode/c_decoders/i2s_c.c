#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

struct i2s_priv {
    int bit_depth;
    int msb_first;
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

#define ANN_WORD  0
#define ANN_FRAME 1
#define ANN_WARN  2
#define ANN_BIT   3

static struct srd_channel i2s_channels[] = {
    {"sclk", "SCLK", "Serial clock", 0, SRD_CHANNEL_SCLK, NULL},
    {"ws", "WS", "Word select", 1, SRD_CHANNEL_COMMON, NULL},
    {"sd", "SD", "Serial data", 2, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option i2s_options[] = {
    {"bit_depth", NULL, "Bit depth", NULL, NULL},
    {"msb_first", NULL, "MSB first", NULL, NULL},
};

static const char *i2s_ann_labels[][3] = {
    {"", "WORD", "Data word"},
    {"", "FRAME", "Frame"},
    {"", "WARN", "Warning"},
    {"", "BIT", "Data bit"},
};

static const int i2s_row_words_classes[] = {ANN_WORD};
static const int i2s_row_frames_classes[] = {ANN_FRAME};
static const int i2s_row_warnings_classes[] = {ANN_WARN};
static const struct srd_c_ann_row i2s_ann_rows[] = {
    {"words", "Words", i2s_row_words_classes, 1},
    {"frames", "Frames", i2s_row_frames_classes, 1},
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
    s->last_ws = -1;
}

static void i2s_start(struct srd_decoder_inst *di)
{
    struct i2s_priv *s = (struct i2s_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "i2s");
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
        c_cond_rise(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int ws = c_decoder_get_pin(di, 1, samplenum);
        int sd = c_decoder_get_pin(di, 2, samplenum);

        if (s->last_ws == -1) {
            s->last_ws = ws;
            s->ss_frame = samplenum;
            if (ws == 0)
                s->ss_left = samplenum;
            else
                s->ss_right = samplenum;
        }

        if (ws != s->last_ws) {
            if (s->last_ws == 0 && s->left_bits > 0) {
                char word_str[32];
                snprintf(word_str, sizeof(word_str), "L: 0x%0*X",
                         (s->bit_depth + 3) / 4, s->left_data);
                C_ANN_PUT(di, s->ss_left, samplenum, s->out_ann, ANN_WORD, word_str);
            } else if (s->last_ws == 1 && s->right_bits > 0) {
                char word_str[32];
                snprintf(word_str, sizeof(word_str), "R: 0x%0*X",
                         (s->bit_depth + 3) / 4, s->right_data);
                C_ANN_PUT(di, s->ss_right, samplenum, s->out_ann, ANN_WORD, word_str);
            }

            if (s->last_ws == 1) {
                C_ANN_PUT(di, s->ss_frame, samplenum, s->out_ann, ANN_FRAME, "Frame");
                s->ss_frame = samplenum;
            }

            s->left_data = 0;
            s->right_data = 0;
            s->left_bits = 0;
            s->right_bits = 0;
            s->last_ws = ws;
        }

        if (ws == 0) {
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

        char bit_str[8];
        snprintf(bit_str, sizeof(bit_str), "%d", sd);
        C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_BIT, bit_str);
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
    .num_options = 2,
    .num_annotations = 4,
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
    i2s_options[0].def = g_variant_new_int64(16);
    i2s_options[1].def = g_variant_new_string("yes");
    return &i2s_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
