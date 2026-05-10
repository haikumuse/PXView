#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum jtag_state {
    TEST_LOGIC_RESET = 0,
    RUN_TEST_IDLE = 1,
    SELECT_DR_SCAN = 2,
    CAPTURE_DR = 3,
    UPDATE_DR = 4,
    PAUSE_DR = 5,
    SHIFT_DR = 6,
    EXIT1_DR = 7,
    EXIT2_DR = 8,
    SELECT_IR_SCAN = 9,
    CAPTURE_IR = 10,
    UPDATE_IR = 11,
    PAUSE_IR = 12,
    SHIFT_IR = 13,
    EXIT1_IR = 14,
    EXIT2_IR = 15,
};

enum jtag_ann {
    ANN_TEST_LOGIC_RESET = 0,
    ANN_RUN_TEST_IDLE,
    ANN_SELECT_DR_SCAN,
    ANN_CAPTURE_DR,
    ANN_UPDATE_DR,
    ANN_PAUSE_DR,
    ANN_SHIFT_DR,
    ANN_EXIT1_DR,
    ANN_EXIT2_DR,
    ANN_SELECT_IR_SCAN,
    ANN_CAPTURE_IR,
    ANN_UPDATE_IR,
    ANN_PAUSE_IR,
    ANN_SHIFT_IR,
    ANN_EXIT1_IR,
    ANN_EXIT2_IR,
    ANN_BIT_TDI,
    ANN_BIT_TDO,
    ANN_BITSTRING_TDI,
    ANN_BITSTRING_TDO,
    NUM_ANN,
};

#define TDI 0
#define TDO 1
#define TCK 2
#define TMS 3
#define TRST 4
#define SRST 5
#define RTCK 6

struct jtag_priv {
    int state;
    int oldstate;
    int bits_tdi[256];
    int bits_tdo[256];
    int bits_cnt;
    uint64_t ss_bitstring;
    uint64_t ss_item;
    uint64_t es_item;
    gboolean first;
    gboolean first_shift_bit;
    gboolean data_ready;
    int out_ann;
    int out_python;
};

static const int next_state[16][2] = {
    {1, 0},
    {1, 2},
    {3, 9},
    {6, 7},
    {1, 2},
    {5, 8},
    {6, 7},
    {5, 4},
    {6, 4},
    {10, 0},
    {13, 14},
    {1, 2},
    {12, 15},
    {13, 14},
    {12, 11},
    {13, 11},
};

static struct srd_channel jtag_channels[] = {
    {"tdi", "TDI", "Test Data In", 0, SRD_CHANNEL_SDATA, NULL},
    {"tdo", "TDO", "Test Data Out", 1, SRD_CHANNEL_SDATA, NULL},
    {"tck", "TCK", "Test Clock", 2, SRD_CHANNEL_SCLK, NULL},
    {"tms", "TMS", "Test Mode Select", 3, SRD_CHANNEL_COMMON, NULL},
};

static struct srd_channel jtag_optional_channels[] = {
    {"trst", "TRST", "Test Reset", 4, SRD_CHANNEL_COMMON, NULL},
    {"srst", "SRST", "System Reset", 5, SRD_CHANNEL_COMMON, NULL},
    {"rtck", "RTCK", "Return Test Clock", 6, SRD_CHANNEL_SCLK, NULL},
};

static const char *jtag_ann_labels[][3] = {
    {"", "TLR", "TEST-LOGIC-RESET"},
    {"", "RTI", "RUN-TEST/IDLE"},
    {"", "SelDR", "SELECT-DR-SCAN"},
    {"", "CapDR", "CAPTURE-DR"},
    {"", "UpdDR", "UPDATE-DR"},
    {"", "PauDR", "PAUSE-DR"},
    {"", "ShfDR", "SHIFT-DR"},
    {"", "Ex1DR", "EXIT1-DR"},
    {"", "Ex2DR", "EXIT2-DR"},
    {"", "SelIR", "SELECT-IR-SCAN"},
    {"", "CapIR", "CAPTURE-IR"},
    {"", "UpdIR", "UPDATE-IR"},
    {"", "PauIR", "PAUSE-IR"},
    {"", "ShfIR", "SHIFT-IR"},
    {"", "Ex1IR", "EXIT1-IR"},
    {"", "Ex2IR", "EXIT2-IR"},
    {"", "TDI", "Bit (TDI)"},
    {"", "TDO", "Bit (TDO)"},
    {"", "TDI", "Bitstring (TDI)"},
    {"", "TDO", "Bitstring (TDO)"},
};

static const int jtag_row_bits_tdi_classes[] = {ANN_BIT_TDI};
static const int jtag_row_bits_tdo_classes[] = {ANN_BIT_TDO};
static const int jtag_row_bitstrings_tdi_classes[] = {ANN_BITSTRING_TDI};
static const int jtag_row_bitstrings_tdo_classes[] = {ANN_BITSTRING_TDO};
static const int jtag_row_states_classes[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

static const struct srd_c_ann_row jtag_ann_rows[] = {
    {"bits-tdi", "Bits (TDI)", jtag_row_bits_tdi_classes, 1},
    {"bits-tdo", "Bits (TDO)", jtag_row_bits_tdo_classes, 1},
    {"bitstrings-tdi", "Bitstrings (TDI)", jtag_row_bitstrings_tdi_classes, 1},
    {"bitstrings-tdo", "Bitstrings (TDO)", jtag_row_bitstrings_tdo_classes, 1},
    {"states", "States", jtag_row_states_classes, 16},
};

static const char *jtag_inputs[] = {"logic"};
static const char *jtag_outputs[] = {"jtag"};
static const char *jtag_tags[] = {"Embedded/industrial"};

static void jtag_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct jtag_priv)));
    }
    struct jtag_priv *priv = (struct jtag_priv *)c_decoder_get_private(di);
    memset(priv, 0, sizeof(struct jtag_priv));
    priv->state = RUN_TEST_IDLE;
    priv->oldstate = RUN_TEST_IDLE;
    priv->first = TRUE;
}

static void jtag_start(struct srd_decoder_inst *di)
{
    struct jtag_priv *priv = (struct jtag_priv *)c_decoder_get_private(di);
    priv->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "jtag");
    priv->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "jtag");
}

static void jtag_decode(struct srd_decoder_inst *di)
{
    struct jtag_priv *priv = (struct jtag_priv *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;
    uint64_t ss_state = 0;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_rise(cb, TCK);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        if (di->dec_num_channels > TRST && di->dec_channelmap[TRST] >= 0) {
            int trst = c_decoder_get_pin(di, TRST, samplenum);
            if (trst == 0) {
                if (priv->state != TEST_LOGIC_RESET) {
                    C_ANN_PUT(di, ss_state, samplenum, priv->out_ann, priv->state,
                              jtag_ann_labels[priv->state][1], jtag_ann_labels[priv->state][2]);
                    c_decoder_put_python(di, ss_state, samplenum, priv->out_python, "NEW STATE", NULL, 0);
                    ss_state = samplenum;
                }
                priv->oldstate = priv->state;
                priv->state = TEST_LOGIC_RESET;
                priv->bits_cnt = 0;
                priv->first = TRUE;
                priv->data_ready = FALSE;
                continue;
            }
        }

        int tms = c_decoder_get_pin(di, TMS, samplenum);
        int oldstate = priv->state;
        int newstate = next_state[oldstate][tms];

        if ((newstate == SHIFT_DR && oldstate != SHIFT_DR) ||
            (newstate == SHIFT_IR && oldstate != SHIFT_IR)) {
            priv->first_shift_bit = TRUE;
        }

        if (oldstate == SHIFT_DR || oldstate == SHIFT_IR) {
            if (priv->first_shift_bit) {
                priv->first_shift_bit = FALSE;
            } else {
                int tdi_val = c_decoder_get_pin(di, TDI, samplenum);
                int tdo_val = c_decoder_get_pin(di, TDO, samplenum);

                if (priv->first) {
                    priv->ss_bitstring = samplenum;
                    priv->ss_item = samplenum;
                    priv->first = FALSE;
                }

                if (priv->bits_cnt < 256) {
                    priv->bits_tdi[priv->bits_cnt] = tdi_val;
                    priv->bits_tdo[priv->bits_cnt] = tdo_val;
                }

                char tdi_str[4];
                char tdo_str[4];
                snprintf(tdi_str, sizeof(tdi_str), "%d", tdi_val);
                snprintf(tdo_str, sizeof(tdo_str), "%d", tdo_val);
                C_ANN_PUT(di, priv->ss_item, samplenum, priv->out_ann, ANN_BIT_TDI, tdi_str);
                C_ANN_PUT(di, priv->ss_item, samplenum, priv->out_ann, ANN_BIT_TDO, tdo_str);

                priv->es_item = samplenum;
                priv->ss_item = samplenum;
                priv->bits_cnt++;
                priv->data_ready = TRUE;
            }
        }

        if ((oldstate == SHIFT_DR && newstate == EXIT1_DR) ||
            (oldstate == SHIFT_IR && newstate == EXIT1_IR)) {
            if (priv->data_ready && priv->bits_cnt > 0) {
                char tdi_str[128];
                char tdo_str[128];
                int i;
                int cnt = priv->bits_cnt > 256 ? 256 : priv->bits_cnt;
                uint64_t tdi_val = 0, tdo_val = 0;
                for (i = 0; i < cnt; i++) {
                    tdi_val |= ((uint64_t)priv->bits_tdi[i] << i);
                    tdo_val |= ((uint64_t)priv->bits_tdo[i] << i);
                }
                const char *dr_ir = (oldstate == SHIFT_DR) ? "DR" : "IR";
                snprintf(tdi_str, sizeof(tdi_str), "%s TDI: (0x%llX), %d bits", dr_ir, (unsigned long long)tdi_val, cnt);
                snprintf(tdo_str, sizeof(tdo_str), "%s TDO: (0x%llX), %d bits", dr_ir, (unsigned long long)tdo_val, cnt);

                C_ANN_PUT(di, priv->ss_bitstring, samplenum, priv->out_ann, ANN_BITSTRING_TDI, tdi_str);
                C_ANN_PUT(di, priv->ss_bitstring, samplenum, priv->out_ann, ANN_BITSTRING_TDO, tdo_str);

                {
                    int is_ir = (oldstate == SHIFT_IR);
                    int byte_count = (cnt + 7) / 8;
                    unsigned char tdi_bytes[32];
                    unsigned char tdo_bytes[32];
                    memset(tdi_bytes, 0, sizeof(tdi_bytes));
                    memset(tdo_bytes, 0, sizeof(tdo_bytes));
                    for (i = 0; i < cnt; i++) {
                        if (priv->bits_tdi[i])
                            tdi_bytes[i / 8] |= (1 << (i % 8));
                        if (priv->bits_tdo[i])
                            tdo_bytes[i / 8] |= (1 << (i % 8));
                    }
                    c_decoder_put_python(di, priv->ss_bitstring, samplenum, priv->out_python,
                        is_ir ? "IR TDI" : "DR TDI", tdi_bytes, byte_count);
                    c_decoder_put_python(di, priv->ss_bitstring, samplenum, priv->out_python,
                        is_ir ? "IR TDO" : "DR TDO", tdo_bytes, byte_count);
                }
            }
            priv->bits_cnt = 0;
            priv->first = TRUE;
            priv->data_ready = FALSE;
        }

        if (newstate != oldstate) {
            C_ANN_PUT(di, ss_state, samplenum, priv->out_ann, oldstate,
                      jtag_ann_labels[oldstate][1], jtag_ann_labels[oldstate][2]);
            c_decoder_put_python(di, ss_state, samplenum, priv->out_python, "NEW STATE", NULL, 0);
            ss_state = samplenum;
        }

        priv->oldstate = oldstate;
        priv->state = newstate;
    }
}

static void jtag_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder jtag_c_decoder = {
    .id = "jtag_c",
    .name = "JTAG(C)",
    .longname = "Joint Test Action Group (C)",
    .desc = "JTAG protocol decoder (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = jtag_channels,
    .num_channels = 4,
    .optional_channels = jtag_optional_channels,
    .num_optional_channels = 3,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = jtag_ann_labels,
    .num_annotation_rows = 5,
    .annotation_rows = jtag_ann_rows,
    .inputs = jtag_inputs,
    .num_inputs = 1,
    .outputs = jtag_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = jtag_tags,
    .num_tags = 1,
    .reset = jtag_reset,
    .start = jtag_start,
    .decode = jtag_decode,
    .destroy = jtag_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &jtag_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
