#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum ps2_ann {
    ANN_BIT = 0,
    ANN_HSTART,
    ANN_DSTART,
    ANN_STOP,
    ANN_PARITY_OK,
    ANN_PARITY_ERR,
    ANN_DATA_BIT,
    ANN_WORD,
    ANN_ACK,
    NUM_ANN,
};

enum ps2_state {
    STATE_IDLE,
    STATE_HtoD_DATA,
    STATE_HtoD_ACK_WAIT,
    STATE_HtoD_ACK,
    STATE_DtoH_DATA,
    STATE_DtoH_NEXT,
};

#define CLK  0
#define DATA 1

struct ps2_priv {
    int state;
    int bitcount;
    int bits[12];
    uint64_t bit_ss[12];
    uint8_t byte_val;
    int htd;
    int htd_clock;
    int dth_clock;
    int out_ann;
};

static struct srd_channel ps2_channels[] = {
    {"clk", "CLK", "Clock line", 0, SRD_CHANNEL_SCLK, NULL},
    {"data", "DATA", "Data line", 1, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option ps2_options[] = {
    {
        .id = "HtoD_Clock",
        .idn = NULL,
        .desc = "HtoD_Clock",
        .def = NULL,
        .values = NULL,
    },
    {
        .id = "DtoH_Clock",
        .idn = NULL,
        .desc = "DtoH_Clock",
        .def = NULL,
        .values = NULL,
    },
};

static const char *ps2_ann_labels[][3] = {
    {"", "bit", "Bit"},
    {"", "HSTART", "HSTART"},
    {"", "DSTART", "DSTART"},
    {"", "stop-bit", "Stop bit"},
    {"", "parity-ok", "Parity OK bit"},
    {"", "parity-err", "Parity error bit"},
    {"", "data-bit", "Data bit"},
    {"", "word", "Word"},
    {"", "ACK", "ACK"},
};

static const int ps2_row_bits_classes[] = {ANN_BIT};
static const int ps2_row_fields_classes[] = {ANN_HSTART, ANN_DSTART, ANN_STOP, ANN_PARITY_OK, ANN_PARITY_ERR, ANN_DATA_BIT, ANN_WORD, ANN_ACK};
static const struct srd_c_ann_row ps2_ann_rows[] = {
    {"bits", "Bits", ps2_row_bits_classes, 1},
    {"fields", "Fields", ps2_row_fields_classes, 8},
};

static const char *ps2_inputs[] = {"logic", NULL};
static const char *ps2_outputs[] = {NULL};
static const char *ps2_tags[] = {"PC", NULL};

static void ps2_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct ps2_priv)));
    }
    struct ps2_priv *s = (struct ps2_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct ps2_priv));
    s->state = STATE_IDLE;
    s->htd = 0;
    s->htd_clock = 0;
    s->dth_clock = 1;
}

static void ps2_start(struct srd_decoder_inst *di)
{
    struct ps2_priv *s = (struct ps2_priv *)c_decoder_get_private(di);

    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "ps2");

    const char *htod_str = c_decoder_get_option_string(di, "HtoD_Clock", "rise");
    const char *dtoh_str = c_decoder_get_option_string(di, "DtoH_Clock", "fall");

    s->htd_clock = (strcmp(htod_str, "rise") == 0) ? 0 : 1;
    s->dth_clock = (strcmp(dtoh_str, "fall") == 0) ? 1 : 0;
}

static void ps2_handle_byte(struct srd_decoder_inst *di, uint64_t samplenum)
{
    struct ps2_priv *s = (struct ps2_priv *)c_decoder_get_private(di);
    int i;

    for (i = 0; i < 11; i++) {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", s->bits[i]);
        uint64_t es = (i < 10) ? s->bit_ss[i + 1] : samplenum;
        C_ANN_PUT(di, s->bit_ss[i], es, s->out_ann, ANN_BIT, bit_str);
    }

    if (s->htd) {
        C_ANN_PUT(di, s->bit_ss[0], s->bit_ss[1], s->out_ann, ANN_HSTART,
                  "Host Start", "HStart", "HS");
    } else {
        C_ANN_PUT(di, s->bit_ss[0], s->bit_ss[1], s->out_ann, ANN_DSTART,
                  "Device Start", "Device", "DS");
    }

    s->byte_val = 0;
    for (i = 0; i < 8; i++) {
        s->byte_val |= (s->bits[i + 1] << i);
    }

    {
        char word_long[16], word_mid[16], word_short[16];
        snprintf(word_long, sizeof(word_long), "Data: %02x", s->byte_val);
        snprintf(word_mid, sizeof(word_mid), "D: %02x", s->byte_val);
        snprintf(word_short, sizeof(word_short), "%02x", s->byte_val);
        C_ANN_PUT(di, s->bit_ss[1], s->bit_ss[9], s->out_ann, ANN_WORD,
                  word_long, word_mid, word_short);
    }

    {
        int ones = 0;
        for (i = 0; i < 8; i++) {
            if (s->byte_val & (1 << i))
                ones++;
        }
        ones += s->bits[9];
        int parity_ok = (ones % 2 == 1);

        if (parity_ok) {
            C_ANN_PUT(di, s->bit_ss[9], s->bit_ss[10], s->out_ann, ANN_PARITY_OK,
                      "Parity OK", "Par OK", "P");
        } else {
            C_ANN_PUT(di, s->bit_ss[9], s->bit_ss[10], s->out_ann, ANN_PARITY_ERR,
                      "Parity error", "Par err", "PE");
        }
    }

    {
        uint64_t bitwidth = s->bit_ss[2] - s->bit_ss[1];
        C_ANN_PUT(di, s->bit_ss[10], s->bit_ss[10] + bitwidth, s->out_ann, ANN_STOP,
                  "Stop bit", "Stop", "St", "T");
    }
}

static void ps2_decode(struct srd_decoder_inst *di)
{
    struct ps2_priv *s = (struct ps2_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;
    int ret;

    while (1) {
        switch (s->state) {

        case STATE_IDLE: {
            srd_cond_builder *b = c_cond_new();
            c_cond_fall(b, CLK);
            c_cond_low(b, DATA);
            c_cond_or(b);
            c_cond_fall(b, CLK);
            c_cond_high(b, DATA);
            c_cond_or(b);
            c_cond_rise(b, CLK);
            c_cond_low(b, DATA);
            ret = c_cond_wait(b, di, &samplenum, &matched);
            c_cond_free(b);
            if (ret != SRD_OK)
                return;

            if (matched & (1ULL << 0)) {
                s->htd = 1;
                s->state = STATE_HtoD_DATA;
                s->bitcount = 0;
                memset(s->bits, 0, sizeof(s->bits));
                memset(s->bit_ss, 0, sizeof(s->bit_ss));
                {
                    int data_val = c_decoder_get_pin(di, DATA, samplenum);
                    s->bits[0] = data_val;
                    s->bit_ss[0] = samplenum;
                    s->bitcount = 1;
                }
            } else if (matched & (1ULL << 1)) {
                s->htd = 1;
                s->state = STATE_HtoD_DATA;
                s->bitcount = 0;
                memset(s->bits, 0, sizeof(s->bits));
                memset(s->bit_ss, 0, sizeof(s->bit_ss));
                {
                    int data_val = c_decoder_get_pin(di, DATA, samplenum);
                    s->bits[0] = data_val;
                    s->bit_ss[0] = samplenum;
                    s->bitcount = 1;
                }
            } else if (matched & (1ULL << 2)) {
                s->htd = 0;
                s->state = STATE_DtoH_DATA;
                s->bitcount = 0;
                memset(s->bits, 0, sizeof(s->bits));
                memset(s->bit_ss, 0, sizeof(s->bit_ss));
                {
                    int data_val = c_decoder_get_pin(di, DATA, samplenum);
                    s->bits[0] = data_val;
                    s->bit_ss[0] = samplenum;
                    s->bitcount = 1;
                }
            }
            break;
        }

        case STATE_HtoD_DATA: {
            srd_cond_builder *b = c_cond_new();
            if (s->htd_clock == 0)
                c_cond_rise(b, CLK);
            else
                c_cond_fall(b, CLK);
            ret = c_cond_wait(b, di, &samplenum, &matched);
            c_cond_free(b);
            if (ret != SRD_OK)
                return;

            {
                int data_val = c_decoder_get_pin(di, DATA, samplenum);
                if (s->bitcount < 12) {
                    s->bits[s->bitcount] = data_val;
                    s->bit_ss[s->bitcount] = samplenum;
                }
                s->bitcount++;

                if (s->bitcount == 10) {
                    s->state = STATE_HtoD_ACK_WAIT;
                }
            }
            break;
        }

        case STATE_HtoD_ACK_WAIT: {
            srd_cond_builder *b = c_cond_new();
            c_cond_rise(b, CLK);
            ret = c_cond_wait(b, di, &samplenum, &matched);
            c_cond_free(b);
            if (ret != SRD_OK)
                return;

            {
                int data_val = c_decoder_get_pin(di, DATA, samplenum);
                if (s->bitcount < 12) {
                    s->bits[s->bitcount] = data_val;
                    s->bit_ss[s->bitcount] = samplenum;
                }
                s->bitcount++;

                if (s->bitcount == 11) {
                    ps2_handle_byte(di, samplenum);
                    s->state = STATE_HtoD_ACK;
                }
            }
            break;
        }

        case STATE_HtoD_ACK: {
            uint64_t ack_ss = samplenum;
            srd_cond_builder *b = c_cond_new();
            c_cond_fall(b, CLK);
            ret = c_cond_wait(b, di, &samplenum, &matched);
            c_cond_free(b);
            if (ret != SRD_OK)
                return;

            {
                int data_val = c_decoder_get_pin(di, DATA, samplenum);
                if (s->bitcount < 12) {
                    s->bits[s->bitcount] = data_val;
                    s->bit_ss[s->bitcount] = samplenum;
                }
                s->bitcount++;
            }

            {
                srd_cond_builder *b2 = c_cond_new();
                c_cond_rise(b2, CLK);
                ret = c_cond_wait(b2, di, &samplenum, &matched);
                c_cond_free(b2);
                if (ret != SRD_OK)
                    return;

                C_ANN_PUT(di, ack_ss, samplenum, s->out_ann, ANN_ACK,
                          "ACK", "ACK", "A");
            }

            s->state = STATE_IDLE;
            s->htd = 0;
            break;
        }

        case STATE_DtoH_DATA: {
            srd_cond_builder *b = c_cond_new();
            if (s->dth_clock == 1)
                c_cond_fall(b, CLK);
            else
                c_cond_rise(b, CLK);
            ret = c_cond_wait(b, di, &samplenum, &matched);
            c_cond_free(b);
            if (ret != SRD_OK)
                return;

            {
                int data_val = c_decoder_get_pin(di, DATA, samplenum);
                if (s->bitcount < 12) {
                    s->bits[s->bitcount] = data_val;
                    s->bit_ss[s->bitcount] = samplenum;
                }
                s->bitcount++;

                if (s->bitcount == 11) {
                    ps2_handle_byte(di, samplenum);
                    s->state = STATE_DtoH_NEXT;
                }
            }
            break;
        }

        case STATE_DtoH_NEXT: {
            srd_cond_builder *b = c_cond_new();
            c_cond_fall(b, DATA);
            c_cond_or(b);
            c_cond_rise(b, CLK);
            ret = c_cond_wait(b, di, &samplenum, &matched);
            c_cond_free(b);
            if (ret != SRD_OK)
                return;

            if (matched & (1ULL << 0)) {
                s->htd = 1;
                s->state = STATE_IDLE;
            } else if (matched & (1ULL << 1)) {
                s->htd = 0;
                s->state = STATE_IDLE;
            }
            break;
        }

        }
    }
}

static void ps2_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder ps2_c_decoder = {
    .id = "ps2_c",
    .name = "PS/2(C)",
    .longname = "PS/2 keyboard/mouse (C)",
    .desc = "PS/2 keyboard/mouse protocol decoder (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = ps2_channels,
    .num_channels = 2,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = ps2_options,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = ps2_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = ps2_ann_rows,
    .inputs = ps2_inputs,
    .num_inputs = 1,
    .outputs = ps2_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = ps2_tags,
    .num_tags = 1,
    .reset = ps2_reset,
    .start = ps2_start,
    .decode = ps2_decode,
    .destroy = ps2_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    ps2_options[0].def = g_variant_new_string("rise");
    GSList *htod_vals = NULL;
    htod_vals = g_slist_append(htod_vals, g_variant_new_string("rise"));
    htod_vals = g_slist_append(htod_vals, g_variant_new_string("fall"));
    ps2_options[0].values = htod_vals;

    ps2_options[1].def = g_variant_new_string("fall");
    GSList *dtoh_vals = NULL;
    dtoh_vals = g_slist_append(dtoh_vals, g_variant_new_string("fall"));
    dtoh_vals = g_slist_append(dtoh_vals, g_variant_new_string("rise"));
    ps2_options[1].values = dtoh_vals;

    return &ps2_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
