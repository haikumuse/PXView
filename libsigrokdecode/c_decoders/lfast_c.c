#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum lfast_ann {
    ANN_BIT = 0,
    ANN_SYNC,
    ANN_HEADER_PL_SIZE,
    ANN_HEADER_CH_TYPE,
    ANN_HEADER_CTS,
    ANN_PAYLOAD,
    ANN_CTRL_DATA,
    ANN_SLEEP,
    ANN_WARNING,
    NUM_ANN,
};

enum {
    STATE_SYNC = 0,
    STATE_HEADER,
    STATE_PAYLOAD,
    STATE_SLEEPBIT,
};

struct lfast_priv {
    int state;
    uint64_t ss, es;
    uint64_t ss_bit, es_bit;
    uint64_t ss_sync, ss_header, ss_byte;
    uint64_t ss_payload, es_payload;
    uint64_t bit_len;
    uint64_t prev_bit_len;
    uint64_t timeout;
    uint8_t bits[64];
    int bit_count;
    int payload_size;
    int ch_type_id;
    uint8_t payload_bytes[64];
    int payload_byte_count;
    int out_ann;
    int out_proto;
};

static const char *payload_sizes[] = {
    "8 bit", "32 bit / 4 byte", "64 bit / 8 byte",
    "96 bit / 12 byte", "128 bit / 16 byte", "256 bit / 32 byte",
    "512 bit / 64 byte", "288 bit / 36 byte"
};
static const int payload_byte_sizes[] = {1, 4, 8, 12, 16, 32, 64, 36};

static const char *channel_types[] = {
    "Control", "Reserved", "Reserved", "Reserved",
    "Data 0", "Data 1", "Data 2", "Data 3",
    "Data 4", "Data 5", "Data 6", "Data 7",
    "Reserved", "Reserved", "Reserved", "Reserved"
};

static const char *control_payloads[] = {
    "Idle", "NACK", "Busy", "Sync",
    "Sleep", "Wake-up", "Reserved", "Reserved"
};

static struct srd_channel lfast_channels[] = {
    {"data", "Data", "TXP or RXP", 0, SRD_CHANNEL_SDATA, "dec_lfast_chan_data"},
};

static const char *lfast_ann_labels[][3] = {
    {"", "bit", "Bits"},
    {"", "sync", "Sync Pattern"},
    {"", "header_pl_size", "Payload Size"},
    {"", "header_ch_type", "Logical Channel Type"},
    {"", "header_cts", "Clear To Send"},
    {"", "payload", "Payload"},
    {"", "ctrl_data", "Control Data"},
    {"", "sleep", "Sleep Bit"},
    {"", "warning", "Warning"},
};

static const int lfast_row_bits_classes[] = {ANN_BIT, -1};
static const int lfast_row_fields_classes[] = {ANN_SYNC, ANN_HEADER_PL_SIZE, ANN_HEADER_CH_TYPE, ANN_HEADER_CTS, ANN_PAYLOAD, ANN_CTRL_DATA, ANN_SLEEP, -1};
static const int lfast_row_warnings_classes[] = {ANN_WARNING, -1};

static const struct srd_c_ann_row lfast_ann_rows[] = {
    {"bits", "Bits", lfast_row_bits_classes, 1},
    {"fields", "Fields", lfast_row_fields_classes, 7},
    {"warnings", "Warnings", lfast_row_warnings_classes, 1},
};

static const char *lfast_inputs[] = {"logic"};
static const char *lfast_outputs[] = {"lfast"};
static const char *lfast_tags[] = {"Embedded/industrial"};

static uint32_t bitpack(uint8_t *bits, int count)
{
    uint32_t val = 0;
    for (int i = 0; i < count && i < 32; i++)
        val = (val << 1) | (bits[i] & 1);
    return val;
}

static void reset_state(struct lfast_priv *s)
{
    s->state = STATE_SYNC;
    s->bit_count = 0;
    s->payload_size = 0;
    s->ch_type_id = 0;
    s->payload_byte_count = 0;
    s->timeout = 0;
    s->prev_bit_len = s->bit_len;
    s->bit_len = 0;
    memset(s->bits, 0, sizeof(s->bits));
    memset(s->payload_bytes, 0, sizeof(s->payload_bytes));
}

static void lfast_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct lfast_priv)));
    struct lfast_priv *s = (struct lfast_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct lfast_priv));
    s->out_ann = -1;
    s->out_proto = -1;
}

static void lfast_start(struct srd_decoder_inst *di)
{
    struct lfast_priv *s = (struct lfast_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "lfast");
    s->out_proto = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "lfast");
}

static void lfast_decode(struct srd_decoder_inst *di)
{
    struct lfast_priv *s = (struct lfast_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched = 0;

    uint64_t samplerate = c_decoder_get_samplerate(di);
    if (!samplerate)
        return;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 0);
        if (s->timeout > 0) {
            c_cond_or(cb);
            c_cond_skip(cb, s->timeout);
        }
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int is_timeout = (s->timeout > 0) &&
                         (matched & (1ULL << 1)) && !(matched & (1ULL << 0));

        if (is_timeout) {
            if (s->state == STATE_SLEEPBIT) {
                /* Sleep bit is 1 (no edge = LVDS sleep) */
                C_ANN_PUT(di, s->ss, samplenum, s->out_ann, ANN_SLEEP, "Sleep: 1");
                reset_state(s);
                continue;
            }
            reset_state(s);
            continue;
        }

        s->es = samplenum;
        int val = c_decoder_get_pin(di, 0, samplenum);

        /* Auto-detect bit_len from first edge interval */
        if (s->bit_len == 0) {
            if (s->ss > 0) {
                s->bit_len = s->es - s->ss;
                if (s->bit_len == 0) {
                    s->ss = s->es;
                    continue;
                }
                /* First valid bit_len detected, mark sync start */
                s->ss_sync = s->ss;
            }
            s->ss = s->es;
            continue;
        }

        /* Calculate bit count between edges */
        uint64_t edge_delta = s->es - s->ss;
        int bit_count = (int)((double)edge_delta / (double)s->bit_len + 0.5);

        if (bit_count == 0) {
            /* Bit time too short, reset */
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_WARNING, "Bit time too short");
            reset_state(s);
            s->ss = s->es;
            continue;
        }

        /* Fill bits: the value before the edge is (1 - val) for bit_count-1 bits,
           then val for the last bit */
        int prev_val = 1 - val;
        for (int i = 0; i < bit_count && s->bit_count < 64; i++) {
            int bval = (i < bit_count - 1) ? prev_val : val;
            s->bits[s->bit_count++] = bval;
            char bit_str[4];
            snprintf(bit_str, sizeof(bit_str), "%d", bval);
            uint64_t bss = s->ss + (uint64_t)((double)i * s->bit_len);
            uint64_t bes = s->ss + (uint64_t)((double)(i + 1) * s->bit_len);
            C_ANN_PUT(di, bss, bes, s->out_ann, ANN_BIT, bit_str);
        }

        s->ss = s->es;

        /* Process based on state */
        if (s->state == STATE_SYNC) {
            if (s->bit_count >= 16) {
                uint32_t sync_val = bitpack(s->bits, 16);
                if (sync_val == 0xA84B) {
                    C_ANN_PUT(di, s->ss_sync, s->ss, s->out_ann, ANN_SYNC, "Sync: 0xA84B");
                    s->state = STATE_HEADER;
                    s->bit_count = 0;
                    s->ss_header = s->ss;
                    s->timeout = (uint64_t)(9.4 * s->bit_len);
                } else {
                    C_ANN_PUT(di, s->ss_sync, s->ss, s->out_ann, ANN_WARNING, "Invalid sync pattern");
                    reset_state(s);
                }
            } else {
                s->timeout = (uint64_t)(16.2 * s->bit_len);
            }
        } else if (s->state == STATE_HEADER) {
            if (s->bit_count >= 8) {
                /* Parse header: 3 bits payload_size, 4 bits channel_type, 1 bit CTS */
                int pl_size_id = (int)bitpack(s->bits, 3);
                s->ch_type_id = (int)bitpack(s->bits + 3, 4);
                int cts = s->bits[7];

                if (pl_size_id < 0 || pl_size_id > 7)
                    pl_size_id = 0;
                s->payload_size = payload_byte_sizes[pl_size_id];

                char pl_str[64];
                snprintf(pl_str, sizeof(pl_str), "Payload size: %s", payload_sizes[pl_size_id]);
                C_ANN_PUT(di, s->ss_header, s->ss, s->out_ann, ANN_HEADER_PL_SIZE, pl_str);

                char ch_str[64];
                snprintf(ch_str, sizeof(ch_str), "Channel type: %s (0x%X)", channel_types[s->ch_type_id], s->ch_type_id);
                C_ANN_PUT(di, s->ss_header, s->ss, s->out_ann, ANN_HEADER_CH_TYPE, ch_str);

                char cts_str[32];
                snprintf(cts_str, sizeof(cts_str), "CTS: %d", cts);
                C_ANN_PUT(di, s->ss_header, s->ss, s->out_ann, ANN_HEADER_CTS, cts_str);

                s->state = STATE_PAYLOAD;
                s->bit_count = 0;
                s->ss_payload = s->ss;
                s->timeout = (uint64_t)(9.4 * s->bit_len);
            }
        } else if (s->state == STATE_PAYLOAD) {
            int needed_bits = s->payload_size * 8;
            if (s->bit_count >= needed_bits && needed_bits <= 64) {
                /* Extract payload bytes */
                s->payload_byte_count = s->payload_size;
                for (int i = 0; i < s->payload_size && i < 64; i++) {
                    s->payload_bytes[i] = (uint8_t)bitpack(s->bits + i * 8, 8);
                }

                int is_data_channel = (s->ch_type_id >= 4 && s->ch_type_id <= 11);
                if (is_data_channel) {
                    /* Data channel */
                    char pl_str[256];
                    int pos = 0;
                    for (int i = 0; i < s->payload_byte_count && pos < 240; i++)
                        pos += snprintf(pl_str + pos, sizeof(pl_str) - pos, "%s%02X", (i > 0) ? " " : "", s->payload_bytes[i]);
                    C_ANN_PUT(di, s->ss_payload, s->ss, s->out_ann, ANN_PAYLOAD, pl_str);

                    /* Output protocol data for data channels */
                    c_decoder_put_proto(di, s->ss_payload, s->ss, s->out_proto,
                                        "DATA", s->payload_bytes, s->payload_byte_count);
                } else {
                    /* Control channel */
                    int ctrl_id = (s->payload_byte_count > 0) ? s->payload_bytes[0] & 0x07 : 0;
                    const char *ctrl_name = (ctrl_id < 8) ? control_payloads[ctrl_id] : "Unknown";
                    char ctrl_str[64];
                    snprintf(ctrl_str, sizeof(ctrl_str), "Control: %s", ctrl_name);
                    C_ANN_PUT(di, s->ss_payload, s->ss, s->out_ann, ANN_CTRL_DATA, ctrl_str);
                }

                s->state = STATE_SLEEPBIT;
                s->bit_count = 0;
                s->timeout = (uint64_t)(1.4 * s->bit_len);
            }
        } else if (s->state == STATE_SLEEPBIT) {
            /* Sleep bit: 0 = active, 1 = LVDS sleep */
            int sleep_bit = val;
            char sleep_str[16];
            snprintf(sleep_str, sizeof(sleep_str), "Sleep: %d", sleep_bit);
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_SLEEP, sleep_str);
            reset_state(s);
        }
    }
}

static void lfast_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder lfast_c_decoder = {
    .id = "lfast_c",
    .name = "LFAST(C)",
    .longname = "NXP LFAST interface (C)",
    .desc = "Differential high-speed P2P interface (C implementation)",
    .license = "gplv2+",
    .channels = lfast_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = lfast_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = lfast_ann_rows,
    .inputs = lfast_inputs,
    .num_inputs = 1,
    .outputs = lfast_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = lfast_tags,
    .num_tags = 1,
    .reset = lfast_reset,
    .start = lfast_start,
    .decode = lfast_decode,
    .destroy = lfast_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &lfast_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
