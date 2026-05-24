#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum miller_ann {
    ANN_BIT = 0,
    ANN_BITSTRING,
    NUM_ANN,
};

struct miller_priv {
    uint64_t samplerate;
    uint64_t timeunit;
    int edge_type;          /* 0=rising, 1=falling, 2=either */
    int prevbit;
    uint64_t prevedge;
    uint64_t expectedstart;
    /* decode_run state */
    uint8_t bits[256];
    int numbits;
    uint32_t bitvalue;
    uint64_t stringstart;
    uint64_t stringend;
    int out_ann;
    int out_binary;
};

static struct srd_channel miller_channels[] = {
    {"data", "Data", "Data signal", 0, SRD_CHANNEL_SDATA, "dec_miller_chan_data"},
};

static struct srd_decoder_option miller_options[] = {
    {"baudrate", "dec_miller_opt_baudrate", "Baud rate", NULL, NULL},
    {"edge", "dec_miller_opt_edge", "Edge", NULL, NULL},
};

static const char *miller_ann_labels[][3] = {
    {"", "bit", "Bit"},
    {"", "bitstring", "Bitstring"},
};

static const int miller_row_bit_classes[] = {ANN_BIT, -1};
static const int miller_row_bitstring_classes[] = {ANN_BITSTRING, -1};

static const struct srd_c_ann_row miller_ann_rows[] = {
    {"bit", "Bit", miller_row_bit_classes, 1},
    {"bitstring", "Bitstring", miller_row_bitstring_classes, 1},
};

static const struct srd_decoder_binary miller_binary[] = {
    {0, "raw", "Raw binary"},
};

static const char *miller_inputs[] = {"logic"};
static const char *miller_tags[] = {"Encoding"};

static void miller_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct miller_priv)));
    struct miller_priv *s = (struct miller_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct miller_priv));
    s->out_ann = -1;
    s->out_binary = -1;
}

static void miller_start(struct srd_decoder_inst *di)
{
    struct miller_priv *s = (struct miller_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "miller");
    s->out_binary = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "miller");

    int64_t baudrate = c_decoder_get_option_int(di, "baudrate", 106000);
    if (baudrate <= 0)
        baudrate = 106000;

    const char *edge_str = c_decoder_get_option_string(di, "edge", "falling");
    if (strcmp(edge_str, "rising") == 0)
        s->edge_type = 0;
    else if (strcmp(edge_str, "either") == 0)
        s->edge_type = 2;
    else
        s->edge_type = 1;  /* falling default */
}

static void miller_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct miller_priv *s = (struct miller_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE)
        s->samplerate = value;
}

static void output_bit(struct srd_decoder_inst *di, struct miller_priv *s,
                       int bit, uint64_t ss, uint64_t es)
{
    char bit_str[4];
    snprintf(bit_str, sizeof(bit_str), "%d", bit);
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_BIT, bit_str);

    if (s->numbits == 0)
        s->stringstart = ss;
    s->stringend = es;

    if (s->numbits < 256) {
        s->bits[s->numbits] = bit;
        s->bitvalue |= ((uint32_t)bit << s->numbits);
        s->numbits++;
    }
}

static void flush_bitstring(struct srd_decoder_inst *di, struct miller_priv *s)
{
    if (s->numbits == 0)
        return;

    /* Format bitstring with 4-bit grouping */
    char bs[512];
    int pos = 0;
    for (int i = 0; i < s->numbits && pos < 500; i++) {
        if (i > 0 && (i % 4) == 0)
            pos += snprintf(bs + pos, sizeof(bs) - pos, " ");
        pos += snprintf(bs + pos, sizeof(bs) - pos, "%d", s->bits[i]);
    }
    C_ANN_PUT(di, s->stringstart, s->stringend, s->out_ann, ANN_BITSTRING, bs);

    /* Binary output */
    int numbytes = (s->numbits + 7) / 8;
    unsigned char bdata[32];
    memset(bdata, 0, sizeof(bdata));
    for (int i = 0; i < s->numbits && i < 256; i++) {
        if (s->bits[i])
            bdata[i / 8] |= (1 << (i % 8));
    }
    c_decoder_put_binary(di, s->stringstart, s->stringend,
                         s->out_binary, 0, numbytes, bdata);

    /* Reset for next bitstring */
    s->numbits = 0;
    s->bitvalue = 0;
}

static int wait_edge_or_timeout(struct srd_decoder_inst *di, struct miller_priv *s,
                                uint64_t *samplenum, uint64_t *matched)
{
    srd_cond_builder *cb = c_cond_new();
    switch (s->edge_type) {
    case 0: c_cond_rise(cb, 0); break;
    case 1: c_cond_fall(cb, 0); break;
    case 2: c_cond_edge(cb, 0); break;
    }
    c_cond_or(cb);
    c_cond_skip(cb, 3 * s->timeunit);
    int ret = c_cond_wait(cb, di, samplenum, matched);
    c_cond_free(cb);
    return ret;
}

static void miller_decode(struct srd_decoder_inst *di)
{
    struct miller_priv *s = (struct miller_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched = 0;

    if (!s->samplerate)
        s->samplerate = c_decoder_get_samplerate(di);
    if (!s->samplerate)
        return;

    int64_t baudrate = c_decoder_get_option_int(di, "baudrate", 106000);
    if (baudrate <= 0)
        baudrate = 106000;
    s->timeunit = s->samplerate / (uint64_t)baudrate;
    if (s->timeunit == 0)
        return;

    /* Wait for first edge */
    srd_cond_builder *cb = c_cond_new();
    switch (s->edge_type) {
    case 0: c_cond_rise(cb, 0); break;
    case 1: c_cond_fall(cb, 0); break;
    case 2: c_cond_edge(cb, 0); break;
    }
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK)
        return;

    s->prevedge = samplenum;
    s->expectedstart = samplenum + s->timeunit / 2;
    s->prevbit = 0;  /* initial bit = 0 */

    while (1) {
        ret = wait_edge_or_timeout(di, s, &samplenum, &matched);
        if (ret != SRD_OK) {
            flush_bitstring(di, s);
            return;
        }

        /* Check for timeout (skip matched, no edge) */
        int is_timeout = (matched & (1ULL << 1)) && !(matched & (1ULL << 0));

        if (is_timeout) {
            flush_bitstring(di, s);
            return;
        }

        uint64_t sampledelta = samplenum - s->prevedge;

        /* Round timedelta to nearest 0.5 */
        double td_exact = (double)sampledelta / (double)s->timeunit;
        double timedelta = round(td_exact * 2.0) / 2.0;

        if (timedelta <= 0.5) {
            /* Error: edges too close */
            s->prevedge = samplenum;
            continue;
        }

        if (s->prevbit == 0) {
            /* After space */
            if (timedelta == 1.0) {
                /* space (0) */
                output_bit(di, s, 0, s->expectedstart, s->expectedstart + s->timeunit);
                s->prevbit = 0;
                s->expectedstart = s->expectedstart + s->timeunit;
            } else if (timedelta == 1.5) {
                /* mark (1) */
                output_bit(di, s, 1, s->expectedstart, samplenum + s->timeunit / 2);
                s->prevbit = 1;
                s->expectedstart = samplenum + s->timeunit / 2;
            } else if (timedelta >= 2.0) {
                /* idle - end of message */
                flush_bitstring(di, s);
                /* Start looking for next message */
                cb = c_cond_new();
                switch (s->edge_type) {
                case 0: c_cond_rise(cb, 0); break;
                case 1: c_cond_fall(cb, 0); break;
                case 2: c_cond_edge(cb, 0); break;
                }
                ret = c_cond_wait(cb, di, &samplenum, &matched);
                c_cond_free(cb);
                if (ret != SRD_OK)
                    return;
                s->prevedge = samplenum;
                s->expectedstart = samplenum + s->timeunit / 2;
                s->prevbit = 0;
                continue;
            }
        } else {
            /* After mark */
            if (timedelta == 1.0) {
                /* mark (1) */
                output_bit(di, s, 1, s->expectedstart, samplenum + s->timeunit / 2);
                s->prevbit = 1;
                s->expectedstart = samplenum + s->timeunit / 2;
            } else if (timedelta == 1.5) {
                /* space (0) + space (0) */
                output_bit(di, s, 0, s->expectedstart, s->expectedstart + s->timeunit);
                output_bit(di, s, 0, s->expectedstart + s->timeunit, s->expectedstart + 2 * s->timeunit);
                s->prevbit = 0;
                s->expectedstart = s->expectedstart + 2 * s->timeunit;
            } else if (timedelta == 2.0) {
                /* space (0) + mark (1) */
                output_bit(di, s, 0, s->expectedstart, s->expectedstart + s->timeunit);
                output_bit(di, s, 1, s->expectedstart + s->timeunit, samplenum + s->timeunit / 2);
                s->prevbit = 1;
                s->expectedstart = samplenum + s->timeunit / 2;
            } else {
                /* space + idle - end */
                output_bit(di, s, 0, s->expectedstart, s->expectedstart + s->timeunit);
                flush_bitstring(di, s);
                /* Start looking for next message */
                cb = c_cond_new();
                switch (s->edge_type) {
                case 0: c_cond_rise(cb, 0); break;
                case 1: c_cond_fall(cb, 0); break;
                case 2: c_cond_edge(cb, 0); break;
                }
                ret = c_cond_wait(cb, di, &samplenum, &matched);
                c_cond_free(cb);
                if (ret != SRD_OK)
                    return;
                s->prevedge = samplenum;
                s->expectedstart = samplenum + s->timeunit / 2;
                s->prevbit = 0;
                continue;
            }
        }

        s->prevedge = samplenum;
    }
}

static void miller_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder miller_c_decoder = {
    .id = "miller_c",
    .name = "Miller(C)",
    .longname = "Miller encoding (C)",
    .desc = "Miller encoding protocol (C implementation)",
    .license = "gplv2+",
    .channels = miller_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = miller_options,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = miller_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = miller_ann_rows,
    .inputs = miller_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = miller_binary,
    .num_binary = 1,
    .tags = miller_tags,
    .num_tags = 1,
    .reset = miller_reset,
    .start = miller_start,
    .decode = miller_decode,
    .destroy = miller_destroy,
    .metadata = miller_metadata,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    miller_options[0].def = g_variant_new_int64(106000);

    GSList *edge_vals = NULL;
    edge_vals = g_slist_append(edge_vals, g_variant_new_string("rising"));
    edge_vals = g_slist_append(edge_vals, g_variant_new_string("falling"));
    edge_vals = g_slist_append(edge_vals, g_variant_new_string("either"));
    miller_options[1].def = g_variant_new_string("falling");
    miller_options[1].values = edge_vals;

    return &miller_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
