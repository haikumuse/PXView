#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_BIT_VAL = 0,
    ANN_BIT_NUM,
    ANN_FRAME,
    ANN_FRAME_IDLE,
    ANN_FRAME_ERROR,
    ANN_DECODE,
    NUM_ANN,
};

enum mdio_state {
    STATE_PRE = 0,
    STATE_ST,
    STATE_OP,
    STATE_PRTAD,
    STATE_DEVAD,
    STATE_TA,
    STATE_DATA,
};

#define CH_MDC  0
#define CH_MDIO 1

struct mdio_priv {
    int state;
    int bitcount;
    int opcode;
    int clause45;
    int clause45_addr;
    int portad;
    int portad_bits;
    int devad;
    int devad_bits;
    int data;
    int data_bits;
    int ta_invalid;
    int op_invalid;
    int is_read;
    int preamble_len;
    uint64_t ss_frame;
    uint64_t ss_frame_field;
    int out_ann;
    int show_debug_bits;
    int read_edge_falling;
    int illegal_bus;
    uint64_t ss_illegal;
    uint64_t ss_bit;
    uint64_t prev_samplenum;
    int mdiobits_head_mdio;
    uint64_t mdiobits_head_ss;
    uint64_t mdiobits_head_es;
    uint64_t cycle_lengths[48];
    int num_cycle_lengths;
};

static struct srd_channel mdio_channels[] = {
    {"mdc", "MDC", "Clock", 0, SRD_CHANNEL_SCLK, NULL},
    {"mdio", "MDIO", "Data", 1, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option mdio_options[] = {
    {"show_debug_bits", NULL, "Show debug bits", NULL, NULL},
    {"read_edge", NULL, "read edge", NULL, NULL},
};

static const char *mdio_ann_labels[][3] = {
    {"", "bit-val", "Bit value"},
    {"", "bit-num", "Bit number"},
    {"", "frame", "Frame"},
    {"", "frame-idle", "Bus idle state"},
    {"", "frame-error", "Frame error"},
    {"", "decode", "Decode"},
};

static const int mdio_row_bitval_classes[] = {ANN_BIT_VAL};
static const int mdio_row_bitnum_classes[] = {ANN_BIT_NUM};
static const int mdio_row_frame_classes[] = {ANN_FRAME, ANN_FRAME_IDLE};
static const int mdio_row_frame_error_classes[] = {ANN_FRAME_ERROR};
static const int mdio_row_decode_classes[] = {ANN_DECODE};
static const struct srd_c_ann_row mdio_ann_rows[] = {
    {"bit-val", "Bit value", mdio_row_bitval_classes, 1},
    {"bit-num", "Bit number", mdio_row_bitnum_classes, 1},
    {"frame", "Frame", mdio_row_frame_classes, 2},
    {"frame-error", "Frame error", mdio_row_frame_error_classes, 1},
    {"decode", "Decode", mdio_row_decode_classes, 1},
};

static const char *mdio_inputs[] = {"logic"};
static const char *mdio_outputs[] = {"mdio"};
static const char *mdio_tags[] = {"Networking"};

static void mdio_reset_state(struct mdio_priv *s)
{
    s->bitcount = -1;
    s->opcode = -1;
    s->clause45 = 0;
    s->ss_frame_field = (uint64_t)-1;
    s->preamble_len = 0;
    s->ta_invalid = -1;
    s->op_invalid = 0;
    s->portad = -1;
    s->portad_bits = 5;
    s->devad = -1;
    s->devad_bits = 5;
    s->data = -1;
    s->data_bits = 16;
    s->state = STATE_PRE;
    s->is_read = 1;
    s->num_cycle_lengths = 0;
}

static void mdio_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct mdio_priv)));
    }
    struct mdio_priv *s = (struct mdio_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct mdio_priv));
    s->clause45_addr = -1;
    s->ss_frame = (uint64_t)-1;
    s->ss_illegal = 0;
    s->illegal_bus = 0;
    mdio_reset_state(s);
}

static void mdio_start(struct srd_decoder_inst *di)
{
    struct mdio_priv *s = (struct mdio_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "mdio");

    const char *debug_str = c_decoder_get_option_string(di, "show_debug_bits", "no");
    s->show_debug_bits = (debug_str && strcmp(debug_str, "yes") == 0) ? 1 : 0;

    const char *edge_str = c_decoder_get_option_string(di, "read_edge", "falling");
    s->read_edge_falling = (edge_str && strcmp(edge_str, "falling") == 0) ? 1 : 0;
}

static void mdio_putbit(struct srd_decoder_inst *di, struct mdio_priv *s,
                         int mdio, uint64_t ss, uint64_t es)
{
    char val_str[4];
    snprintf(val_str, sizeof(val_str), "%d", mdio);
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_BIT_VAL, val_str);

    if (s->show_debug_bits) {
        char num_str[16];
        char num_short[4];
        snprintf(num_str, sizeof(num_str), "%d", s->bitcount - 1);
        snprintf(num_short, sizeof(num_short), "%d", (s->bitcount - 1) % 10);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_BIT_NUM, num_str, num_short);
    }
}



static uint64_t mdio_quartile_cycle_length(struct mdio_priv *s)
{
    if (s->num_cycle_lengths < 1)
        return 1;
    int count = s->num_cycle_lengths;
    uint64_t sorted[48];
    memcpy(sorted, s->cycle_lengths, sizeof(uint64_t) * count);
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (sorted[j] < sorted[i]) {
                uint64_t tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }
    int idx = count / 4;
    if (idx < 0) idx = 0;
    if (idx >= count) idx = count - 1;
    return sorted[idx] > 0 ? sorted[idx] : 1;
}

static void mdio_putdata(struct srd_decoder_inst *di, struct mdio_priv *s)
{
    char data_str[32];
    snprintf(data_str, sizeof(data_str), "DATA: %04X", s->data);
    C_ANN_PUT(di, s->ss_frame_field, s->mdiobits_head_es, s->out_ann, ANN_FRAME,
               data_str, "DATA", "D");

    if (s->clause45 && s->opcode == 0) {
        s->clause45_addr = s->data;
    }

    if (s->opcode > 0 || !s->clause45) {
        char decoded_min[128] = {0};
        int pos = 0;

        if (s->clause45 && s->clause45_addr != -1) {
            pos += snprintf(decoded_min + pos, sizeof(decoded_min) - pos,
                            "ADDR: %04X ", s->clause45_addr);
        } else if (s->clause45) {
            pos += snprintf(decoded_min + pos, sizeof(decoded_min) - pos,
                            "ADDR: UKWN ");
        }

        int is_read = 0;
        if ((s->clause45 && s->opcode > 1) || (!s->clause45 && s->opcode)) {
            pos += snprintf(decoded_min + pos, sizeof(decoded_min) - pos,
                            "READ:  %04X", s->data);
            is_read = 1;
        } else {
            pos += snprintf(decoded_min + pos, sizeof(decoded_min) - pos,
                            "WRITE: %04X", s->data);
            is_read = 0;
        }

        char decoded_ext[128] = {0};
        int epos = 0;
        epos += snprintf(decoded_ext + epos, sizeof(decoded_ext) - epos,
                         " %s: %02d",
                         s->clause45 ? "PRTAD" : "PHYAD", s->portad);
        epos += snprintf(decoded_ext + epos, sizeof(decoded_ext) - epos,
                         " %s: %02d",
                         s->clause45 ? "DEVAD" : "REGAD", s->devad);
        if (s->ta_invalid || s->op_invalid) {
            epos += snprintf(decoded_ext + epos, sizeof(decoded_ext) - epos, " ERROR");
        }

        char full[256];
        snprintf(full, sizeof(full), "%s%s", decoded_min, decoded_ext);
        C_ANN_PUT(di, s->ss_frame, s->mdiobits_head_es, s->out_ann, ANN_DECODE,
                   full, decoded_min);
    }

    if (s->clause45 && s->opcode == 2 && s->clause45_addr != -1) {
        s->clause45_addr += 1;
    }
}

static void mdio_state_PRE(struct srd_decoder_inst *di, struct mdio_priv *s, int mdio, uint64_t samplenum)
{
    if (s->illegal_bus) {
        if (mdio == 0) {
            return;
        } else {
            s->illegal_bus = 0;
            C_ANN_PUT(di, s->ss_illegal, samplenum, s->out_ann, ANN_FRAME_ERROR,
                       "ILLEGAL BUS STATE", "ILL");
            s->ss_frame = samplenum;
        }
    }

    if (s->ss_frame == (uint64_t)-1) {
        s->ss_frame = samplenum;
    }

    if (mdio == 1) {
        s->preamble_len += 1;
    }

    if (s->preamble_len > 16) {
        if (mdio == 0) {
            if (s->preamble_len < 32) {
                s->ss_frame = samplenum;
                C_ANN_PUT(di, s->ss_frame, samplenum, s->out_ann, ANN_FRAME_ERROR,
                           "SHORT PREAMBLE", "SHRT PRE");
            } else if (s->preamble_len > 32) {
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_FRAME_IDLE,
                           "IDLE", "I");
                s->preamble_len = 32;
            }
            char pre_str[32];
            snprintf(pre_str, sizeof(pre_str), "PRE #%d", s->preamble_len);
            C_ANN_PUT(di, s->ss_frame, samplenum, s->out_ann, ANN_FRAME,
                       pre_str, "PRE", "P");
            s->ss_frame_field = samplenum;
            s->state = STATE_ST;
        }
    } else if (mdio == 0) {
        s->ss_illegal = s->ss_frame;
        s->illegal_bus = 1;
    }
}

static void mdio_state_ST(struct srd_decoder_inst *di, struct mdio_priv *s, int mdio, uint64_t samplenum)
{
    if (mdio == 0) {
        s->clause45 = 1;
    }
    s->state = STATE_OP;
}

static void mdio_state_OP(struct srd_decoder_inst *di, struct mdio_priv *s, int mdio, uint64_t samplenum)
{
    if (s->opcode == -1) {
        if (s->clause45) {
            C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME, "ST (Clause 45)", "ST 45", "ST", "S");
        } else {
            C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME, "ST (Clause 22)", "ST 22", "ST", "S");
        }
        s->ss_frame_field = samplenum;

        if (mdio) {
            s->opcode = 2;
        } else {
            s->opcode = 0;
        }
    } else {
        if (s->clause45) {
            s->opcode += mdio;
            s->state = STATE_PRTAD;
        } else {
            if (mdio == s->opcode) {
                s->op_invalid = 1;
            }
            s->state = STATE_PRTAD;
        }
    }
}

static void mdio_state_PRTAD(struct srd_decoder_inst *di, struct mdio_priv *s, int mdio, uint64_t samplenum)
{
    if (s->portad == -1) {
        s->portad = 0;
        const char *op_long = "";
        const char *op_short = "";
        if (s->clause45) {
            if (s->opcode == 0) { op_long = "OP: ADDR"; op_short = "OP: A"; }
            else if (s->opcode == 1) { op_long = "OP: WRITE"; op_short = "OP: W"; s->is_read = 0; }
            else if (s->opcode == 2) { op_long = "OP: READINC"; op_short = "OP: RI"; s->is_read = 1; }
            else if (s->opcode == 3) { op_long = "OP: READ"; op_short = "OP: R"; s->is_read = 1; }
        } else {
            if (s->opcode) { op_long = "OP: READ"; op_short = "OP: R"; s->is_read = 1; }
            else { op_long = "OP: WRITE"; op_short = "OP: W"; s->is_read = 0; }
        }
        C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME, op_long, op_short, "OP", "O");
        if (s->op_invalid) {
            C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME_ERROR, "OP invalid for Clause 22", "OP", "O");
        }
        s->ss_frame_field = samplenum;
    }

    s->portad_bits--;
    s->portad |= mdio << s->portad_bits;
    if (!s->portad_bits) {
        s->state = STATE_DEVAD;
    }
}

static void mdio_state_DEVAD(struct srd_decoder_inst *di, struct mdio_priv *s, int mdio, uint64_t samplenum)
{
    if (s->devad == -1) {
        s->devad = 0;
        char prtad_str[32];
        if (s->clause45) {
            snprintf(prtad_str, sizeof(prtad_str), "PRTAD: %02d", s->portad);
            C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME, prtad_str, "PRT", "P");
        } else {
            snprintf(prtad_str, sizeof(prtad_str), "PHYAD: %02d", s->portad);
            C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME, prtad_str, "PHY", "P");
        }
        s->ss_frame_field = samplenum;
    }
    s->devad_bits--;
    s->devad |= mdio << s->devad_bits;
    if (!s->devad_bits) {
        s->state = STATE_TA;
    }
}

static void mdio_state_TA(struct srd_decoder_inst *di, struct mdio_priv *s, int mdio, uint64_t samplenum)
{
    if (s->ta_invalid == -1) {
        s->ta_invalid = 0;
        char regad_str[32];
        if (s->clause45) {
            snprintf(regad_str, sizeof(regad_str), "DEVAD: %02d", s->devad);
            C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME, regad_str, "DEV", "D");
        } else {
            snprintf(regad_str, sizeof(regad_str), "REGAD: %02d", s->devad);
            C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME, regad_str, "REG", "R");
        }
        s->ss_frame_field = samplenum;
        if (mdio != 1 && ((s->clause45 && s->opcode < 2) || (!s->clause45 && s->opcode == 0))) {
            s->ta_invalid = 1;
        }
    } else {
        if (mdio != 0) {
            s->ta_invalid = 2;
        }
        s->state = STATE_DATA;
    }
}

static void mdio_state_DATA(struct srd_decoder_inst *di, struct mdio_priv *s, int mdio, uint64_t samplenum)
{
    if (s->data == -1) {
        s->data = 0;
        C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME, "TA", "T");
        if (s->ta_invalid) {
            if (s->ta_invalid == 2) {
                C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME_ERROR, "TA invalid (bit1 and bit2)", "TA", "T");
            } else {
                C_ANN_PUT(di, s->ss_frame_field, s->prev_samplenum, s->out_ann, ANN_FRAME_ERROR, "TA invalid (bit1)", "TA", "T");
            }
        }
        s->ss_frame_field = samplenum;
    }
    s->data_bits--;
    s->data |= mdio << s->data_bits;
    if (!s->data_bits) {
        s->mdiobits_head_es = s->mdiobits_head_ss + mdio_quartile_cycle_length(s);
        s->bitcount++;
        mdio_putbit(di, s, s->mdiobits_head_mdio, s->mdiobits_head_ss, s->mdiobits_head_es);
        mdio_putdata(di, s);
        mdio_reset_state(s);
    }
}

static void mdio_handle_bit(struct srd_decoder_inst *di, struct mdio_priv *s,
                             int mdio, uint64_t samplenum)
{
    if (s->bitcount > 0 && s->num_cycle_lengths < 48) {
        s->cycle_lengths[s->num_cycle_lengths++] = samplenum - s->mdiobits_head_ss;
    }

    if (s->bitcount >= 0) {
        mdio_putbit(di, s, s->mdiobits_head_mdio, s->mdiobits_head_ss, samplenum);
    }

    s->mdiobits_head_mdio = mdio;
    s->mdiobits_head_ss = samplenum;
    s->bitcount++;

    s->prev_samplenum = samplenum;

    switch (s->state) {
    case STATE_PRE:
        mdio_state_PRE(di, s, mdio, samplenum);
        break;
    case STATE_ST:
        mdio_state_ST(di, s, mdio, samplenum);
        break;
    case STATE_OP:
        mdio_state_OP(di, s, mdio, samplenum);
        break;
    case STATE_PRTAD:
        mdio_state_PRTAD(di, s, mdio, samplenum);
        break;
    case STATE_DEVAD:
        mdio_state_DEVAD(di, s, mdio, samplenum);
        break;
    case STATE_TA:
        mdio_state_TA(di, s, mdio, samplenum);
        break;
    case STATE_DATA:
        mdio_state_DATA(di, s, mdio, samplenum);
        break;
    }
}

static void mdio_decode(struct srd_decoder_inst *di)
{
    struct mdio_priv *s = (struct mdio_priv *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;
    int use_falling = 0;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        if (use_falling) {
            c_cond_fall(cb, CH_MDC);
        } else {
            c_cond_rise(cb, CH_MDC);
        }
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int mdio = c_decoder_get_pin(di, CH_MDIO, samplenum);
        mdio_handle_bit(di, s, mdio, samplenum);

        if (s->state == STATE_DATA && s->is_read && s->read_edge_falling) {
            use_falling = 1;
        } else {
            use_falling = 0;
        }
    }
}

static void mdio_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder mdio_c_decoder = {
    .id = "mdio_c",
    .name = "MDIO(C)",
    .longname = "Management Data Input/Output (C)",
    .desc = "MII management bus between MAC and PHY (C implementation)",
    .license = "bsd",
    .channels = mdio_channels,
    .num_channels = 2,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = mdio_options,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = mdio_ann_labels,
    .num_annotation_rows = 5,
    .annotation_rows = mdio_ann_rows,
    .inputs = mdio_inputs,
    .num_inputs = 1,
    .outputs = mdio_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = mdio_tags,
    .num_tags = 1,
    .reset = mdio_reset,
    .start = mdio_start,
    .decode = mdio_decode,
    .destroy = mdio_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    mdio_options[0].def = g_variant_new_string("no");
    mdio_options[1].def = g_variant_new_string("falling");
    return &mdio_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
