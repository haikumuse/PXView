#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_EDGE_COUNT = 0,
    ANN_WORD_COUNT,
    ANN_WORD_RESET,
    NUM_ANN,
};

typedef struct {
    uint64_t samplerate;
    int data_edge;
    int divider;
    int reset_edge;
    int64_t edge_off;
    int64_t word_off;
    int dead_cycles;
    int start_with_reset;
    int64_t edge_count;
    int64_t word_count;
    int dead_count;
    int prev_data;
    int prev_reset;
    uint64_t ss_edge;
    uint64_t ss_word;
    int out_ann;
} counter_state;

static struct srd_channel counter_channels[] = {
    {"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_channel counter_optional_channels[] = {
    {"reset", "Reset", "Reset line", 0, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option counter_options[] = {
    {"data_edge", NULL, "Edges to count (data)", NULL, NULL},
    {"divider", NULL, "Count divider (word width)", NULL, NULL},
    {"reset_edge", NULL, "Edge which clears counters (reset)", NULL, NULL},
    {"edge_off", NULL, "Edge counter value after start/reset", NULL, NULL},
    {"word_off", NULL, "Word counter value after start/reset", NULL, NULL},
    {"dead_cycles", NULL, "Ignore this many edges after reset", NULL, NULL},
    {"start_with_reset", NULL, "Assume decode starts with reset", NULL, NULL},
};

static const char *counter_inputs[] = {"logic", NULL};
static const char *counter_outputs[] = {"counter", NULL};
static const char *counter_tags[] = {"Util", NULL};

static const char *counter_ann_labels[][3] = {
    {"", "edge_count", "Edge count"},
    {"", "word_count", "Word count"},
    {"", "word_reset", "Word reset"},
};

static const int counter_row_edge_classes[] = {ANN_EDGE_COUNT, -1};
static const int counter_row_word_classes[] = {ANN_WORD_COUNT, -1};
static const int counter_row_reset_classes[] = {ANN_WORD_RESET, -1};
static const struct srd_c_ann_row counter_ann_rows[] = {
    {"edge_counts", "Edges", counter_row_edge_classes, 1},
    {"word_counts", "Words", counter_row_word_classes, 1},
    {"word_resets", "Word resets", counter_row_reset_classes, 1},
};

static void counter_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(counter_state)));
    }
    counter_state *s = (counter_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(counter_state));
    s->prev_data = -1;
    s->prev_reset = -1;
}

static void counter_start(struct srd_decoder_inst *di)
{
    counter_state *s = (counter_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "counter");
    s->samplerate = c_decoder_get_samplerate(di);

    const char *de = c_decoder_get_option_string(di, "data_edge", "any");
    if (strcmp(de, "rising") == 0)
        s->data_edge = 1;
    else if (strcmp(de, "falling") == 0)
        s->data_edge = 2;
    else
        s->data_edge = 0;

    s->divider = (int)c_decoder_get_option_int(di, "divider", 0);

    const char *re = c_decoder_get_option_string(di, "reset_edge", "falling");
    s->reset_edge = (strcmp(re, "rising") == 0) ? 1 : 2;

    s->edge_off = (int64_t)c_decoder_get_option_int(di, "edge_off", 0);
    s->word_off = (int64_t)c_decoder_get_option_int(di, "word_off", 0);
    s->dead_cycles = (int)c_decoder_get_option_int(di, "dead_cycles", 0);

    const char *swr = c_decoder_get_option_string(di, "start_with_reset", "no");
    s->start_with_reset = (strcmp(swr, "yes") == 0) ? 1 : 0;

    if (s->start_with_reset) {
        s->edge_count = s->edge_off;
        s->word_count = s->word_off;
        s->dead_count = s->dead_cycles;
    }
}

static int is_data_edge(counter_state *s, int old_val, int new_val)
{
    if (s->data_edge == 0) return (old_val != new_val);
    if (s->data_edge == 1) return (old_val == 0 && new_val == 1);
    if (s->data_edge == 2) return (old_val == 1 && new_val == 0);
    return 0;
}

static int is_reset_edge(counter_state *s, int old_val, int new_val)
{
    if (s->reset_edge == 1) return (old_val == 0 && new_val == 1);
    return (old_val == 1 && new_val == 0);
}

static void counter_decode(struct srd_decoder_inst *di)
{
    counter_state *s = (counter_state *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;
    int has_reset = c_decoder_has_channel(di, 1);

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 0);
        if (has_reset)
            c_cond_or(cb);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int data = c_decoder_get_pin(di, 0, samplenum);
        int reset = 0;
        if (has_reset)
            reset = c_decoder_get_pin(di, 1, samplenum);

        if (has_reset && s->prev_reset >= 0 && is_reset_edge(s, s->prev_reset, reset)) {
            C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_WORD_RESET, "Reset");
            s->edge_count = s->edge_off;
            s->word_count = s->word_off;
            s->dead_count = s->dead_cycles;
            s->prev_data = data;
            s->prev_reset = reset;
            continue;
        }

        if (s->prev_data >= 0 && is_data_edge(s, s->prev_data, data)) {
            if (s->dead_count > 0) {
                s->dead_count--;
                s->prev_data = data;
                s->prev_reset = reset;
                continue;
            }

            s->edge_count++;
            s->ss_edge = samplenum;

            char t1[32];
            snprintf(t1, sizeof(t1), "%lld", (long long)s->edge_count);
            C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_EDGE_COUNT, t1);

            if (s->divider > 0 && (s->edge_count % s->divider) == 0) {
                s->word_count++;
                snprintf(t1, sizeof(t1), "%lld", (long long)s->word_count);
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_WORD_COUNT, t1);
            }
        }

        s->prev_data = data;
        s->prev_reset = reset;
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
    .num_channels = 1,
    .optional_channels = counter_optional_channels,
    .num_optional_channels = 1,
    .options = counter_options,
    .num_options = 7,
    .num_annotations = NUM_ANN,
    .ann_labels = counter_ann_labels,
    .num_annotation_rows = 3,
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
    counter_options[0].def = g_variant_new_string("any");
    counter_options[1].def = g_variant_new_int64(0);
    counter_options[2].def = g_variant_new_string("falling");
    counter_options[3].def = g_variant_new_int64(0);
    counter_options[4].def = g_variant_new_int64(0);
    counter_options[5].def = g_variant_new_int64(0);
    counter_options[6].def = g_variant_new_string("no");
    return &counter_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
