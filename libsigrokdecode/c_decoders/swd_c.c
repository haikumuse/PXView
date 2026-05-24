#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum swd_state {
    UNKNOWN = 0,
    REQ = 1,
    ACK = 2,
    DATA = 3,
    DPARITY = 4,
};

enum swd_ann {
    ANN_RESET = 0,
    ANN_ENABLE = 1,
    ANN_READ = 2,
    ANN_WRITE = 3,
    ANN_ACK = 4,
    ANN_DATA = 5,
    ANN_PARITY = 6,
    NUM_ANN = 7,
};

#define SWCLK 0
#define SWDIO 1
#define RISING 1
#define FALLING 0
#define ADDR_DP_SELECT 0x8
#define ADDR_DP_CTRLSTAT 0x4

struct swd_priv {
    int state;
    int sample_edge;
    int turnaround;
    int ack;
    uint64_t ss_req;
    char bits[128];
    int bits_len;
    uint64_t samplenums[128];
    int linereset_count;
    uint32_t data;
    int addr;
    int rw;
    int apdp;
    int ctrlsel;
    int orundetect;
    int dparity;
    int out_ann;
    int out_python;
    uint64_t ss_linereset;
};

static struct srd_channel swd_channels[] = {
    { "swclk", "SWCLK", "Master clock", 0, SRD_CHANNEL_SCLK, "dec_swd_chan_swclk" },
    { "swdio", "SWDIO", "Data input/output", 1, SRD_CHANNEL_SDATA, "dec_swd_chan_swdio" },
};

static const char* swd_ann_labels[][3] = {
    { "", "reset", "RESET" },
    { "", "enable", "ENABLE" },
    { "", "read", "READ" },
    { "", "write", "WRITE" },
    { "", "ack", "ACK" },
    { "", "data", "DATA" },
    { "", "parity", "PARITY" },
};

static const int swd_row_all_classes[] = { ANN_RESET, ANN_ENABLE, ANN_READ, ANN_WRITE, ANN_ACK, ANN_DATA, ANN_PARITY };

static const struct srd_c_ann_row swd_ann_rows[] = {
    { "swd", "SWD", swd_row_all_classes, 7 },
};

static struct srd_decoder_option swd_options[] = {
    { "strict_start", "dec_swd_opt_strict_start", "Wait for a line reset before starting to decode", NULL, NULL },
};

static const char* swd_inputs[] = { "logic", NULL };
static const char* swd_outputs[] = { "swd", NULL };
static const char* swd_tags[] = { "Debug/trace", NULL };

static const char* get_address_description(struct swd_priv* s)
{
    static char buf[32];
    if (s->apdp == 0) {
        if (s->rw == 1) {
            switch (s->addr) {
            case 0x0:
                return "IDCODE";
            case 0x4:
                return s->ctrlsel == 0 ? "R CTRL/STAT" : "R DLCR";
            case 0x8:
                return "RESEND";
            case 0xC:
                return "RDBUFF";
            }
        } else {
            switch (s->addr) {
            case 0x0:
                return "W ABORT";
            case 0x4:
                return s->ctrlsel == 0 ? "W CTRL/STAT" : "W DLCR";
            case 0x8:
                return "W SELECT";
            case 0xC:
                return "W RESERVED";
            }
        }
    } else {
        if (s->rw == 1) {
            snprintf(buf, sizeof(buf), "R AP%x", s->addr);
        } else {
            snprintf(buf, sizeof(buf), "W AP%x", s->addr);
        }
        return buf;
    }
    snprintf(buf, sizeof(buf), "? %c%c%x", s->rw ? 'R' : 'W', s->apdp ? 'A' : 'D', s->addr);
    return buf;
}

static void swd_reset_state(struct swd_priv* s)
{
    s->bits_len = 0;
    s->linereset_count = 0;
    s->turnaround = 0;
    s->sample_edge = RISING;
    s->state = REQ;
}

static void swd_next_state(struct swd_priv* s)
{
    s->bits_len = 0;
    s->linereset_count = 0;
    switch (s->state) {
    case UNKNOWN:
        s->state = REQ;
        s->sample_edge = RISING;
        s->turnaround = 0;
        break;
    case REQ:
        s->state = ACK;
        s->sample_edge = FALLING;
        s->turnaround = 1;
        break;
    case ACK:
        s->state = DATA;
        if (s->rw == 0) {
            s->sample_edge = RISING;
            s->turnaround = 2;
        } else {
            s->sample_edge = FALLING;
            s->turnaround = 0;
        }
        break;
    case DATA:
        s->state = DPARITY;
        break;
    case DPARITY:
        s->state = REQ;
        s->sample_edge = RISING;
        s->turnaround = (s->rw == 1) ? 1 : 0;
        break;
    }
}

static void handle_completed_write(struct swd_priv* s)
{
    if (s->apdp != 0)
        return;
    if (s->addr == ADDR_DP_SELECT)
        s->ctrlsel = s->data & 1;
    else if (s->addr == ADDR_DP_CTRLSTAT && s->ctrlsel == 0)
        s->orundetect = s->data & 1;
}

static void swd_reset(struct srd_decoder_inst* di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct swd_priv)));
    }
    struct swd_priv* s = (struct swd_priv*)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct swd_priv));
    s->state = UNKNOWN;
    s->sample_edge = RISING;
}

static void swd_start(struct srd_decoder_inst* di)
{
    struct swd_priv* s = (struct swd_priv*)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "swd");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "swd");
    const char* strict = c_decoder_get_option_string(di, "strict_start", "no");
    if (strcmp(strict, "no") == 0)
        s->state = REQ;
}

static void swd_decode(struct srd_decoder_inst* di)
{
    struct swd_priv* s = (struct swd_priv*)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        srd_cond_builder* cb = c_cond_new();
        c_cond_edge(cb, SWCLK);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int clk = c_decoder_get_pin(di, SWCLK, samplenum);
        int dio = c_decoder_get_pin(di, SWDIO, samplenum);

        if (clk == RISING) {
            if (dio == 1) {
                if (s->linereset_count == 0)
                    s->ss_linereset = samplenum;
                s->linereset_count++;
            } else {
                if (s->linereset_count >= 50) {
                    C_ANN_PUT(di, s->ss_linereset, samplenum, s->out_ann, ANN_RESET, "LINERESET");
                    c_decoder_put_python(di, s->ss_linereset, samplenum, s->out_python, "LINE_RESET", NULL, 0);
                    swd_reset_state(s);
                }
                s->linereset_count = 0;
            }
        }

        if (clk != s->sample_edge)
            continue;

        if (s->turnaround > 0) {
            s->turnaround--;
            continue;
        }

        if (s->bits_len < 128) {
            s->bits[s->bits_len] = dio ? '1' : '0';
            s->samplenums[s->bits_len] = samplenum;
            s->bits_len++;
        }

        switch (s->state) {
        case UNKNOWN:
            break;

        case REQ: {
            if (s->bits_len >= 16) {
                static const char jtag_swd_pat[] = "0111100111100111";
                int match = 1;
                int i;
                for (i = 0; i < 16; i++) {
                    if (s->bits[s->bits_len - 16 + i] != jtag_swd_pat[i]) {
                        match = 0;
                        break;
                    }
                }
                if (match) {
                    uint64_t ss = s->samplenums[s->bits_len - 16];
                    s->ss_req = ss;
                    C_ANN_PUT(di, ss, samplenum, s->out_ann, ANN_ENABLE, "JTAG->SWD");
                    swd_reset_state(s);
                    break;
                }
            }

            if (s->bits_len >= 8) {
                int start_bit = s->bits[s->bits_len - 8] - '0';
                int apdp_bit = s->bits[s->bits_len - 7] - '0';
                int rw_bit = s->bits[s->bits_len - 6] - '0';
                int addr_bit0 = s->bits[s->bits_len - 5] - '0';
                int addr_bit1 = s->bits[s->bits_len - 4] - '0';
                int stop_bit = s->bits[s->bits_len - 2] - '0';
                int park_bit = s->bits[s->bits_len - 1] - '0';

                if (start_bit == 1 && stop_bit == 0 && park_bit == 1) {
                    s->rw = rw_bit;
                    s->apdp = apdp_bit;
                    s->addr = (addr_bit1 * 2 + addr_bit0) << 2;

                    uint64_t ss = s->samplenums[s->bits_len - 8];
                    s->ss_req = ss;
                    const char* desc = get_address_description(s);
                    int ann = (s->rw == 1) ? ANN_READ : ANN_WRITE;
                    C_ANN_PUT(di, ss, samplenum, s->out_ann, ann, desc);
                    swd_next_state(s);
                }
            }
            break;
        }

        case ACK: {
            if (s->bits_len < 3)
                break;

            uint64_t ss = s->samplenums[s->bits_len - 3];

            if (s->bits[s->bits_len - 3] == '1' && s->bits[s->bits_len - 2] == '0' && s->bits[s->bits_len - 1] == '0') {
                C_ANN_PUT(di, ss, samplenum, s->out_ann, ANN_ACK, "OK");
                s->ack = 0;
                swd_next_state(s);
            } else if (s->bits[s->bits_len - 3] == '0' && s->bits[s->bits_len - 2] == '0' && s->bits[s->bits_len - 1] == '1') {
                C_ANN_PUT(di, ss, samplenum, s->out_ann, ANN_ACK, "FAULT");
                s->ack = 1;
                if (s->orundetect == 1)
                    swd_next_state(s);
                else
                    swd_reset_state(s);
                s->turnaround = 1;
            } else if (s->bits[s->bits_len - 3] == '0' && s->bits[s->bits_len - 2] == '1' && s->bits[s->bits_len - 1] == '0') {
                C_ANN_PUT(di, ss, samplenum, s->out_ann, ANN_ACK, "WAIT");
                s->ack = 2;
                if (s->orundetect == 1)
                    swd_next_state(s);
                else
                    swd_reset_state(s);
                s->turnaround = 1;
            } else if (s->bits[s->bits_len - 3] == '1' && s->bits[s->bits_len - 2] == '1' && s->bits[s->bits_len - 1] == '1') {
                C_ANN_PUT(di, ss, samplenum, s->out_ann, ANN_ACK, "NOREPLY");
                s->ack = 3;
                swd_reset_state(s);
            } else {
                C_ANN_PUT(di, ss, samplenum, s->out_ann, ANN_ACK, "ERROR");
                s->ack = 4;
                swd_reset_state(s);
            }
            break;
        }

        case DATA: {
            if (s->bits_len < 32)
                break;

            s->data = 0;
            s->dparity = 0;
            int i;
            for (i = 0; i < 32; i++) {
                if (s->bits[s->bits_len - 32 + i] == '1') {
                    s->data |= (1u << i);
                    s->dparity++;
                }
            }
            s->dparity %= 2;

            uint64_t ss = s->samplenums[s->bits_len - 32];
            char data_str[16];
            snprintf(data_str, sizeof(data_str), "0x%08x", s->data);
            C_ANN_PUT(di, ss, samplenum, s->out_ann, ANN_DATA, data_str);
            swd_next_state(s);
            break;
        }

        case DPARITY: {
            int parity_received = s->bits[s->bits_len - 1] - '0';
            uint64_t ss = s->samplenums[s->bits_len - 1];

            if (s->dparity != parity_received) {
                char ptext[16];
                snprintf(ptext, sizeof(ptext), "%d%d", s->dparity, parity_received);
                C_ANN_PUT(di, ss, samplenum, s->out_ann, ANN_PARITY, ptext);
            } else {
                unsigned char py_data[8];
                uint32_t addr32 = (uint32_t)s->addr;
                memcpy(py_data, &addr32, 4);
                memcpy(py_data + 4, &s->data, 4);
                if (s->rw == 1) {
                    c_decoder_put_python(di, s->ss_req, samplenum, s->out_python,
                        s->apdp ? "AP READ" : "DP READ", py_data, 8);
                } else {
                    c_decoder_put_python(di, s->ss_req, samplenum, s->out_python,
                        s->apdp ? "AP WRITE" : "DP WRITE", py_data, 8);
                    handle_completed_write(s);
                }
            }
            swd_next_state(s);
            break;
        }
        }
    }
}

static void swd_destroy(struct srd_decoder_inst* di)
{
    void* priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder swd_c_decoder = {
    .id = "swd_c",
    .name = "SWD(C)",
    .longname = "Serial Wire Debug (C)",
    .desc = "SWD protocol decoder (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = swd_channels,
    .num_channels = 2,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = swd_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = swd_ann_labels,
    .num_annotation_rows = 1,
    .annotation_rows = swd_ann_rows,
    .inputs = swd_inputs,
    .num_inputs = 1,
    .outputs = swd_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = swd_tags,
    .num_tags = 1,
    .reset = swd_reset,
    .start = swd_start,
    .decode = swd_decode,
    .destroy = swd_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder* srd_c_decoder_entry(void)
{
    GVariant* vals[] = {
        g_variant_new_string("yes"),
        g_variant_new_string("no"),
    };
    GSList* val_list = NULL;
    val_list = g_slist_append(val_list, vals[0]);
    val_list = g_slist_append(val_list, vals[1]);
    swd_options[0].def = g_variant_new_string("no");
    swd_options[0].values = val_list;
    return &swd_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
