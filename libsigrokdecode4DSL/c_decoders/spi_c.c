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
} spi_state;

static struct srd_channel spi_channels[] = {
    {"clk", "CLK", "Clock", 0, SRD_CHANNEL_SCLK, NULL},
};

static struct srd_channel spi_optional_channels[] = {
    {"miso", "MISO", "Master In Slave Out", 1, SRD_CHANNEL_SDATA, NULL},
    {"mosi", "MOSI", "Master Out Slave In", 2, SRD_CHANNEL_SDATA, NULL},
    {"cs", "CS#", "Chip Select", 3, SRD_CHANNEL_COMMON, NULL},
};

static const char *spi_ann_labels[][2] = {
    {"DATA", "SPI data"},
};

static void spi_reset(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    if (!di->user_data) {
        di->user_data = g_malloc0(sizeof(spi_state));
    }
    spi_state *s = (spi_state *)di->user_data;
    memset(s, 0, sizeof(spi_state));
    s->cpol = 0;
    s->cpha = 0;
    s->bit_order = 0;
    s->cs_polarity = 0;
    s->cs_active = 0;
    s->last_clk = -1;
}

static void spi_start(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;

    c_decoder_register_output(di, SRD_OUTPUT_ANN, "spi");
}

static uint8_t get_pin(struct srd_decoder_inst *di, int ch, uint64_t samplenum)
{
    if (ch < 0 || ch >= di->dec_num_channels)
        return 0;
    int sig_idx = di->dec_channelmap[ch];
    if (sig_idx < 0 || !di->inbuf || !di->inbuf[sig_idx])
        return 0;
    uint64_t byte_offset = samplenum / 8;
    uint8_t bit_offset = samplenum % 8;
    return (di->inbuf[sig_idx][byte_offset] >> bit_offset) & 1;
}

static void spi_decode(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    spi_state *s = (spi_state *)di->user_data;
    uint64_t samplenum;
    uint64_t matched;

    int CLK = 0;
    int MISO = 1;
    int MOSI = 2;
    int CS = 3;

    while (1) {
        GSList *cond = NULL;
        struct srd_term *t;

        t = g_malloc0(sizeof(struct srd_term));
        t->type = SRD_TERM_RISING_EDGE;
        t->channel = CLK;
        GSList *term_list = g_slist_append(NULL, t);

        t = g_malloc0(sizeof(struct srd_term));
        t->type = SRD_TERM_FALLING_EDGE;
        t->channel = CLK;
        term_list = g_slist_append(term_list, t);

        cond = g_slist_append(NULL, term_list);

        int ret = c_decoder_wait(di, cond, &samplenum, &matched);

        g_slist_free(cond);

        if (ret != SRD_OK)
            return;

        int cs_val = (di->dec_num_channels > CS && di->dec_channelmap[CS] >= 0)
                     ? get_pin(di, CS, samplenum) : 1;

        s->cs_active = (s->cs_polarity == 0) ? (cs_val == 0) : (cs_val == 1);

        if (!s->cs_active)
            continue;

        int sample_edge;
        if (s->cpha == 0)
            sample_edge = (matched & 1) ? 1 : 0;
        else
            sample_edge = (matched & 2) ? 1 : 0;

        if (!sample_edge)
            continue;

        if (s->bit_count == 0)
            s->start_sample = samplenum;

        if (di->dec_num_channels > MOSI && di->dec_channelmap[MOSI] >= 0) {
            int mosi_val = get_pin(di, MOSI, samplenum);
            if (s->bit_order == 0)
                s->mosi_byte = (s->mosi_byte << 1) | mosi_val;
            else
                s->mosi_byte |= (mosi_val << s->bit_count);
        }

        if (di->dec_num_channels > MISO && di->dec_channelmap[MISO] >= 0) {
            int miso_val = get_pin(di, MISO, samplenum);
            if (s->bit_order == 0)
                s->miso_byte = (s->miso_byte << 1) | miso_val;
            else
                s->miso_byte |= (miso_val << s->bit_count);
        }

        s->bit_count++;

        if (s->bit_count == 8) {
            char mosi_str[16];
            char miso_str[16];
            char *ann_texts[3] = {NULL, NULL, NULL};

            snprintf(mosi_str, sizeof(mosi_str), "0x%02X", s->mosi_byte);
            snprintf(miso_str, sizeof(miso_str), "0x%02X", s->miso_byte);

            ann_texts[0] = mosi_str;
            ann_texts[1] = miso_str;

            struct srd_c_annotation ann;
            ann.ann_class = 0;
            ann.ann_text = ann_texts;

            c_decoder_put(di, s->start_sample, samplenum, 0, &ann);

            s->bit_count = 0;
            s->mosi_byte = 0;
            s->miso_byte = 0;
        }
    }
}

static void spi_destroy(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    if (di->user_data) {
        g_free(di->user_data);
        di->user_data = NULL;
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
    .num_annotation_rows = 0,
    .annotation_rows = NULL,
    .reset = spi_reset,
    .start = spi_start,
    .decode = spi_decode,
    .destroy = spi_destroy,
};
