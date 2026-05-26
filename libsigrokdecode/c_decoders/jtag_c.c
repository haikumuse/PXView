#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    uint64_t bits_ss[256];
    int bits_cnt;
    uint64_t ss_bitstring;
    uint64_t ss_item;
    uint64_t es_item;
    gboolean first;
    gboolean first_shift_bit;
    gboolean data_ready;
    int out_ann;
    int out_python;
    int last_bit_tdi;
    int last_bit_tdo;
    uint64_t last_bit_ss;
};

static const int next_state[16][2] = {
    { 1, 0 },
    { 1, 2 },
    { 3, 9 },
    { 6, 7 },
    { 1, 2 },
    { 5, 8 },
    { 6, 7 },
    { 5, 4 },
    { 6, 4 },
    { 10, 0 },
    { 13, 14 },
    { 1, 2 },
    { 12, 15 },
    { 13, 14 },
    { 12, 11 },
    { 13, 11 },
};

static const char* jtag_state_names[] = {
    "TEST-LOGIC-RESET",
    "RUN-TEST/IDLE",
    "SELECT-DR-SCAN",
    "CAPTURE-DR",
    "UPDATE-DR",
    "PAUSE-DR",
    "SHIFT-DR",
    "EXIT1-DR",
    "EXIT2-DR",
    "SELECT-IR-SCAN",
    "CAPTURE-IR",
    "UPDATE-IR",
    "PAUSE-IR",
    "SHIFT-IR",
    "EXIT1-IR",
    "EXIT2-IR",
};

static struct srd_channel jtag_channels[] = {
    { "tdi", "TDI", "Test data input", 0, SRD_CHANNEL_SDATA, "dec_jtag_chan_tdi" },
    { "tdo", "TDO", "Test data output", 1, SRD_CHANNEL_SDATA, "dec_jtag_chan_tdo" },
    { "tck", "TCK", "Test clock", 2, SRD_CHANNEL_SCLK, "dec_jtag_chan_tck" },
    { "tms", "TMS", "Test mode select", 3, SRD_CHANNEL_COMMON, "dec_jtag_chan_tms" },
};

static struct srd_channel jtag_optional_channels[] = {
    { "trst", "TRST#", "Test reset", 4, SRD_CHANNEL_COMMON, "dec_jtag_opt_chan_trst" },
    { "srst", "SRST#", "System reset", 5, SRD_CHANNEL_COMMON, "dec_jtag_opt_chan_srst" },
    { "rtck", "RTCK", "Return clock signal", 6, SRD_CHANNEL_SCLK, "dec_jtag_opt_chan_rtck" },
};

static const char* jtag_ann_labels[][3] = {
    { "", "test-logic-reset", "TEST-LOGIC-RESET" },
    { "", "run-test/idle", "RUN-TEST/IDLE" },
    { "", "select-dr-scan", "SELECT-DR-SCAN" },
    { "", "capture-dr", "CAPTURE-DR" },
    { "", "update-dr", "UPDATE-DR" },
    { "", "pause-dr", "PAUSE-DR" },
    { "", "shift-dr", "SHIFT-DR" },
    { "", "exit1-dr", "EXIT1-DR" },
    { "", "exit2-dr", "EXIT2-DR" },
    { "", "select-ir-scan", "SELECT-IR-SCAN" },
    { "", "capture-ir", "CAPTURE-IR" },
    { "", "update-ir", "UPDATE-IR" },
    { "", "pause-ir", "PAUSE-IR" },
    { "", "shift-ir", "SHIFT-IR" },
    { "", "exit1-ir", "EXIT1-IR" },
    { "", "exit2-ir", "EXIT2-IR" },
    { "", "bit-tdi", "Bit (TDI)" },
    { "", "bit-tdo", "Bit (TDO)" },
    { "", "bitstring-tdi", "Bitstring (TDI)" },
    { "", "bitstring-tdo", "Bitstring (TDO)" },
};

static const int jtag_row_bits_tdi_classes[] = { ANN_BIT_TDI, -1 };
static const int jtag_row_bits_tdo_classes[] = { ANN_BIT_TDO, -1 };
static const int jtag_row_bitstrings_tdi_classes[] = { ANN_BITSTRING_TDI, -1 };
static const int jtag_row_bitstrings_tdo_classes[] = { ANN_BITSTRING_TDO, -1 };
static const int jtag_row_states_classes[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1
};

static const struct srd_c_ann_row jtag_ann_rows[] = {
    { "bits-tdi", "Bits (TDI)", jtag_row_bits_tdi_classes, 1 },
    { "bits-tdo", "Bits (TDO)", jtag_row_bits_tdo_classes, 1 },
    { "bitstrings-tdi", "Bitstring (TDI)", jtag_row_bitstrings_tdi_classes, 1 },
    { "bitstrings-tdo", "Bitstring (TDO)", jtag_row_bitstrings_tdo_classes, 1 },
    { "states", "States", jtag_row_states_classes, 16 },
};

static const char* jtag_inputs[] = { "logic", NULL };
static const char* jtag_outputs[] = { "jtag", NULL };
static const char* jtag_tags[] = { "Debug/trace", NULL };

static void jtag_reset(struct srd_decoder_inst* di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct jtag_priv)));
    }
    struct jtag_priv* priv = (struct jtag_priv*)c_decoder_get_private(di);
    memset(priv, 0, sizeof(struct jtag_priv));
    priv->state = RUN_TEST_IDLE;
    priv->oldstate = RUN_TEST_IDLE;
    priv->first = TRUE;
}

static void jtag_start(struct srd_decoder_inst* di)
{
    struct jtag_priv* priv = (struct jtag_priv*)c_decoder_get_private(di);
    priv->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "jtag");
    priv->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "jtag");
}

static void jtag_decode(struct srd_decoder_inst* di)
{
    struct jtag_priv* priv = (struct jtag_priv*)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;
    uint64_t ss_state = 0;

    while (1) {
        srd_cond_builder* cb = c_cond_new();
        c_cond_rise(cb, TCK);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        if (c_decoder_has_channel(di, TRST)) {
            int trst = c_decoder_get_pin(di, TRST, samplenum);
            if (trst == 0) {
                if (priv->state != TEST_LOGIC_RESET) {
                    C_ANN_PUT(di, ss_state, samplenum, priv->out_ann, priv->state,
                        jtag_ann_labels[priv->state][2]);
                    const char* trst_state_name = jtag_state_names[TEST_LOGIC_RESET];
                    c_decoder_put_python(di, ss_state, samplenum, priv->out_python,
                        "NEW STATE", (const unsigned char*)trst_state_name, strlen(trst_state_name));
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
        gboolean shifting_to_exit1 = (oldstate == SHIFT_DR && newstate == EXIT1_DR) ||
                                     (oldstate == SHIFT_IR && newstate == EXIT1_IR);

        if ((newstate == SHIFT_DR && oldstate != SHIFT_DR) || (newstate == SHIFT_IR && oldstate != SHIFT_IR)) {
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
                    priv->bits_ss[priv->bits_cnt] = samplenum;
                }

                /* Fix #3: Defer last bit annotation when transitioning to EXIT1 */
                if (!shifting_to_exit1) {
                    char tdi_str[4];
                    char tdo_str[4];
                    snprintf(tdi_str, sizeof(tdi_str), "%d", tdi_val);
                    snprintf(tdo_str, sizeof(tdo_str), "%d", tdo_val);
                    C_ANN_PUT(di, priv->ss_item, samplenum, priv->out_ann, ANN_BIT_TDI, tdi_str);
                    C_ANN_PUT(di, priv->ss_item, samplenum, priv->out_ann, ANN_BIT_TDO, tdo_str);
                } else {
                    priv->last_bit_tdi = tdi_val;
                    priv->last_bit_tdo = tdo_val;
                    priv->last_bit_ss = samplenum;
                }

                priv->es_item = samplenum;
                priv->ss_item = samplenum;
                priv->bits_cnt++;
                priv->data_ready = TRUE;
            }
        }

        /* SHIFT->EXIT1 transition: defer bitstring/protocol output to EXIT1->next */
        if (shifting_to_exit1) {
            /* data_ready is already set in the SHIFT block above */
        }

        /* EXIT1/EXIT2 -> next transition: output deferred bitstring, protocol, and last bit */
        if ((oldstate == EXIT1_DR || oldstate == EXIT2_DR ||
             oldstate == EXIT1_IR || oldstate == EXIT2_IR) && newstate != oldstate) {
            if (priv->data_ready && priv->bits_cnt > 0) {
                int is_ir = (oldstate == EXIT1_IR || oldstate == EXIT2_IR);
                int cnt = priv->bits_cnt > 256 ? 256 : priv->bits_cnt;
                int i;
                uint64_t tdi_val = 0, tdo_val = 0;
                for (i = 0; i < cnt; i++) {
                    tdi_val |= ((uint64_t)priv->bits_tdi[i] << i);
                    tdo_val |= ((uint64_t)priv->bits_tdo[i] << i);
                }
                const char* dr_ir = is_ir ? "IR" : "DR";

                /* Bitstring annotations */
                char tdi_str[128];
                char tdo_str[128];
                snprintf(tdi_str, sizeof(tdi_str), "%s TDI: (0x%llX), %d bits", dr_ir, (unsigned long long)tdi_val, cnt);
                snprintf(tdo_str, sizeof(tdo_str), "%s TDO: (0x%llX), %d bits", dr_ir, (unsigned long long)tdo_val, cnt);
                C_ANN_PUT(di, priv->ss_bitstring, samplenum, priv->out_ann, ANN_BITSTRING_TDI, tdi_str);
                C_ANN_PUT(di, priv->ss_bitstring, samplenum, priv->out_ann, ANN_BITSTRING_TDO, tdo_str);

                /* Fix #2: Protocol output with bitstring and per-bit ss/es data.
                 * Binary data format:
                 *   [0..cnt]       = bitstring as null-terminated string (reversed: last-bit-first)
                 *   [cnt+1..]      = per-bit ss/es pairs, each 16 bytes (8B ss LE + 8B es LE),
                 *                    in same order as bitstring */
                {
                    char bitstring_tdi[257];
                    char bitstring_tdo[257];
                    for (i = 0; i < cnt; i++) {
                        bitstring_tdi[i] = '0' + priv->bits_tdi[cnt - 1 - i];
                        bitstring_tdo[i] = '0' + priv->bits_tdo[cnt - 1 - i];
                    }
                    bitstring_tdi[cnt] = '\0';
                    bitstring_tdo[cnt] = '\0';

                    int bitstring_len = cnt + 1;
                    int per_bit_size = 16;
                    int data_size = bitstring_len + cnt * per_bit_size;
                    unsigned char* proto_data = (unsigned char*)g_malloc(data_size);

                    /* TDI protocol output */
                    memcpy(proto_data, bitstring_tdi, bitstring_len);
                    int pos = bitstring_len;
                    for (i = 0; i < cnt; i++) {
                        int bit_idx = cnt - 1 - i;
                        uint64_t ss = priv->bits_ss[bit_idx];
                        uint64_t es;
                        if (bit_idx == cnt - 1)
                            es = samplenum;
                        else
                            es = priv->bits_ss[bit_idx + 1];
                        memcpy(proto_data + pos, &ss, 8);
                        memcpy(proto_data + pos + 8, &es, 8);
                        pos += per_bit_size;
                    }
                    c_decoder_put_python(di, priv->ss_bitstring, samplenum, priv->out_python,
                        is_ir ? "IR TDI" : "DR TDI", proto_data, data_size);

                    /* TDO protocol output */
                    memcpy(proto_data, bitstring_tdo, bitstring_len);
                    pos = bitstring_len;
                    for (i = 0; i < cnt; i++) {
                        int bit_idx = cnt - 1 - i;
                        uint64_t ss = priv->bits_ss[bit_idx];
                        uint64_t es;
                        if (bit_idx == cnt - 1)
                            es = samplenum;
                        else
                            es = priv->bits_ss[bit_idx + 1];
                        memcpy(proto_data + pos, &ss, 8);
                        memcpy(proto_data + pos + 8, &es, 8);
                        pos += per_bit_size;
                    }
                    c_decoder_put_python(di, priv->ss_bitstring, samplenum, priv->out_python,
                        is_ir ? "IR TDO" : "DR TDO", proto_data, data_size);

                    g_free(proto_data);
                }

                /* Fix #3: Output deferred last bit annotation */
                {
                    char tdi_str[4];
                    char tdo_str[4];
                    snprintf(tdi_str, sizeof(tdi_str), "%d", priv->last_bit_tdi);
                    snprintf(tdo_str, sizeof(tdo_str), "%d", priv->last_bit_tdo);
                    C_ANN_PUT(di, priv->last_bit_ss, samplenum, priv->out_ann, ANN_BIT_TDI, tdi_str);
                    C_ANN_PUT(di, priv->last_bit_ss, samplenum, priv->out_ann, ANN_BIT_TDO, tdo_str);
                }

                priv->bits_cnt = 0;
                priv->first = TRUE;
                priv->data_ready = FALSE;
            }
        }

        /* Emit state annotation on every rising TCK edge (except the first),
         * matching Python decoder behavior. Python emits the OLD state
         * annotation from the previous edge to the current edge. */
        if (priv->first) {
            priv->first = FALSE;
        } else {
            C_ANN_PUT(di, ss_state, samplenum, priv->out_ann, oldstate,
                jtag_ann_labels[oldstate][2]);
            /* Include state name in NEW STATE protocol output */
            const char* state_name = jtag_state_names[newstate];
            c_decoder_put_python(di, ss_state, samplenum, priv->out_python,
                "NEW STATE", (const unsigned char*)state_name, strlen(state_name));
        }
        ss_state = samplenum;

        priv->oldstate = oldstate;
        priv->state = newstate;
    }
}

static void jtag_destroy(struct srd_decoder_inst* di)
{
    void* priv = c_decoder_get_private(di);
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

SRD_C_DECODER_EXPORT struct srd_c_decoder* srd_c_decoder_entry(void)
{
    return &jtag_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
