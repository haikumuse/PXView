#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ANN_DUTY = 0,
    ANN_PERIOD,
    NUM_ANN,
};

typedef struct {
    uint64_t samplerate;
    int polarity;
    uint64_t first_samplenum;
    uint64_t ss_block;
    uint64_t es_block;
    int out_ann;
    int out_binary;
    int out_average;
    int64_t num_cycles;
    double average;
} pwm_state;

static struct srd_channel pwm_channels[] = {
    { "data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, "dec_pwm_chan_data" },
};

static struct srd_decoder_option pwm_options[] = {
    { "polarity", NULL, "Polarity", NULL, NULL },
};

static const char* pwm_inputs[] = { "logic", NULL };
static const char* pwm_outputs[] = { NULL };
static const char* pwm_tags[] = { "Encoding", NULL };

static const char* pwm_ann_labels[][3] = {
    { "", "duty-cycle", "Duty cycle" },
    { "", "period", "Period" },
};

static const int pwm_row_duty_classes[] = { ANN_DUTY };
static const int pwm_row_period_classes[] = { ANN_PERIOD };
static const struct srd_c_ann_row pwm_ann_rows[] = {
    { "duty-cycle", "Duty cycle", pwm_row_duty_classes, 1 },
    { "period", "Period", pwm_row_period_classes, 1 },
};

static const struct srd_decoder_binary pwm_binary[] = {
    { 0, "raw", "RAW file" },
};

static void pwm_reset(struct srd_decoder_inst* di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(pwm_state)));
    }
    pwm_state* s = (pwm_state*)c_decoder_get_private(di);
    memset(s, 0, sizeof(pwm_state));
}

static void pwm_start(struct srd_decoder_inst* di)
{
    pwm_state* s = (pwm_state*)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "pwm");
    s->out_binary = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "pwm");
    s->out_average = c_decoder_register_output_meta(di, SRD_OUTPUT_META, "pwm",
        "float", "Average", "PWM base (cycle) frequency");
    s->samplerate = c_decoder_get_samplerate(di);

    const char* pol = c_decoder_get_option_string(di, "polarity", "active-high");
    s->polarity = (strcmp(pol, "active-low") == 0) ? 1 : 0;
}

static void pwm_decode(struct srd_decoder_inst* di)
{
    pwm_state* s = (pwm_state*)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    if (!s->samplerate)
        s->samplerate = c_decoder_get_samplerate(di);
    if (!s->samplerate)
        return;

    {
        srd_cond_builder* cb = c_cond_new();
        if (s->polarity == 1)
            c_cond_fall(cb, 0);
        else
            c_cond_rise(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;
        s->first_samplenum = samplenum;
    }

    while (1) {
        uint64_t start_samplenum = samplenum;

        {
            srd_cond_builder* cb = c_cond_new();
            c_cond_edge(cb, 0);
            int ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;
        }
        uint64_t end_samplenum = samplenum;

        {
            srd_cond_builder* cb = c_cond_new();
            c_cond_edge(cb, 0);
            int ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;
        }

        s->ss_block = start_samplenum;
        s->es_block = samplenum;

        uint64_t period = samplenum - start_samplenum;
        uint64_t duty = end_samplenum - start_samplenum;
        double ratio = (double)duty / (double)period;
        double percent = ratio * 100.0;

        char pct_str[32];
        snprintf(pct_str, sizeof(pct_str), "%f%%", percent);
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_DUTY, pct_str);

        {
            unsigned char b = (unsigned char)(ratio * 256.0);
            if (ratio * 256.0 >= 256.0)
                b = 255;
            c_decoder_put_binary(di, s->ss_block, s->es_block, s->out_binary, 0, 1, &b);
        }

        double period_t = (double)period / (double)s->samplerate;
        char period_s[32];
        if (period_t == 0 || period_t >= 1) {
            snprintf(period_s, sizeof(period_s), "%.1f s", period_t);
        } else if (period_t <= 1e-12) {
            snprintf(period_s, sizeof(period_s), "%.1f fs", period_t * 1e15);
        } else if (period_t <= 1e-9) {
            snprintf(period_s, sizeof(period_s), "%.1f ps", period_t * 1e12);
        } else if (period_t <= 1e-6) {
            snprintf(period_s, sizeof(period_s), "%.1f ns", period_t * 1e9);
        } else if (period_t <= 1e-3) {
            snprintf(period_s, sizeof(period_s), "%.1f \xce\xbcs", period_t * 1e6);
        } else {
            snprintf(period_s, sizeof(period_s), "%.1f ms", period_t * 1e3);
        }
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_PERIOD, period_s);

        s->num_cycles++;
        s->average += percent;
        c_decoder_put_meta_double(di, s->first_samplenum, s->es_block,
            s->out_average, s->average / (double)s->num_cycles);
    }
}

static void pwm_destroy(struct srd_decoder_inst* di)
{
    void* priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder pwm_c_decoder = {
    .id = "pwm_c",
    .name = "PWM(C)",
    .longname = "Pulse-width modulation (C)",
    .desc = "Analog level encoded in duty cycle percentage. (C implementation)",
    .license = "gplv2+",
    .channels = pwm_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = pwm_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = pwm_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = pwm_ann_rows,
    .inputs = pwm_inputs,
    .num_inputs = 1,
    .outputs = pwm_outputs,
    .num_outputs = 0,
    .binary = pwm_binary,
    .num_binary = 1,
    .tags = pwm_tags,
    .num_tags = 1,
    .reset = pwm_reset,
    .start = pwm_start,
    .decode = pwm_decode,
    .destroy = pwm_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder* srd_c_decoder_entry(void)
{
    GSList* pol_vals = NULL;
    pol_vals = g_slist_append(pol_vals, g_variant_new_string("active-low"));
    pol_vals = g_slist_append(pol_vals, g_variant_new_string("active-high"));
    pwm_options[0].id = "polarity";
    pwm_options[0].idn = "dec_pwm_opt_polarity";
    pwm_options[0].desc = "Polarity";
    pwm_options[0].def = g_variant_new_string("active-high");
    pwm_options[0].values = pol_vals;
    return &pwm_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
