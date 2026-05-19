#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum decode_state {
    STATE_SYNC,
    STATE_DECODE,
};

enum {
    ANN_DATA_SYMBOL = 0,
    ANN_CTRL_SYMBOL,
    ANN_BIT,
    ANN_BYTE,
    NUM_ANN,
};

struct priv {
    enum decode_state state;
    uint64_t sync_cycles[64];
    int sync_count;
    int preamble_len;
    uint64_t symbol_len;
    uint64_t samplerate;
    uint64_t preamble_start;
    int jk_seen_j;
    int jk_seen_k;
    uint64_t sym_start;
    uint64_t data_start;
    int symbol;
    int bit_count;
    int last_nibble;
    int has_last_nibble;
    int out_ann;
    int out_python;
    int bit_offset;
};

static const int data_sym[32] = {
    -1, -1, -1, -1,
    -1, -1, -1, -1,
    -1, 0x1, 0x4, 0x5,
    -1, -1, 0x6, 0x7,
    -1, -1, 0x8, 0x9,
    0x2, 0x3, 0xA, 0xB,
    -1, -1, 0xC, 0xD,
    0xE, 0xF, 0x0, -1,
};

static const char *ctrl_long[32] = {
    "QUIET", NULL, NULL, NULL,
    "HALT", NULL, "L", "RESET",
    NULL, NULL, NULL, NULL,
    NULL, "TERMINATE", NULL, NULL,
    NULL, "K", NULL, NULL,
    NULL, NULL, NULL, NULL,
    "J", "SET", NULL, NULL,
    NULL, NULL, NULL, "IDLE",
};

static const char *ctrl_short[32] = {
    "Q", NULL, NULL, NULL,
    "H", NULL, "L", "R",
    NULL, NULL, NULL, NULL,
    NULL, "T", NULL, NULL,
    NULL, "K", NULL, NULL,
    NULL, NULL, NULL, NULL,
    "J", "S", NULL, NULL,
    NULL, NULL, NULL, "I",
};

static struct srd_channel channels[] = {
    {"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option fourb5b_options[] = {
    {
        .id = "bit_offset",
        .idn = NULL,
        .desc = "Bit offset",
        .def = NULL,
        .values = NULL,
    },
};

static const char *ann_labels[][3] = {
    {"", "symbol_data", "Data symbol"},
    {"", "symbol_ctrl", "Control symbol"},
    {"", "bit", "Decoded bit"},
    {"", "byte", "Decoded byte"},
};

static const int row_symbol_classes[] = {0, 1};
static const int row_bits_classes[] = {2};
static const int row_bytes_classes[] = {3};
static const struct srd_c_ann_row ann_rows[] = {
    {"symbol", "Symbols", row_symbol_classes, 2},
    {"bits", "Bits", row_bits_classes, 1},
    {"bytes", "Bytes", row_bytes_classes, 1},
};

static const char *inputs[] = {"logic", NULL};
static const char *outputs[] = {"4b5b", NULL};
static const char *tags[] = {"Encoding", NULL};

static void _4b5b_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct priv *s = (struct priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
    }
}

static void reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct priv)));
    struct priv *s = (struct priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct priv));
    s->state = STATE_SYNC;
    s->preamble_len = 16;
    s->last_nibble = -1;
    s->out_ann = 0;
}

static void start(struct srd_decoder_inst *di)
{
    struct priv *s = (struct priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "4b5b");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "4b5b");
    s->samplerate = c_decoder_get_samplerate(di);
    s->bit_offset = atoi(c_decoder_get_option_string(di, "bit_offset", "0"));
}

static void decode(struct srd_decoder_inst *di)
{
    struct priv *s = (struct priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;

    if (s->samplerate == 0)
        return;

    {
        srd_cond_builder *cb = c_cond_new();
        c_cond_rise(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;
    }
    s->preamble_start = samplenum;

    while (1) {
        if (s->state == STATE_SYNC) {
            uint64_t start = samplenum;

            srd_cond_builder *cb = c_cond_new();
            c_cond_rise(cb, 0);
            int ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            uint64_t end = samplenum;
            if (s->sync_count < 64) {
                s->sync_cycles[s->sync_count] = end - start;
                s->sync_count++;
            }

            if (s->sync_count == s->preamble_len) {
                uint64_t sum = 0;
                for (int i = 0; i < s->sync_count; i++)
                    sum += s->sync_cycles[i];
                double avg_cycle = (double)sum / (double)s->sync_count;
                s->symbol_len = (uint64_t)(avg_cycle / 2.0 + 0.5);

                {
                    uint64_t skip_count = s->symbol_len / 2;
                    srd_cond_builder *cb2 = c_cond_new();
                    c_cond_skip(cb2, skip_count);
                    int ret2 = c_cond_wait(cb2, di, &samplenum, &matched);
                    c_cond_free(cb2);
                    if (ret2 != SRD_OK)
                        return;
                }

                s->state = STATE_DECODE;
            }
        } else if (s->state == STATE_DECODE) {
            uint64_t start_sample = samplenum;

            srd_cond_builder *cb = c_cond_new();
            c_cond_edge(cb, 0);
            c_cond_or(cb);
            c_cond_skip(cb, s->symbol_len);
            int ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            if (matched & 1) {
                uint64_t edge_samp = samplenum - start_sample;
                int64_t offset = (int64_t)(s->symbol_len / 2) - (int64_t)edge_samp;
                int64_t remaining = (int64_t)s->symbol_len - (int64_t)edge_samp - offset;
                if (remaining > 0) {
                    srd_cond_builder *cb2 = c_cond_new();
                    c_cond_skip(cb2, (uint64_t)remaining);
                    int ret2 = c_cond_wait(cb2, di, &samplenum, &matched);
                    c_cond_free(cb2);
                    if (ret2 != SRD_OK)
                        return;
                }
            }

            int bit_val = (matched & 1) ? 1 : 0;
            uint64_t bit_end = samplenum;

            if (s->bit_offset > 0) {
                s->bit_offset--;
                continue;
            }

            if (s->bit_count == 0) {
                s->sym_start = start_sample;
                if (!s->has_last_nibble) {
                    s->data_start = start_sample;
                }
            }

            s->symbol = (s->symbol << 1) | bit_val;
            s->bit_count++;

            if (s->bit_count == 5) {
                if (ctrl_long[s->symbol]) {
                    s->jk_seen_j = (s->symbol == 24) || s->jk_seen_j;
                    s->jk_seen_k = (s->symbol == 17) || s->jk_seen_k;
                    C_ANN_PUT(di, s->sym_start, bit_end, s->out_ann, ANN_CTRL_SYMBOL, ctrl_long[s->symbol], ctrl_short[s->symbol]);
                    c_decoder_put_python(di, s->sym_start, bit_end, s->out_python, ctrl_short[s->symbol], NULL, 0);
                } else if (data_sym[s->symbol] >= 0 && s->jk_seen_j && s->jk_seen_k) {
                    char sym_str[6];
                    for (int i = 4; i >= 0; i--)
                        sym_str[4 - i] = ((s->symbol >> i) & 1) + '0';
                    sym_str[5] = '\0';
                    C_ANN_PUT(di, s->sym_start, bit_end, s->out_ann, ANN_DATA_SYMBOL, sym_str);

                    int nibble = data_sym[s->symbol];
                    char bit_str[5];
                    for (int i = 3; i >= 0; i--)
                        bit_str[3 - i] = ((nibble >> i) & 1) + '0';
                    bit_str[4] = '\0';
                    C_ANN_PUT(di, s->sym_start, bit_end, s->out_ann, ANN_BIT, bit_str);

                    if (s->has_last_nibble) {
                        int data_byte = (nibble << 4) | s->last_nibble;
                        char byte_str[8];
                        snprintf(byte_str, sizeof(byte_str), "0x%02X", data_byte);
                        C_ANN_PUT(di, s->data_start, bit_end, s->out_ann, ANN_BYTE, byte_str);
                        unsigned char py_byte = (unsigned char)data_byte;
                        c_decoder_put_python(di, s->data_start, bit_end, s->out_python, "DATA", &py_byte, 1);
                        s->data_start = bit_end;
                        s->has_last_nibble = 0;
                    } else {
                        s->last_nibble = nibble;
                        s->has_last_nibble = 1;
                    }
                }

                s->symbol = 0;
                s->bit_count = 0;
            }
        }
    }
}

static void destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder fourb5b_c_decoder = {
    .id = "4b5b_c",
    .name = "4B5B(C)",
    .longname = "4B5B Line Code (C)",
    .desc = "Maps 4 data bits to 5 bit symbols for transmission. (C implementation)",
    .license = "gplv2+",
    .channels = channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = fourb5b_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = ann_rows,
    .inputs = inputs,
    .num_inputs = 1,
    .outputs = outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = tags,
    .num_tags = 1,
    .reset = reset,
    .start = start,
    .decode = decode,
    .metadata = _4b5b_metadata,
    .destroy = destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    GVariant *vals[] = {
        g_variant_new_string("0"),
        g_variant_new_string("1"),
        g_variant_new_string("2"),
        g_variant_new_string("3"),
        g_variant_new_string("4"),
    };
    GSList *val_list = NULL;
    for (int i = 0; i < 5; i++)
        val_list = g_slist_append(val_list, vals[i]);
    fourb5b_options[0].def = g_variant_new_string("0");
    fourb5b_options[0].values = val_list;
    return &fourb5b_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
