#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_START_BIT = 0,
    ANN_SI_BIT,
    ANN_SO_BIT,
    ANN_STATUS_READY,
    ANN_STATUS_BUSY,
    ANN_WARNING,
    NUM_ANN,
};

#define CH_CS 0
#define CH_SK 1
#define CH_SI 2
#define CH_SO 3

struct mw_py_entry {
    uint64_t ss;
    uint64_t es;
    int si;
    int so;
};

struct mw_priv {
    int out_ann;
    int out_python;
};

static struct srd_channel mw_channels[] = {
    {"cs", "CS", "Chip select", 0, SRD_CHANNEL_COMMON, NULL},
    {"sk", "SK", "Clock", 1, SRD_CHANNEL_SCLK, NULL},
    {"si", "SI", "Slave in", 2, SRD_CHANNEL_SDATA, NULL},
    {"so", "SO", "Slave out", 3, SRD_CHANNEL_SDATA, NULL},
};

static const char *mw_ann_labels[][3] = {
    {"", "start-bit", "Start bit"},
    {"", "si-bit", "SI bit"},
    {"", "so-bit", "SO bit"},
    {"", "status-check-ready", "Status check ready"},
    {"", "status-check-busy", "Status check busy"},
    {"", "warning", "Warning"},
};

static const int mw_row_si_classes[] = {ANN_START_BIT, ANN_SI_BIT};
static const int mw_row_so_classes[] = {ANN_SO_BIT};
static const int mw_row_status_classes[] = {ANN_STATUS_READY, ANN_STATUS_BUSY};
static const int mw_row_warnings_classes[] = {ANN_WARNING};
static const struct srd_c_ann_row mw_ann_rows[] = {
    {"si-bits", "SI bits", mw_row_si_classes, 2},
    {"so-bits", "SO bits", mw_row_so_classes, 1},
    {"status", "Status", mw_row_status_classes, 2},
    {"warnings", "Warnings", mw_row_warnings_classes, 1},
};

static const char *mw_inputs[] = {"logic"};
static const char *mw_outputs[] = {"microwire"};
static const char *mw_tags[] = {"Embedded/industrial"};

struct mw_packet_entry {
    uint64_t samplenum;
    uint64_t matched;
    int cs;
    int sk;
    int si;
    int so;
};

static void mw_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct mw_priv)));
    }
    struct mw_priv *p = (struct mw_priv *)c_decoder_get_private(di);
    memset(p, 0, sizeof(struct mw_priv));
}

static void mw_start(struct srd_decoder_inst *di)
{
    struct mw_priv *p = (struct mw_priv *)c_decoder_get_private(di);
    p->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "microwire");
    p->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "microwire");
}

static void mw_decode(struct srd_decoder_inst *di)
{
    struct mw_priv *p = (struct mw_priv *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_rise(cb, CH_CS);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int sk = c_decoder_get_pin(di, CH_SK, samplenum);
        if (sk) {
            C_ANN_PUT(di, samplenum, samplenum, p->out_ann, ANN_WARNING,
                       "Clock should be low on start", "Clock high on start", "Clock high", "SK high");
        }

        GArray *packet = g_array_new(FALSE, FALSE, sizeof(struct mw_packet_entry));

        int cs = 1;
        while (cs) {
            cb = c_cond_new();
            c_cond_fall(cb, CH_CS);
            c_cond_or(cb);
            int sk_val = c_decoder_get_pin(di, CH_SK, samplenum);
            if (sk_val == 0)
                c_cond_rise(cb, CH_SK);
            else
                c_cond_fall(cb, CH_SK);
            c_cond_or(cb);
            c_cond_edge(cb, CH_SO);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) {
                g_array_free(packet, TRUE);
                return;
            }

            cs = c_decoder_get_pin(di, CH_CS, samplenum);
            sk = c_decoder_get_pin(di, CH_SK, samplenum);
            int si = c_decoder_get_pin(di, CH_SI, samplenum);
            int so = c_decoder_get_pin(di, CH_SO, samplenum);

            struct mw_packet_entry entry;
            entry.samplenum = samplenum;
            entry.matched = matched;
            entry.cs = cs;
            entry.sk = sk;
            entry.si = si;
            entry.so = so;
            g_array_append_val(packet, entry);
        }

        int status_check = 1;
        for (guint i = 0; i < packet->len; i++) {
            struct mw_packet_entry *e = &g_array_index(packet, struct mw_packet_entry, i);
            if (e->matched & (1ULL << 1)) {
                if (e->sk) {
                    if (e->si)
                        status_check = 0;
                    break;
                }
            }
        }

        if (status_check) {
            uint64_t start_sn = g_array_index(packet, struct mw_packet_entry, 0).samplenum;
            int bit_so = g_array_index(packet, struct mw_packet_entry, 0).so;

            for (guint i = 0; i < packet->len; i++) {
                struct mw_packet_entry *e = &g_array_index(packet, struct mw_packet_entry, i);
                if (e->matched & (1ULL << 2)) {
                    if (bit_so == 0 && e->so) {
                        C_ANN_PUT(di, start_sn, e->samplenum, p->out_ann, ANN_STATUS_BUSY, "Busy", "B");
                    }
                    start_sn = e->samplenum;
                    bit_so = e->so;
                }
            }

            struct mw_packet_entry *last = &g_array_index(packet, struct mw_packet_entry, packet->len - 1);
            if (bit_so == 0) {
                C_ANN_PUT(di, start_sn, last->samplenum, p->out_ann, ANN_STATUS_BUSY, "Busy", "B");
            } else {
                C_ANN_PUT(di, start_sn, last->samplenum, p->out_ann, ANN_STATUS_READY, "Ready", "R");
            }
        } else {
            uint64_t bit_start = 0;
            int bit_si = 0;
            int bit_so = 0;
            int start_bit = 1;
            GArray *pydata = g_array_new(FALSE, FALSE, sizeof(struct mw_py_entry));

            for (guint i = 0; i < packet->len; i++) {
                struct mw_packet_entry *e = &g_array_index(packet, struct mw_packet_entry, i);

                if (e->matched & (1ULL << 1)) {
                    if (e->sk) {
                        if (bit_start > 0) {
                            if (start_bit) {
                                if (bit_si == 0) {
                                    C_ANN_PUT(di, bit_start, e->samplenum, p->out_ann, ANN_WARNING,
                                               "Start bit not high", "Start bit low");
                                } else {
                                    C_ANN_PUT(di, bit_start, e->samplenum, p->out_ann, ANN_START_BIT,
                                               "Start bit", "S");
                                }
                                start_bit = 0;
                            } else {
                                char si_long[16], so_long[16];
                                char si_mid[8], so_mid[8];
                                char si_short[4], so_short[4];
                                snprintf(si_long, sizeof(si_long), "SI bit: %d", bit_si);
                                snprintf(so_long, sizeof(so_long), "SO bit: %d", bit_so);
                                snprintf(si_mid, sizeof(si_mid), "SI: %d", bit_si);
                                snprintf(so_mid, sizeof(so_mid), "SO: %d", bit_so);
                                snprintf(si_short, sizeof(si_short), "%d", bit_si);
                                snprintf(so_short, sizeof(so_short), "%d", bit_so);
                                C_ANN_PUT(di, bit_start, e->samplenum, p->out_ann, ANN_SI_BIT,
                                           si_long, si_mid, si_short);
                                C_ANN_PUT(di, bit_start, e->samplenum, p->out_ann, ANN_SO_BIT,
                                           so_long, so_mid, so_short);
                                struct mw_py_entry pye = {bit_start, e->samplenum, bit_si, bit_so};
                                g_array_append_val(pydata, pye);
                            }
                        }
                        bit_start = e->samplenum;
                        bit_si = e->si;
                    } else {
                        bit_so = e->so;
                    }
                } else if ((e->matched & (1ULL << 0)) && e->cs == 0 && e->sk == 0) {
                    char si_long[16], so_long[16];
                    char si_mid[8], so_mid[8];
                    char si_short[4], so_short[4];
                    snprintf(si_long, sizeof(si_long), "SI bit: %d", bit_si);
                    snprintf(so_long, sizeof(so_long), "SO bit: %d", bit_so);
                    snprintf(si_mid, sizeof(si_mid), "SI: %d", bit_si);
                    snprintf(so_mid, sizeof(so_mid), "SO: %d", bit_so);
                    snprintf(si_short, sizeof(si_short), "%d", bit_si);
                    snprintf(so_short, sizeof(so_short), "%d", bit_so);
                    C_ANN_PUT(di, bit_start, e->samplenum, p->out_ann, ANN_SI_BIT,
                               si_long, si_mid, si_short);
                    C_ANN_PUT(di, bit_start, e->samplenum, p->out_ann, ANN_SO_BIT,
                               so_long, so_mid, so_short);
                    struct mw_py_entry pye = {bit_start, e->samplenum, bit_si, bit_so};
                    g_array_append_val(pydata, pye);
                }
            }

            if (p->out_python >= 0 && pydata->len > 0) {
                struct mw_packet_entry *first = &g_array_index(packet, struct mw_packet_entry, 0);
                struct mw_packet_entry *last = &g_array_index(packet, struct mw_packet_entry, packet->len - 1);
                c_decoder_put_python(di, first->samplenum, last->samplenum,
                                     p->out_python, "microwire",
                                     (unsigned char *)pydata->data,
                                     pydata->len * sizeof(struct mw_py_entry));
            }
            g_array_free(pydata, TRUE);
        }

        g_array_free(packet, TRUE);
    }
}

static void mw_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder microwire_c_decoder = {
    .id = "microwire_c",
    .name = "Microwire(C)",
    .longname = "Microwire (C)",
    .desc = "3-wire, half-duplex, synchronous serial bus (C implementation)",
    .license = "gplv2+",
    .channels = mw_channels,
    .num_channels = 4,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = mw_ann_labels,
    .num_annotation_rows = 4,
    .annotation_rows = mw_ann_rows,
    .inputs = mw_inputs,
    .num_inputs = 1,
    .outputs = mw_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = mw_tags,
    .num_tags = 1,
    .reset = mw_reset,
    .start = mw_start,
    .decode = mw_decode,
    .destroy = mw_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &microwire_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
