#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

typedef struct {
    int bit_count;
    uint8_t mosi_byte;
    uint8_t miso_byte;
    uint64_t start_sample;
    int cs_active;
    int last_clk;
    int cpol;
    int cpha;
    int bit_order;
    int cs_polarity;
    int have_cs;
    int have_miso;
    int have_mosi;
    int sample_edge_rise;
    int out_ann;
} spi_state;

static struct srd_channel spi_channels[] = {
    {"clk", "CLK", "Clock", 0, SRD_CHANNEL_SCLK, NULL},
};

static struct srd_channel spi_optional_channels[] = {
    {"miso", "MISO", "Master In Slave Out", 1, SRD_CHANNEL_SDATA, NULL},
    {"mosi", "MOSI", "Master Out Slave In", 2, SRD_CHANNEL_SDATA, NULL},
    {"cs", "CS#", "Chip Select", 3, SRD_CHANNEL_COMMON, NULL},
};

static const char *spi_ann_labels[][3] = {
    {"", "DATA", "SPI data"},
};

static const int spi_row_data_classes[] = {0};
static const struct srd_c_ann_row spi_ann_rows[] = {
    {"data", "Data", spi_row_data_classes, 1},
};

static void spi_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(spi_state)));
    }
    spi_state *s = (spi_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(spi_state));
    s->cpol = 0;
    s->cpha = 0;
    s->bit_order = 0;
    s->cs_polarity = 0;
    s->cs_active = 0;
    s->last_clk = -1;
}

static void spi_start(struct srd_decoder_inst *di)
{
    spi_state *s = (spi_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "spi");

    s->cpol = (int)c_decoder_get_option_int(di, "cpol", 0);
    s->cpha = (int)c_decoder_get_option_int(di, "cpha", 0);
    s->bit_order = (strcmp(c_decoder_get_option_string(di, "bitorder", "msb-first"), "msb-first") == 0) ? 0 : 1;
    s->cs_polarity = (strcmp(c_decoder_get_option_string(di, "cs_polarity", "active-low"), "active-low") == 0) ? 0 : 1;

    s->have_miso = c_decoder_has_channel(di, 1);
    s->have_mosi = c_decoder_has_channel(di, 2);
    s->have_cs = c_decoder_has_channel(di, 3);

    int mode;
    if (s->cpol == 0 && s->cpha == 0) mode = 0;
    else if (s->cpol == 0 && s->cpha == 1) mode = 1;
    else if (s->cpol == 1 && s->cpha == 0) mode = 2;
    else mode = 3;

    s->sample_edge_rise = (mode == 0 || mode == 3) ? 1 : 0;
}

static void spi_reset_state(spi_state *s)
{
    s->bit_count = 0;
    s->mosi_byte = 0;
    s->miso_byte = 0;
}

static int spi_cs_asserted(spi_state *s, int cs_val)
{
    return (s->cs_polarity == 0) ? (cs_val == 0) : (cs_val == 1);
}

static void spi_decode(struct srd_decoder_inst *di)
{
    spi_state *s = (spi_state *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    int CLK = 0;
    int MISO = 1;
    int MOSI = 2;
    int CS = 3;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        if (s->sample_edge_rise)
            c_cond_rise(cb, CLK);
        else
            c_cond_fall(cb, CLK);
        int cs_cond_idx = -1;
        if (s->have_cs) {
            cs_cond_idx = 1;
            c_cond_or(cb);
            c_cond_edge(cb, CS);
        }
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);

        if (ret != SRD_OK)
            return;

        if (s->have_cs) {
            int cs_val = c_decoder_get_pin(di, CS, samplenum);
            if (matched & (1ULL << cs_cond_idx)) {
                spi_reset_state(s);
                s->cs_active = spi_cs_asserted(s, cs_val);
                continue;
            }
            s->cs_active = spi_cs_asserted(s, cs_val);
            if (!s->cs_active)
                continue;
        }

        if (s->bit_count == 0)
            s->start_sample = samplenum;

        if (s->have_mosi) {
            int mosi_val = c_decoder_get_pin(di, MOSI, samplenum);
            if (s->bit_order == 0)
                s->mosi_byte = (s->mosi_byte << 1) | mosi_val;
            else
                s->mosi_byte |= (mosi_val << s->bit_count);
        }

        if (s->have_miso) {
            int miso_val = c_decoder_get_pin(di, MISO, samplenum);
            if (s->bit_order == 0)
                s->miso_byte = (s->miso_byte << 1) | miso_val;
            else
                s->miso_byte |= (miso_val << s->bit_count);
        }

        s->bit_count++;

        if (s->bit_count == 8) {
            char mosi_str[16];
            char miso_str[16];
            snprintf(mosi_str, sizeof(mosi_str), "0x%02X", s->mosi_byte);
            snprintf(miso_str, sizeof(miso_str), "0x%02X", s->miso_byte);
            C_ANN_PUT(di, s->start_sample, samplenum, s->out_ann, 0, mosi_str, miso_str);
            spi_reset_state(s);
        }
    }
}

static void spi_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder spi_c_decoder = {
    .id = "spi_c",
    .name = "SPI(C)",
    .longname = "Serial Peripheral Interface (C)",
    .desc = "SPI protocol decoder (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = spi_channels,
    .num_channels = 1,
    .optional_channels = spi_optional_channels,
    .num_optional_channels = 3,
    .options = NULL,
    .num_options = 0,
    .num_annotations = 1,
    .ann_labels = spi_ann_labels,
    .num_annotation_rows = 1,
    .annotation_rows = spi_ann_rows,
    .reset = spi_reset,
    .start = spi_start,
    .decode = spi_decode,
    .destroy = spi_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &spi_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
