#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"
#include "c_decoder_utils.h"

enum lm75_state {
    STATE_IDLE,
};

typedef struct {
    enum lm75_state state;
    uint64_t start_sample;
} lm75_state;

static struct srd_channel lm75_channels[] = {};

static const char *lm75_inputs[] = {"i2c", NULL};
static const char *lm75_outputs[] = {, NULL};
static const char *lm75_tags[] = {"Sensor", NULL};

static void lm75_reset(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    if (!di->user_data) {
        di->user_data = g_malloc0(sizeof(lm75_state));
    }
    lm75_state *s = (lm75_state *)di->user_data;
    memset(s, 0, sizeof(lm75_state));
    s->state = STATE_IDLE;
}

static void lm75_start(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "lm75");
}

static void lm75_decode(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    lm75_state *s = (lm75_state *)di->user_data;
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        GSList *cond = NULL;
        int ret = c_decoder_wait(di, cond, &samplenum, &matched);
        if (ret != SRD_OK)
            return;
    }
}

static void lm75_destroy(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    if (di->user_data) {
        g_free(di->user_data);
        di->user_data = NULL;
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
    .num_annotations = 0,
    .ann_labels = NULL,
    .num_annotation_rows = 0,
    .annotation_rows = NULL,
    .inputs = lm75_inputs,
    .num_inputs = 1,
    .outputs = lm75_outputs,
    .num_outputs = 0,
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