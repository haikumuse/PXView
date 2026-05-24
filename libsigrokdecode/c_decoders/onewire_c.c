#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum ow_state {
    STATE_WAIT_FALL,
    STATE_MEASURE_PULSE,
    STATE_WAIT_PRESENCE_FALL,
    STATE_WAIT_PRESENCE_RISE,
};

struct ow_priv {
    int state;
    uint8_t byte_val;
    int bit_cnt;
    uint64_t ss_rise;
    int overdrive;
    int out_ann;
    int out_python;
};

#define ANN_BIT 0
#define ANN_WARN 1
#define ANN_RESET 2
#define ANN_PRESENCE 3
#define ANN_OVERDRIVE 4
#define NUM_ANN 5

static struct srd_channel ow_channels[] = {
    { "owr", "OWR", "1-Wire signal line", 0, SRD_CHANNEL_SDATA, "dec_onewire_link_chan_owr" },
};

static struct srd_decoder_option ow_options[] = {
    { "overdrive", NULL, "Start in overdrive speed", NULL, NULL },
};

static const char* ow_ann_labels[][3] = {
    { "", "bit", "Bit" },
    { "", "warnings", "Warnings" },
    { "", "reset", "Reset" },
    { "", "presence", "Presence" },
    { "", "overdrive", "Overdrive speed notifications" },
};

static const int ow_row_bits_classes[] = { ANN_BIT, ANN_RESET, ANN_PRESENCE };
static const int ow_row_info_classes[] = { ANN_OVERDRIVE };
static const int ow_row_warnings_classes[] = { ANN_WARN };
static const struct srd_c_ann_row ow_ann_rows[] = {
    { "bits", "Bits", ow_row_bits_classes, 3 },
    { "info", "Info", ow_row_info_classes, 1 },
    { "warnings", "Warnings", ow_row_warnings_classes, 1 },
};

static const char* ow_inputs[] = { "logic", NULL };
static const char* ow_outputs[] = { "onewire_link", NULL };
static const char* ow_tags[] = { "Embedded/industrial", NULL };

static void onewire_reset(struct srd_decoder_inst* di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct ow_priv)));
    }
    struct ow_priv* s = (struct ow_priv*)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct ow_priv));
    s->state = STATE_WAIT_FALL;
    s->bit_cnt = -1;
}

static void onewire_start(struct srd_decoder_inst* di)
{
    struct ow_priv* s = (struct ow_priv*)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "onewire_link");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "onewire_link");
    const char* od = c_decoder_get_option_string(di, "overdrive", "no");
    s->overdrive = (strcmp(od, "yes") == 0) ? 1 : 0;
}

static void onewire_decode(struct srd_decoder_inst* di)
{
    struct ow_priv* s = (struct ow_priv*)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;
    uint64_t samplerate = c_decoder_get_samplerate(di);
    uint64_t reset_thresh = 480 * samplerate / 1000000;
    uint64_t short_thresh = 15 * samplerate / 1000000;
    uint64_t long_thresh = 60 * samplerate / 1000000;
    uint64_t ss_fall = 0;

    if (s->overdrive) {
        reset_thresh = 48 * samplerate / 1000000;
        short_thresh = 1 * samplerate / 1000000;
        long_thresh = 6 * samplerate / 1000000;
    }

    while (1) {
        srd_cond_builder* cb;
        int ret;

        switch (s->state) {

        case STATE_WAIT_FALL: {
            cb = c_cond_new();
            c_cond_fall(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;
            ss_fall = samplenum;
            s->state = STATE_MEASURE_PULSE;
            break;
        }

        case STATE_MEASURE_PULSE: {
            cb = c_cond_new();
            c_cond_rise(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            uint64_t pulse_width = samplenum - ss_fall;

            if (pulse_width > reset_thresh) {
                if (s->overdrive) {
                    C_ANN_PUT(di, ss_fall, samplenum, s->out_ann, ANN_OVERDRIVE,
                        "Exiting overdrive mode", "Overdrive off");
                    s->overdrive = 0;
                    reset_thresh = 480 * samplerate / 1000000;
                    short_thresh = 15 * samplerate / 1000000;
                    long_thresh = 60 * samplerate / 1000000;
                }
                C_ANN_PUT(di, ss_fall, samplenum, s->out_ann, ANN_RESET,
                    "Reset", "Rst", "R");
                s->ss_rise = samplenum;
                s->state = STATE_WAIT_PRESENCE_FALL;
            } else {
                int bit_val;
                if (pulse_width < short_thresh) {
                    bit_val = 1;
                } else if (pulse_width >= long_thresh) {
                    bit_val = 0;
                } else {
                    bit_val = 1;
                    C_ANN_PUT(di, ss_fall, samplenum, s->out_ann, ANN_WARN,
                        "Ambiguous bit width");
                }

                char bit_long[16], bit_short[4];
                snprintf(bit_long, sizeof(bit_long), "Bit: %d", bit_val);
                snprintf(bit_short, sizeof(bit_short), "%d", bit_val);
                C_ANN_PUT(di, ss_fall, samplenum, s->out_ann, ANN_BIT,
                    bit_long, bit_short);

                unsigned char bit_byte = (unsigned char)bit_val;
                c_decoder_put_python(di, ss_fall, samplenum, s->out_python, "BIT", &bit_byte, 1);

                if (s->bit_cnt >= 0) {
                    s->byte_val |= (bit_val << s->bit_cnt);
                    s->bit_cnt++;
                }

                if (s->bit_cnt == 8) {
                    if ((s->byte_val == 0x3C || s->byte_val == 0x69) && !s->overdrive) {
                        s->overdrive = 1;
                        C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_OVERDRIVE,
                            "Entering overdrive mode", "Overdrive on");
                        reset_thresh = 48 * samplerate / 1000000;
                        short_thresh = 1 * samplerate / 1000000;
                        long_thresh = 6 * samplerate / 1000000;
                    }
                    s->bit_cnt = -1;
                    s->byte_val = 0;
                }

                s->state = STATE_WAIT_FALL;
            }
            break;
        }

        case STATE_WAIT_PRESENCE_FALL: {
            cb = c_cond_new();
            c_cond_fall(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;
            ss_fall = samplenum;
            s->state = STATE_WAIT_PRESENCE_RISE;
            break;
        }

        case STATE_WAIT_PRESENCE_RISE: {
            cb = c_cond_new();
            c_cond_rise(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            C_ANN_PUT(di, ss_fall, samplenum, s->out_ann, ANN_PRESENCE,
                "Presence: true", "Presence", "Pres", "P");
            unsigned char pres_byte = 1;
            c_decoder_put_python(di, s->ss_rise, samplenum, s->out_python, "RESET/PRESENCE", &pres_byte, 1);

            s->bit_cnt = 0;
            s->byte_val = 0;
            s->state = STATE_WAIT_FALL;
            break;
        }
        }
    }
}

static void onewire_destroy(struct srd_decoder_inst* di)
{
    void* priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder onewire_c_decoder = {
    .id = "onewire_c",
    .name = "OneWire link layer(C)",
    .longname = "1-Wire serial communication bus (link layer)(C)",
    .desc = "Bidirectional, half-duplex, asynchronous serial bus.(C implementation)",
    .license = "gplv2+",
    .channels = ow_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = ow_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = ow_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = ow_ann_rows,
    .inputs = ow_inputs,
    .num_inputs = 1,
    .outputs = ow_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = ow_tags,
    .num_tags = 1,
    .reset = onewire_reset,
    .start = onewire_start,
    .decode = onewire_decode,
    .destroy = onewire_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder* srd_c_decoder_entry(void)
{
    ow_options[0].idn = "dec_onewire_link_opt_overdrive";
    ow_options[0].def = g_variant_new_string("no");
    GSList* od_vals = NULL;
    od_vals = g_slist_append(od_vals, g_variant_new_string("yes"));
    od_vals = g_slist_append(od_vals, g_variant_new_string("no"));
    ow_options[0].values = od_vals;
    return &onewire_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
