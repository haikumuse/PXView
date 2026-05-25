#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum usb_sym {
    SYM_J = 0,
    SYM_K,
    SYM_SE0,
    SYM_SE1,
    SYM_FS_J,
    SYM_LS_J,
};

enum usb_state {
    STATE_IDLE,
    STATE_GET_BIT,
    STATE_GET_EOP,
    STATE_WAIT_IDLE,
};

enum usb_signalling_mode {
    SIG_AUTO = 0,
    SIG_FULL_SPEED,
    SIG_LOW_SPEED,
    SIG_LOW_SPEED_RP,
};

#define ANN_J           0
#define ANN_K           1
#define ANN_SE0         2
#define ANN_SE1         3
#define ANN_SOP         4
#define ANN_EOP         5
#define ANN_BIT         6
#define ANN_STUFFBIT    7
#define ANN_ERROR       8
#define ANN_KEEP_ALIVE  9
#define ANN_RESET       10
#define NUM_ANN         11

typedef struct {
    enum usb_state state;
    enum usb_signalling_mode signalling;
    enum usb_sym oldsym;
    uint64_t samplerate;
    double bitrate;
    double bitwidth;
    double samplepos;
    uint64_t samplenum_target;
    uint64_t samplenum_edge;
    uint64_t samplenum_lastedge;
    uint8_t edgepins_dp;
    uint8_t edgepins_dm;
    int consecutive_ones;
    char bits[17];
    int bits_len;
    uint64_t ss_block;
    uint64_t samplenum;
    uint64_t matched;
    int out_ann;
    int out_python;
} usb_priv;

#define CH_DP 0
#define CH_DM 1

static struct srd_channel usb_channels[] = {
    {"dp", "D+", "USB D+ signal", 0, SRD_CHANNEL_COMMON, "dec_usb_signalling_chan_dp"},
    {"dm", "D-", "USB D- signal", 1, SRD_CHANNEL_COMMON, "dec_usb_signalling_chan_dm"},
};

static const char *usb_ann_labels[][3] = {
    {"", "sym-j", "J symbol"},
    {"", "sym-k", "K symbol"},
    {"", "sym-se0", "SE0 symbol"},
    {"", "sym-se1", "SE1 symbol"},
    {"", "sop", "Start of packet (SOP)"},
    {"", "eop", "End of packet (EOP)"},
    {"", "bit", "Bit"},
    {"", "stuffbit", "Stuff bit"},
    {"", "error", "Error"},
    {"", "keep-alive", "Low-speed keep-alive"},
    {"", "reset", "Reset"},
};

static const int usb_row_bits_classes[] = {ANN_SOP, ANN_EOP, ANN_BIT, ANN_STUFFBIT, ANN_ERROR, ANN_KEEP_ALIVE, ANN_RESET};
static const int usb_row_symbols_classes[] = {ANN_J, ANN_K, ANN_SE0, ANN_SE1};
static const struct srd_c_ann_row usb_ann_rows[] = {
    {"bits", "Bits", usb_row_bits_classes, 7},
    {"symbols", "Symbols", usb_row_symbols_classes, 4},
};

static const char *usb_inputs[] = {"logic", NULL};
static const char *usb_outputs[] = {"usb_signalling", NULL};
static const char *usb_tags[] = {"PC", NULL};

static enum usb_sym get_symbol(enum usb_signalling_mode mode, uint8_t dp, uint8_t dm)
{
    if (dp == 0 && dm == 0) return SYM_SE0;
    if (dp == 1 && dm == 1) return SYM_SE1;
    if (mode == SIG_LOW_SPEED || mode == SIG_LOW_SPEED_RP) {
        return (dp == 1 && dm == 0) ? SYM_K : SYM_J;
    } else if (mode == SIG_FULL_SPEED) {
        return (dp == 1 && dm == 0) ? SYM_J : SYM_K;
    } else {
        return (dp == 1 && dm == 0) ? SYM_FS_J : SYM_LS_J;
    }
}

static int sym_is_j(enum usb_sym s, enum usb_signalling_mode mode)
{
    if (mode == SIG_LOW_SPEED || mode == SIG_LOW_SPEED_RP)
        return s == SYM_J;
    return s == SYM_J || s == SYM_FS_J;
}

static int sym_is_k(enum usb_sym s, enum usb_signalling_mode mode)
{
    if (mode == SIG_LOW_SPEED || mode == SIG_LOW_SPEED_RP)
        return s == SYM_K;
    return s == SYM_K || s == SYM_LS_J;
}

static const char *get_sym_name(enum usb_sym sym)
{
    switch (sym) {
    case SYM_J: case SYM_FS_J: case SYM_LS_J: return "J";
    case SYM_K: return "K";
    case SYM_SE0: return "SE0";
    case SYM_SE1: return "SE1";
    default: return "";
    }
}

static void put_sym_ann(struct srd_decoder_inst *di, usb_priv *s, enum usb_sym sym)
{
    int cls = -1;
    switch (sym) {
    case SYM_J: case SYM_FS_J: case SYM_LS_J:
        cls = ANN_J; C_ANN_PUT(di, s->samplenum_lastedge, s->samplenum_edge, s->out_ann, cls, "J"); break;
    case SYM_K:
        cls = ANN_K; C_ANN_PUT(di, s->samplenum_lastedge, s->samplenum_edge, s->out_ann, cls, "K"); break;
    case SYM_SE0:
        cls = ANN_SE0; C_ANN_PUT(di, s->samplenum_lastedge, s->samplenum_edge, s->out_ann, cls, "SE0", "0"); break;
    case SYM_SE1:
        cls = ANN_SE1; C_ANN_PUT(di, s->samplenum_lastedge, s->samplenum_edge, s->out_ann, cls, "SE1", "1"); break;
    default: break;
    }
}

static void update_bitrate(usb_priv *s)
{
    if (s->signalling == SIG_LOW_SPEED || s->signalling == SIG_LOW_SPEED_RP)
        s->bitrate = 1500000.0;
    else if (s->signalling == SIG_FULL_SPEED)
        s->bitrate = 12000000.0;
    else
        return;
    if (s->samplerate > 0)
        s->bitwidth = (double)s->samplerate / s->bitrate;
}

static void set_new_target(usb_priv *s)
{
    s->samplepos += s->bitwidth;
    s->samplenum_target = (uint64_t)s->samplepos;
    s->samplenum_lastedge = s->samplenum_edge;
    s->samplenum_edge = (uint64_t)(s->samplepos - (s->bitwidth / 2.0));
}

static void usb_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(usb_priv)));
    usb_priv *s = (usb_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(usb_priv));
    s->state = STATE_IDLE;
    s->oldsym = SYM_J;
    s->signalling = SIG_AUTO;
}

static void usb_start(struct srd_decoder_inst *di)
{
    usb_priv *s = (usb_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "usb_signalling");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "usb_signalling");
    const char *sig = c_decoder_get_option_string(di, "signalling", "automatic");
    if (strcmp(sig, "full-speed") == 0)
        s->signalling = SIG_FULL_SPEED;
    else if (strcmp(sig, "low-speed") == 0)
        s->signalling = SIG_LOW_SPEED;
    else
        s->signalling = SIG_AUTO;
}

static void usb_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    usb_priv *s = (usb_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
        update_bitrate(s);
    }
}

static void usb_decode(struct srd_decoder_inst *di)
{
    usb_priv *s = (usb_priv *)c_decoder_get_private(di);
    if (!s->samplerate) return;

    srd_cond_builder *cb;
    uint64_t samplenum = 0, matched = 0;
    uint8_t dp, dm;
    enum usb_sym sym;

    /* Read the first sample to seed internal state, matching Python's
       "self.wait()" before the main loop, followed by handle_idle(). */
    cb = c_cond_new();
    c_cond_skip(cb, 0);
    if (c_cond_wait(cb, di, &samplenum, &matched) != SRD_OK) {
        c_cond_free(cb);
        return;
    }
    c_cond_free(cb);
    dp = c_decoder_get_pin(di, CH_DP, samplenum);
    dm = c_decoder_get_pin(di, CH_DM, samplenum);
    sym = get_symbol(s->signalling, dp, dm);

    /* handle_idle(sym) equivalent:
       - samplenum_edge = samplenum
       - se0_length check (0 since samplenum_lastedge == samplenum == 0)
       - auto-detect signalling mode
       - ALWAYS set oldsym = SYM_J (matching Python's handle_idle)
       - state = IDLE (already set) */
    s->samplenum_edge = samplenum;
    /* se0_length = 0, no Reset/Keep-alive emitted */

    /* Auto-detect signalling mode, matching Python's handle_idle() logic:
       if options['signalling'] == 'automatic' and sym == 'FS_J': full-speed
       elif options['signalling'] == 'automatic' and sym == 'LS_J': low-speed
       else: signalling = options['signalling'] */
    {
        const char *sig = c_decoder_get_option_string(di, "signalling", "automatic");
        if (strcmp(sig, "automatic") == 0) {
            if (sym == SYM_FS_J) {
                s->signalling = SIG_FULL_SPEED;
                update_bitrate(s);
            } else if (sym == SYM_LS_J) {
                s->signalling = SIG_LOW_SPEED;
                update_bitrate(s);
            } else {
                s->signalling = SIG_AUTO;
            }
        } else if (strcmp(sig, "full-speed") == 0) {
            s->signalling = SIG_FULL_SPEED;
            update_bitrate(s);
        } else if (strcmp(sig, "low-speed") == 0) {
            s->signalling = SIG_LOW_SPEED;
            update_bitrate(s);
        }
    }
    /* Always set oldsym = SYM_J, matching Python's handle_idle() */
    s->oldsym = SYM_J;
    s->edgepins_dp = dp;
    s->edgepins_dm = dm;

    while (1) {
        if (s->state == STATE_IDLE) {
            cb = c_cond_new();
            c_cond_edge(cb, CH_DP);
            c_cond_or(cb);
            c_cond_edge(cb, CH_DM);
            if (c_cond_wait(cb, di, &samplenum, &matched) != SRD_OK) {
                c_cond_free(cb);
                return;
            }
            c_cond_free(cb);
            s->samplenum = samplenum;
            dp = c_decoder_get_pin(di, CH_DP, samplenum);
            dm = c_decoder_get_pin(di, CH_DM, samplenum);
            sym = get_symbol(s->signalling, dp, dm);
            s->edgepins_dp = dp;
            s->edgepins_dm = dm;

            if (sym == SYM_SE0) {
                s->samplenum_lastedge = samplenum;
                s->state = STATE_WAIT_IDLE;
                /* Python does NOT update oldsym or samplenum_edge here */
            } else {
                int sop_detected = 0;
                if (sym_is_k(sym, s->signalling) && sym_is_j(s->oldsym, s->signalling)) {
                    s->consecutive_ones = 0;
                    s->bits_len = 0;
                    update_bitrate(s);
                    s->samplepos = (double)samplenum - (s->bitwidth / 2.0) + 0.5;
                    set_new_target(s);
                    C_ANN_PUT(di, s->samplenum_edge, s->samplenum_edge, s->out_ann, ANN_SOP, "SOP", "S");
                    c_decoder_put_python(di, s->samplenum_edge, s->samplenum_edge, s->out_python, "SOP", NULL, 0);
                    s->state = STATE_GET_BIT;
                    sop_detected = 1;
                }
                /* Python does NOT update oldsym or samplenum_edge when
                   SOP is not detected in IDLE state. Only edgepins is set. */
            }

        } else if (s->state == STATE_GET_BIT || s->state == STATE_GET_EOP) {
            if (s->samplenum_edge > s->samplenum) {
                cb = c_cond_new();
                c_cond_skip(cb, s->samplenum_edge - s->samplenum);
                if (c_cond_wait(cb, di, &samplenum, &matched) != SRD_OK) {
                    c_cond_free(cb);
                    return;
                }
                c_cond_free(cb);
                s->samplenum = samplenum;
                dp = c_decoder_get_pin(di, CH_DP, samplenum);
                dm = c_decoder_get_pin(di, CH_DM, samplenum);
                s->edgepins_dp = dp;
                s->edgepins_dm = dm;
            }
            if (s->samplenum_target > s->samplenum) {
                cb = c_cond_new();
                c_cond_skip(cb, s->samplenum_target - s->samplenum);
                if (c_cond_wait(cb, di, &samplenum, &matched) != SRD_OK) {
                    c_cond_free(cb);
                    return;
                }
                c_cond_free(cb);
                s->samplenum = samplenum;
                dp = c_decoder_get_pin(di, CH_DP, samplenum);
                dm = c_decoder_get_pin(di, CH_DM, samplenum);
            }

            sym = get_symbol(s->signalling, dp, dm);

            if (s->state == STATE_GET_BIT) {
                set_new_target(s);
                int b = (s->oldsym == sym) ? 1 : 0;
                s->oldsym = sym;

                if (sym == SYM_SE0) {
                    s->state = STATE_GET_EOP;
                    s->ss_block = s->samplenum_lastedge;
                } else {
                    if (s->consecutive_ones == 6) {
                        if (b == 0) {
                            C_ANN_PUT(di, s->samplenum_lastedge, s->samplenum_edge, s->out_ann, ANN_STUFFBIT, "Stuff bit: 0", "SB: 0", "0");
                            c_decoder_put_python(di, s->samplenum_lastedge, s->samplenum_edge, s->out_python, "STUFF BIT", NULL, 0);
                            s->consecutive_ones = 0;
                        } else {
                            C_ANN_PUT(di, s->samplenum_lastedge, s->samplenum_edge, s->out_ann, ANN_ERROR, "Bit stuff error", "BS ERR", "B");
                            c_decoder_put_python(di, s->samplenum_lastedge, s->samplenum_edge, s->out_python, "ERR", NULL, 0);
                            s->state = STATE_IDLE;
                        }
                    } else {
                        char bstr[2] = {(char)('0' + b), 0};
                        C_ANN_PUT(di, s->samplenum_lastedge, s->samplenum_edge, s->out_ann, ANN_BIT, bstr);
                        c_decoder_put_python(di, s->samplenum_lastedge, s->samplenum_edge, s->out_python, "BIT", (const unsigned char *)bstr, 1);
                        if (b == 1)
                            s->consecutive_ones++;
                        else
                            s->consecutive_ones = 0;
                    }

                    /* Output SYM after BIT/STUFF BIT/ERR, matching Python order */
                    put_sym_ann(di, s, sym);
                    {
                        const char *sn = get_sym_name(sym);
                        c_decoder_put_python(di, s->samplenum_lastedge, s->samplenum_edge, s->out_python, "SYM", (const unsigned char *)sn, strlen(sn));
                    }

                    if (s->state == STATE_IDLE)
                        continue;

                    if (s->bits_len < 16) {
                        s->bits[s->bits_len++] = '0' + b;
                        s->bits[s->bits_len] = 0;
                    }
                    if (s->bits_len == 16 && strcmp(s->bits, "0000000100111100") == 0) {
                        s->signalling = SIG_LOW_SPEED_RP;
                        update_bitrate(s);
                        c_decoder_put_python(di, s->samplenum_edge, s->samplenum_edge, s->out_python, "EOP", NULL, 0);
                        s->oldsym = SYM_J;
                        s->state = STATE_IDLE;
                        continue;
                    }

                    if (b == 0) {
                        enum usb_sym edgesym = get_symbol(s->signalling, s->edgepins_dp, s->edgepins_dm);
                        if (edgesym != SYM_SE0 && edgesym != SYM_SE1) {
                            if (edgesym == sym) {
                                s->bitwidth = s->bitwidth - (0.001 * s->bitwidth);
                                s->samplepos = s->samplepos - (0.01 * s->bitwidth);
                            } else {
                                s->bitwidth = s->bitwidth + (0.001 * s->bitwidth);
                                s->samplepos = s->samplepos + (0.01 * s->bitwidth);
                            }
                        }
                    }
                }

                if (sym == SYM_SE0) {
                    put_sym_ann(di, s, sym);
                    {
                        const char *sn = get_sym_name(sym);
                        c_decoder_put_python(di, s->samplenum_lastedge, s->samplenum_edge, s->out_python, "SYM", (const unsigned char *)sn, strlen(sn));
                    }
                }

            } else if (s->state == STATE_GET_EOP) {
                set_new_target(s);
                put_sym_ann(di, s, sym);
                {
                    const char *sn = get_sym_name(sym);
                    c_decoder_put_python(di, s->samplenum_lastedge, s->samplenum_edge, s->out_python, "SYM", (const unsigned char *)sn, strlen(sn));
                }
                s->oldsym = sym;

                if (sym == SYM_SE0) {
                    /* continue */
                } else if (sym_is_j(sym, s->signalling)) {
                    C_ANN_PUT(di, s->ss_block, s->samplenum_edge, s->out_ann, ANN_EOP, "EOP", "E");
                    c_decoder_put_python(di, s->ss_block, s->samplenum_edge, s->out_python, "EOP", NULL, 0);
                    s->state = STATE_WAIT_IDLE;
                } else {
                    C_ANN_PUT(di, s->ss_block, s->samplenum_edge, s->out_ann, ANN_ERROR, "EOP Error", "EErr", "E");
                    c_decoder_put_python(di, s->ss_block, s->samplenum_edge, s->out_python, "ERR", NULL, 0);
                    s->state = STATE_IDLE;
                }
            }

        } else if (s->state == STATE_WAIT_IDLE) {
            /* Skip "all-low" input. Wait for high level on either DP or DM.
               Matching Python: self.wait() then while not dp and not dm: self.wait([{0:'h'},{1:'h'}]) */
            cb = c_cond_new();
            c_cond_skip(cb, 1);
            if (c_cond_wait(cb, di, &samplenum, &matched) != SRD_OK) {
                c_cond_free(cb);
                return;
            }
            c_cond_free(cb);
            s->samplenum = samplenum;
            dp = c_decoder_get_pin(di, CH_DP, samplenum);
            dm = c_decoder_get_pin(di, CH_DM, samplenum);

            while (!dp && !dm) {
                cb = c_cond_new();
                c_cond_high(cb, CH_DP);
                c_cond_or(cb);
                c_cond_high(cb, CH_DM);
                int ret = c_cond_wait(cb, di, &samplenum, &matched);
                c_cond_free(cb);
                if (ret != SRD_OK)
                    return;
                s->samplenum = samplenum;
                dp = c_decoder_get_pin(di, CH_DP, samplenum);
                dm = c_decoder_get_pin(di, CH_DM, samplenum);
            }

            s->edgepins_dp = dp;
            s->edgepins_dm = dm;

            if (samplenum - s->samplenum_lastedge > 1) {
                /* handle_idle(sym) equivalent */
                s->samplenum_edge = samplenum;
                double se0_length = (double)(samplenum - s->samplenum_lastedge) / (double)s->samplerate;
                if (se0_length > 2.5e-6) {
                    C_ANN_PUT(di, s->samplenum_lastedge, s->samplenum_edge, s->out_ann, ANN_RESET, "Reset", "Res", "R");
                    c_decoder_put_python(di, s->samplenum_lastedge, s->samplenum_edge, s->out_python, "RESET", NULL, 0);
                    /* Reset signalling to option value, matching Python */
                    const char *sig = c_decoder_get_option_string(di, "signalling", "automatic");
                    if (strcmp(sig, "full-speed") == 0)
                        s->signalling = SIG_FULL_SPEED;
                    else if (strcmp(sig, "low-speed") == 0)
                        s->signalling = SIG_LOW_SPEED;
                    else
                        s->signalling = SIG_AUTO;
                } else if (se0_length > 1.2e-6 && s->signalling == SIG_LOW_SPEED) {
                    C_ANN_PUT(di, s->samplenum_lastedge, s->samplenum_edge, s->out_ann, ANN_KEEP_ALIVE, "Keep-alive", "KA", "A");
                    c_decoder_put_python(di, s->samplenum_lastedge, s->samplenum_edge, s->out_python, "KEEP ALIVE", NULL, 0);
                }

                /* Auto-detect signalling mode, matching Python's handle_idle():
                   Uses options['signalling'] for symbol lookup */
                sym = get_symbol(s->signalling, dp, dm);
                {
                    const char *sig = c_decoder_get_option_string(di, "signalling", "automatic");
                    if (strcmp(sig, "automatic") == 0) {
                        if (sym == SYM_FS_J) {
                            s->signalling = SIG_FULL_SPEED;
                        } else if (sym == SYM_LS_J) {
                            s->signalling = SIG_LOW_SPEED;
                        } else {
                            s->signalling = SIG_AUTO;
                        }
                    } else if (strcmp(sig, "full-speed") == 0) {
                        s->signalling = SIG_FULL_SPEED;
                    } else if (strcmp(sig, "low-speed") == 0) {
                        s->signalling = SIG_LOW_SPEED;
                    }
                }
                update_bitrate(s);
                /* Always set oldsym = SYM_J, matching Python's handle_idle() */
                s->oldsym = SYM_J;
                s->state = STATE_IDLE;
            } else {
                /* samplenum - samplenum_lastedge <= 1: check for SOP.
                   Python stays in WAIT IDLE if no SOP detected - does NOT go
                   back to IDLE. This allows accumulating SE0 duration across
                   multiple short SE0+J cycles until a Reset is detected. */
                sym = get_symbol(s->signalling, dp, dm);
                if (sym_is_k(sym, s->signalling) && sym_is_j(s->oldsym, s->signalling)) {
                    s->consecutive_ones = 0;
                    s->bits_len = 0;
                    update_bitrate(s);
                    s->samplepos = (double)samplenum - (s->bitwidth / 2.0) + 0.5;
                    set_new_target(s);
                    C_ANN_PUT(di, s->samplenum_edge, s->samplenum_edge, s->out_ann, ANN_SOP, "SOP", "S");
                    c_decoder_put_python(di, s->samplenum_edge, s->samplenum_edge, s->out_python, "SOP", NULL, 0);
                    s->state = STATE_GET_BIT;
                }
                /* If no SOP: state remains STATE_WAIT_IDLE, matching Python */
            }
        }
    }
}

static void usb_destroy(struct srd_decoder_inst *di)
{
    usb_priv *s = (usb_priv *)c_decoder_get_private(di);
    if (s) g_free(s);
    c_decoder_set_private(di, NULL);
}

static struct srd_decoder_option usb_options_arr[1];

static struct srd_c_decoder usb_c_decoder = {
    "usb_signalling_c",
    "USB signalling",
    "Universal Serial Bus (LS/FS) signalling",
    "USB (low-speed/full-speed) signalling protocol.",
    "gplv2+",
    usb_channels,
    2,
    NULL,
    0,
    usb_options_arr,
    1,
    NUM_ANN,
    usb_ann_labels,
    2,
    usb_ann_rows,
    usb_inputs,
    1,
    usb_outputs,
    1,
    NULL,
    0,
    usb_tags,
    1,
    usb_reset,
    usb_start,
    usb_decode,
    NULL,
    usb_metadata,
    usb_destroy,
    NULL,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    GVariant *vals[] = {
        g_variant_new_string("automatic"),
        g_variant_new_string("full-speed"),
        g_variant_new_string("low-speed"),
    };
    GSList *val_list = NULL;
    val_list = g_slist_append(val_list, vals[0]);
    val_list = g_slist_append(val_list, vals[1]);
    val_list = g_slist_append(val_list, vals[2]);
    usb_options_arr[0].id = "signalling";
    usb_options_arr[0].idn = "dec_usb_signalling_opt_signalling";
    usb_options_arr[0].desc = "Signalling";
    usb_options_arr[0].def = g_variant_new_string("automatic");
    usb_options_arr[0].values = val_list;
    return &usb_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
