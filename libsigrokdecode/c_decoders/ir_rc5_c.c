#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum rc5_state {
    STATE_IDLE,
    STATE_MID1,
    STATE_MID0,
    STATE_START1,
    STATE_START0,
};

enum rc5_ann {
    ANN_BIT = 0,
    ANN_STARTBIT1,
    ANN_STARTBIT2,
    ANN_TOGGLEBIT0,
    ANN_TOGGLEBIT1,
    ANN_ADDRESS,
    ANN_COMMAND,
    NUM_ANN,
};

#define IR_CH 0
#define MAX_EDGES 32
#define MAX_BITS 14

struct rc5_priv {
    enum rc5_state state;
    uint64_t samplerate;
    uint64_t halfbit;
    uint64_t edges[MAX_EDGES];
    int num_edges;
    uint64_t bits_ss[MAX_BITS];
    int bits_val[MAX_BITS];
    int num_bits;
    uint64_t ss_es_bits_ss[MAX_BITS];
    uint64_t ss_es_bits_es[MAX_BITS];
    int num_ss_es_bits;
    int next_edge_is_low;
    int is_extended;
    int out_ann;
};

static struct srd_channel rc5_channels[] = {
    { "ir", "IR", "IR data line", 0, SRD_CHANNEL_SDATA, "dec_ir_rc5_chan_ir" },
};

static struct srd_decoder_option rc5_options_arr[2];

static const char* rc5_ann_labels[][3] = {
    { "", "bit", "Bit" },
    { "", "startbit1", "Startbit 1" },
    { "", "startbit2", "Startbit 2" },
    { "", "togglebit-0", "Toggle bit 0" },
    { "", "togglebit-1", "Toggle bit 1" },
    { "", "address", "Address" },
    { "", "command", "Command" },
};

static const int rc5_row_bits_classes[] = { ANN_BIT, -1 };
static const int rc5_row_fields_classes[] = { ANN_STARTBIT1, ANN_STARTBIT2, ANN_TOGGLEBIT0, ANN_TOGGLEBIT1, ANN_ADDRESS, ANN_COMMAND, -1 };
static const struct srd_c_ann_row rc5_ann_rows[] = {
    { "bits", "Bits", rc5_row_bits_classes, 1 },
    { "fields", "Fields", rc5_row_fields_classes, 6 },
};

static const char* rc5_inputs[] = { "logic", NULL };
static const char* rc5_outputs[] = { NULL };
static const char* rc5_tags[] = { "IR", NULL };

static char rc5_edge_type(struct rc5_priv* s, uint64_t samplenum)
{
    uint64_t distance = samplenum - s->edges[s->num_edges - 1];
    uint64_t half = s->halfbit;
    uint64_t long_dist = half * 2;
    uint64_t margin = half / 2;

    if (distance >= long_dist - margin && distance <= long_dist + margin)
        return 'l';
    if (distance >= half - margin && distance <= half + margin)
        return 's';
    return 'e';
}

static void rc5_handle_bits(struct srd_decoder_inst* di, struct rc5_priv* s)
{
    int i;
    int a = 0, c = 0;

    s->num_ss_es_bits = 0;
    for (i = 0; i < s->num_bits; i++) {
        uint64_t ss, es;
        if (i == 0) {
            ss = (s->bits_ss[0] > s->halfbit) ? (s->bits_ss[0] - s->halfbit) : 0;
        } else {
            ss = s->ss_es_bits_es[i - 1];
        }
        es = s->bits_ss[i] + s->halfbit;
        s->ss_es_bits_ss[i] = ss;
        s->ss_es_bits_es[i] = es;
        s->num_ss_es_bits++;

        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", s->bits_val[i]);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_BIT, bit_str);
    }

    {
        char str1[32], str2[16], str3[8], str4[8], str5[4];
        snprintf(str1, sizeof(str1), "Startbit1: %d", s->bits_val[0]);
        snprintf(str2, sizeof(str2), "SB1: %d", s->bits_val[0]);
        snprintf(str3, sizeof(str3), "SB1");
        snprintf(str4, sizeof(str4), "S1");
        snprintf(str5, sizeof(str5), "S");
        C_ANN_PUT(di, s->ss_es_bits_ss[0], s->ss_es_bits_es[0], s->out_ann, ANN_STARTBIT1,
            str1, str2, str3, str4, str5);
    }

    {
        int ann_idx = ANN_STARTBIT2;
        if (s->is_extended) {
            char str1[32], str2[16], str3[8], str4[8], str5[4];
            snprintf(str1, sizeof(str1), "CMD[6]#: %d", s->bits_val[1]);
            snprintf(str2, sizeof(str2), "C6#: %d", s->bits_val[1]);
            snprintf(str3, sizeof(str3), "C6#");
            snprintf(str4, sizeof(str4), "C#");
            snprintf(str5, sizeof(str5), "C");
            ann_idx = ANN_COMMAND;
            C_ANN_PUT(di, s->ss_es_bits_ss[1], s->ss_es_bits_es[1], s->out_ann, ann_idx,
                str1, str2, str3, str4, str5);
        } else {
            char str1[32], str2[16], str3[8], str4[8], str5[4];
            snprintf(str1, sizeof(str1), "Startbit2: %d", s->bits_val[1]);
            snprintf(str2, sizeof(str2), "SB2: %d", s->bits_val[1]);
            snprintf(str3, sizeof(str3), "SB2");
            snprintf(str4, sizeof(str4), "S2");
            snprintf(str5, sizeof(str5), "S");
            C_ANN_PUT(di, s->ss_es_bits_ss[1], s->ss_es_bits_es[1], s->out_ann, ann_idx,
                str1, str2, str3, str4, str5);
        }
    }

    {
        int ann_idx = (s->bits_val[2] == 0) ? ANN_TOGGLEBIT0 : ANN_TOGGLEBIT1;
        char str1[32], str2[16], str3[16], str4[8], str5[4];
        snprintf(str1, sizeof(str1), "Togglebit: %d", s->bits_val[2]);
        snprintf(str2, sizeof(str2), "Toggle: %d", s->bits_val[2]);
        snprintf(str3, sizeof(str3), "TB: %d", s->bits_val[2]);
        snprintf(str4, sizeof(str4), "TB");
        snprintf(str5, sizeof(str5), "T");
        C_ANN_PUT(di, s->ss_es_bits_ss[2], s->ss_es_bits_es[2], s->out_ann, ann_idx,
            str1, str2, str3, str4, str5);
    }

    for (i = 0; i < 5; i++)
        a |= (s->bits_val[3 + i] << (4 - i));
    {
        char str1[32], str2[32], str3[16], str4[8], str5[4];
        snprintf(str1, sizeof(str1), "Address: %d", a);
        snprintf(str2, sizeof(str2), "Addr: %d", a);
        snprintf(str3, sizeof(str3), "Addr: %d", a);
        snprintf(str4, sizeof(str4), "A: %d", a);
        snprintf(str5, sizeof(str5), "A");
        C_ANN_PUT(di, s->ss_es_bits_ss[3], s->ss_es_bits_es[7], s->out_ann, ANN_ADDRESS,
            str1, str2, str3, str4, str5);
    }

    for (i = 0; i < 6; i++)
        c |= (s->bits_val[8 + i] << (5 - i));
    if (s->is_extended) {
        int inverted_bit6 = (s->bits_val[1] == 0) ? 1 : 0;
        c |= (inverted_bit6 << 6);
    }
    {
        char str1[32], str2[32], str3[16], str4[8], str5[4];
        snprintf(str1, sizeof(str1), "Command: %d", c);
        snprintf(str2, sizeof(str2), "Cmd: %d", c);
        snprintf(str3, sizeof(str3), "Cmd: %d", c);
        snprintf(str4, sizeof(str4), "C: %d", c);
        snprintf(str5, sizeof(str5), "C");
        C_ANN_PUT(di, s->ss_es_bits_ss[8], s->ss_es_bits_es[13], s->out_ann, ANN_COMMAND,
            str1, str2, str3, str4, str5);
    }
}

static void rc5_reset(struct srd_decoder_inst* di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct rc5_priv)));
    }
    struct rc5_priv* s = (struct rc5_priv*)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct rc5_priv));
    s->state = STATE_IDLE;
}

static void rc5_start(struct srd_decoder_inst* di)
{
    struct rc5_priv* s = (struct rc5_priv*)c_decoder_get_private(di);

    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "ir_rc5");

    const char* polarity = c_decoder_get_option_string(di, "polarity", "active-low");
    s->next_edge_is_low = (strcmp(polarity, "active-low") == 0) ? 1 : 0;

    const char* protocol = c_decoder_get_option_string(di, "protocol", "standard");
    s->is_extended = (strcmp(protocol, "extended") == 0) ? 1 : 0;

    s->samplerate = c_decoder_get_samplerate(di);
    if (s->samplerate)
        s->halfbit = (uint64_t)((double)s->samplerate * 0.00178 / 2.0);
}

static void rc5_metadata(struct srd_decoder_inst* di, int key, uint64_t value)
{
    struct rc5_priv* s = (struct rc5_priv*)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
        s->halfbit = (uint64_t)((double)value * 0.00178 / 2.0);
    }
}

static void rc5_decode(struct srd_decoder_inst* di)
{
    struct rc5_priv* s = (struct rc5_priv*)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;

    if (!s->samplerate)
        return;

    while (1) {
        srd_cond_builder* cb = c_cond_new();
        if (s->next_edge_is_low)
            c_cond_fall(cb, IR_CH);
        else
            c_cond_rise(cb, IR_CH);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int ir = c_decoder_get_pin(di, IR_CH, samplenum);

        if (s->state == STATE_IDLE) {
            s->num_edges = 0;
            s->num_bits = 0;
            s->edges[0] = samplenum;
            s->num_edges = 1;
            s->bits_ss[0] = samplenum;
            s->bits_val[0] = 1;
            s->num_bits = 1;
            s->state = STATE_MID1;
            s->next_edge_is_low = ir ? 1 : 0;
            continue;
        }

        char etype = rc5_edge_type(s, samplenum);
        if (etype == 'e') {
            s->num_edges = 0;
            s->num_bits = 0;
            s->state = STATE_IDLE;
            continue;
        }

        int bit = -1;

        if (s->state == STATE_MID1) {
            if (etype == 's') {
                s->state = STATE_START1;
                bit = -1;
            } else {
                s->state = STATE_MID0;
                bit = 0;
            }
        } else if (s->state == STATE_MID0) {
            if (etype == 's') {
                s->state = STATE_START0;
                bit = -1;
            } else {
                s->state = STATE_MID1;
                bit = 1;
            }
        } else if (s->state == STATE_START1) {
            if (etype == 's') {
                s->state = STATE_MID1;
                bit = 1;
            } else {
                bit = -1;
            }
        } else if (s->state == STATE_START0) {
            if (etype == 's') {
                s->state = STATE_MID0;
                bit = 0;
            } else {
                bit = -1;
            }
        }

        if (s->num_edges < MAX_EDGES) {
            s->edges[s->num_edges] = samplenum;
            s->num_edges++;
        }

        if (bit >= 0 && s->num_bits < MAX_BITS) {
            s->bits_ss[s->num_bits] = samplenum;
            s->bits_val[s->num_bits] = bit;
            s->num_bits++;
        }

        if (s->num_bits == MAX_BITS) {
            rc5_handle_bits(di, s);
            s->num_edges = 0;
            s->num_bits = 0;
            s->state = STATE_IDLE;
        }

        s->next_edge_is_low = ir ? 1 : 0;
    }
}

static void rc5_destroy(struct srd_decoder_inst* di)
{
    void* priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder ir_rc5_c_decoder = {
    .id = "ir_rc5_c",
    .name = "IR RC-5(C)",
    .longname = "IR RC-5(C)",
    .desc = "RC-5 infrared remote control protocol (C implementation)",
    .license = "gplv2+",
    .channels = rc5_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = rc5_options_arr,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = rc5_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = rc5_ann_rows,
    .inputs = rc5_inputs,
    .num_inputs = 1,
    .outputs = rc5_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = rc5_tags,
    .num_tags = 1,
    .reset = rc5_reset,
    .start = rc5_start,
    .decode = rc5_decode,
    .end = NULL,
    .metadata = rc5_metadata,
    .destroy = rc5_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder* srd_c_decoder_entry(void)
{
    GVariant* polarity_vals[] = {
        g_variant_new_string("active-low"),
        g_variant_new_string("active-high"),
    };
    GSList* polarity_list = NULL;
    polarity_list = g_slist_append(polarity_list, polarity_vals[0]);
    polarity_list = g_slist_append(polarity_list, polarity_vals[1]);
    rc5_options_arr[0].id = "polarity";
    rc5_options_arr[0].idn = "dec_ir_rc5_opt_polarity";
    rc5_options_arr[0].desc = "Polarity";
    rc5_options_arr[0].def = g_variant_new_string("active-low");
    rc5_options_arr[0].values = polarity_list;

    GVariant* protocol_vals[] = {
        g_variant_new_string("standard"),
        g_variant_new_string("extended"),
    };
    GSList* protocol_list = NULL;
    protocol_list = g_slist_append(protocol_list, protocol_vals[0]);
    protocol_list = g_slist_append(protocol_list, protocol_vals[1]);
    rc5_options_arr[1].id = "protocol";
    rc5_options_arr[1].idn = "dec_ir_rc5_opt_protocol";
    rc5_options_arr[1].desc = "Protocol type";
    rc5_options_arr[1].def = g_variant_new_string("standard");
    rc5_options_arr[1].values = protocol_list;

    return &ir_rc5_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
