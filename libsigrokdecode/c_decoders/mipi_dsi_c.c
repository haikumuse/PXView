#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum mipi_dsi_state {
    STATE_FIND_START,
    STATE_FIND_MODE_S0,
    STATE_FIND_MODE_S1,
    STATE_FIND_MODE_S2,
    STATE_FIND_DATA_EDGE,
    STATE_FIND_DATA_VALID,
};

enum mipi_dsi_ann {
    ANN_LP00 = 0,
    ANN_LP01,
    ANN_LP10,
    ANN_LP11,
    ANN_ESCAPE_MODE,
    ANN_BTA,
    ANN_LPDT,
    ANN_DI,
    ANN_ECC,
    ANN_WC,
    ANN_CRC,
    ANN_STOP,
    ANN_IDLE,
    NUM_ANN,
};

struct mipi_dsi_priv {
    int state;
    uint64_t samplerate;
    int out_ann;
    int out_python;
    int out_binary;

    int bitcount;
    uint8_t databyte;
    uint64_t ss;
    uint64_t es;
    uint64_t ss_byte;

    uint8_t saved_d0n;
    uint8_t saved_d0p;

    uint8_t data_d0n;
    uint8_t data_d0p;
};

static struct srd_channel mipi_dsi_channels[] = {
    {"D0N", "D0N", "LP data 0 neg", 0, SRD_CHANNEL_SDATA, "dec_mipi_dsi_chan_D0N"},
    {"D0P", "D0P", "LP data 0 pos", 1, SRD_CHANNEL_ADATA, "dec_mipi_dsi_chan_D0P"},
};

static const char *mipi_dsi_ann_labels[][3] = {
    {"", "LP-00", "LP-00"},
    {"", "LP-01", "LP-01"},
    {"", "LP-10", "LP-10"},
    {"", "LP-11", "LP-11"},
    {"", "EscapeMode", "Escape mode"},
    {"", "BTA", "Bi-directional Data Lane Turnaround"},
    {"", "LPDT", "LPDT"},
    {"", "DI", "Data identifier"},
    {"", "ECC", "ECC"},
    {"", "WC", "Word count"},
    {"", "CRC", "CheckSUM"},
    {"", "Stop", "Stop condition"},
    {"", "Idle", "Idle"},
};

static const int mipi_dsi_row_lpdata_classes[] = {ANN_LP00, ANN_LP01, ANN_LP10, ANN_LP11, -1};
static const int mipi_dsi_row_lp_classes[] = {ANN_ESCAPE_MODE, ANN_BTA, ANN_LPDT, ANN_DI, ANN_ECC, ANN_WC, ANN_CRC, ANN_STOP, ANN_IDLE, -1};

static const struct srd_c_ann_row mipi_dsi_ann_rows[] = {
    {"LPData", "LPData", mipi_dsi_row_lpdata_classes, 4},
    {"LP", "LP", mipi_dsi_row_lp_classes, 9},
};

static const char *mipi_dsi_inputs[] = {"logic"};
static const char *mipi_dsi_outputs[] = {"mipi_dsi"};
static const char *mipi_dsi_tags[] = {"Embedded/industrial"};

static void mipi_dsi_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct mipi_dsi_priv)));
    struct mipi_dsi_priv *s = (struct mipi_dsi_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct mipi_dsi_priv));
    s->out_ann = -1;
    s->out_python = -1;
    s->out_binary = -1;
    s->state = STATE_FIND_START;
}

static void mipi_dsi_start(struct srd_decoder_inst *di)
{
    struct mipi_dsi_priv *s = (struct mipi_dsi_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "mipi_dsi");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "mipi_dsi");
    s->out_binary = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "mipi_dsi");
}

static void mipi_dsi_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct mipi_dsi_priv *s = (struct mipi_dsi_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE)
        s->samplerate = value;
}

static void mipi_dsi_decode(struct srd_decoder_inst *di)
{
    struct mipi_dsi_priv *s = (struct mipi_dsi_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched = 0;
    int ret;

    if (s->samplerate == 0)
        return;

    while (1) {
        switch (s->state) {
        case STATE_FIND_START: {
            srd_cond_builder *cb = c_cond_new();
            c_cond_fall(cb, 0);  /* D0N falling edge */
            c_cond_high(cb, 1);  /* D0P high */
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
            /* handle_start */
            s->ss = s->es = samplenum;
            s->bitcount = 0;
            s->databyte = 0;
            s->state = STATE_FIND_MODE_S0;
            break;
        }

        case STATE_FIND_MODE_S0: {
            srd_cond_builder *cb = c_cond_new();
            c_cond_low(cb, 0);   /* D0N low */
            c_cond_low(cb, 1);   /* D0P low */
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
            s->state = STATE_FIND_MODE_S1;
            break;
        }

        case STATE_FIND_MODE_S1: {
            srd_cond_builder *cb = c_cond_new();
            c_cond_high(cb, 0);  /* D0N high AND D0P low */
            c_cond_low(cb, 1);
            c_cond_or(cb);
            c_cond_low(cb, 0);   /* D0N low AND D0P high */
            c_cond_high(cb, 1);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
            s->saved_d0n = c_decoder_get_pin(di, 0, samplenum);
            s->saved_d0p = c_decoder_get_pin(di, 1, samplenum);
            s->state = STATE_FIND_MODE_S2;
            break;
        }

        case STATE_FIND_MODE_S2: {
            srd_cond_builder *cb = c_cond_new();
            c_cond_low(cb, 0);   /* D0N low */
            c_cond_low(cb, 1);   /* D0P low */
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
            /* handle_esc_bta */
            if (s->saved_d0n) {
                C_ANN_PUT(di, s->ss, samplenum, s->out_ann, ANN_ESCAPE_MODE, "Escape mode", "ESC");
                c_decoder_put_python(di, s->ss, samplenum, s->out_python, "ESC", NULL, 0);
            } else {
                C_ANN_PUT(di, s->ss, samplenum, s->out_ann, ANN_BTA, "BTA", "BTA");
                c_decoder_put_python(di, s->ss, samplenum, s->out_python, "BTA", NULL, 0);
            }
            s->ss = samplenum;
            s->state = STATE_FIND_DATA_EDGE;
            break;
        }

        case STATE_FIND_DATA_EDGE: {
            /* Wait for [{0:'h',1:'l'}, {0:'l',1:'h'}] */
            srd_cond_builder *cb = c_cond_new();
            c_cond_high(cb, 0); c_cond_low(cb, 1);
            c_cond_or(cb);
            c_cond_low(cb, 0); c_cond_high(cb, 1);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
            s->data_d0n = c_decoder_get_pin(di, 0, samplenum);
            s->data_d0p = c_decoder_get_pin(di, 1, samplenum);
            s->state = STATE_FIND_DATA_VALID;
            break;
        }

        case STATE_FIND_DATA_VALID: {
            /* Wait for [{0:'l',1:'l'}, {0:'h',1:'h'}] */
            srd_cond_builder *cb = c_cond_new();
            c_cond_low(cb, 0); c_cond_low(cb, 1);
            c_cond_or(cb);
            c_cond_high(cb, 0); c_cond_high(cb, 1);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;

            if (matched & (1ULL << 0)) {
                /* handle_data: matched D0N low, D0P low → data bit */
                s->databyte >>= 1;
                if (s->data_d0p)
                    s->databyte |= 0x80;
                if (s->bitcount == 0)
                    s->ss_byte = samplenum;
                if (s->bitcount < 7) {
                    s->bitcount++;
                    s->state = STATE_FIND_DATA_EDGE;
                } else {
                    char h[8];
                    snprintf(h, sizeof(h), "0x%02X", s->databyte);
                    C_ANN_PUT(di, s->ss_byte, samplenum, s->out_ann, ANN_DI, h);
                    c_decoder_put_python(di, s->ss_byte, samplenum, s->out_python, "DATA", NULL, 0);
                    c_decoder_put_binary(di, s->ss_byte, samplenum, s->out_binary, 0, 1, &s->databyte);
                    s->bitcount = 0;
                    s->databyte = 0;
                    s->ss = samplenum;
                    s->state = STATE_FIND_DATA_EDGE;
                }
            } else {
                /* handle_stop: matched D0N high, D0P high → stop */
                C_ANN_PUT(di, s->ss, samplenum, s->out_ann, ANN_STOP, "Stop", "S");
                c_decoder_put_python(di, s->ss, samplenum, s->out_python, "STOP", NULL, 0);
                s->state = STATE_FIND_START;
            }
            break;
        }

        default:
            return;
        }
    }
}

static void mipi_dsi_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder mipi_dsi_c_decoder = {
    .id = "mipi_dsi_c",
    .name = "MIPI_DSI(C)",
    .longname = "MIPI Display Serial Interface (C)",
    .desc = "MIPI Display Serial Interface low power communication (C implementation)",
    .license = "gplv2+",
    .channels = mipi_dsi_channels,
    .num_channels = 2,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = mipi_dsi_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = mipi_dsi_ann_rows,
    .inputs = mipi_dsi_inputs,
    .num_inputs = 1,
    .outputs = mipi_dsi_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = mipi_dsi_tags,
    .num_tags = 1,
    .reset = mipi_dsi_reset,
    .start = mipi_dsi_start,
    .decode = mipi_dsi_decode,
    .destroy = mipi_dsi_destroy,
    .metadata = mipi_dsi_metadata,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &mipi_dsi_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
