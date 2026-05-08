#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

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
    uint64_t ss_byte;
    uint64_t ss_slot;
    int out_ann;
};

#define ANN_RESET_PRESENCE 0
#define ANN_PRESENCE       1
#define ANN_BIT            2
#define ANN_BYTE           3
#define ANN_RESET          4
#define ANN_SLOT           5

static struct srd_channel ow_channels[] = {
    {"ow", "OW", "1-Wire data line", 0, SRD_CHANNEL_SDATA, NULL},
};

static const char *ow_ann_labels[][3] = {
    {"", "RESET/PRESENCE", "Reset/presence"},
    {"", "PRESENCE", "Presence pulse"},
    {"", "BIT", "Data bit"},
    {"", "BYTE", "Data byte"},
    {"", "RESET", "Reset pulse"},
    {"", "SLOT", "Time slot"},
};

static const int ow_row_bits_classes[] = {ANN_BIT, ANN_SLOT};
static const int ow_row_bytes_classes[] = {ANN_BYTE};
static const int ow_row_control_classes[] = {ANN_RESET_PRESENCE, ANN_PRESENCE, ANN_RESET};
static const struct srd_c_ann_row ow_ann_rows[] = {
    {"bits", "Bits", ow_row_bits_classes, 2},
    {"bytes", "Bytes", ow_row_bytes_classes, 1},
    {"control", "Control", ow_row_control_classes, 3},
};

static const char *ow_inputs[] = {"logic", NULL};
static const char *ow_outputs[] = {"onewire", NULL};
static const char *ow_tags[] = {"Embedded/industrial", NULL};

static void onewire_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct ow_priv)));
    }
    struct ow_priv *s = (struct ow_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct ow_priv));
    s->state = STATE_WAIT_FALL;
}

static void onewire_start(struct srd_decoder_inst *di)
{
    struct ow_priv *s = (struct ow_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "onewire");
}

static void onewire_decode(struct srd_decoder_inst *di)
{
    struct ow_priv *s = (struct ow_priv *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;
    uint64_t samplerate = c_decoder_get_samplerate(di);
    uint64_t reset_thresh = 480 * samplerate / 1000000;
    uint64_t short_thresh = 15 * samplerate / 1000000;
    uint64_t long_thresh = 60 * samplerate / 1000000;
    uint64_t ss_fall = 0;

    while (1) {
        srd_cond_builder *cb;
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
            s->ss_slot = samplenum;
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
                C_ANN_PUT(di, ss_fall, samplenum, s->out_ann, ANN_RESET, "Reset", "RST");
                C_ANN_PUT(di, ss_fall, samplenum, s->out_ann, ANN_RESET_PRESENCE, "Reset/presence", "R/P");
                s->state = STATE_WAIT_PRESENCE_FALL;
            } else {
                int bit_val;
                if (pulse_width < short_thresh) {
                    bit_val = 1;
                } else if (pulse_width >= long_thresh) {
                    bit_val = 0;
                } else {
                    bit_val = 1;
                }

                char bit_str[8];
                snprintf(bit_str, sizeof(bit_str), "%d", bit_val);
                C_ANN_PUT(di, ss_fall, samplenum, s->out_ann, ANN_BIT, bit_str);
                C_ANN_PUT(di, s->ss_slot, samplenum, s->out_ann, ANN_SLOT, bit_str);

                if (s->bit_cnt == 0)
                    s->ss_byte = s->ss_slot;

                s->byte_val |= (bit_val << s->bit_cnt);
                s->bit_cnt++;

                if (s->bit_cnt == 8) {
                    char byte_str[16];
                    snprintf(byte_str, sizeof(byte_str), "0x%02X", s->byte_val);
                    C_ANN_PUT(di, s->ss_byte, samplenum, s->out_ann, ANN_BYTE, byte_str);
                    s->bit_cnt = 0;
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

            C_ANN_PUT(di, ss_fall, samplenum, s->out_ann, ANN_PRESENCE, "Presence", "P");
            s->bit_cnt = 0;
            s->byte_val = 0;
            s->state = STATE_WAIT_FALL;
            break;
        }

        }
    }
}

static void onewire_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder onewire_c_decoder = {
    .id = "onewire_c",
    .name = "1-Wire(C)",
    .longname = "1-Wire link layer (C)",
    .desc = "1-Wire protocol decoder (C implementation)",
    .license = "gplv2+",
    .channels = ow_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = 6,
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

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &onewire_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
