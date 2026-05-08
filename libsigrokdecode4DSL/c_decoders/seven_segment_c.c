#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum segment_7_state {
    STATE_IDLE,
};

typedef struct {
    enum segment_7_state state;
    uint64_t start_sample;
} segment_7_state;

static struct srd_channel segment_7_channels[] = {
    {"a", "A", "Segment A", 0, SRD_CHANNEL_SDATA, NULL},
    {"b", "B", "Segment B", 1, SRD_CHANNEL_SDATA, NULL},
    {"c", "C", "Segment C", 2, SRD_CHANNEL_SDATA, NULL},
    {"d", "D", "Segment D", 3, SRD_CHANNEL_SDATA, NULL},
    {"e", "E", "Segment E", 4, SRD_CHANNEL_SDATA, NULL},
    {"f", "F", "Segment F", 5, SRD_CHANNEL_SDATA, NULL},
    {"g", "G", "Segment G", 6, SRD_CHANNEL_SDATA, NULL},
    {"dp", "DP", "Decimal point", 7, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_channel segment_7_optional_channels[] = {
    {"dp", "DP", "Decimal point", 0, SRD_CHANNEL_SDATA, NULL},
};

static const char *segment_7_inputs[] = {"logic", NULL};
static const char *segment_7_outputs[] = {"seven_segment", NULL};
static const char *segment_7_tags[] = {"Display", NULL};

static const char *segment_7_ann_labels[][3] = {
    {"", "digit", "Digit"},
    {"", "segment", "Segment"},
};

static const int segment_7_row_digit_classes[] = {0};
static const int segment_7_row_segment_classes[] = {1};
static const struct srd_c_ann_row segment_7_ann_rows[] = {
    {"digits", "Digits", segment_7_row_digit_classes, 1},
    {"segments", "Segments", segment_7_row_segment_classes, 1},
};

static void segment_7_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(segment_7_state)));
    }
    segment_7_state *s = (segment_7_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(segment_7_state));
    s->state = STATE_IDLE;
}

static void segment_7_start(struct srd_decoder_inst *di)
{
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "seven_segment");
}

static void segment_7_decode(struct srd_decoder_inst *di)
{
    segment_7_state *s = (segment_7_state *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;
    }
}

static void segment_7_destroy(struct srd_decoder_inst *di)
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
    .channels = segment_7_channels,
    .num_channels = 8,
    .optional_channels = segment_7_optional_channels,
    .num_optional_channels = 1,
    .options = NULL,
    .num_options = 0,
    .num_annotations = 2,
    .ann_labels = segment_7_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = segment_7_ann_rows,
    .inputs = segment_7_inputs,
    .num_inputs = 1,
    .outputs = segment_7_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = segment_7_tags,
    .num_tags = 1,
    .reset = segment_7_reset,
    .start = segment_7_start,
    .decode = segment_7_decode,
    .destroy = segment_7_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &segment_7_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
