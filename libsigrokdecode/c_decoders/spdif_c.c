#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

#define ANN_BITRATE   0
#define ANN_PREAMBLE  1
#define ANN_BITS      2
#define ANN_AUX       3
#define ANN_SAMPLES   4
#define ANN_VALIDITY  5
#define ANN_SUBCODE   6
#define ANN_CHAN_STAT 7
#define ANN_PARITY    8

#define STATE_GET_FIRST_PULSE  0
#define STATE_GET_SECOND_PULSE 1
#define STATE_GET_THIRD_PULSE  2
#define STATE_DECODE_STREAM    3
#define STATE_DECODE_PREAMBLE  4

#define MAX_SUBFRAME 28
#define MAX_PULSE_BUF 64

struct subframe_entry {
    int pulse_type;
    uint64_t ss;
    uint64_t es;
};

struct spdif_priv {
    int state;
    uint64_t ss_edge;
    int first_edge;
    uint64_t samplenum_prev_edge;
    uint64_t pulse_width;

    uint64_t clocks[3];
    int num_clocks;
    double range1;
    double range2;

    int preamble_state;
    int preamble[4];
    int preamble_count;
    int seen_preamble;
    uint64_t last_preamble;

    int first_one;
    struct subframe_entry subframe[MAX_SUBFRAME];
    int bitcount;

    uint64_t temp_pulse_width[MAX_PULSE_BUF];
    uint64_t temp_samplenum[MAX_PULSE_BUF];
    int temp_count;

    int out_ann;
};

static struct srd_channel spdif_channels[] = {
    {"data", "Data", "Data line", 0, SRD_CHANNEL_COMMON, "dec_spdif_chan_data"},
};

static const char *spdif_ann_labels[][3] = {
    {"", "bitrate", "Bitrate / baudrate"},
    {"", "preamble", "Preamble"},
    {"", "bits", "Bits"},
    {"", "aux", "Auxillary-audio-databits"},
    {"", "samples", "Audio Samples"},
    {"", "validity", "Data Valid"},
    {"", "subcode", "Subcode data"},
    {"", "chan_stat", "Channnel Status"},
    {"", "parity", "Parity Bit"},
};

static const int spdif_row_info_classes[] = {ANN_BITRATE, ANN_PREAMBLE, ANN_AUX, ANN_VALIDITY, ANN_SUBCODE, ANN_CHAN_STAT, ANN_PARITY};
static const int spdif_row_bits_classes[] = {ANN_BITS};
static const int spdif_row_samples_classes[] = {ANN_SAMPLES};
static const struct srd_c_ann_row spdif_ann_rows[] = {
    {"info", "Info", spdif_row_info_classes, 7},
    {"bits", "Bits", spdif_row_bits_classes, 1},
    {"samples", "Samples", spdif_row_samples_classes, 1},
};

static const char *spdif_inputs[] = {"logic", NULL};
static const char *spdif_outputs[] = {NULL};
static const char *spdif_tags[] = {"Audio", "PC", NULL};

static int get_pulse_type(struct spdif_priv *s)
{
    if (s->range1 == 0 || s->range2 == 0)
        return -1;
    if ((double)s->pulse_width >= s->range2)
        return 2;
    else if ((double)s->pulse_width >= s->range1)
        return 0;
    else
        return 1;
}

static int __attribute__((unused)) get_pulse_type_for_width(struct spdif_priv *s, uint64_t width)
{
    if (s->range1 == 0 || s->range2 == 0)
        return -1;
    if ((double)width >= s->range2)
        return 2;
    else if ((double)width >= s->range1)
        return 0;
    else
        return 1;
}

static void emit_subframe(struct srd_decoder_inst *di, struct spdif_priv *s)
{
    struct subframe_entry *sf = s->subframe;
    char str_buf[64];

    uint32_t aux_val = 0;
    for (int i = 0; i < 4; i++)
        aux_val = (aux_val << 1) | (sf[i].pulse_type == 1 ? 1 : 0);

    uint32_t sample_val = 0;
    for (int i = 4; i < 24; i++)
        sample_val = (sample_val << 1) | (sf[i].pulse_type == 1 ? 1 : 0);

    uint32_t full_val = (aux_val << 20) | sample_val;

    uint32_t aux_rot = 0;
    for (int i = 3; i >= 0; i--)
        aux_rot = (aux_rot << 1) | (sf[i].pulse_type == 1 ? 1 : 0);

    uint32_t sample_rot = 0;
    for (int i = 23; i >= 4; i--)
        sample_rot = (sample_rot << 1) | (sf[i].pulse_type == 1 ? 1 : 0);

    uint32_t full_rot = (aux_rot << 20) | sample_rot;

    snprintf(str_buf, sizeof(str_buf), "Aux 0x%x", aux_val);
    char aux_short[16];
    snprintf(aux_short, sizeof(aux_short), "0x%x", aux_val);
    C_ANN_PUT(di, sf[0].ss, sf[3].es, s->out_ann, ANN_AUX, str_buf, aux_short);

    snprintf(str_buf, sizeof(str_buf), "Sample 0x%x", full_val);
    char sample_short[16];
    snprintf(sample_short, sizeof(sample_short), "0x%x", full_val);
    C_ANN_PUT(di, sf[4].ss, sf[23].es, s->out_ann, ANN_AUX, str_buf, sample_short);

    snprintf(str_buf, sizeof(str_buf), "Audio 0x%x", full_rot);
    char audio_short[16];
    snprintf(audio_short, sizeof(audio_short), "0x%x", full_rot);
    C_ANN_PUT(di, sf[0].ss, sf[23].es, s->out_ann, ANN_SAMPLES, str_buf, audio_short);

    if (sf[24].pulse_type == 0)
        C_ANN_PUT(di, sf[24].ss, sf[24].es, s->out_ann, ANN_VALIDITY, "V");
    else
        C_ANN_PUT(di, sf[24].ss, sf[24].es, s->out_ann, ANN_VALIDITY, "E");

    snprintf(str_buf, sizeof(str_buf), "S: %d", sf[25].pulse_type == 1 ? 1 : 0);
    C_ANN_PUT(di, sf[25].ss, sf[25].es, s->out_ann, ANN_SUBCODE, str_buf);

    snprintf(str_buf, sizeof(str_buf), "C: %d", sf[26].pulse_type == 1 ? 1 : 0);
    C_ANN_PUT(di, sf[26].ss, sf[26].es, s->out_ann, ANN_CHAN_STAT, str_buf);

    snprintf(str_buf, sizeof(str_buf), "P: %d", sf[27].pulse_type == 1 ? 1 : 0);
    C_ANN_PUT(di, sf[27].ss, sf[27].es, s->out_ann, ANN_PARITY, str_buf);
}

static void spdif_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct spdif_priv)));
    }
    struct spdif_priv *s = (struct spdif_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct spdif_priv));
    s->state = STATE_GET_FIRST_PULSE;
    s->first_edge = 1;
    s->seen_preamble = 0;
    s->first_one = 1;
    s->preamble_state = -1;
}

static void spdif_start(struct srd_decoder_inst *di)
{
    struct spdif_priv *s = (struct spdif_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "spdif");
}

static void spdif_decode(struct srd_decoder_inst *di)
{
    struct spdif_priv *s = (struct spdif_priv *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    srd_cond_builder *cb = c_cond_new();
    c_cond_edge(cb, 0);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK)
        return;

    s->samplenum_prev_edge = samplenum;

    while (1) {
        cb = c_cond_new();
        c_cond_edge(cb, 0);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        s->pulse_width = samplenum - s->samplenum_prev_edge - 1;
        s->samplenum_prev_edge = samplenum;

        if (s->state == STATE_GET_FIRST_PULSE) {
            if (s->pulse_width != 0) {
                s->clocks[0] = s->pulse_width;
                s->num_clocks = 1;
                s->state = STATE_GET_SECOND_PULSE;
            }
        } else if (s->state == STATE_GET_SECOND_PULSE) {
            if (s->pulse_width > (s->clocks[0] * 13 / 10) ||
                s->pulse_width < (s->clocks[0] * 7 / 10)) {
                s->clocks[1] = s->pulse_width;
                s->num_clocks = 2;
                s->state = STATE_GET_THIRD_PULSE;
            }
        } else if (s->state == STATE_GET_THIRD_PULSE) {
            if ((s->pulse_width <= (s->clocks[0] * 13 / 10) &&
                 s->pulse_width >= (s->clocks[0] * 7 / 10)) ||
                (s->pulse_width <= (s->clocks[1] * 13 / 10) &&
                 s->pulse_width >= (s->clocks[1] * 7 / 10))) {
                continue;
            }

            s->clocks[2] = s->pulse_width;
            s->num_clocks = 3;

            if (s->clocks[0] > s->clocks[1]) {
                uint64_t tmp = s->clocks[0];
                s->clocks[0] = s->clocks[1];
                s->clocks[1] = tmp;
            }
            if (s->clocks[1] > s->clocks[2]) {
                uint64_t tmp = s->clocks[1];
                s->clocks[1] = s->clocks[2];
                s->clocks[2] = tmp;
            }
            if (s->clocks[0] > s->clocks[1]) {
                uint64_t tmp = s->clocks[0];
                s->clocks[0] = s->clocks[1];
                s->clocks[1] = tmp;
            }

            s->range1 = ((double)s->clocks[0] + (double)s->clocks[1]) / 2.0;
            s->range2 = ((double)s->clocks[1] + (double)s->clocks[2]) / 2.0;

            uint64_t samplerate = c_decoder_get_samplerate(di);
            int spdif_bitrate = (int)((double)samplerate / ((double)s->clocks[2] / 1.5));

            char bitrate_str[128];
            snprintf(bitrate_str, sizeof(bitrate_str),
                     "Signal Bitrate: %d Mbit/s (=> %d kHz)",
                     spdif_bitrate, spdif_bitrate / (2 * 32));
            C_ANN_PUT(di, 0, samplenum - 24, s->out_ann, ANN_BITRATE, bitrate_str);

            s->last_preamble = samplenum;
            s->ss_edge = 0;
            s->state = STATE_DECODE_STREAM;
        } else if (s->state == STATE_DECODE_STREAM) {
            int pulse = get_pulse_type(s);

            if (!s->seen_preamble) {
                if (pulse == 2) {
                    s->preamble[0] = pulse;
                    s->preamble_count = 1;
                    s->preamble_state = 0;
                    s->state = STATE_DECODE_PREAMBLE;
                    s->ss_edge = samplenum - s->pulse_width - 1;
                }
                continue;
            }

            if (pulse == 1 && s->first_one) {
                s->first_one = 0;
                s->subframe[s->bitcount].pulse_type = pulse;
                s->subframe[s->bitcount].ss = samplenum - s->pulse_width - 1;
                s->subframe[s->bitcount].es = samplenum;
            } else if (pulse == 1 && !s->first_one) {
                s->subframe[s->bitcount].es = samplenum;
                C_ANN_PUT(di, s->subframe[s->bitcount].ss, samplenum,
                          s->out_ann, ANN_BITS, "1");
                s->bitcount++;
                s->first_one = 1;
            } else {
                s->subframe[s->bitcount].pulse_type = pulse;
                s->subframe[s->bitcount].ss = samplenum - s->pulse_width - 1;
                s->subframe[s->bitcount].es = samplenum;
                C_ANN_PUT(di, samplenum - s->pulse_width - 1, samplenum,
                          s->out_ann, ANN_BITS, "0");
                s->bitcount++;
            }

            if (s->bitcount == 28) {
                emit_subframe(di, s);
                s->bitcount = 0;
                s->seen_preamble = 0;
                s->first_one = 1;
            }
        } else if (s->state == STATE_DECODE_PREAMBLE) {
            int pulse = get_pulse_type(s);

            s->preamble[s->preamble_count] = pulse;
            s->preamble_count++;

            if (s->preamble_count == 4) {
                if (s->preamble[0] == 2 && s->preamble[1] == 0 &&
                    s->preamble[2] == 1 && s->preamble[3] == 0) {
                    C_ANN_PUT(di, s->ss_edge, samplenum, s->out_ann, ANN_PREAMBLE,
                              "Preamble W", "W");
                    s->seen_preamble = 1;
                } else if (s->preamble[0] == 2 && s->preamble[1] == 2 &&
                           s->preamble[2] == 1 && s->preamble[3] == 1) {
                    C_ANN_PUT(di, s->ss_edge, samplenum, s->out_ann, ANN_PREAMBLE,
                              "Preamble M", "M");
                    s->seen_preamble = 1;
                } else if (s->preamble[0] == 2 && s->preamble[1] == 1 &&
                           s->preamble[2] == 1 && s->preamble[3] == 2) {
                    C_ANN_PUT(di, s->ss_edge, samplenum, s->out_ann, ANN_PREAMBLE,
                              "Preamble B", "B");
                    s->seen_preamble = 1;
                } else {
                    C_ANN_PUT(di, s->ss_edge, samplenum, s->out_ann, ANN_PREAMBLE,
                              "Unknown Preamble", "Unknown Prea.", "U");
                }

                s->bitcount = 0;
                s->first_one = 1;
                s->last_preamble = samplenum;
                s->state = STATE_DECODE_STREAM;
            }
        }
    }
}

static void spdif_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder spdif_c_decoder = {
    .id = "spdif_c",
    .name = "S/PDIF(C)",
    .longname = "Sony/Philips Digital Interface Format (C)",
    .desc = "S/PDIF protocol decoder (C implementation)",
    .license = "gplv2+",
    .channels = spdif_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = 9,
    .ann_labels = spdif_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = spdif_ann_rows,
    .inputs = spdif_inputs,
    .num_inputs = 1,
    .outputs = spdif_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = spdif_tags,
    .num_tags = 2,
    .reset = spdif_reset,
    .start = spdif_start,
    .decode = spdif_decode,
    .destroy = spdif_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &spdif_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
