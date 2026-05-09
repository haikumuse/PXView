#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_CELSIUS = 0,
    ANN_KELVIN,
    ANN_TEXT_VERBOSE,
    ANN_TEXT,
    ANN_WARN,
    NUM_ANN,
};

typedef struct {
    int out_ann;
} lm75_state;

static struct srd_channel lm75_channels[] = {};

static const char *lm75_inputs[] = {"i2c", NULL};
static const char *lm75_outputs[] = {"lm75", NULL};
static const char *lm75_tags[] = {"Sensor", NULL};

static const char *lm75_ann_labels[][3] = {
    {"", "celsius", "Temperature in degrees Celsius"},
    {"", "kelvin", "Temperature in Kelvin"},
    {"", "text-verbose", "Human-readable text (verbose)"},
    {"", "text", "Human-readable text"},
    {"", "warnings", "Warnings"},
};

static const int lm75_row_celsius_classes[] = {ANN_CELSIUS, -1};
static const int lm75_row_kelvin_classes[] = {ANN_KELVIN, -1};
static const int lm75_row_text_classes[] = {ANN_TEXT_VERBOSE, ANN_TEXT, -1};
static const int lm75_row_warnings_classes[] = {ANN_WARN, -1};
static const struct srd_c_ann_row lm75_ann_rows[] = {
    {"celsius", "Celsius", lm75_row_celsius_classes, 1},
    {"kelvin", "Kelvin", lm75_row_kelvin_classes, 1},
    {"text", "Text", lm75_row_text_classes, 2},
    {"warnings", "Warnings", lm75_row_warnings_classes, 1},
};

static void lm75_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(lm75_state)));
    }
    lm75_state *s = (lm75_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(lm75_state));
}

static void lm75_start(struct srd_decoder_inst *di)
{
    lm75_state *s = (lm75_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "lm75");
}

static void lm75_decode(struct srd_decoder_inst *di)
{
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
    .num_annotations = NUM_ANN,
    .ann_labels = lm75_ann_labels,
    .num_annotation_rows = 4,
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
