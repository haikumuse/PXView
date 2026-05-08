#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"
#include "c_decoder_utils.h"

enum ds3231_state {
    STATE_IDLE,
};

typedef struct {
    enum ds3231_state state;
    uint64_t start_sample;
} ds3231_state;

static struct srd_channel ds3231_channels[] = {};

static struct srd_decoder_option ds3231_options[] = {
    {"day0", NULL, "First day of week", NULL, NULL},
};

static const char *ds3231_inputs[] = {"i2c", NULL};
static const char *ds3231_outputs[] = {, NULL};
static const char *ds3231_tags[] = {"Clock/timing", "IC", NULL};

static void ds3231_reset(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    if (!di->user_data) {
        di->user_data = g_malloc0(sizeof(ds3231_state));
    }
    ds3231_state *s = (ds3231_state *)di->user_data;
    memset(s, 0, sizeof(ds3231_state));
    s->state = STATE_IDLE;
}

static void ds3231_start(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "ds3231");
}

static void ds3231_decode(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    ds3231_state *s = (ds3231_state *)di->user_data;
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        GSList *cond = NULL;
        int ret = c_decoder_wait(di, cond, &samplenum, &matched);
        if (ret != SRD_OK)
            return;
    }
}

static void ds3231_destroy(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    if (di->user_data) {
        g_free(di->user_data);
        di->user_data = NULL;
    }
}

struct srd_c_decoder ds3231_c_decoder = {
    .id = "ds3231_c",
    .name = "Ds3231(C)",
    .longname = "Maxim DS3231 (C)",
    .desc = "Maxim DS3231 realtime clock module protocol. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = ds3231_options,
    .num_options = 1,
    .num_annotations = 0,
    .ann_labels = NULL,
    .num_annotation_rows = 0,
    .annotation_rows = NULL,
    .inputs = ds3231_inputs,
    .num_inputs = 1,
    .outputs = ds3231_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = ds3231_tags,
    .num_tags = 2,
    .reset = ds3231_reset,
    .start = ds3231_start,
    .decode = ds3231_decode,
    .destroy = ds3231_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &ds3231_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}