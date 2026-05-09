#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum counter_state {
    STATE_IDLE,
};

typedef struct {
    enum counter_state state;
    uint64_t start_sample;
} counter_state;

static struct srd_channel counter_channels[] = {
    {"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, NULL},
    {"reset", "Reset", "Reset line", 1, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_channel counter_optional_channels[] = {
    {"reset", "Reset", "Reset line", 0, SRD_CHANNEL_SDATA, NULL},
};

static const char *counter_inputs[] = {"logic", NULL};
static const char *counter_outputs[] = {"counter", NULL};
static const char *counter_tags[] = {"Util", NULL};

static const char *counter_ann_labels[][3] = {
    {"", "count", "Count"},
    {"", "edge", "Edge"},
};

static const int counter_row_count_classes[] = {0};
static const int counter_row_edge_classes[] = {1};
static const struct srd_c_ann_row counter_ann_rows[] = {
    {"count", "Count", counter_row_count_classes, 1},
    {"edges", "Edges", counter_row_edge_classes, 1},
};

static void counter_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(counter_state)));
    }
    counter_state *s = (counter_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(counter_state));
    s->state = STATE_IDLE;
}

static void counter_start(struct srd_decoder_inst *di)
{
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "counter");
}

static void counter_decode(struct srd_decoder_inst *di)
{
    counter_state *s = (counter_state *)c_decoder_get_private(di);
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

static void counter_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder counter_c_decoder = {
    .id = "counter_c",
    .name = "Counter(C)",
    .longname = "Edge counter (C)",
    .desc = "Count the number of edges in a signal. (C implementation)",
    .license = "gplv2+",
    .channels = counter_channels,
    .num_channels = 2,
    .optional_channels = counter_optional_channels,
    .num_optional_channels = 1,
    .options = NULL,
    .num_options = 0,
    .num_annotations = 2,
    .ann_labels = counter_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = counter_ann_rows,
    .inputs = counter_inputs,
    .num_inputs = 1,
    .outputs = counter_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = counter_tags,
    .num_tags = 1,
    .reset = counter_reset,
    .start = counter_start,
    .decode = counter_decode,
    .destroy = counter_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &counter_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
