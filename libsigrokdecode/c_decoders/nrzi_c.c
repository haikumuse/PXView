#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum nrzi_state {
    STATE_SYNC,
    STATE_DECODE,
};

enum {
    ANN_PREAMBLE = 0,
    ANN_BIT,
    NUM_ANN,
};

struct nrzi_priv {
    enum nrzi_state state;
    uint64_t sync_cycles[64];
    int sync_count;
    int preamble_len;
    uint64_t symbol_len;
    uint64_t samplerate;
    uint64_t ss_block;
    uint64_t es_block;
    uint64_t preamble_start;
    int out_ann;
    int out_python;
};

static struct srd_channel nrzi_channels[] = {
    {"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option nrzi_options[] = {
    {"preamble_len", NULL, "Preamble Length", NULL, NULL},
};

static const char *nrzi_ann_labels[][3] = {
    {"", "preamble", "Preamble"},
    {"", "bit", "Decoded bits"},
};

static const int nrzi_row_bits_classes[] = {0, 1};
static const struct srd_c_ann_row nrzi_ann_rows[] = {
    {"bits", "Bits", nrzi_row_bits_classes, 2},
};

static const char *nrzi_inputs[] = {"logic", NULL};
static const char *nrzi_outputs[] = {"nrzi", NULL};
static const char *nrzi_tags[] = {"Encoding", NULL};

static void nrzi_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct nrzi_priv)));
    struct nrzi_priv *s = (struct nrzi_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct nrzi_priv));
    s->state = STATE_SYNC;
    s->preamble_len = 16;
    s->out_ann = 0;
    s->out_python = -1;
}

static void nrzi_start(struct srd_decoder_inst *di)
{
    struct nrzi_priv *s = (struct nrzi_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "nrzi");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "nrzi");
    const char *plen_str = c_decoder_get_option_string(di, "preamble_len", "16");
    s->preamble_len = plen_str ? atoi(plen_str) : 16;
    s->samplerate = c_decoder_get_samplerate(di);
}

static void nrzi_decode(struct srd_decoder_inst *di)
{
    struct nrzi_priv *s = (struct nrzi_priv *)c_decoder_get_private(di);
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

                double clock_rate = (double)s->samplerate / ((double)s->symbol_len * 2.0);

                char freq_str[64];
                if (clock_rate >= 1e6)
                    snprintf(freq_str, sizeof(freq_str), "%g MHz", clock_rate / 1e6);
                else if (clock_rate >= 1e3)
                    snprintf(freq_str, sizeof(freq_str), "%g kHz", clock_rate / 1e3);
                else
                    snprintf(freq_str, sizeof(freq_str), "%g Hz", clock_rate);

                char preamble_str[128];
                snprintf(preamble_str, sizeof(preamble_str), "Preamble (%s)", freq_str);

                C_ANN_PUT(di, s->preamble_start, samplenum, s->out_ann, ANN_PREAMBLE, preamble_str);

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
            char bit_str[4];
            snprintf(bit_str, sizeof(bit_str), "%d", bit_val);
            C_ANN_PUT(di, start_sample, samplenum, s->out_ann, ANN_BIT, bit_str);

            if (s->out_python >= 0) {
                int32_t py_bit = (int32_t)bit_val;
                c_decoder_put_python(di, start_sample, samplenum, s->out_python, "bit",
                                     (unsigned char *)&py_bit, sizeof(int32_t));
            }
        }
    }
}

static void nrzi_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder nrzi_c_decoder = {
    .id = "nrzi_c",
    .name = "NRZ-I(C)",
    .longname = "Non-return-to-zero Inverted (C)",
    .desc = "Bits encoded as presence or absence of a transition. (C implementation)",
    .license = "gplv2+",
    .channels = nrzi_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = nrzi_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = nrzi_ann_labels,
    .num_annotation_rows = 1,
    .annotation_rows = nrzi_ann_rows,
    .inputs = nrzi_inputs,
    .num_inputs = 1,
    .outputs = nrzi_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = nrzi_tags,
    .num_tags = 1,
    .reset = nrzi_reset,
    .start = nrzi_start,
    .decode = nrzi_decode,
    .destroy = nrzi_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    nrzi_options[0].def = g_variant_new_string("16");
    GSList *vals = NULL;
    vals = g_slist_append(vals, g_variant_new_string("4"));
    vals = g_slist_append(vals, g_variant_new_string("8"));
    vals = g_slist_append(vals, g_variant_new_string("16"));
    vals = g_slist_append(vals, g_variant_new_string("32"));
    vals = g_slist_append(vals, g_variant_new_string("64"));
    nrzi_options[0].values = vals;
    return &nrzi_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
