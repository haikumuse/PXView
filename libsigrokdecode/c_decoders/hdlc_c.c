#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_RX_BIT = 0,
    ANN_DATA,
    ANN_DATA_TYPE,
    ANN_WARNING,
    ANN_TRANSFER,
    NUM_ANN,
};

struct hdlc_priv {
    int bitcount;
    uint8_t rxdata;
    uint64_t ss_bits[8];
    int one_count;
    gboolean flag_found;
    uint8_t rxbytes[1024];
    uint64_t rxbytes_ss[1024];
    uint64_t rxbytes_es[1024];
    int rxbytes_cnt;
    uint64_t ss_prev_clock;
    int prev_bit;
    uint64_t ss_flag_start;
    int have_en;
    int en_active_high;
    int cpol;
    int out_ann;
    int pending_flag;
    int pending_abort;
    uint64_t ss_pending_start;
};

static uint16_t hdlc_crc16(uint8_t *data, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0x8408;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFF;
}

static struct srd_channel hdlc_channels[] = {
    {"clk", "CLK", "Clock", 0, SRD_CHANNEL_SCLK, NULL},
    {"data", "DATA", "Data in", 1, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_channel hdlc_optional_channels[] = {
    {"en", "ENABLE", "RX enabled", 2, SRD_CHANNEL_COMMON, NULL},
};

static struct srd_decoder_option hdlc_options[] = {
    {"en_polarity", NULL, "ENABLE polarity", NULL, NULL},
    {"cpol", NULL, "Clock polarity", NULL, NULL},
};

static const char *hdlc_ann_labels[][3] = {
    {"", "rx-bit", "RX bit"},
    {"", "data", "data"},
    {"", "data-type", "data-type"},
    {"", "warning", "Warning"},
    {"", "transfer", "transfer"},
};

static const int hdlc_row_rxbits_classes[] = {ANN_RX_BIT};
static const int hdlc_row_datavals_classes[] = {ANN_DATA};
static const int hdlc_row_datatypes_classes[] = {ANN_DATA_TYPE};
static const int hdlc_row_transfers_classes[] = {ANN_TRANSFER};
static const int hdlc_row_other_classes[] = {ANN_WARNING};

static const struct srd_c_ann_row hdlc_ann_rows[] = {
    {"rx-bits", "RX bits", hdlc_row_rxbits_classes, 1},
    {"data-vals", "data", hdlc_row_datavals_classes, 1},
    {"data-types", "type", hdlc_row_datatypes_classes, 1},
    {"transfers", "transfers", hdlc_row_transfers_classes, 1},
    {"other", "Other", hdlc_row_other_classes, 1},
};

static const char *hdlc_inputs[] = {"logic"};
static const char *hdlc_outputs[] = {"hdlc"};
static const char *hdlc_tags[] = {"Embedded/industrial"};

static void hdlc_reset_state(struct hdlc_priv *priv)
{
    priv->bitcount = 0;
    priv->rxdata = 0;
    priv->one_count = 0;
    priv->flag_found = FALSE;
    priv->ss_prev_clock = (uint64_t)-1;
    priv->prev_bit = 0;
    priv->ss_flag_start = 0;
    priv->rxbytes_cnt = 0;
    priv->pending_flag = 0;
    priv->pending_abort = 0;
    priv->ss_pending_start = 0;
}

static void hdlc_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct hdlc_priv)));
    struct hdlc_priv *priv = (struct hdlc_priv *)c_decoder_get_private(di);
    memset(priv, 0, sizeof(struct hdlc_priv));
    hdlc_reset_state(priv);
    priv->have_en = 0;
    priv->en_active_high = 1;
    priv->cpol = 1;
    priv->out_ann = 0;
}

static void hdlc_start(struct srd_decoder_inst *di)
{
    struct hdlc_priv *priv = (struct hdlc_priv *)c_decoder_get_private(di);
    priv->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "hdlc");
    const char *en_pol = c_decoder_get_option_string(di, "en_polarity", "active-high");
    priv->en_active_high = (en_pol && strcmp(en_pol, "active-low") == 0) ? 0 : 1;
    priv->cpol = (int)c_decoder_get_option_int(di, "cpol", 1);
    priv->have_en = c_decoder_has_channel(di, 2);
}

static void hdlc_putt(struct srd_decoder_inst *di, struct hdlc_priv *priv)
{
    if (priv->rxbytes_cnt <= 4)
        return;

    int cnt = priv->rxbytes_cnt;

    C_ANN_PUT(di, priv->rxbytes_ss[0], priv->rxbytes_ss[cnt - 2],
              priv->out_ann, ANN_DATA_TYPE, "TRANSFER");

    C_ANN_PUT(di, priv->rxbytes_ss[cnt - 2], priv->rxbytes_es[cnt - 1],
              priv->out_ann, ANN_DATA_TYPE, "CRC");

    uint16_t crc = hdlc_crc16(priv->rxbytes, cnt - 2);
    uint16_t rxcrc = ((uint16_t)priv->rxbytes[cnt - 1] << 8) |
                     (uint16_t)priv->rxbytes[cnt - 2];

    if (crc != rxcrc) {
        C_ANN_PUT(di, priv->rxbytes_ss[0], priv->rxbytes_es[cnt - 1],
                  priv->out_ann, ANN_WARNING, "BAD CRC!");
    }

    char hex_str[3072];
    int pos = 0;
    for (int i = 0; i < cnt - 2 && pos < (int)sizeof(hex_str) - 4; i++) {
        if (i > 0)
            pos += snprintf(hex_str + pos, sizeof(hex_str) - pos, " ");
        pos += snprintf(hex_str + pos, sizeof(hex_str) - pos, "%02X", priv->rxbytes[i]);
    }

    C_ANN_PUT(di, priv->rxbytes_ss[0], priv->rxbytes_es[cnt - 1],
              priv->out_ann, ANN_TRANSFER, hex_str);
}

static void hdlc_shift_bit(struct hdlc_priv *priv, int data, uint64_t samplenum)
{
    if (priv->flag_found) {
        if (priv->bitcount < 8)
            priv->ss_bits[priv->bitcount] = samplenum;
        priv->rxdata |= data << priv->bitcount;
        priv->bitcount++;
    }
}

static void hdlc_handle_bit(struct srd_decoder_inst *di, struct hdlc_priv *priv,
                            int data, uint64_t samplenum)
{
    if (priv->ss_prev_clock != (uint64_t)-1) {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", priv->prev_bit);
        C_ANN_PUT(di, priv->ss_prev_clock, samplenum, priv->out_ann, ANN_RX_BIT, bit_str);
    }
    priv->ss_prev_clock = samplenum;
    priv->prev_bit = data;

    if (priv->bitcount == 8) {
        char data_str[8];
        snprintf(data_str, sizeof(data_str), "%02X", priv->rxdata);
        C_ANN_PUT(di, priv->ss_bits[0], samplenum, priv->out_ann, ANN_DATA, data_str);
        if (priv->rxbytes_cnt < 1024) {
            priv->rxbytes[priv->rxbytes_cnt] = priv->rxdata;
            priv->rxbytes_ss[priv->rxbytes_cnt] = priv->ss_bits[0];
            priv->rxbytes_es[priv->rxbytes_cnt] = samplenum;
            priv->rxbytes_cnt++;
        }
        priv->rxdata = 0;
        priv->bitcount = 0;
    }

    if (priv->pending_abort) {
        C_ANN_PUT(di, priv->ss_pending_start, samplenum,
                  priv->out_ann, ANN_DATA_TYPE, "ABORT");
        priv->pending_abort = 0;
    }

    if (priv->pending_flag) {
        C_ANN_PUT(di, priv->ss_pending_start, samplenum,
                  priv->out_ann, ANN_DATA_TYPE, "FLAG");
        priv->pending_flag = 0;
        hdlc_putt(di, priv);
        priv->rxbytes_cnt = 0;
    }

    if (data == 1) {
        if (priv->one_count < 5)
            hdlc_shift_bit(priv, data, samplenum);
        priv->one_count++;
    } else {
        if (priv->one_count == 6) {
            priv->flag_found = TRUE;
            priv->pending_flag = 1;
            priv->ss_pending_start = priv->ss_flag_start;
            priv->rxdata = 0;
            priv->bitcount = 0;
        } else if (priv->one_count > 6) {
            priv->pending_abort = 1;
            priv->ss_pending_start = priv->ss_flag_start;
            priv->flag_found = FALSE;
            priv->rxdata = 0;
            priv->bitcount = 0;
            priv->rxbytes_cnt = 0;
        } else if (priv->one_count < 5) {
            hdlc_shift_bit(priv, data, samplenum);
        }

        priv->one_count = 0;
        priv->ss_flag_start = samplenum;
    }
}

static void hdlc_decode(struct srd_decoder_inst *di)
{
    struct hdlc_priv *priv = (struct hdlc_priv *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    int CLK = 0;
    int DATA = 1;
    int ENABLE = 2;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        if (priv->cpol == 1)
            c_cond_rise(cb, CLK);
        else
            c_cond_fall(cb, CLK);
        if (priv->have_en) {
            c_cond_or(cb);
            c_cond_edge(cb, ENABLE);
        }
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);

        if (ret != SRD_OK)
            return;

        if (priv->have_en) {
            int en_val = c_decoder_get_pin(di, ENABLE, samplenum);
            int en_asserted = priv->en_active_high ? (en_val == 1) : (en_val == 0);
            if (!en_asserted) {
                hdlc_reset_state(priv);
                continue;
            }
            if (!(matched & 1))
                continue;
        }

        int data = c_decoder_get_pin(di, DATA, samplenum);
        hdlc_handle_bit(di, priv, data, samplenum);
    }
}

static void hdlc_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder hdlc_c_decoder = {
    .id = "hdlc_c",
    .name = "HDLC(C)",
    .longname = "High-Level Data Link Control (C)",
    .desc = "HDLC protocol decoder (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = hdlc_channels,
    .num_channels = 2,
    .optional_channels = hdlc_optional_channels,
    .num_optional_channels = 1,
    .options = hdlc_options,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = hdlc_ann_labels,
    .num_annotation_rows = 5,
    .annotation_rows = hdlc_ann_rows,
    .inputs = hdlc_inputs,
    .num_inputs = 1,
    .outputs = hdlc_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = hdlc_tags,
    .num_tags = 1,
    .reset = hdlc_reset,
    .start = hdlc_start,
    .decode = hdlc_decode,
    .destroy = hdlc_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    hdlc_options[0].def = g_variant_new_string("active-high");
    hdlc_options[1].def = g_variant_new_int64(1);
    return &hdlc_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
