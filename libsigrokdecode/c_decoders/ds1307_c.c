#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum ds1307_state {
    STATE_IDLE,
};

typedef struct {
    enum ds1307_state state;
    uint64_t start_sample;
} ds1307_state;

static struct srd_channel ds1307_channels[] = {};

static const char *ds1307_inputs[] = {"i2c", NULL};
static const char *ds1307_outputs[] = {"ds1307", NULL};
static const char *ds1307_tags[] = {"Clock/timing", "IC", NULL};

static const char *ds1307_ann_labels[][3] = {
    {"", "datetime", "Date/Time"},
    {"", "register", "Register"},
};

static const int ds1307_row_datetime_classes[] = {0};
static const int ds1307_row_register_classes[] = {1};
static const struct srd_c_ann_row ds1307_ann_rows[] = {
    {"datetime", "Date/Time", ds1307_row_datetime_classes, 1},
    {"registers", "Registers", ds1307_row_register_classes, 1},
};

static void ds1307_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(ds1307_state)));
    }
    ds1307_state *s = (ds1307_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(ds1307_state));
    s->state = STATE_IDLE;
}

static void ds1307_start(struct srd_decoder_inst *di)
{
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "ds1307");
}

static void ds1307_decode(struct srd_decoder_inst *di)
{
    ds1307_state *s = (ds1307_state *)c_decoder_get_private(di);
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

static void ds1307_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder ds1307_c_decoder = {
    .id = "ds1307_c",
    .name = "Ds1307(C)",
    .longname = "Dallas DS1307 (C)",
    .desc = "Dallas DS1307 realtime clock module protocol. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = 2,
    .ann_labels = ds1307_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = ds1307_ann_rows,
    .inputs = ds1307_inputs,
    .num_inputs = 1,
    .outputs = ds1307_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = ds1307_tags,
    .num_tags = 2,
    .reset = ds1307_reset,
    .start = ds1307_start,
    .decode = ds1307_decode,
    .destroy = ds1307_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &ds1307_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
