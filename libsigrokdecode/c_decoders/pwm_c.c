#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum pwm_state {
    STATE_IDLE,
};

typedef struct {
    enum pwm_state state;
    uint64_t start_sample;
} pwm_state;

static struct srd_channel pwm_channels[] = {
    {"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, NULL},
};

static const char *pwm_inputs[] = {"logic", NULL};
static const char *pwm_outputs[] = {"pwm", NULL};
static const char *pwm_tags[] = {"Encoding", NULL};

static const char *pwm_ann_labels[][3] = {
    {"", "duty", "Duty cycle"},
    {"", "period", "Period"},
};

static const int pwm_row_duty_classes[] = {0};
static const int pwm_row_period_classes[] = {1};
static const struct srd_c_ann_row pwm_ann_rows[] = {
    {"duty", "Duty cycle", pwm_row_duty_classes, 1},
    {"period", "Period", pwm_row_period_classes, 1},
};

static void pwm_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(pwm_state)));
    }
    pwm_state *s = (pwm_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(pwm_state));
    s->state = STATE_IDLE;
}

static void pwm_start(struct srd_decoder_inst *di)
{
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "pwm");
}

static void pwm_decode(struct srd_decoder_inst *di)
{
    pwm_state *s = (pwm_state *)c_decoder_get_private(di);
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

static void pwm_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder pwm_c_decoder = {
    .id = "pwm_c",
    .name = "Pwm(C)",
    .longname = "Pulse-width modulation (C)",
    .desc = "Analog level encoded in duty cycle percentage. (C implementation)",
    .license = "gplv2+",
    .channels = pwm_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = 2,
    .ann_labels = pwm_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = pwm_ann_rows,
    .inputs = pwm_inputs,
    .num_inputs = 1,
    .outputs = pwm_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = pwm_tags,
    .num_tags = 1,
    .reset = pwm_reset,
    .start = pwm_start,
    .decode = pwm_decode,
    .destroy = pwm_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &pwm_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
