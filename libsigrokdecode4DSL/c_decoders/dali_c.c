#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum dali_state {
    STATE_IDLE,
    STATE_PHASE0,
    STATE_PHASE1,
};

enum dali_ann {
    ANN_BIT = 0,
    ANN_STARTBIT,
    ANN_SBIT,
    ANN_YBIT,
    ANN_ADDRESS,
    ANN_COMMAND,
    ANN_REPLY,
    ANN_RAW,
    NUM_ANN,
};

#define DALI_CH 0
#define MAX_EDGES 40
#define MAX_BITS 17

struct dali_priv {
    enum dali_state state;
    uint64_t samplerate;
    uint64_t halfbit;
    int old_dali;
    int phase0;
    uint64_t edges[MAX_EDGES];
    int num_edges;
    uint64_t bits_samplenum[MAX_BITS];
    int bits_value[MAX_BITS];
    int num_bits;
    uint64_t ss_es_bits[MAX_BITS][2];
    int num_ss_es_bits;
    int dev_type;
    int polarity_invert;
    int out_ann;
};

static struct srd_channel dali_channels[] = {
    {"dali", "DALI", "DALI data line", 0, SRD_CHANNEL_SDATA, "dec_dali_chan_dali"},
};

static struct srd_decoder_option dali_options_arr[] = {
    {
        .id = "polarity",
        .idn = "dec_dali_opt_polarity",
        .desc = "Polarity",
        .def = NULL,
        .values = NULL,
    },
};

static const char *dali_ann_labels[][3] = {
    {"", "bit", "Bit"},
    {"", "startbit", "Startbit"},
    {"", "sbit", "Select bit"},
    {"", "ybit", "Individual or group"},
    {"", "address", "Address"},
    {"", "command", "Command"},
    {"", "reply", "Reply data"},
    {"", "raw", "Raw data"},
};

static const int dali_row_bits_classes[] = {ANN_BIT, -1};
static const int dali_row_raw_classes[] = {ANN_RAW, -1};
static const int dali_row_fields_classes[] = {ANN_STARTBIT, ANN_SBIT, ANN_YBIT, ANN_ADDRESS, ANN_COMMAND, ANN_REPLY, -1};
static const struct srd_c_ann_row dali_ann_rows[] = {
    {"bits", "Bits", dali_row_bits_classes, 1},
    {"raw", "Raw data", dali_row_raw_classes, 1},
    {"fields", "Fields", dali_row_fields_classes, 6},
};

static const char *dali_inputs[] = {"logic", NULL};
static const char *dali_outputs[] = {NULL};
static const char *dali_tags[] = {"Embedded/industrial", "Lighting", NULL};

static const char *extended_cmd_lookup(uint8_t addr)
{
    switch (addr) {
    case 0xA1: return "Terminate";
    case 0xA3: return "DTR";
    case 0xA5: return "INIT";
    case 0xA7: return "RAND";
    case 0xA9: return "COMP";
    case 0xAB: return "WDRAW";
    case 0xB1: return "SAH";
    case 0xB3: return "SAM";
    case 0xB5: return "SAL";
    case 0xB7: return "ProgSA";
    case 0xB9: return "VfySA";
    case 0xBB: return "QryShort";
    case 0xBD: return "PysSel";
    case 0xC1: return "EnTyp";
    case 0xC3: return "DTR1";
    case 0xC5: return "DTR2";
    case 0xC7: return "WRI";
    default: return "Unknown";
    }
}

static void dali_putb(struct srd_decoder_inst *di, struct dali_priv *s,
                       int bit1, int bit2, int ann_class, const char *txt1,
                       const char *txt2, const char *txt3, const char *txt4,
                       const char *txt5)
{
    uint64_t ss = s->ss_es_bits[bit1][0];
    uint64_t es = s->ss_es_bits[bit2][1];
    C_ANN_PUT(di, ss, es, s->out_ann, ann_class, txt1, txt2, txt3, txt4, txt5);
}

static void dali_handle_bits(struct srd_decoder_inst *di, struct dali_priv *s, int length)
{
    int i;
    uint8_t f = 0, c = 0;
    int b[MAX_BITS];

    for (i = 0; i < length; i++)
        b[i] = s->bits_value[i];

    for (i = 0; i < length; i++) {
        uint64_t ss;
        if (i == 0)
            ss = s->bits_samplenum[0] > 0 ? s->bits_samplenum[0] : 0;
        else
            ss = s->ss_es_bits[i - 1][1];
        uint64_t es = s->bits_samplenum[i] + s->halfbit * 2;
        s->ss_es_bits[i][0] = ss;
        s->ss_es_bits[i][1] = es;

        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", b[i]);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_BIT, bit_str);
    }
    s->num_ss_es_bits = length;

    {
        char st_long[32], st_mid[16], st_short[8], st_tiny[4], st_min[4];
        snprintf(st_long, sizeof(st_long), "Startbit: %d", b[0]);
        snprintf(st_mid, sizeof(st_mid), "ST: %d", b[0]);
        snprintf(st_short, sizeof(st_short), "ST");
        snprintf(st_tiny, sizeof(st_tiny), "S");
        snprintf(st_min, sizeof(st_min), "S");
        dali_putb(di, s, 0, 0, ANN_STARTBIT, st_long, st_mid, st_short, st_tiny, st_min);
        dali_putb(di, s, 0, 0, ANN_RAW, st_long, st_mid, st_short, st_tiny, st_min);
    }

    for (i = 0; i < 8; i++)
        f |= (b[1 + i] << (7 - i));

    if (length == 9) {
        char r_long[32], r_mid[24], r_short[16], r_tiny[8], r_min[4];
        snprintf(r_long, sizeof(r_long), "Reply: %02X", f);
        snprintf(r_mid, sizeof(r_mid), "Rply: %02X", f);
        snprintf(r_short, sizeof(r_short), "Rep: %02X", f);
        snprintf(r_tiny, sizeof(r_tiny), "R: %02X", f);
        snprintf(r_min, sizeof(r_min), "R");
        dali_putb(di, s, 1, 8, ANN_RAW, r_long, r_mid, r_short, r_tiny, r_min);

        char rd_long[32], rd_mid[24], rd_short[16], rd_tiny[8], rd_min[4];
        snprintf(rd_long, sizeof(rd_long), "Reply: %d", f);
        snprintf(rd_mid, sizeof(rd_mid), "Rply: %d", f);
        snprintf(rd_short, sizeof(rd_short), "Rep: %d", f);
        snprintf(rd_tiny, sizeof(rd_tiny), "R: %d", f);
        snprintf(rd_min, sizeof(rd_min), "R");
        dali_putb(di, s, 1, 8, ANN_REPLY, rd_long, rd_mid, rd_short, rd_tiny, rd_min);
        return;
    }

    for (i = 0; i < 8; i++)
        c |= (b[9 + i] << (7 - i));

    {
        char raw_long[32], raw_mid[24], raw_short[16], raw_tiny[8], raw_min[4];
        snprintf(raw_long, sizeof(raw_long), "Raw data: %02X", f);
        snprintf(raw_mid, sizeof(raw_mid), "Raw: %02X", f);
        snprintf(raw_short, sizeof(raw_short), "Raw: %02X", f);
        snprintf(raw_tiny, sizeof(raw_tiny), "R: %02X", f);
        snprintf(raw_min, sizeof(raw_min), "R");
        dali_putb(di, s, 1, 8, ANN_RAW, raw_long, raw_mid, raw_short, raw_tiny, raw_min);
    }

    {
        char raw_long[32], raw_mid[24], raw_short[16], raw_tiny[8], raw_min[4];
        snprintf(raw_long, sizeof(raw_long), "Raw data: %02X", c);
        snprintf(raw_mid, sizeof(raw_mid), "Raw: %02X", c);
        snprintf(raw_short, sizeof(raw_short), "Raw: %02X", c);
        snprintf(raw_tiny, sizeof(raw_tiny), "R: %02X", c);
        snprintf(raw_min, sizeof(raw_min), "R");
        dali_putb(di, s, 9, 16, ANN_RAW, raw_long, raw_mid, raw_short, raw_tiny, raw_min);
    }

    if (b[8] == 1) {
        dali_putb(di, s, 8, 8, ANN_SBIT, "Command", "Comd", "COM", "CO", "C");
    } else {
        dali_putb(di, s, 8, 8, ANN_SBIT, "Arc Power Level", "Arc Pwr", "ARC", "AC", "A");
    }

    if (f >= 254) {
        dali_putb(di, s, 1, 7, ANN_ADDRESS, "BROADCAST", "Brdcast", "BC", "B", "B");
    } else if (f >= 160) {
        if (f == 0xC1)
            s->dev_type = -1;
        const char *xc_name = extended_cmd_lookup(f);
        char xc_long[64], xc_mid[48], xc_short[24], xc_tiny[16], xc_min[4];
        snprintf(xc_long, sizeof(xc_long), "Extended Command: %02X (%s)", f, xc_name);
        snprintf(xc_mid, sizeof(xc_mid), "XC: %02X (%s)", f, xc_name);
        snprintf(xc_short, sizeof(xc_short), "XC: %02X", f);
        snprintf(xc_tiny, sizeof(xc_tiny), "X: %02X", f);
        snprintf(xc_min, sizeof(xc_min), "X");
        dali_putb(di, s, 1, 8, ANN_ADDRESS, xc_long, xc_mid, xc_short, xc_tiny, xc_min);
    } else if (f >= 128) {
        {
            char yb_long[24], yb_mid[16], yb_short[8], yb_tiny[4], yb_min[4];
            snprintf(yb_long, sizeof(yb_long), "YBit: %d", b[1]);
            snprintf(yb_mid, sizeof(yb_mid), "YB: %d", b[1]);
            snprintf(yb_short, sizeof(yb_short), "YB");
            snprintf(yb_tiny, sizeof(yb_tiny), "Y");
            snprintf(yb_min, sizeof(yb_min), "Y");
            dali_putb(di, s, 1, 1, ANN_YBIT, yb_long, yb_mid, yb_short, yb_tiny, yb_min);
        }
        {
            int g = (f & 127) >> 1;
            char ga_long[32], ga_mid[24], ga_short[16], ga_tiny[8], ga_min[4];
            snprintf(ga_long, sizeof(ga_long), "Group address: %d", g);
            snprintf(ga_mid, sizeof(ga_mid), "Group: %d", g);
            snprintf(ga_short, sizeof(ga_short), "GP: %d", g);
            snprintf(ga_tiny, sizeof(ga_tiny), "G: %d", g);
            snprintf(ga_min, sizeof(ga_min), "G");
            dali_putb(di, s, 2, 7, ANN_ADDRESS, ga_long, ga_mid, ga_short, ga_tiny, ga_min);
        }
    } else {
        {
            char yb_long[24], yb_mid[16], yb_short[8], yb_tiny[4], yb_min[4];
            snprintf(yb_long, sizeof(yb_long), "YBit: %d", b[1]);
            snprintf(yb_mid, sizeof(yb_mid), "YB: %d", b[1]);
            snprintf(yb_short, sizeof(yb_short), "YB");
            snprintf(yb_tiny, sizeof(yb_tiny), "Y");
            snprintf(yb_min, sizeof(yb_min), "Y");
            dali_putb(di, s, 1, 1, ANN_YBIT, yb_long, yb_mid, yb_short, yb_tiny, yb_min);
        }
        {
            int a = f >> 1;
            char sa_long[32], sa_mid[24], sa_short[16], sa_tiny[8], sa_min[4];
            snprintf(sa_long, sizeof(sa_long), "Short address: %d", a);
            snprintf(sa_mid, sizeof(sa_mid), "Addr: %d", a);
            snprintf(sa_short, sizeof(sa_short), "Addr: %d", a);
            snprintf(sa_tiny, sizeof(sa_tiny), "A: %d", a);
            snprintf(sa_min, sizeof(sa_min), "A");
            dali_putb(di, s, 2, 7, ANN_ADDRESS, sa_long, sa_mid, sa_short, sa_tiny, sa_min);
        }
    }

    if (f >= 160 && f < 254) {
        if (s->dev_type == -1) {
            s->dev_type = c;
            char t_long[24], t_mid[16], t_short[16], t_tiny[8], t_min[4];
            snprintf(t_long, sizeof(t_long), "Type: %d", c);
            snprintf(t_mid, sizeof(t_mid), "Typ: %d", c);
            snprintf(t_short, sizeof(t_short), "Typ: %d", c);
            snprintf(t_tiny, sizeof(t_tiny), "T: %d", c);
            snprintf(t_min, sizeof(t_min), "D");
            dali_putb(di, s, 9, 16, ANN_COMMAND, t_long, t_mid, t_short, t_tiny, t_min);
        } else {
            s->dev_type = 0;
            char d_long[24], d_mid[16], d_short[16], d_tiny[8], d_min[4];
            snprintf(d_long, sizeof(d_long), "Data: %d", c);
            snprintf(d_mid, sizeof(d_mid), "Dat: %d", c);
            snprintf(d_short, sizeof(d_short), "Dat: %d", c);
            snprintf(d_tiny, sizeof(d_tiny), "D: %d", c);
            snprintf(d_min, sizeof(d_min), "D");
            dali_putb(di, s, 9, 16, ANN_COMMAND, d_long, d_mid, d_short, d_tiny, d_min);
        }
    } else if (b[8] == 1) {
        char cmd_long[32], cmd_mid[24], cmd_short[16], cmd_tiny[8], cmd_min[4];
        snprintf(cmd_long, sizeof(cmd_long), "Command: %d", c);
        snprintf(cmd_mid, sizeof(cmd_mid), "Com: %d", c);
        snprintf(cmd_short, sizeof(cmd_short), "Com: %d", c);
        snprintf(cmd_tiny, sizeof(cmd_tiny), "C: %d", c);
        snprintf(cmd_min, sizeof(cmd_min), "C");
        dali_putb(di, s, 9, 16, ANN_COMMAND, cmd_long, cmd_mid, cmd_short, cmd_tiny, cmd_min);
    } else {
        char arc_long[32], arc_mid[24], arc_short[16], arc_tiny[8], arc_min[4];
        snprintf(arc_long, sizeof(arc_long), "Arc Power Level: %d", c);
        snprintf(arc_mid, sizeof(arc_mid), "Level: %d", c);
        snprintf(arc_short, sizeof(arc_short), "Lev: %d", c);
        snprintf(arc_tiny, sizeof(arc_tiny), "L: %d", c);
        snprintf(arc_min, sizeof(arc_min), "L");
        dali_putb(di, s, 9, 16, ANN_COMMAND, arc_long, arc_mid, arc_short, arc_tiny, arc_min);
    }
}

static void dali_reset_decoder_state(struct dali_priv *s)
{
    s->num_edges = 0;
    s->num_bits = 0;
    s->num_ss_es_bits = 0;
    s->state = STATE_IDLE;
}

static void dali_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct dali_priv)));
    }
    struct dali_priv *s = (struct dali_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct dali_priv));
    s->state = STATE_IDLE;
    s->dev_type = 0;
}

static void dali_start(struct srd_decoder_inst *di)
{
    struct dali_priv *s = (struct dali_priv *)c_decoder_get_private(di);

    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "dali");

    const char *polarity = c_decoder_get_option_string(di, "polarity", "active-low");
    if (polarity && strcmp(polarity, "active-high") == 0) {
        s->old_dali = 0;
        s->polarity_invert = 1;
    } else {
        s->old_dali = 1;
        s->polarity_invert = 0;
    }

    s->samplerate = c_decoder_get_samplerate(di);
    if (s->samplerate > 0)
        s->halfbit = (uint64_t)((s->samplerate * 0.0008333) / 2.0);
}

static void dali_decode(struct srd_decoder_inst *di)
{
    struct dali_priv *s = (struct dali_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;

    if (s->samplerate == 0)
        return;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int dali = c_decoder_get_pin(di, DALI_CH, samplenum);

        if (s->polarity_invert)
            dali ^= 1;

        if (s->state == STATE_IDLE) {
            if (s->old_dali == dali)
                continue;
            if (s->num_edges < MAX_EDGES)
                s->edges[s->num_edges++] = samplenum;
            s->state = STATE_PHASE0;
            s->old_dali = dali;
            continue;
        }

        if (s->old_dali != dali) {
            if (s->num_edges < MAX_EDGES)
                s->edges[s->num_edges++] = samplenum;
        } else if (samplenum == (s->edges[s->num_edges - 1] + (uint64_t)(s->halfbit * 1.5))) {
            if (s->num_edges < MAX_EDGES)
                s->edges[s->num_edges++] = samplenum - (uint64_t)(s->halfbit * 0.5);
        } else {
            continue;
        }

        int bit = s->old_dali;

        if (s->state == STATE_PHASE0) {
            s->phase0 = bit;
            s->state = STATE_PHASE1;
        } else if (s->state == STATE_PHASE1) {
            if (bit == 1 && s->phase0 == 1) {
                if (s->num_bits == 17 || s->num_bits == 9)
                    dali_handle_bits(di, s, s->num_bits);
                dali_reset_decoder_state(s);
                s->old_dali = dali;
                continue;
            } else {
                if (s->num_bits < MAX_BITS) {
                    s->bits_samplenum[s->num_bits] = s->edges[s->num_edges - 3];
                    s->bits_value[s->num_bits] = bit;
                    s->num_bits++;
                }
                s->state = STATE_PHASE0;
            }
        }

        s->old_dali = dali;
    }
}

static void dali_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder dali_c_decoder = {
    .id = "dali_c",
    .name = "DALI(C)",
    .longname = "Digital Addressable Lighting Interface (C)",
    .desc = "DALI protocol decoder (C implementation)",
    .license = "gplv2+",
    .channels = dali_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = dali_options_arr,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = dali_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = dali_ann_rows,
    .inputs = dali_inputs,
    .num_inputs = 1,
    .outputs = dali_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = dali_tags,
    .num_tags = 2,
    .reset = dali_reset,
    .start = dali_start,
    .decode = dali_decode,
    .destroy = dali_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    GVariant *vals[] = {
        g_variant_new_string("active-low"),
        g_variant_new_string("active-high"),
    };
    GSList *val_list = NULL;
    val_list = g_slist_append(val_list, vals[0]);
    val_list = g_slist_append(val_list, vals[1]);
    dali_options_arr[0].def = g_variant_new_string("active-low");
    dali_options_arr[0].values = val_list;
    return &dali_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
