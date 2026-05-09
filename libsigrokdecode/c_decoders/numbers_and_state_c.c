#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_RAW = 0,
    ANN_NUMBER,
    ANN_WARN,
    NUM_ANN,
};

typedef struct {
    int clk_edge;
    int bit_count;
    int interp;
    int format;
    int prev_clk;
    uint64_t ss_word;
    uint32_t data;
    int bits_collected;
    int out_ann;
} nas_state;

static struct srd_channel nas_channels[] = {
    {"clk", "Clock", "Clock", 0, SRD_CHANNEL_SCLK, NULL},
};

static struct srd_channel nas_optional_channels[] = {
    {"bit0", "Bit0", "Bit 0", 0, SRD_CHANNEL_SDATA, NULL},
    {"bit1", "Bit1", "Bit 1", 1, SRD_CHANNEL_SDATA, NULL},
    {"bit2", "Bit2", "Bit 2", 2, SRD_CHANNEL_SDATA, NULL},
    {"bit3", "Bit3", "Bit 3", 3, SRD_CHANNEL_SDATA, NULL},
    {"bit4", "Bit4", "Bit 4", 4, SRD_CHANNEL_SDATA, NULL},
    {"bit5", "Bit5", "Bit 5", 5, SRD_CHANNEL_SDATA, NULL},
    {"bit6", "Bit6", "Bit 6", 6, SRD_CHANNEL_SDATA, NULL},
    {"bit7", "Bit7", "Bit 7", 7, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option nas_options[] = {
    {"clkedge", NULL, "Clock edge", NULL, NULL},
    {"count", NULL, "Total bits count", NULL, NULL},
    {"interp", NULL, "Interpretation", NULL, NULL},
    {"format", NULL, "Number format", NULL, NULL},
};

static const char *nas_inputs[] = {"logic", NULL};
static const char *nas_outputs[] = {"numbers_and_state", NULL};
static const char *nas_tags[] = {"Encoding", "Util", NULL};

static const char *nas_ann_labels[][3] = {
    {"", "raw", "Raw pattern"},
    {"", "number", "Number"},
    {"", "warning", "Warning"},
};

static const int nas_row_raw_classes[] = {ANN_RAW, -1};
static const int nas_row_number_classes[] = {ANN_NUMBER, -1};
static const int nas_row_warnings_classes[] = {ANN_WARN, -1};
static const struct srd_c_ann_row nas_ann_rows[] = {
    {"raws", "Raw bits", nas_row_raw_classes, 1},
    {"numbers", "Numbers", nas_row_number_classes, 1},
    {"warnings", "Warnings", nas_row_warnings_classes, 1},
};

static void nas_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(nas_state)));
    }
    nas_state *s = (nas_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(nas_state));
    s->prev_clk = -1;
}

static void nas_start(struct srd_decoder_inst *di)
{
    nas_state *s = (nas_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "numbers_and_state");

    const char *ce = c_decoder_get_option_string(di, "clkedge", "rising");
    if (strcmp(ce, "falling") == 0)
        s->clk_edge = 2;
    else if (strcmp(ce, "either") == 0)
        s->clk_edge = 0;
    else
        s->clk_edge = 1;

    s->bit_count = (int)c_decoder_get_option_int(di, "count", 0);

    const char *interp = c_decoder_get_option_string(di, "interp", "unsigned");
    if (strcmp(interp, "signed") == 0)
        s->interp = 1;
    else if (strcmp(interp, "hex") == 0)
        s->interp = 3;
    else
        s->interp = 0;

    const char *fmt = c_decoder_get_option_string(di, "format", "-");
    if (strcmp(fmt, "bin") == 0)
        s->format = 1;
    else if (strcmp(fmt, "oct") == 0)
        s->format = 2;
    else if (strcmp(fmt, "dec") == 0)
        s->format = 3;
    else if (strcmp(fmt, "hex") == 0)
        s->format = 4;
    else
        s->format = 0;
}

static void nas_output_word(struct srd_decoder_inst *di, nas_state *s, uint64_t es)
{
    int num_bits = s->bits_collected;
    if (num_bits == 0)
        return;

    char raw_str[65];
    for (int i = 0; i < num_bits && i < 64; i++) {
        int bit = (s->data >> (num_bits - 1 - i)) & 1;
        raw_str[i] = bit ? '1' : '0';
    }
    raw_str[num_bits] = '\0';
    C_ANN_PUT(di, s->ss_word, es, s->out_ann, ANN_RAW, raw_str);

    char num_str[80];
    if (s->interp == 1 && num_bits <= 32) {
        int32_t signed_val = (int32_t)s->data;
        if (num_bits < 32)
            signed_val = (int32_t)(s->data << (32 - num_bits)) >> (32 - num_bits);
        if (s->format == 4)
            snprintf(num_str, sizeof(num_str), "0x%X", (uint32_t)signed_val);
        else if (s->format == 1)
            snprintf(num_str, sizeof(num_str), "0b%s", raw_str);
        else
            snprintf(num_str, sizeof(num_str), "%d", signed_val);
    } else {
        if (s->format == 4)
            snprintf(num_str, sizeof(num_str), "0x%X", s->data);
        else if (s->format == 1)
            snprintf(num_str, sizeof(num_str), "0b%s", raw_str);
        else if (s->format == 2)
            snprintf(num_str, sizeof(num_str), "0o%o", s->data);
        else if (s->format == 3)
            snprintf(num_str, sizeof(num_str), "%u", s->data);
        else
            snprintf(num_str, sizeof(num_str), "0x%X", s->data);
    }
    C_ANN_PUT(di, s->ss_word, es, s->out_ann, ANN_NUMBER, num_str);

    s->data = 0;
    s->bits_collected = 0;
}

static void nas_decode(struct srd_decoder_inst *di)
{
    nas_state *s = (nas_state *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        if (s->clk_edge == 1)
            c_cond_rise(cb, 0);
        else if (s->clk_edge == 2)
            c_cond_fall(cb, 0);
        else
            c_cond_edge(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        uint32_t val = 0;
        int bits_read = 0;
        for (int i = 0; i < 8; i++) {
            if (c_decoder_has_channel(di, i + 1)) {
                int bit = c_decoder_get_pin(di, i + 1, samplenum);
                val |= ((uint32_t)bit << i);
                bits_read++;
            }
        }

        if (bits_read == 0)
            continue;

        if (s->bits_collected == 0)
            s->ss_word = samplenum;

        s->data |= (val << s->bits_collected);
        s->bits_collected += bits_read;

        if (s->bit_count > 0 && s->bits_collected >= s->bit_count) {
            nas_output_word(di, s, samplenum);
        }
    }
}

static void nas_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder numbers_and_state_c_decoder = {
    .id = "numbers_and_state_c",
    .name = "Numbers And State(C)",
    .longname = "Interpret bit patters as numbers or state enums (C)",
    .desc = "Interpret bit patterns as different kinds of numbers (integer, float, enum). (C implementation)",
    .license = "gplv2+",
    .channels = nas_channels,
    .num_channels = 1,
    .optional_channels = nas_optional_channels,
    .num_optional_channels = 8,
    .options = nas_options,
    .num_options = 4,
    .num_annotations = NUM_ANN,
    .ann_labels = nas_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = nas_ann_rows,
    .inputs = nas_inputs,
    .num_inputs = 1,
    .outputs = nas_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = nas_tags,
    .num_tags = 2,
    .reset = nas_reset,
    .start = nas_start,
    .decode = nas_decode,
    .destroy = nas_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    nas_options[0].def = g_variant_new_string("rising");
    nas_options[1].def = g_variant_new_int64(0);
    nas_options[2].def = g_variant_new_string("unsigned");
    nas_options[3].def = g_variant_new_string("-");
    return &numbers_and_state_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
