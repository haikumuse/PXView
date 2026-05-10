#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum spi_ann {
    ANN_MISO_DATA = 0,
    ANN_MOSI_DATA,
    ANN_MISO_BIT,
    ANN_MOSI_BIT,
    ANN_WARNING,
    ANN_MISO_TRANSFER,
    ANN_MOSI_TRANSFER,
    ANN_ATK_DATA_POINT,
    ANN_ATK_RISING_EDGE,
    ANN_ATK_FALLING_EDGE,
    ANN_CS_CHANGE,
    NUM_ANN,
};

typedef struct {
    int bit_count;
    uint64_t mosi_byte;
    uint64_t miso_byte;
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
    int wordsize;
    int out_ann;
    int out_python;

    uint64_t miso_bits_ss[64];
    uint64_t miso_bits_es[64];
    int miso_bits_val[64];
    uint64_t mosi_bits_ss[64];
    uint64_t mosi_bits_es[64];
    int mosi_bits_val[64];
    uint64_t last_bit_sample;
    uint64_t transfer_start;
    int show_data_point;
    int format;
} spi_state;

static struct srd_channel spi_channels[] = {
    {"clk", "CLK", "Clock(串行时钟)", 0, SRD_CHANNEL_SCLK, NULL},
};

static struct srd_channel spi_optional_channels[] = {
    {"miso", "MISO", "Master in, slave out(主入从出)", 1, SRD_CHANNEL_SDATA, NULL},
    {"mosi", "MOSI", "Master out, slave in(主出从入)", 2, SRD_CHANNEL_SDATA, NULL},
    {"cs", "CS#", "Chip-select(片选信号)", 3, SRD_CHANNEL_COMMON, NULL},
};

static struct srd_decoder_option spi_options[] = {
    {"cs_polarity", NULL, "CS# polarity(片选极性)", NULL, NULL},
    {"cpol", NULL, "Clock polarity(时钟极性)", NULL, NULL},
    {"cpha", NULL, "Clock phase(时钟相位)", NULL, NULL},
    {"bitorder", NULL, "Bit order(位序)", NULL, NULL},
    {"wordsize", NULL, "Word size(字长)", NULL, NULL},
    {"format", NULL, "Data format(数据格式)", NULL, NULL},
    {"show_data_point", NULL, "Show data point(数据点显示)", NULL, NULL},
};

static const char *spi_ann_labels[][3] = {
    {"", "MISO", "MISO data"},
    {"", "MOSI", "MOSI data"},
    {"", "MISO bit", "MISO bit"},
    {"", "MOSI bit", "MOSI bit"},
    {"", "Warning", "Warning"},
    {"", "MISO transfer", "MISO transfer"},
    {"", "MOSI transfer", "MOSI transfer"},
    {"", "ATK Data point", "ATK Data point"},
    {"", "ATK Rising edge", "ATK Rising edge"},
    {"", "ATK Falling edge", "ATK Falling edge"},
    {"", "CS change", "CS change"},
};

static const int spi_row_miso_bits_classes[] = {ANN_MISO_BIT, -1};
static const int spi_row_miso_data_classes[] = {ANN_MISO_DATA, -1};
static const int spi_row_miso_transfer_classes[] = {ANN_MISO_TRANSFER, -1};
static const int spi_row_mosi_bits_classes[] = {ANN_MOSI_BIT, -1};
static const int spi_row_mosi_data_classes[] = {ANN_MOSI_DATA, -1};
static const int spi_row_mosi_transfer_classes[] = {ANN_MOSI_TRANSFER, -1};
static const int spi_row_other_classes[] = {ANN_WARNING, -1};
static const int spi_row_atk_classes[] = {ANN_ATK_DATA_POINT, ANN_ATK_RISING_EDGE, ANN_ATK_FALLING_EDGE, -1};
static const int spi_row_cs_change_classes[] = {ANN_CS_CHANGE, -1};

static const struct srd_c_ann_row spi_ann_rows[] = {
    {"miso-bits", "MISO bits", spi_row_miso_bits_classes, 1},
    {"miso-data-vals", "MISO data", spi_row_miso_data_classes, 1},
    {"miso-transfers", "MISO transfers", spi_row_miso_transfer_classes, 1},
    {"mosi-bits", "MOSI bits", spi_row_mosi_bits_classes, 1},
    {"mosi-data-vals", "MOSI data", spi_row_mosi_data_classes, 1},
    {"mosi-transfers", "MOSI transfers", spi_row_mosi_transfer_classes, 1},
    {"other", "Other", spi_row_other_classes, 1},
    {"atk-signs", "ATK signs", spi_row_atk_classes, 3},
    {"cs-change", "CS change", spi_row_cs_change_classes, 1},
};

static const char *spi_inputs[] = {"logic"};
static const char *spi_outputs[] = {"spi"};
static const char *spi_tags[] = {"Embedded/industrial"};

static void spi_format_value(uint64_t val, int wordsize, const char *fmt, char *out, int out_size)
{
    if (strcmp(fmt, "hex") == 0) {
        snprintf(out, out_size, "%0*llX", (wordsize + 3) / 4, (unsigned long long)val);
    } else if (strcmp(fmt, "dec") == 0) {
        snprintf(out, out_size, "%llu", (unsigned long long)val);
    } else if (strcmp(fmt, "oct") == 0) {
        snprintf(out, out_size, "%0*llo", (wordsize + 2) / 3, (unsigned long long)val);
    } else if (strcmp(fmt, "bin") == 0) {
        char tmp[65];
        for (int i = 0; i < wordsize; i++) {
            int bit_idx = (strcmp(fmt, "bin") == 0) ? (wordsize - 1 - i) : i;
            tmp[i] = ((val >> bit_idx) & 1) ? '1' : '0';
        }
        tmp[wordsize] = '\0';
        snprintf(out, out_size, "%s", tmp);
    } else {
        if (val >= 32 && val <= 126) {
            snprintf(out, out_size, "%c", (char)val);
        } else {
            snprintf(out, out_size, "%02llX", (unsigned long long)val);
        }
    }
}

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
    s->wordsize = 8;
    s->show_data_point = 1;
}

static void spi_start(struct srd_decoder_inst *di)
{
    spi_state *s = (spi_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "spi");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "spi");

    const char *cs_pol_str = c_decoder_get_option_string(di, "cs_polarity", "active-low");
    s->cs_polarity = (strcmp(cs_pol_str, "active-low") == 0) ? 0 : 1;

    s->cpol = (int)c_decoder_get_option_int(di, "cpol", 0);
    s->cpha = (int)c_decoder_get_option_int(di, "cpha", 0);

    const char *bitorder_str = c_decoder_get_option_string(di, "bitorder", "msb-first");
    s->bit_order = (strcmp(bitorder_str, "msb-first") == 0) ? 0 : 1;

    s->wordsize = (int)c_decoder_get_option_int(di, "wordsize", 8);
    if (s->wordsize < 1 || s->wordsize > 64)
        s->wordsize = 8;

    const char *show_dp_str = c_decoder_get_option_string(di, "show_data_point", "yes");
    s->show_data_point = (strcmp(show_dp_str, "yes") == 0) ? 1 : 0;

    const char *format_str = c_decoder_get_option_string(di, "format", "hex");
    if (strcmp(format_str, "hex") == 0)
        s->format = 0;
    else if (strcmp(format_str, "dec") == 0)
        s->format = 1;
    else if (strcmp(format_str, "oct") == 0)
        s->format = 2;
    else if (strcmp(format_str, "bin") == 0)
        s->format = 3;
    else if (strcmp(format_str, "ascii") == 0)
        s->format = 4;
    else
        s->format = 0;

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

static int spi_cs_asserted(spi_state *s, int cs_val)
{
    return (s->cs_polarity == 0) ? (cs_val == 0) : (cs_val == 1);
}

static void spi_reset_word(spi_state *s)
{
    s->bit_count = 0;
    s->mosi_byte = 0;
    s->miso_byte = 0;
}

static void spi_put_data(struct srd_decoder_inst *di, spi_state *s)
{
    const char *fmt;
    switch (s->format) {
        case 1: fmt = "dec"; break;
        case 2: fmt = "oct"; break;
        case 3: fmt = "bin"; break;
        case 4: fmt = "ascii"; break;
        default: fmt = "hex"; break;
    }
    char miso_str[128];
    char mosi_str[128];

    if (s->have_miso) {
        for (int i = 0; i < s->bit_count; i++) {
            char bit_str[4];
            snprintf(bit_str, sizeof(bit_str), "%d", s->miso_bits_val[i]);
            C_ANN_PUT(di, s->miso_bits_ss[i], s->miso_bits_es[i], s->out_ann, ANN_MISO_BIT, bit_str);
        }
        spi_format_value(s->miso_byte, s->wordsize, fmt, miso_str, sizeof(miso_str));
        C_ANN_PUT(di, s->start_sample, s->last_bit_sample, s->out_ann, ANN_MISO_DATA, miso_str);
    }

    if (s->have_mosi) {
        for (int i = 0; i < s->bit_count; i++) {
            char bit_str[4];
            snprintf(bit_str, sizeof(bit_str), "%d", s->mosi_bits_val[i]);
            C_ANN_PUT(di, s->mosi_bits_ss[i], s->mosi_bits_es[i], s->out_ann, ANN_MOSI_BIT, bit_str);
        }
        spi_format_value(s->mosi_byte, s->wordsize, fmt, mosi_str, sizeof(mosi_str));
        C_ANN_PUT(di, s->start_sample, s->last_bit_sample, s->out_ann, ANN_MOSI_DATA, mosi_str);
    }
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

    C_ANN_PUT(di, 0, 0, s->out_ann, ANN_ATK_DATA_POINT, "color:#F32FDC");
    C_ANN_PUT(di, 0, 0, s->out_ann, ANN_ATK_RISING_EDGE, "color:#F32FDC");
    C_ANN_PUT(di, 0, 0, s->out_ann, ANN_ATK_FALLING_EDGE, "color:#F32FDC");

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
                int was_active = s->cs_active;
                s->cs_active = spi_cs_asserted(s, cs_val);

                const char *cs_change_str = s->cs_active ? "CS active" : "CS inactive";
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_CS_CHANGE, cs_change_str);
                c_decoder_put_python(di, samplenum, samplenum, s->out_python, "CS-CHANGE", NULL, 0);

                if (was_active && !s->cs_active && s->transfer_start > 0 && s->bit_count == 0) {
                    if (s->have_miso) {
                        C_ANN_PUT(di, s->transfer_start, samplenum, s->out_ann, ANN_MISO_TRANSFER, "");
                    }
                    if (s->have_mosi) {
                        C_ANN_PUT(di, s->transfer_start, samplenum, s->out_ann, ANN_MOSI_TRANSFER, "");
                    }
                    c_decoder_put_python(di, s->transfer_start, samplenum, s->out_python, "TRANSFER", NULL, 0);
                }

                spi_reset_word(s);
                continue;
            }
            s->cs_active = spi_cs_asserted(s, cs_val);
            if (!s->cs_active)
                continue;
        }

        if (s->bit_count == 0) {
            s->start_sample = samplenum;
            if (s->have_cs && s->cs_active) {
                s->transfer_start = samplenum;
            } else if (!s->have_cs) {
                s->transfer_start = samplenum;
            }
        }

        if (s->bit_count > 0) {
            if (s->have_miso && s->bit_count > 0 && s->bit_count <= s->wordsize) {
                s->miso_bits_es[s->bit_count - 1] = samplenum;
            }
            if (s->have_mosi && s->bit_count > 0 && s->bit_count <= s->wordsize) {
                s->mosi_bits_es[s->bit_count - 1] = samplenum;
            }
        }
        s->last_bit_sample = samplenum;

        if (s->have_mosi) {
            int mosi_val = c_decoder_get_pin(di, MOSI, samplenum);
            if (s->bit_order == 0)
                s->mosi_byte = (s->mosi_byte << 1) | mosi_val;
            else
                s->mosi_byte |= ((uint64_t)mosi_val << s->bit_count);

            if (s->bit_count < s->wordsize) {
                s->mosi_bits_ss[s->bit_count] = samplenum;
                s->mosi_bits_es[s->bit_count] = samplenum;
                s->mosi_bits_val[s->bit_count] = mosi_val;
            }
        }

        if (s->have_miso) {
            int miso_val = c_decoder_get_pin(di, MISO, samplenum);
            if (s->bit_order == 0)
                s->miso_byte = (s->miso_byte << 1) | miso_val;
            else
                s->miso_byte |= ((uint64_t)miso_val << s->bit_count);

            if (s->bit_count < s->wordsize) {
                s->miso_bits_ss[s->bit_count] = samplenum;
                s->miso_bits_es[s->bit_count] = samplenum;
                s->miso_bits_val[s->bit_count] = miso_val;
            }
        }

        if (s->show_data_point) {
            char dp_str[8];
            if (s->sample_edge_rise) {
                snprintf(dp_str, sizeof(dp_str), "%d", CLK);
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_ATK_RISING_EDGE, dp_str);
            } else {
                snprintf(dp_str, sizeof(dp_str), "%d", CLK);
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_ATK_FALLING_EDGE, dp_str);
            }
            if (s->have_miso) {
                snprintf(dp_str, sizeof(dp_str), "%d", MISO);
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_ATK_DATA_POINT, dp_str);
            }
            if (s->have_mosi) {
                snprintf(dp_str, sizeof(dp_str), "%d", MOSI);
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_ATK_DATA_POINT, dp_str);
            }
        }

        s->bit_count++;

        if (s->bit_count == s->wordsize) {
            spi_put_data(di, s);
            unsigned char data_bytes[2];
            data_bytes[0] = (unsigned char)s->mosi_byte;
            data_bytes[1] = (unsigned char)s->miso_byte;
            c_decoder_put_python(di, s->start_sample, s->last_bit_sample, s->out_python, "DATA", data_bytes, 2);
            spi_reset_word(s);
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
    .options = spi_options,
    .num_options = 7,
    .num_annotations = NUM_ANN,
    .ann_labels = spi_ann_labels,
    .num_annotation_rows = 9,
    .annotation_rows = spi_ann_rows,
    .inputs = spi_inputs,
    .num_inputs = 1,
    .outputs = spi_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = spi_tags,
    .num_tags = 1,
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
