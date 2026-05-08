#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum numbers_and_state_state {
    STATE_IDLE,
};

typedef struct {
    enum numbers_and_state_state state;
    uint64_t start_sample;
} numbers_and_state_state;

static struct srd_channel numbers_and_state_channels[] = {
    {"clk", "Clock", "Clock", 0, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_channel numbers_and_state_optional_channels[] = {
    {"clk", "Clock", "Clock", 0, SRD_CHANNEL_SDATA, NULL},
};

static const char *numbers_and_state_inputs[] = {"logic", NULL};
static const char *numbers_and_state_outputs[] = {"numbers_and_state", NULL};
static const char *numbers_and_state_tags[] = {"Encoding", "Util", NULL};

static const char *numbers_and_state_ann_labels[][3] = {
    {"", "number", "Number"},
    {"", "state", "State"},
};

static const int numbers_and_state_row_number_classes[] = {0};
static const int numbers_and_state_row_state_classes[] = {1};
static const struct srd_c_ann_row numbers_and_state_ann_rows[] = {
    {"numbers", "Numbers", numbers_and_state_row_number_classes, 1},
    {"states", "States", numbers_and_state_row_state_classes, 1},
};

static void numbers_and_state_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(numbers_and_state_state)));
    }
    numbers_and_state_state *s = (numbers_and_state_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(numbers_and_state_state));
    s->state = STATE_IDLE;
}

static void numbers_and_state_start(struct srd_decoder_inst *di)
{
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "numbers_and_state");
}

static void numbers_and_state_decode(struct srd_decoder_inst *di)
{
    numbers_and_state_state *s = (numbers_and_state_state *)c_decoder_get_private(di);
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

static void numbers_and_state_destroy(struct srd_decoder_inst *di)
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
    .channels = numbers_and_state_channels,
    .num_channels = 1,
    .optional_channels = numbers_and_state_optional_channels,
    .num_optional_channels = 1,
    .options = NULL,
    .num_options = 0,
    .num_annotations = 2,
    .ann_labels = numbers_and_state_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = numbers_and_state_ann_rows,
    .inputs = numbers_and_state_inputs,
    .num_inputs = 1,
    .outputs = numbers_and_state_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = numbers_and_state_tags,
    .num_tags = 2,
    .reset = numbers_and_state_reset,
    .start = numbers_and_state_start,
    .decode = numbers_and_state_decode,
    .destroy = numbers_and_state_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &numbers_and_state_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
