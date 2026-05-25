#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OneWire Link Layer decoder - Full implementation with overdrive support */

#define CH_OWR 0

#define ANN_BIT       0
#define ANN_WARN      1
#define ANN_RESET     2
#define ANN_PRESENCE  3
#define ANN_OVERDRIVE 4
#define NUM_ANN       5

enum owlink_state {
    STATE_INITIAL,
    STATE_IDLE,
    STATE_LOW,
    STATE_PRESENCE_DETECT_HIGH,
    STATE_PRESENCE_DETECT_LOW,
    STATE_SLOT,
    STATE_PRESENCE_DETECT,
};

/* Timing values in us */
struct ow_timing {
    double RSTL_min[2];   /* [0]=normal, [1]=overdrive */
    double RSTL_max[2];
    double RSTH_min[2];
    double PDH_min[2];
    double PDH_max[2];
    double PDL_min[2];
    double PDL_max[2];
    double SLOT_min[2];
    double SLOT_max[2];
    double REC_min[2];
    double LOWR_min[2];
    double LOWR_max[2];
};

static const struct ow_timing ow_timing = {
    .RSTL_min = {480.0, 48.0},
    .RSTL_max = {960.0, 80.0},
    .RSTH_min = {480.0, 48.0},
    .PDH_min  = {15.0,  2.0},
    .PDH_max  = {60.0,  6.0},
    .PDL_min  = {60.0,  8.0},
    .PDL_max  = {240.0, 24.0},
    .SLOT_min = {60.0,  6.0},
    .SLOT_max = {120.0, 16.0},
    .REC_min  = {1.0,   1.0},
    .LOWR_min = {1.0,   1.0},
    .LOWR_max = {15.0,  2.0},
};

struct owlink_priv {
    int state;
    uint8_t byte_val;
    int bit_cnt;
    uint64_t ss_rise;
    uint64_t ss_fall;
    int overdrive;
    int present;
    int bit_val;
    uint64_t samplerate;
    int out_ann;
    int out_python;
};

static uint64_t us_to_samples(uint64_t samplerate, double us)
{
    return (uint64_t)(us * (double)samplerate / 1000000.0);
}

static struct srd_channel owlink_channels[] = {
    { "owr", "OWR", "1-Wire signal line", 0, SRD_CHANNEL_SDATA, "dec_onewire_link_chan_owr" },
};

static struct srd_decoder_option owlink_options[] = {
    { "overdrive", "dec_onewire_link_opt_overdrive", "Start in overdrive speed", NULL, NULL },
};

static const char *owlink_ann_labels[][3] = {
    { "", "Bit", "B" },
    { "", "Warnings", "Warn", "W" },
    { "", "Reset", "Rst", "R" },
    { "", "Presence", "Pres", "P" },
    { "", "Overdrive speed notifications", "Overdrive" },
};

static const int owlink_row_bits_classes[] = { ANN_BIT, ANN_RESET, ANN_PRESENCE, -1 };
static const int owlink_row_info_classes[] = { ANN_OVERDRIVE, -1 };
static const int owlink_row_warnings_classes[] = { ANN_WARN, -1 };

static const struct srd_c_ann_row owlink_ann_rows[] = {
    { "bits", "Bits", owlink_row_bits_classes, 3 },
    { "info", "Info", owlink_row_info_classes, 1 },
    { "warnings", "Warnings", owlink_row_warnings_classes, 1 },
};

static const char *owlink_inputs[] = { "logic" };
static const char *owlink_outputs[] = { "onewire_link" };
static const char *owlink_tags[] = { "Embedded/industrial" };

static void owlink_checks(struct srd_decoder_inst *di, struct owlink_priv *s)
{
    if (s->overdrive) {
        if (s->samplerate < 2000000) {
            C_ANN_PUT(di, 0, 0, s->out_ann, ANN_WARN,
                "Sampling rate is too low. Must be above 2MHz for proper overdrive mode decoding.");
        } else if (s->samplerate < 5000000) {
            C_ANN_PUT(di, 0, 0, s->out_ann, ANN_WARN,
                "Sampling rate is suggested to be above 5MHz for proper overdrive mode decoding.");
        }
    } else {
        if (s->samplerate < 400000) {
            C_ANN_PUT(di, 0, 0, s->out_ann, ANN_WARN,
                "Sampling rate is too low. Must be above 400kHz for proper normal mode decoding.");
        } else if (s->samplerate < 1000000) {
            C_ANN_PUT(di, 0, 0, s->out_ann, ANN_WARN,
                "Sampling rate is suggested to be above 1MHz for proper normal mode decoding.");
        }
    }
}

static void owlink_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct owlink_priv)));
    struct owlink_priv *s = (struct owlink_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct owlink_priv));
    s->state = STATE_INITIAL;
    s->bit_cnt = -1;
    s->out_ann = -1;
    s->out_python = -1;
}

static void owlink_start(struct srd_decoder_inst *di)
{
    struct owlink_priv *s = (struct owlink_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "onewire_link");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "onewire_link");
    const char *od = c_decoder_get_option_string(di, "overdrive", "no");
    s->overdrive = (strcmp(od, "yes") == 0) ? 1 : 0;
    s->bit_cnt = -1;
}

static void owlink_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct owlink_priv *s = (struct owlink_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE)
        s->samplerate = value;
}

static void owlink_decode(struct srd_decoder_inst *di)
{
    struct owlink_priv *s = (struct owlink_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched = 0;

    if (!s->samplerate)
        s->samplerate = c_decoder_get_samplerate(di);
    if (!s->samplerate)
        return;

    owlink_checks(di, s);

    while (1) {
        srd_cond_builder *cb;
        int ret;
        int od = s->overdrive ? 1 : 0;

        switch (s->state) {

        case STATE_INITIAL: {
            /* Wait until we reach the idle high state */
            cb = c_cond_new();
            c_cond_high(cb, CH_OWR);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;
            s->ss_rise = samplenum;
            s->state = STATE_IDLE;
            break;
        }

        case STATE_IDLE: {
            /* Wait for falling edge */
            cb = c_cond_new();
            c_cond_fall(cb, CH_OWR);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;
            s->ss_fall = samplenum;

            /* Check recovery time */
            if (s->ss_rise > 0) {
                double time_us = (double)(s->ss_fall - s->ss_rise) / (double)s->samplerate * 1000000.0;
                if (time_us < ow_timing.REC_min[od]) {
                    char txt[64], txt2[64], txt3[64];
                    snprintf(txt, sizeof(txt), "Recovery time not long enough");
                    snprintf(txt2, sizeof(txt2), "Recovery too short");
                    snprintf(txt3, sizeof(txt3), "REC < %.0f", ow_timing.REC_min[od]);
                    C_ANN_PUT(di, s->ss_rise, s->ss_fall, s->out_ann, ANN_WARN, txt, txt2, txt3);
                }
            }
            s->state = STATE_LOW;
            break;
        }

        case STATE_LOW: {
            /* Wait for rising edge */
            cb = c_cond_new();
            c_cond_rise(cb, CH_OWR);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;
            s->ss_rise = samplenum;

            double time_us = (double)(s->ss_rise - s->ss_fall) / (double)s->samplerate * 1000000.0;

            if (time_us >= ow_timing.RSTL_min[0]) {
                /* Normal reset pulse */
                if (time_us > ow_timing.RSTL_max[0]) {
                    char txt[128], txt2[64], txt3[64];
                    snprintf(txt, sizeof(txt), "Too long reset pulse might mask interrupt signalling by other devices");
                    snprintf(txt2, sizeof(txt2), "Reset pulse too long");
                    snprintf(txt3, sizeof(txt3), "RST > %.0f", ow_timing.RSTL_max[0]);
                    C_ANN_PUT(di, s->ss_fall, s->ss_rise, s->out_ann, ANN_WARN, txt, txt2, txt3);
                }
                if (s->overdrive) {
                    C_ANN_PUT(di, s->ss_fall, s->ss_rise, s->out_ann, ANN_OVERDRIVE,
                        "Exiting overdrive mode", "Overdrive off");
                    s->overdrive = 0;
                }
                C_ANN_PUT(di, s->ss_fall, s->ss_rise, s->out_ann, ANN_RESET, "Reset", "Rst", "R");
                s->state = STATE_PRESENCE_DETECT_HIGH;
            } else if (s->overdrive && time_us >= ow_timing.RSTL_min[1] && time_us < ow_timing.RSTL_max[1]) {
                /* Overdrive reset pulse */
                C_ANN_PUT(di, s->ss_fall, s->ss_rise, s->out_ann, ANN_RESET, "Reset", "Rst", "R");
                s->state = STATE_PRESENCE_DETECT_HIGH;
            } else if (time_us < ow_timing.SLOT_max[od]) {
                /* Read/write time slot */
                if (time_us < ow_timing.LOWR_min[od]) {
                    char txt[64], txt2[64], txt3[64];
                    snprintf(txt, sizeof(txt), "Low signal not long enough");
                    snprintf(txt2, sizeof(txt2), "Low too short");
                    snprintf(txt3, sizeof(txt3), "LOW < %.0f", ow_timing.LOWR_min[od]);
                    C_ANN_PUT(di, s->ss_fall, s->ss_rise, s->out_ann, ANN_WARN, txt, txt2, txt3);
                }
                if (time_us < ow_timing.LOWR_max[od])
                    s->bit_val = 1; /* Short pulse = 1 bit */
                else
                    s->bit_val = 0; /* Long pulse = 0 bit */
                s->state = STATE_SLOT;
            } else {
                /* Timing outside known states */
                C_ANN_PUT(di, s->ss_fall, s->ss_rise, s->out_ann, ANN_WARN,
                    "Erroneous signal", "Error", "Err", "E");
                s->state = STATE_IDLE;
            }
            break;
        }

        case STATE_PRESENCE_DETECT_HIGH: {
            /* Wait for falling edge or timeout (PDH max) */
            uint64_t pdh_max_samples = us_to_samples(s->samplerate, ow_timing.PDH_max[od]);
            uint64_t skip_count = 0;
            if (s->ss_rise + pdh_max_samples > samplenum)
                skip_count = s->ss_rise + pdh_max_samples - samplenum;

            cb = c_cond_new();
            c_cond_fall(cb, CH_OWR);
            c_cond_or(cb);
            c_cond_skip(cb, skip_count);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            double time_us = (double)(samplenum - s->ss_rise) / (double)s->samplerate * 1000000.0;

            if ((matched & 0b1) && !(matched & 0b10)) {
                /* Presence detected (falling edge, not timeout) */
                if (time_us < ow_timing.PDH_min[od]) {
                    char txt[64], txt2[64], txt3[64];
                    snprintf(txt, sizeof(txt), "Presence detect signal is too early");
                    snprintf(txt2, sizeof(txt2), "Presence detect too early");
                    snprintf(txt3, sizeof(txt3), "PDH < %.0f", ow_timing.PDH_min[od]);
                    C_ANN_PUT(di, s->ss_rise, samplenum, s->out_ann, ANN_WARN, txt, txt2, txt3);
                }
                s->ss_fall = samplenum;
                s->state = STATE_PRESENCE_DETECT_LOW;
            } else {
                /* No presence detected */
                C_ANN_PUT(di, s->ss_rise, samplenum, s->out_ann, ANN_PRESENCE,
                    "Presence: false", "Presence", "Pres", "P");
                unsigned char pres = 0;
                c_decoder_put_python(di, s->ss_rise, samplenum, s->out_python, "RESET/PRESENCE", &pres, 1);
                s->state = STATE_IDLE;
            }
            break;
        }

        case STATE_PRESENCE_DETECT_LOW: {
            /* Wait for rising edge (end of presence signal) */
            cb = c_cond_new();
            c_cond_rise(cb, CH_OWR);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            double time_us = (double)(samplenum - s->ss_fall) / (double)s->samplerate * 1000000.0;
            od = s->overdrive ? 1 : 0;

            if (time_us < ow_timing.PDL_min[od]) {
                char txt[64], txt2[64], txt3[64];
                snprintf(txt, sizeof(txt), "Presence detect signal is too short");
                snprintf(txt2, sizeof(txt2), "Presence detect too short");
                snprintf(txt3, sizeof(txt3), "PDL < %.0f", ow_timing.PDL_min[od]);
                C_ANN_PUT(di, s->ss_fall, samplenum, s->out_ann, ANN_WARN, txt, txt2, txt3);
            } else if (time_us > ow_timing.PDL_max[od]) {
                char txt[64], txt2[64], txt3[64];
                snprintf(txt, sizeof(txt), "Presence detect signal is too long");
                snprintf(txt2, sizeof(txt2), "Presence detect too long");
                snprintf(txt3, sizeof(txt3), "PDL > %.0f", ow_timing.PDL_max[od]);
                C_ANN_PUT(di, s->ss_fall, samplenum, s->out_ann, ANN_WARN, txt, txt2, txt3);
            }

            if (time_us > ow_timing.RSTH_min[od])
                s->ss_rise = samplenum;

            s->state = STATE_PRESENCE_DETECT;
            break;
        }

        case STATE_SLOT: {
            /* Wait for falling edge or end of time slot */
            uint64_t slot_min_samples = us_to_samples(s->samplerate, ow_timing.SLOT_min[od]);
            uint64_t skip_count = 0;
            if (s->ss_fall + slot_min_samples > samplenum)
                skip_count = s->ss_fall + slot_min_samples - samplenum;

            cb = c_cond_new();
            c_cond_fall(cb, CH_OWR);
            c_cond_or(cb);
            c_cond_skip(cb, skip_count);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            if ((matched & 0b1) && !(matched & 0b10)) {
                /* Low detected before end of slot */
                char txt[64], txt2[64], txt3[64];
                snprintf(txt, sizeof(txt), "Time slot not long enough");
                snprintf(txt2, sizeof(txt2), "Slot too short");
                snprintf(txt3, sizeof(txt3), "SLOT < %.1f", ow_timing.SLOT_min[od]);
                C_ANN_PUT(di, s->ss_fall, samplenum, s->out_ann, ANN_WARN, txt, txt2, txt3);
                s->ss_fall = samplenum;
                s->state = STATE_LOW;
            } else {
                /* End of time slot - output bit */
                char bit_long[16], bit_short[4];
                snprintf(bit_long, sizeof(bit_long), "Bit: %d", s->bit_val);
                snprintf(bit_short, sizeof(bit_short), "%d", s->bit_val);
                C_ANN_PUT(di, s->ss_fall, samplenum, s->out_ann, ANN_BIT, bit_long, bit_short);

                unsigned char bit_byte = (unsigned char)s->bit_val;
                c_decoder_put_python(di, s->ss_fall, samplenum, s->out_python, "BIT", &bit_byte, 1);

                /* Save command bits */
                if (s->bit_cnt >= 0) {
                    s->byte_val |= (s->bit_val << s->bit_cnt);
                    s->bit_cnt++;
                }

                /* Check for overdrive ROM command */
                if (s->bit_cnt >= 8) {
                    if ((s->byte_val == 0x3C || s->byte_val == 0x69) && !s->overdrive) {
                        s->overdrive = 1;
                        C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_OVERDRIVE,
                            "Entering overdrive mode", "Overdrive on");
                    }
                    s->bit_cnt = -1;
                    s->byte_val = 0;
                }

                s->state = STATE_IDLE;
            }
            break;
        }

        case STATE_PRESENCE_DETECT: {
            /* Wait for falling edge or end of presence detect */
            uint64_t rsth_min_samples = us_to_samples(s->samplerate, ow_timing.RSTH_min[od]);
            uint64_t skip_count = 0;
            if (s->ss_rise + rsth_min_samples > samplenum)
                skip_count = s->ss_rise + rsth_min_samples - samplenum;

            cb = c_cond_new();
            c_cond_fall(cb, CH_OWR);
            c_cond_or(cb);
            c_cond_skip(cb, skip_count);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            if ((matched & 0b1) && !(matched & 0b10)) {
                /* Low detected before end of presence detect */
                char txt[64], txt2[64], txt3[64];
                snprintf(txt, sizeof(txt), "Presence detect not long enough");
                snprintf(txt2, sizeof(txt2), "Presence detect too short");
                snprintf(txt3, sizeof(txt3), "RTSH < %.0f", ow_timing.RSTH_min[od]);
                C_ANN_PUT(di, s->ss_rise, samplenum, s->out_ann, ANN_WARN, txt, txt2, txt3);

                C_ANN_PUT(di, s->ss_rise, samplenum, s->out_ann, ANN_PRESENCE,
                    "Slave presence detected", "Slave present", "Present", "P");
                unsigned char pres = 1;
                c_decoder_put_python(di, s->ss_rise, samplenum, s->out_python, "RESET/PRESENCE", &pres, 1);
                s->ss_fall = samplenum;
                s->state = STATE_LOW;
            } else {
                /* End of presence detect */
                C_ANN_PUT(di, s->ss_rise, samplenum, s->out_ann, ANN_PRESENCE,
                    "Presence: true", "Presence", "Pres", "P");
                unsigned char pres = 1;
                c_decoder_put_python(di, s->ss_rise, samplenum, s->out_python, "RESET/PRESENCE", &pres, 1);
                s->ss_rise = samplenum;
                /* Start counting the first 8 bits to get the ROM command */
                s->bit_cnt = 0;
                s->byte_val = 0;
                s->state = STATE_IDLE;
            }
            break;
        }
        }
    }
}

static void owlink_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder onewire_link_c_decoder = {
    .id = "onewire_link_c",
    .name = "OneWire link layer(C)",
    .longname = "1-Wire serial communication bus (link layer)(C)",
    .desc = "Bidirectional, half-duplex, asynchronous serial bus.(C implementation)",
    .license = "gplv2+",
    .channels = owlink_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = owlink_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = owlink_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = owlink_ann_rows,
    .inputs = owlink_inputs,
    .num_inputs = 1,
    .outputs = owlink_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = owlink_tags,
    .num_tags = 1,
    .reset = owlink_reset,
    .start = owlink_start,
    .decode = owlink_decode,
    .destroy = owlink_destroy,
    .metadata = owlink_metadata,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    owlink_options[0].def = g_variant_new_string("no");
    GSList *od_vals = NULL;
    od_vals = g_slist_append(od_vals, g_variant_new_string("yes"));
    od_vals = g_slist_append(od_vals, g_variant_new_string("no"));
    owlink_options[0].values = od_vals;
    return &onewire_link_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
