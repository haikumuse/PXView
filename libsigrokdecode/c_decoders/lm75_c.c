#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum lm75_state {
    STATE_IDLE,
};

typedef struct {
    enum lm75_state state;
    uint64_t start_sample;
} lm75_state;

static struct srd_channel lm75_channels[] = {};

static const char *lm75_inputs[] = {"i2c", NULL};
static const char *lm75_outputs[] = {"lm75", NULL};
static const char *lm75_tags[] = {"Sensor", NULL};

static const char *lm75_ann_labels[][3] = {
    {"", "temp", "Temperature"},
    {"", "register", "Register"},
};

static const int lm75_row_temp_classes[] = {0};
static const int lm75_row_register_classes[] = {1};
static const struct srd_c_ann_row lm75_ann_rows[] = {
    {"temperature", "Temperature", lm75_row_temp_classes, 1},
    {"registers", "Registers", lm75_row_register_classes, 1},
};

static void lm75_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(lm75_state)));
    }
    lm75_state *s = (lm75_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(lm75_state));
    s->state = STATE_IDLE;
}

static void lm75_start(struct srd_decoder_inst *di)
{
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "lm75");
}

static void lm75_decode(struct srd_decoder_inst *di)
{
    lm75_state *s = (lm75_state *)c_decoder_get_private(di);
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

static void lm75_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder lm75_c_decoder = {
    .id = "lm75_c",
    .name = "Lm75(C)",
    .longname = "National LM75 (C)",
    .desc = "National LM75 (and compatibles) temperature sensor. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = 2,
    .ann_labels = lm75_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = lm75_ann_rows,
    .inputs = lm75_inputs,
    .num_inputs = 1,
    .outputs = lm75_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = lm75_tags,
    .num_tags = 1,
    .reset = lm75_reset,
    .start = lm75_start,
    .decode = lm75_decode,
    .destroy = lm75_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &lm75_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
