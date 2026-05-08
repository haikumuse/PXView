#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum iso7816_ann {
    ANN_WARN = 0,
    ANN_BYTE,
    ANN_ATR,
    ANN_PPS,
    ANN_T0,
    ANN_T1,
    ANN_T1_IBLOCK,
    ANN_T1_RBLOCK,
    ANN_T1_SBLOCK,
    ANN_APDU,
    NUM_ANN,
};

enum iso7816_state {
    STATE_FIND_START,
    STATE_DATA,
};

#define CH_CLK  0
#define CH_DATA 1

struct iso7816_priv {
    enum iso7816_state state;
    int sample_as_clock;
    int detect_clock;
    int has_t0;
    int has_t1;
    int has_t15;
    int clock_skip;
    int detected_clock_skip;
    int fi;
    int di;
    uint64_t peeked_byte;
    uint64_t peeked_samplenum;
    int has_peeked;
    uint64_t atr_bytes[64];
    int atr_count;
    int out_ann;
};

static const int clock_rate_table[] = {
    372, 372, 558, 744, 1116, 1488, 1860, 0, 0, 512, 768, 1024, 1536, 2048
};
static const int clock_rate_count = 14;

static const int baud_rate_table[] = {
    1, 1, 2, 4, 8, 16, 32, 64, 12, 20
};
static const int baud_rate_count = 10;

static struct srd_channel iso7816_channels[] = {
    {"clk", "CLK", "clock", 0, SRD_CHANNEL_SCLK, NULL},
    {"data", "data", "data", 1, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option iso7816_options[] = {
    {
        .id = "clock_option",
        .idn = NULL,
        .desc = "Clock option",
        .def = NULL,
        .values = NULL,
    },
    {
        .id = "protocol",
        .idn = NULL,
        .desc = "Protocol",
        .def = NULL,
        .values = NULL,
    },
    {
        .id = "starts_with_atr",
        .idn = NULL,
        .desc = "Starts with ATR",
        .def = NULL,
        .values = NULL,
    },
};

static const char *iso7816_ann_labels[][3] = {
    {"", "warning", "Human-readable warnings"},
    {"", "byte", "Byte"},
    {"", "atr", "ATR (Answer to Reset)"},
    {"", "pps", "PPS (Protocol and parameters selection)"},
    {"", "t0", "T=0 packet"},
    {"", "t1", "T=1 packet"},
    {"", "t1-iblock", "T=1 I-Block"},
    {"", "t1-rblock", "T=1 R-Block"},
    {"", "t1-sblock", "T=1 S-Block"},
    {"", "apdu", "APDU"},
};

static const int row_warnings_classes[] = {ANN_WARN};
static const int row_bytes_classes[] = {ANN_BYTE};
static const int row_type_classes[] = {ANN_ATR, ANN_PPS, ANN_T0, ANN_T1};
static const int row_t1s_classes[] = {ANN_T1_IBLOCK, ANN_T1_RBLOCK, ANN_T1_SBLOCK};
static const int row_apdus_classes[] = {ANN_APDU};

static const struct srd_c_ann_row iso7816_ann_rows[] = {
    {"warnings", "Warnings", row_warnings_classes, 1},
    {"bytes", "Bytes", row_bytes_classes, 1},
    {"type", "Type", row_type_classes, 4},
    {"t1s", "T=1 Decode", row_t1s_classes, 3},
    {"apdus", "apdus", row_apdus_classes, 1},
};

static const char *iso7816_inputs[] = {"logic"};
static const char *iso7816_outputs[] = {"iso7816"};
static const char *iso7816_tags[] = {"Embedded/industrial"};

static int get_clock_rate(int idx)
{
    if (idx < 0 || idx >= clock_rate_count)
        return 372;
    if (clock_rate_table[idx] == 0)
        return 372;
    return clock_rate_table[idx];
}

static int get_baud_rate(int idx)
{
    if (idx < 0 || idx >= baud_rate_count)
        return 1;
    return baud_rate_table[idx];
}

static void iso7816_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct iso7816_priv)));
    }
    struct iso7816_priv *s = (struct iso7816_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct iso7816_priv));
    s->state = STATE_FIND_START;
    s->clock_skip = 372;
    s->fi = 372;
    s->di = 1;
    s->has_peeked = 0;
    s->has_t0 = 1;
    s->has_t1 = 0;
    s->has_t15 = 0;
}

static void iso7816_start(struct srd_decoder_inst *di)
{
    struct iso7816_priv *s = (struct iso7816_priv *)c_decoder_get_private(di);

    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "iso7816");

    const char *clock_opt = c_decoder_get_option_string(di, "clock_option", "native");
    s->sample_as_clock = (clock_opt && strcmp(clock_opt, "sample_as_clock") == 0);
    s->detect_clock = (clock_opt && strcmp(clock_opt, "detect") == 0);

    const char *starts_atr = c_decoder_get_option_string(di, "starts_with_atr", "true");
    if (starts_atr && strcmp(starts_atr, "false") == 0)
        s->state = STATE_DATA;
    else
        s->state = STATE_FIND_START;

    const char *protocol = c_decoder_get_option_string(di, "protocol", "auto");
    if (protocol && strcmp(protocol, "T=0") == 0) {
        s->has_t0 = 1;
        s->has_t1 = 0;
    } else if (protocol && strcmp(protocol, "T=1") == 0) {
        s->has_t0 = 0;
        s->has_t1 = 1;
    }
}

static int wait_clk_rise(struct srd_decoder_inst *di, uint64_t *samplenum, uint64_t *matched)
{
    srd_cond_builder *b = c_cond_new();
    c_cond_rise(b, CH_CLK);
    int ret = c_cond_wait(b, di, samplenum, matched);
    c_cond_free(b);
    return ret;
}

static int wait_data_fall(struct srd_decoder_inst *di, uint64_t *samplenum, uint64_t *matched)
{
    srd_cond_builder *b = c_cond_new();
    c_cond_fall(b, CH_DATA);
    int ret = c_cond_wait(b, di, samplenum, matched);
    c_cond_free(b);
    return ret;
}

static int wait_data_high(struct srd_decoder_inst *di, uint64_t *samplenum, uint64_t *matched)
{
    srd_cond_builder *b = c_cond_new();
    c_cond_high(b, CH_DATA);
    int ret = c_cond_wait(b, di, samplenum, matched);
    c_cond_free(b);
    return ret;
}

static int skip_samples(struct srd_decoder_inst *di, uint64_t count, uint64_t *samplenum, uint64_t *matched)
{
    srd_cond_builder *b = c_cond_new();
    c_cond_skip(b, count);
    int ret = c_cond_wait(b, di, samplenum, matched);
    c_cond_free(b);
    return ret;
}

static int sleep_cycles(struct srd_decoder_inst *di, struct iso7816_priv *s,
                        uint64_t *samplenum, uint64_t *matched)
{
    int count = s->clock_skip / 3;
    if (count < 1) count = 1;

    if (s->sample_as_clock) {
        return skip_samples(di, (uint64_t)count, samplenum, matched);
    } else {
        int i;
        for (i = 0; i < count; i++) {
            int ret = wait_clk_rise(di, samplenum, matched);
            if (ret != SRD_OK)
                return ret;
        }
        return SRD_OK;
    }
}

static int wait_clock_edge_for_bit(struct srd_decoder_inst *di, struct iso7816_priv *s,
                                   uint64_t *samplenum, uint64_t *matched)
{
    if (s->sample_as_clock) {
        return skip_samples(di, (uint64_t)(s->clock_skip - 4), samplenum, matched);
    } else {
        int i;
        for (i = 0; i < s->clock_skip; i++) {
            int ret = wait_clk_rise(di, samplenum, matched);
            if (ret != SRD_OK)
                return ret;
        }
        return SRD_OK;
    }
}

static int read_byte_no_wait(struct srd_decoder_inst *di, struct iso7816_priv *s,
                             uint64_t *samplenum, uint64_t *matched, uint8_t *out_byte,
                             uint64_t *out_ss, uint64_t *out_es)
{
    int bits[10];
    int i;
    uint64_t ss = *samplenum;

    for (i = 0; i < 10; i++) {
        srd_cond_builder *b = c_cond_new();
        c_cond_skip(b, 0);
        int ret = c_cond_wait(b, di, samplenum, matched);
        c_cond_free(b);
        if (ret != SRD_OK)
            return ret;

        bits[i] = c_decoder_get_pin(di, CH_DATA, *samplenum);

        if (i < 9) {
            ret = wait_clock_edge_for_bit(di, s, samplenum, matched);
            if (ret != SRD_OK)
                return ret;
        }
    }

    uint64_t es = *samplenum;

    int ones = 0;
    for (i = 0; i < 10; i++) {
        if (bits[i]) ones++;
    }

    if (ones % 2 != 0) {
        char warn_str[128];
        snprintf(warn_str, sizeof(warn_str),
                 "CHKSUM ERROR bits=[%d%d%d%d%d%d%d%d%d%d]",
                 bits[0], bits[1], bits[2], bits[3], bits[4],
                 bits[5], bits[6], bits[7], bits[8], bits[9]);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, warn_str);
    }

    uint8_t byte_val = 0;
    for (i = 0; i < 8; i++) {
        byte_val |= (bits[i + 1] << i);
    }

    char hex_str[8];
    snprintf(hex_str, sizeof(hex_str), "0x%02x", byte_val);
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_BYTE, hex_str);

    *out_byte = byte_val;
    if (out_ss) *out_ss = ss;
    if (out_es) *out_es = es;
    return SRD_OK;
}

static int read_first_byte(struct srd_decoder_inst *di, struct iso7816_priv *s,
                           uint64_t *samplenum, uint64_t *matched, uint8_t *out_byte,
                           uint64_t *out_ss, uint64_t *out_es)
{
    uint64_t ss = *samplenum;
    s->clock_skip = 0;
    int bits[10];
    int i;

    if (s->sample_as_clock) {
        srd_cond_builder *b = c_cond_new();
        c_cond_rise(b, CH_DATA);
        int ret = c_cond_wait(b, di, samplenum, matched);
        c_cond_free(b);
        if (ret != SRD_OK)
            return ret;

        s->clock_skip = (int)(*samplenum - ss) + 2;
        s->detected_clock_skip = s->clock_skip;

        ret = skip_samples(di, (uint64_t)(s->clock_skip / 3), samplenum, matched);
        if (ret != SRD_OK)
            return ret;

        bits[0] = 0;
    } else if (s->detect_clock) {
        while (1) {
            int ret = wait_clk_rise(di, samplenum, matched);
            if (ret != SRD_OK)
                return ret;
            s->clock_skip++;

            int data_val = c_decoder_get_pin(di, CH_DATA, *samplenum);
            if (data_val == 1) {
                bits[0] = 0;
                int half_skip = s->clock_skip / 2;
                int c;
                for (c = 0; c < half_skip; c++) {
                    ret = wait_clk_rise(di, samplenum, matched);
                    if (ret != SRD_OK)
                        return ret;
                }
                break;
            }
        }
        s->detected_clock_skip = s->clock_skip;
    } else {
        s->clock_skip = 372;
        bits[0] = 0;
        int total = s->clock_skip / 2 + s->clock_skip;
        int c;
        for (c = 0; c < total; c++) {
            int ret = wait_clk_rise(di, samplenum, matched);
            if (ret != SRD_OK)
                return ret;
        }
    }

    for (i = 0; i < 9; i++) {
        srd_cond_builder *b = c_cond_new();
        c_cond_skip(b, 0);
        int ret = c_cond_wait(b, di, samplenum, matched);
        c_cond_free(b);
        if (ret != SRD_OK)
            return ret;

        bits[i + 1] = c_decoder_get_pin(di, CH_DATA, *samplenum);

        if (s->sample_as_clock) {
            ret = skip_samples(di, (uint64_t)(s->clock_skip - 4), samplenum, matched);
            if (ret != SRD_OK)
                return ret;
        } else {
            int c;
            for (c = 0; c < s->clock_skip; c++) {
                ret = wait_clk_rise(di, samplenum, matched);
                if (ret != SRD_OK)
                    return ret;
            }
        }
    }

    uint64_t es = *samplenum;

    int ones = 0;
    for (i = 0; i < 10; i++) {
        if (bits[i]) ones++;
    }

    if (ones % 2 != 0) {
        char warn_str[128];
        snprintf(warn_str, sizeof(warn_str),
                 "CHKSUM ERROR bits=[%d%d%d%d%d%d%d%d%d%d]",
                 bits[0], bits[1], bits[2], bits[3], bits[4],
                 bits[5], bits[6], bits[7], bits[8], bits[9]);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, warn_str);
    }

    uint8_t byte_val = 0;
    for (i = 0; i < 8; i++) {
        byte_val |= (bits[i + 1] << i);
    }

    char hex_str[8];
    snprintf(hex_str, sizeof(hex_str), "0x%02x", byte_val);
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_BYTE, hex_str);

    *out_byte = byte_val;
    if (out_ss) *out_ss = ss;
    if (out_es) *out_es = es;
    return SRD_OK;
}

static int read_byte(struct srd_decoder_inst *di, struct iso7816_priv *s,
                     uint64_t *samplenum, uint64_t *matched, uint8_t *out_byte,
                     uint64_t *out_ss, uint64_t *out_es)
{
    if (s->has_peeked) {
        *out_byte = (uint8_t)s->peeked_byte;
        if (out_ss) *out_ss = s->peeked_samplenum;
        if (out_es) *out_es = *samplenum;
        s->has_peeked = 0;
        return SRD_OK;
    }

    int ret = wait_data_fall(di, samplenum, matched);
    if (ret != SRD_OK)
        return ret;

    ret = sleep_cycles(di, s, samplenum, matched);
    if (ret != SRD_OK)
        return ret;

    return read_byte_no_wait(di, s, samplenum, matched, out_byte, out_ss, out_es);
}

static int peek_byte(struct srd_decoder_inst *di, struct iso7816_priv *s,
                     uint64_t *samplenum, uint64_t *matched, uint8_t *out_byte,
                     uint64_t *out_ss)
{
    int ret = wait_data_fall(di, samplenum, matched);
    if (ret != SRD_OK)
        return ret;

    ret = sleep_cycles(di, s, samplenum, matched);
    if (ret != SRD_OK)
        return ret;

    s->peeked_samplenum = *samplenum;
    uint64_t es_unused;
    ret = read_byte_no_wait(di, s, samplenum, matched, out_byte, &s->peeked_samplenum, &es_unused);
    if (ret != SRD_OK)
        return ret;

    s->peeked_byte = *out_byte;
    s->has_peeked = 1;
    if (out_ss) *out_ss = s->peeked_samplenum;
    return SRD_OK;
}

static int handle_atr(struct srd_decoder_inst *di, struct iso7816_priv *s,
                      uint64_t *samplenum, uint64_t *matched, int is_first)
{
    uint64_t atr_start = *samplenum;
    uint8_t byte_val;
    uint64_t byte_ss, byte_es;
    int ret;

    s->atr_count = 0;

    if (s->has_peeked) {
        atr_start = s->peeked_samplenum;
        ret = read_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
    } else if (is_first) {
        ret = read_first_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
    } else {
        ret = read_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
    }
    if (ret != SRD_OK) return ret;

    s->atr_bytes[s->atr_count++] = byte_val;

    uint8_t t0;
    ret = read_byte(di, s, samplenum, matched, &t0, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    s->atr_bytes[s->atr_count++] = t0;

    uint8_t first_t0 = t0;

    s->has_t0 = 0;
    s->has_t1 = 0;
    s->has_t15 = 0;

    while (first_t0 & 0xF0) {
        if (first_t0 & 0x10) {
            ret = read_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
            if (ret != SRD_OK) return ret;
            s->atr_bytes[s->atr_count++] = byte_val;
        }
        if (first_t0 & 0x20) {
            ret = read_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
            if (ret != SRD_OK) return ret;
            s->atr_bytes[s->atr_count++] = byte_val;
        }
        if (first_t0 & 0x40) {
            ret = read_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
            if (ret != SRD_OK) return ret;
            s->atr_bytes[s->atr_count++] = byte_val;
        }
        if (first_t0 & 0x80) {
            ret = read_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
            if (ret != SRD_OK) return ret;
            s->atr_bytes[s->atr_count++] = byte_val;

            int proto = byte_val & 0x0F;
            if (proto == 0)
                s->has_t0 = 1;
            else if (proto == 1)
                s->has_t1 = 1;
            else if (proto == 15)
                s->has_t15 = 1;
            else {
                char warn_str[64];
                snprintf(warn_str, sizeof(warn_str),
                         "Invalid Protocol in ATR T=%d", proto);
                C_ANN_PUT(di, atr_start, *samplenum, s->out_ann, ANN_WARN, warn_str);
            }

            first_t0 = byte_val;
        } else {
            first_t0 = 0;
        }
    }

    int hist_count = t0 & 0x0F;
    int h;
    for (h = 0; h < hist_count; h++) {
        ret = read_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
        s->atr_bytes[s->atr_count++] = byte_val;
    }

    if (!s->has_t0 && !s->has_t1)
        s->has_t0 = 1;

    if (s->has_t1 || s->has_t15) {
        ret = read_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
        s->atr_bytes[s->atr_count++] = byte_val;

        uint8_t xor_val = 0;
        int i;
        for (i = 1; i < s->atr_count; i++)
            xor_val ^= (uint8_t)s->atr_bytes[i];

        if (xor_val != 0) {
            char warn_str[128];
            snprintf(warn_str, sizeof(warn_str),
                     "Invalid TCK in ATR, got=0x%02x expected=0x%02x",
                     byte_val, xor_val);
            C_ANN_PUT(di, atr_start, *samplenum, s->out_ann, ANN_WARN, warn_str);
        }
    }

    char atr_hex[256];
    int pos = 0;
    int i;
    for (i = 0; i < s->atr_count && pos < (int)sizeof(atr_hex) - 4; i++) {
        pos += snprintf(atr_hex + pos, sizeof(atr_hex) - pos, "%02x", (uint8_t)s->atr_bytes[i]);
    }

    C_ANN_PUT(di, atr_start, *samplenum, s->out_ann, ANN_ATR, "ATR", atr_hex);

    s->state = STATE_DATA;

    const char *protocol = c_decoder_get_option_string(di, "protocol", "auto");
    if (protocol && strcmp(protocol, "T=0") == 0) {
        s->has_t0 = 1;
        s->has_t1 = 0;
    } else if (protocol && strcmp(protocol, "T=1") == 0) {
        s->has_t0 = 0;
        s->has_t1 = 1;
    }

    return SRD_OK;
}

static int handle_pps(struct srd_decoder_inst *di, struct iso7816_priv *s,
                      uint64_t *samplenum, uint64_t *matched)
{
    uint64_t pps_start = s->peeked_samplenum;
    uint8_t pps, pps0, pps1 = 0, pps2 = 0, pps3 = 0, pck;
    uint8_t byte_val;
    uint64_t byte_ss, byte_es;
    int ret;
    uint8_t lrc = 0;

    ret = read_byte(di, s, samplenum, matched, &pps, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    ret = read_byte(di, s, samplenum, matched, &pps0, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;

    if (pps0 & 0x10) {
        ret = read_byte(di, s, samplenum, matched, &pps1, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
        lrc ^= pps1;
    }
    if (pps0 & 0x20) {
        ret = read_byte(di, s, samplenum, matched, &pps2, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
        lrc ^= pps2;
    }
    if (pps0 & 0x40) {
        ret = read_byte(di, s, samplenum, matched, &pps3, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
        lrc ^= pps3;
    }
    ret = read_byte(di, s, samplenum, matched, &pck, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;

    lrc ^= pps ^ pps0 ^ pck;
    if (lrc != 0) {
        char warn_str[128];
        snprintf(warn_str, sizeof(warn_str),
                 "INVALID Checksum on PPS Request, got=0x%02x expected=0x%02x",
                 pck, (uint8_t)(lrc ^ pps ^ pps0));
        C_ANN_PUT(di, pps_start, *samplenum, s->out_ann, ANN_WARN, warn_str);
    }

    uint8_t r_lrc = 0;
    uint8_t r_pps, r_pps0, r_pps1 = 0, r_pps2 = 0, r_pps3 = 0, r_pck;

    ret = read_byte(di, s, samplenum, matched, &r_pps, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    if (r_pps != 0xFF) {
        C_ANN_PUT(di, pps_start, *samplenum, s->out_ann, ANN_WARN,
                  "PPS Request not confirmed");
    }
    ret = read_byte(di, s, samplenum, matched, &r_pps0, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;

    if (r_pps0 & 0x10) {
        ret = read_byte(di, s, samplenum, matched, &r_pps1, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
        r_lrc ^= r_pps1;
    }
    if (r_pps0 & 0x20) {
        ret = read_byte(di, s, samplenum, matched, &r_pps2, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
        r_lrc ^= r_pps2;
    }
    if (r_pps0 & 0x40) {
        ret = read_byte(di, s, samplenum, matched, &r_pps3, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
        r_lrc ^= r_pps3;
    }
    ret = read_byte(di, s, samplenum, matched, &r_pck, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;

    r_lrc ^= r_pps ^ r_pps0 ^ r_pck;
    if (r_lrc != 0) {
        char warn_str[128];
        snprintf(warn_str, sizeof(warn_str),
                 "INVALID Checksum on PPS Response, got=0x%02x expected=0x%02x",
                 r_pck, (uint8_t)(r_lrc ^ r_pps ^ r_pps0));
        C_ANN_PUT(di, pps_start, *samplenum, s->out_ann, ANN_WARN, warn_str);
    }

    if (pps0 == r_pps0 && pps1 == r_pps1 && pps2 == r_pps2 && pps3 == r_pps3) {
        if (s->detect_clock || s->sample_as_clock) {
            int tmp_fi = get_clock_rate(pps1 >> 4);
            int tmp_di = get_baud_rate(pps1 & 0x0F);
            int tmp_clock_skip = tmp_fi / tmp_di;
            s->clock_skip = (int)((uint64_t)tmp_clock_skip * s->detected_clock_skip / 372);
            s->fi = tmp_fi;
            s->di = tmp_di;
        } else {
            s->fi = get_clock_rate(pps1 >> 4);
            s->di = get_baud_rate(pps1 & 0x0F);
            s->clock_skip = s->fi / s->di;
        }
    } else {
        C_ANN_PUT(di, pps_start, *samplenum, s->out_ann, ANN_WARN,
                  "INVALID PPS. Request & Response not matching");
    }

    char pps_str[128];
    snprintf(pps_str, sizeof(pps_str),
             "PPS DI=%d FI=%d clock_skip=%d", s->di, s->fi, s->clock_skip);
    C_ANN_PUT(di, pps_start, *samplenum, s->out_ann, ANN_PPS, "PPS", pps_str);

    return SRD_OK;
}

static int handle_t0_packet(struct srd_decoder_inst *di, struct iso7816_priv *s,
                            uint64_t *samplenum, uint64_t *matched, uint64_t pkt_start)
{
    uint8_t bClass, bIns, p1, p2, p3, procByte;
    uint8_t byte_val;
    uint64_t byte_ss, byte_es;
    int ret;

    ret = read_byte(di, s, samplenum, matched, &bClass, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    ret = read_byte(di, s, samplenum, matched, &bIns, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    ret = read_byte(di, s, samplenum, matched, &p1, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    ret = read_byte(di, s, samplenum, matched, &p2, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    ret = read_byte(di, s, samplenum, matched, &p3, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;

    ret = read_byte(di, s, samplenum, matched, &procByte, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;

    uint8_t sw1 = 0, sw2 = 0;

    if (procByte == bIns) {
        int d;
        for (d = 0; d < p3; d++) {
            ret = read_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
            if (ret != SRD_OK) return ret;
        }
        ret = read_byte(di, s, samplenum, matched, &sw1, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
        ret = read_byte(di, s, samplenum, matched, &sw2, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
    } else if (procByte == 0x60) {
        sw1 = procByte;
        ret = read_byte(di, s, samplenum, matched, &sw2, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
    } else if ((procByte & 0xF0) == 0x60 || (procByte & 0xF0) == 0x90) {
        sw1 = procByte;
        ret = read_byte(di, s, samplenum, matched, &sw2, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
    } else {
        C_ANN_PUT(di, pkt_start, *samplenum, s->out_ann, ANN_WARN,
                  "INVALID Procedure Byte");
    }

    C_ANN_PUT(di, pkt_start, *samplenum, s->out_ann, ANN_T0, "T=0");

    char apdu_short[64];
    snprintf(apdu_short, sizeof(apdu_short),
             "APDU cls=0x%02x ins=0x%02x", bClass, bIns);
    char apdu_long[256];
    snprintf(apdu_long, sizeof(apdu_long),
             "APDU cls=0x%02x ins=0x%02x p1=0x%02x p2=0x%02x p3=0x%02x len=%d status=0x%02x%02x",
             bClass, bIns, p1, p2, p3, (int)p3, sw1, sw2);
    C_ANN_PUT(di, pkt_start, *samplenum, s->out_ann, ANN_APDU, apdu_short, apdu_long);

    return SRD_OK;
}

static int handle_t1_block(struct srd_decoder_inst *di, struct iso7816_priv *s,
                           uint64_t *samplenum, uint64_t *matched, uint64_t pkt_start)
{
    uint8_t nad, pcb, bLen, bLrc;
    uint8_t byte_val;
    uint64_t byte_ss, byte_es;
    int ret;
    uint8_t lrc = 0;

    ret = read_byte(di, s, samplenum, matched, &nad, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    lrc ^= nad;

    ret = read_byte(di, s, samplenum, matched, &pcb, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    lrc ^= pcb;

    int is_iblock = 0, is_rblock = 0, is_sblock = 0;
    if ((pcb & 0xC0) == 0xC0) {
        is_sblock = 1;
    } else if ((pcb & 0x80) == 0x80) {
        is_rblock = 1;
    } else {
        is_iblock = 1;
    }

    ret = read_byte(di, s, samplenum, matched, &bLen, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    lrc ^= bLen;

    int b;
    for (b = 0; b < bLen; b++) {
        ret = read_byte(di, s, samplenum, matched, &byte_val, &byte_ss, &byte_es);
        if (ret != SRD_OK) return ret;
        lrc ^= byte_val;
    }

    ret = read_byte(di, s, samplenum, matched, &bLrc, &byte_ss, &byte_es);
    if (ret != SRD_OK) return ret;
    lrc ^= bLrc;

    if (lrc != 0) {
        char warn_str[128];
        snprintf(warn_str, sizeof(warn_str),
                 "Invalid checksum on T=1 block, got=0x%02x expected=0x%02x",
                 lrc, bLrc);
        C_ANN_PUT(di, pkt_start, *samplenum, s->out_ann, ANN_WARN, warn_str);
    }

    if (is_iblock) {
        char iblock_str[64];
        snprintf(iblock_str, sizeof(iblock_str),
                 "I-Block len=%d isMultiBlock=%d", (int)bLen, (pcb & 0x20) > 0);
        C_ANN_PUT(di, pkt_start, *samplenum, s->out_ann, ANN_T1_IBLOCK, "I-Block", iblock_str);
    }
    if (is_rblock) {
        char rblock_str[64];
        snprintf(rblock_str, sizeof(rblock_str),
                 "R-Block flag=0x%02x", pcb & 0x1F);
        C_ANN_PUT(di, pkt_start, *samplenum, s->out_ann, ANN_T1_RBLOCK, "R-Block", rblock_str);
    }
    if (is_sblock) {
        char sblock_str[64];
        snprintf(sblock_str, sizeof(sblock_str),
                 "S-Block flag=0x%02x", pcb & 0x3F);
        C_ANN_PUT(di, pkt_start, *samplenum, s->out_ann, ANN_T1_SBLOCK, "S-Block", sblock_str);
    }

    C_ANN_PUT(di, pkt_start, *samplenum, s->out_ann, ANN_T1, "T=1");

    return SRD_OK;
}

static void iso7816_decode(struct srd_decoder_inst *di)
{
    struct iso7816_priv *s = (struct iso7816_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;
    int ret;

    while (1) {
        if (s->state == STATE_FIND_START) {
            ret = wait_data_high(di, &samplenum, &matched);
            if (ret != SRD_OK) return;

            ret = wait_data_fall(di, &samplenum, &matched);
            if (ret != SRD_OK) return;

            ret = handle_atr(di, s, &samplenum, &matched, 1);
            if (ret != SRD_OK) return;

        } else if (s->state == STATE_DATA) {
            uint8_t first_byte;
            uint64_t peek_ss;

            ret = peek_byte(di, s, &samplenum, &matched, &first_byte, &peek_ss);
            if (ret != SRD_OK) return;

            if (first_byte == 0xFF) {
                ret = handle_pps(di, s, &samplenum, &matched);
                if (ret != SRD_OK) return;
                continue;
            }

            if (first_byte == 0x3B) {
                srd_cond_builder *b = c_cond_new();
                c_cond_skip(b, 0);
                ret = c_cond_wait(b, di, &samplenum, &matched);
                c_cond_free(b);
                if (ret != SRD_OK) return;

                s->has_peeked = 0;
                ret = handle_atr(di, s, &samplenum, &matched, 0);
                if (ret != SRD_OK) return;
                continue;
            }

            uint64_t pkt_start = s->peeked_samplenum;

            if (s->has_t0) {
                ret = handle_t0_packet(di, s, &samplenum, &matched, pkt_start);
                if (ret != SRD_OK) return;
            } else if (s->has_t1) {
                ret = handle_t1_block(di, s, &samplenum, &matched, pkt_start);
                if (ret != SRD_OK) return;
            }
        } else {
            break;
        }
    }
}

static void iso7816_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder iso7816_c_decoder = {
    .id = "iso7816_c",
    .name = "ISO 7816(C)",
    .longname = "Smartcard (C)",
    .desc = "ISO 7816 decoder (smartcard, C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = iso7816_channels,
    .num_channels = 2,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = iso7816_options,
    .num_options = 3,
    .num_annotations = NUM_ANN,
    .ann_labels = iso7816_ann_labels,
    .num_annotation_rows = 5,
    .annotation_rows = iso7816_ann_rows,
    .inputs = iso7816_inputs,
    .num_inputs = 1,
    .outputs = iso7816_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = iso7816_tags,
    .num_tags = 1,
    .reset = iso7816_reset,
    .start = iso7816_start,
    .decode = iso7816_decode,
    .destroy = iso7816_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &iso7816_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
