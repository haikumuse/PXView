#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_PHASE = 0,
    ANN_INCREMENT,
    ANN_COUNT,
    ANN_TURNS,
    ANN_INTERVAL,
    ANN_AVERAGE,
    ANN_RPM,
    NUM_ANN,
};

typedef struct {
    uint64_t samplerate;
    int edges_per_rotation;
    int avg_period_samples;
    int prev_gray;
    int prev_bin;
    int64_t count;
    int64_t total_increments;
    int64_t total_edges;
    uint64_t last_edge_sample;
    double sum_intervals;
    int interval_count;
    int out_ann;
} gray_code_state;

static uint32_t gray_to_binary(uint32_t gray, int bits)
{
    uint32_t bin = gray;
    for (int i = 1; i < bits; i++)
        bin ^= (gray >> i);
    return bin;
}

static struct srd_decoder_option gray_code_options[] = {
    {"edges", NULL, "Edges per rotation", NULL, NULL},
    {"avg_period", NULL, "Averaging period (ms)", NULL, NULL},
};

static const char *gray_code_inputs[] = {"logic", NULL};
static const char *gray_code_outputs[] = {NULL};
static const char *gray_code_tags[] = {"Encoding", NULL};

static struct srd_channel gray_code_optional_channels[] = {
    {"d0", "D0", "Data line 0", 0, SRD_CHANNEL_COMMON, NULL},
    {"d1", "D1", "Data line 1", 1, SRD_CHANNEL_COMMON, NULL},
    {"d2", "D2", "Data line 2", 2, SRD_CHANNEL_COMMON, NULL},
    {"d3", "D3", "Data line 3", 3, SRD_CHANNEL_COMMON, NULL},
    {"d4", "D4", "Data line 4", 4, SRD_CHANNEL_COMMON, NULL},
    {"d5", "D5", "Data line 5", 5, SRD_CHANNEL_COMMON, NULL},
    {"d6", "D6", "Data line 6", 6, SRD_CHANNEL_COMMON, NULL},
    {"d7", "D7", "Data line 7", 7, SRD_CHANNEL_COMMON, NULL},
};

static const char *gray_code_ann_labels[][3] = {
    {"", "phase", "Phase"},
    {"", "increment", "Increment"},
    {"", "count", "Count"},
    {"", "turns", "Turns"},
    {"", "interval", "Interval"},
    {"", "average", "Average"},
    {"", "rpm", "Rate"},
};

static const int gray_code_row_phase_classes[] = {ANN_PHASE, -1};
static const int gray_code_row_increment_classes[] = {ANN_INCREMENT, -1};
static const int gray_code_row_count_classes[] = {ANN_COUNT, -1};
static const int gray_code_row_turns_classes[] = {ANN_TURNS, -1};
static const int gray_code_row_interval_classes[] = {ANN_INTERVAL, -1};
static const int gray_code_row_average_classes[] = {ANN_AVERAGE, -1};
static const int gray_code_row_rpm_classes[] = {ANN_RPM, -1};
static const struct srd_c_ann_row gray_code_ann_rows[] = {
    {"phase", "Phase", gray_code_row_phase_classes, 1},
    {"increment", "Increment", gray_code_row_increment_classes, 1},
    {"count", "Count", gray_code_row_count_classes, 1},
    {"turns", "Turns", gray_code_row_turns_classes, 1},
    {"interval", "Interval", gray_code_row_interval_classes, 1},
    {"average", "Average", gray_code_row_average_classes, 1},
    {"rpm", "Rate", gray_code_row_rpm_classes, 1},
};

static void gray_code_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(gray_code_state)));
    }
    gray_code_state *s = (gray_code_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(gray_code_state));
    s->edges_per_rotation = 0;
    s->avg_period_samples = 0;
    s->prev_gray = -1;
    s->prev_bin = -1;
}

static void gray_code_start(struct srd_decoder_inst *di)
{
    gray_code_state *s = (gray_code_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "graycode");
    s->samplerate = c_decoder_get_samplerate(di);
    s->edges_per_rotation = (int)c_decoder_get_option_int(di, "edges", 0);
    int avg_ms = (int)c_decoder_get_option_int(di, "avg_period", 0);
    if (s->samplerate > 0 && avg_ms > 0)
        s->avg_period_samples = (int)(s->samplerate * avg_ms / 1000);
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

        int cur_gray = c_decoder_get_pin(di, 0, samplenum);

        if (s->prev_gray == -1) {
            s->prev_gray = cur_gray;
            s->prev_bin = (int)gray_to_binary((uint32_t)cur_gray, 2);
            s->last_edge_sample = samplenum;
            s->total_edges = 1;
            continue;
        }

        if (cur_gray == s->prev_gray)
            continue;

        int cur_bin = (int)gray_to_binary((uint32_t)cur_gray, 2);
        int increment = cur_bin - s->prev_bin;

        if (increment > 3)
            increment -= 4;
        else if (increment < -3)
            increment += 4;

        s->total_edges++;
        s->total_increments += increment;
        s->count += increment;

        char t1[64];
        snprintf(t1, sizeof(t1), "Phase: %d", cur_bin);
        C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_PHASE, t1);

        snprintf(t1, sizeof(t1), "Inc: %+d", increment);
        C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_INCREMENT, t1);

        snprintf(t1, sizeof(t1), "Count: %lld", (long long)s->count);
        C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_COUNT, t1);

        if (s->last_edge_sample > 0 && samplenum > s->last_edge_sample) {
            uint64_t interval_samples = samplenum - s->last_edge_sample;

            if (s->samplerate > 0) {
                double interval_us = (double)interval_samples * 1000000.0 / s->samplerate;
                snprintf(t1, sizeof(t1), "Interval: %.1f us", interval_us);
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_INTERVAL, t1);

                s->sum_intervals += interval_us;
                s->interval_count++;

                if (s->interval_count > 0) {
                    double avg = s->sum_intervals / s->interval_count;
                    snprintf(t1, sizeof(t1), "Avg: %.1f us", avg);
                    C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_AVERAGE, t1);

                    if (avg > 0) {
                        double rpm = 60000000.0 / avg;
                        if (s->edges_per_rotation > 0)
                            rpm = rpm / s->edges_per_rotation;
                        snprintf(t1, sizeof(t1), "RPM: %.1f", rpm);
                        C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_RPM, t1);
                    }
                }
            }
        }

        if (s->edges_per_rotation > 0) {
            double turns = (double)s->count / s->edges_per_rotation;
            snprintf(t1, sizeof(t1), "Turns: %.2f", turns);
            C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_TURNS, t1);
        }

        if (s->avg_period_samples > 0 && s->interval_count > 0) {
            uint64_t window_start = (samplenum > (uint64_t)s->avg_period_samples)
                ? samplenum - s->avg_period_samples : 0;
            (void)window_start;
        }

        s->prev_gray = cur_gray;
        s->prev_bin = cur_bin;
        s->last_edge_sample = samplenum;
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
    .optional_channels = gray_code_optional_channels,
    .num_optional_channels = 8,
    .options = gray_code_options,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = gray_code_ann_labels,
    .num_annotation_rows = 7,
    .annotation_rows = gray_code_ann_rows,
    .inputs = gray_code_inputs,
    .num_inputs = 1,
    .outputs = gray_code_outputs,
    .num_outputs = 0,
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
    gray_code_options[0].def = g_variant_new_int64(0);
    gray_code_options[1].def = g_variant_new_int64(10);
    return &gray_code_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
