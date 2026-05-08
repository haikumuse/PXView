#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum gray_code_state {
    STATE_IDLE,
};

typedef struct {
    enum gray_code_state state;
    uint64_t start_sample;
} gray_code_state;

static struct srd_channel gray_code_channels[] = {};

static struct srd_decoder_option gray_code_options[] = {
    {"edges", NULL, "Edges per rotation", NULL, NULL},
    {"avg_period", NULL, "Averaging period", NULL, NULL},
};

static const char *gray_code_inputs[] = {"logic", NULL};
static const char *gray_code_outputs[] = {"graycode", NULL};
static const char *gray_code_tags[] = {"Encoding", NULL};

static const char *gray_code_ann_labels[][3] = {
    {"", "code", "Gray code"},
    {"", "position", "Position"},
};

static const int gray_code_row_code_classes[] = {0};
static const int gray_code_row_position_classes[] = {1};
static const struct srd_c_ann_row gray_code_ann_rows[] = {
    {"code", "Gray code", gray_code_row_code_classes, 1},
    {"position", "Position", gray_code_row_position_classes, 1},
};

static void gray_code_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(gray_code_state)));
    }
    gray_code_state *s = (gray_code_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(gray_code_state));
    s->state = STATE_IDLE;
}

static void gray_code_start(struct srd_decoder_inst *di)
{
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "graycode");
}

static void gray_code_decode(struct srd_decoder_inst *di)
{
    gray_code_state *s = (gray_code_state *)c_decoder_get_private(di);
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

static void gray_code_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder gray_code_c_decoder = {
    .id = "graycode_c",
    .name = "Gray Code(C)",
    .longname = "Gray code and rotary encoder (C)",
    .desc = "Accumulate rotary encoder increments, provide statistics. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = gray_code_options,
    .num_options = 2,
    .num_annotations = 2,
    .ann_labels = gray_code_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = gray_code_ann_rows,
    .inputs = gray_code_inputs,
    .num_inputs = 1,
    .outputs = gray_code_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = gray_code_tags,
    .num_tags = 1,
    .reset = gray_code_reset,
    .start = gray_code_start,
    .decode = gray_code_decode,
    .destroy = gray_code_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &gray_code_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
