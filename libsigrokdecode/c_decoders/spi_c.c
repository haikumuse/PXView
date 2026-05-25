#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TRANSFER_BYTES 256

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
    NUM_ANN,
};

typedef struct {
    int bit_count;
    uint64_t mosi_byte;
    uint64_t miso_byte;
    uint64_t start_sample;
    uint64_t last_bit_sample;
    int cs_active;
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
    int out_binary;
    int out_bitrate;
    int format;
    int show_data_point;
    int cs_was_deasserted;
    uint64_t samplerate;
    int bw;
    int first_edge;  /* Skip first CLK edge (phantom edge at sample 0) */

    uint64_t miso_bits_ss[64];
    uint64_t miso_bits_es[64];
    int miso_bits_val[64];
    uint64_t mosi_bits_ss[64];
    uint64_t mosi_bits_es[64];
    int mosi_bits_val[64];

    uint64_t transfer_start;

    uint64_t misobytes_val[MAX_TRANSFER_BYTES];
    int misobytes_cnt;
    uint64_t mosibytes_val[MAX_TRANSFER_BYTES];
    int mosibytes_cnt;
} spi_state;

static struct srd_channel spi_channels[] = {
    { "clk", "CLK", "Clock(串行时钟)", 0, SRD_CHANNEL_SCLK, NULL },
};

static struct srd_channel spi_optional_channels[] = {
    { "miso", "MISO", "Master in, slave out(主入从出)", 1, SRD_CHANNEL_SDATA, NULL },
    { "mosi", "MOSI", "Master out, slave in(主出从入)", 2, SRD_CHANNEL_SDATA, NULL },
    { "cs", "CS#", "Chip-select(片选信号)", 3, SRD_CHANNEL_COMMON, NULL },
};

static struct srd_decoder_option spi_options[] = {
    { "cs_polarity", NULL, "CS# polarity(片选极性)", NULL, NULL },
    { "cpol", NULL, "Clock polarity(时钟极性)", NULL, NULL },
    { "cpha", NULL, "Clock phase(时钟相位)", NULL, NULL },
    { "bitorder", NULL, "Bit order(位序)", NULL, NULL },
    { "wordsize", NULL, "Word size(字长)", NULL, NULL },
    { "format", NULL, "Data format(数据格式)", NULL, NULL },
    { "show_data_point", NULL, "Show data point(数据点显示)", NULL, NULL },
};

static const char* spi_ann_labels[][3] = {
    { "", "MISO", "MISO data" },
    { "", "MOSI", "MOSI data" },
    { "", "MISO bit", "MISO bit" },
    { "", "MOSI bit", "MOSI bit" },
    { "", "Warning", "Warning" },
    { "", "MISO transfer", "MISO transfer" },
    { "", "MOSI transfer", "MOSI transfer" },
    { "", "ATK Data point", "ATK Data point" },
    { "", "ATK Rising edge", "ATK Rising edge" },
    { "", "ATK Falling edge", "ATK Falling edge" },
};

static const int spi_row_miso_bits_classes[] = { ANN_MISO_BIT, -1 };
static const int spi_row_miso_data_classes[] = { ANN_MISO_DATA, -1 };
static const int spi_row_miso_transfer_classes[] = { ANN_MISO_TRANSFER, -1 };
static const int spi_row_mosi_bits_classes[] = { ANN_MOSI_BIT, -1 };
static const int spi_row_mosi_data_classes[] = { ANN_MOSI_DATA, -1 };
static const int spi_row_mosi_transfer_classes[] = { ANN_MOSI_TRANSFER, -1 };
static const int spi_row_other_classes[] = { ANN_WARNING, -1 };
static const int spi_row_atk_classes[] = { ANN_ATK_DATA_POINT, ANN_ATK_RISING_EDGE, ANN_ATK_FALLING_EDGE, -1 };

static const struct srd_c_ann_row spi_ann_rows[] = {
    { "miso-bits", "MISO bits", spi_row_miso_bits_classes, 1 },
    { "miso-data-vals", "MISO data", spi_row_miso_data_classes, 1 },
    { "miso-transfers", "MISO transfers", spi_row_miso_transfer_classes, 1 },
    { "mosi-bits", "MOSI bits", spi_row_mosi_bits_classes, 1 },
    { "mosi-data-vals", "MOSI data", spi_row_mosi_data_classes, 1 },
    { "mosi-transfers", "MOSI transfers", spi_row_mosi_transfer_classes, 1 },
    { "other", "Other", spi_row_other_classes, 1 },
    { "atk-signs", "ATK signs", spi_row_atk_classes, 3 },
};

static const struct srd_decoder_binary spi_binary[] = {
    { 0, "miso", "MISO" },
    { 1, "mosi", "MOSI" },
};

static const char* spi_inputs[] = { "logic" };
static const char* spi_outputs[] = { "spi" };
static const char* spi_tags[] = { "Embedded/industrial" };

static void spi_format_value(uint64_t val, int wordsize, int format, char* out, int out_size)
{
    if (format == 0) {
        snprintf(out, out_size, "%02llx", (unsigned long long)val);
    } else if (format == 1) {
        snprintf(out, out_size, "%llu", (unsigned long long)val);
    } else if (format == 2) {
        snprintf(out, out_size, "%03llo", (unsigned long long)val);
    } else if (format == 3) {
        int width = wordsize > 8 ? wordsize : 8;
        char tmp[65];
        for (int i = 0; i < width; i++) {
            int bit_idx = width - 1 - i;
            tmp[i] = ((val >> bit_idx) & 1) ? '1' : '0';
        }
        tmp[width] = '\0';
        snprintf(out, out_size, "%s", tmp);
    } else {
        if (val >= 32 && val <= 126) {
            snprintf(out, out_size, "%c", (char)val);
        } else {
            snprintf(out, out_size, "%02llX", (unsigned long long)val);
        }
    }
}

static void spi_format_transfer(uint64_t* vals, int cnt, int format, int wordsize, char* out, int out_size)
{
    int pos = 0;
    out[0] = '\0';
    for (int i = 0; i < cnt && pos < out_size - 16; i++) {
        if (i > 0 && format != 4)
            pos += snprintf(out + pos, out_size - pos, " ");
        if (format == 0) {
            pos += snprintf(out + pos, out_size - pos, "%02llX", (unsigned long long)vals[i]);
        } else if (format == 1) {
            pos += snprintf(out + pos, out_size - pos, "%llu", (unsigned long long)vals[i]);
        } else if (format == 2) {
            pos += snprintf(out + pos, out_size - pos, "%03llo", (unsigned long long)vals[i]);
        } else if (format == 3) {
            char btmp[65];
            int bwidth = wordsize > 8 ? wordsize : 8;
            for (int b = 0; b < bwidth; b++) {
                int bit_idx = bwidth - 1 - b;
                btmp[b] = ((vals[i] >> bit_idx) & 1) ? '1' : '0';
            }
            btmp[bwidth] = '\0';
            pos += snprintf(out + pos, out_size - pos, "%s", btmp);
        } else {
            if (vals[i] >= 32 && vals[i] <= 126) {
                pos += snprintf(out + pos, out_size - pos, "%c", (char)vals[i]);
            } else {
                pos += snprintf(out + pos, out_size - pos, "\\x%02llX", (unsigned long long)vals[i]);
            }
        }
    }
}

static void spi_reset(struct srd_decoder_inst* di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(spi_state)));
    }
    spi_state* s = (spi_state*)c_decoder_get_private(di);
    memset(s, 0, sizeof(spi_state));
    s->cpol = 0;
    s->cpha = 0;
    s->bit_order = 0;
    s->cs_polarity = 0;
    s->cs_active = 0;
    s->wordsize = 8;
    s->show_data_point = 1;
    s->transfer_start = (uint64_t)-1;
    s->out_ann = -1;
    s->out_python = -1;
    s->out_binary = -1;
    s->out_bitrate = -1;
    s->first_edge = 1;
}

static void spi_start(struct srd_decoder_inst* di)
{
    spi_state* s = (spi_state*)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "spi");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "spi");
    s->out_binary = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "spi");
    s->out_bitrate = c_decoder_register_output(di, SRD_OUTPUT_META, "spi");

    const char* cs_pol_str = c_decoder_get_option_string(di, "cs_polarity", "active-low");
    s->cs_polarity = (strcmp(cs_pol_str, "active-low") == 0) ? 0 : 1;

    s->cpol = (int)c_decoder_get_option_int(di, "cpol", 0);
    s->cpha = (int)c_decoder_get_option_int(di, "cpha", 0);

    const char* bitorder_str = c_decoder_get_option_string(di, "bitorder", "msb-first");
    s->bit_order = (strcmp(bitorder_str, "msb-first") == 0) ? 0 : 1;

    s->wordsize = (int)c_decoder_get_option_int(di, "wordsize", 8);
    if (s->wordsize < 1 || s->wordsize > 64)
        s->wordsize = 8;

    s->bw = (s->wordsize + 7) / 8;

    const char* show_dp_str = c_decoder_get_option_string(di, "show_data_point", "yes");
    s->show_data_point = (strcmp(show_dp_str, "yes") == 0) ? 1 : 0;

    const char* format_str = c_decoder_get_option_string(di, "format", "hex");
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
    if (s->cpol == 0 && s->cpha == 0)
        mode = 0;
    else if (s->cpol == 0 && s->cpha == 1)
        mode = 1;
    else if (s->cpol == 1 && s->cpha == 0)
        mode = 2;
    else
        mode = 3;

    s->sample_edge_rise = (mode == 0 || mode == 3) ? 1 : 0;
}

static void spi_metadata(struct srd_decoder_inst* di, int key, uint64_t value)
{
    spi_state* s = (spi_state*)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
    }
}

static int spi_cs_asserted(spi_state* s, int cs_val)
{
    return (s->cs_polarity == 0) ? (cs_val == 0) : (cs_val == 1);
}

static void spi_reset_word(spi_state* s)
{
    s->bit_count = 0;
    s->mosi_byte = 0;
    s->miso_byte = 0;
}

static void spi_put_data(struct srd_decoder_inst* di, spi_state* s)
{
    uint64_t ss = s->start_sample;
    uint64_t es = s->last_bit_sample;

    if (s->have_miso) {
        uint64_t miso_ss = s->miso_bits_ss[0];
        uint64_t miso_es = s->miso_bits_es[s->wordsize - 1];
        unsigned char bdata[8];
        for (int i = 0; i < s->bw; i++)
            bdata[i] = (unsigned char)(s->miso_byte >> (8 * (s->bw - 1 - i)));
        c_decoder_put_binary(di, miso_ss, miso_es, s->out_binary, 0, s->bw, bdata);
        ss = miso_ss;
        es = miso_es;
    }
    if (s->have_mosi) {
        uint64_t mosi_ss = s->mosi_bits_ss[0];
        uint64_t mosi_es = s->mosi_bits_es[s->wordsize - 1];
        unsigned char bdata[8];
        for (int i = 0; i < s->bw; i++)
            bdata[i] = (unsigned char)(s->mosi_byte >> (8 * (s->bw - 1 - i)));
        c_decoder_put_binary(di, mosi_ss, mosi_es, s->out_binary, 1, s->bw, bdata);
        ss = mosi_ss;
        es = mosi_es;
    }

    {
        /* BITS format with per-bit timestamps:
         * data[0] = have_mosi (bit0) | have_miso (bit1)
         * data[1] = mosi_bit_count (uint8_t)
         * data[2..2+count*17-1] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
         * data[2+count*17] = 0x00 (reserved/alignment)
         * data[2+count*17+1] = miso_bit_count (uint8_t)
         * data[2+count*17+2..] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
         */
        int mosi_cnt = s->have_mosi ? s->wordsize : 0;
        int miso_cnt = s->have_miso ? s->wordsize : 0;
        unsigned char bits_data[2200];
        int bpos = 0;

        bits_data[bpos++] = (s->have_mosi ? 1 : 0) | (s->have_miso ? 2 : 0);
        bits_data[bpos++] = (unsigned char)mosi_cnt;

        for (int i = 0; i < mosi_cnt && bpos + 17 <= (int)sizeof(bits_data); i++) {
            bits_data[bpos++] = (unsigned char)s->mosi_bits_val[i];
            uint64_t ss_val = s->mosi_bits_ss[i];
            for (int b = 0; b < 8; b++)
                bits_data[bpos++] = (unsigned char)(ss_val >> (8 * b));
            uint64_t es_val = s->mosi_bits_es[i];
            for (int b = 0; b < 8; b++)
                bits_data[bpos++] = (unsigned char)(es_val >> (8 * b));
        }

        bits_data[bpos++] = 0x00;
        bits_data[bpos++] = (unsigned char)miso_cnt;

        for (int i = 0; i < miso_cnt && bpos + 17 <= (int)sizeof(bits_data); i++) {
            bits_data[bpos++] = (unsigned char)s->miso_bits_val[i];
            uint64_t ss_val = s->miso_bits_ss[i];
            for (int b = 0; b < 8; b++)
                bits_data[bpos++] = (unsigned char)(ss_val >> (8 * b));
            uint64_t es_val = s->miso_bits_es[i];
            for (int b = 0; b < 8; b++)
                bits_data[bpos++] = (unsigned char)(es_val >> (8 * b));
        }

        c_decoder_put_python(di, ss, es, s->out_python, "BITS", bits_data, bpos);
    }

    {
        unsigned char data_data[17];
        int dpos = 0;
        data_data[dpos++] = (s->have_mosi ? 1 : 0) | (s->have_miso ? 2 : 0);
        uint64_t mv = s->have_mosi ? s->mosi_byte : 0;
        uint64_t sv = s->have_miso ? s->miso_byte : 0;
        for (int i = 0; i < 8; i++)
            data_data[dpos++] = (unsigned char)(mv >> (8 * i));
        for (int i = 0; i < 8; i++)
            data_data[dpos++] = (unsigned char)(sv >> (8 * i));
        c_decoder_put_python(di, ss, es, s->out_python, "DATA", data_data, dpos);
    }

    if (s->have_miso && s->misobytes_cnt < MAX_TRANSFER_BYTES) {
        s->misobytes_val[s->misobytes_cnt] = s->miso_byte;
        s->misobytes_cnt++;
    }
    if (s->have_mosi && s->mosibytes_cnt < MAX_TRANSFER_BYTES) {
        s->mosibytes_val[s->mosibytes_cnt] = s->mosi_byte;
        s->mosibytes_cnt++;
    }

    if (s->have_miso) {
        for (int i = 0; i < s->bit_count; i++) {
            char bit_str[4];
            snprintf(bit_str, sizeof(bit_str), "%d", s->miso_bits_val[i]);
            C_ANN_PUT(di, s->miso_bits_ss[i], s->miso_bits_es[i], s->out_ann, ANN_MISO_BIT, bit_str);
        }
    }
    if (s->have_mosi) {
        for (int i = 0; i < s->bit_count; i++) {
            char bit_str[4];
            snprintf(bit_str, sizeof(bit_str), "%d", s->mosi_bits_val[i]);
            C_ANN_PUT(di, s->mosi_bits_ss[i], s->mosi_bits_es[i], s->out_ann, ANN_MOSI_BIT, bit_str);
        }
    }

    if (s->have_miso) {
        char miso_str[128];
        spi_format_value(s->miso_byte, s->wordsize, s->format, miso_str, sizeof(miso_str));
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_MISO_DATA, miso_str);
    }
    if (s->have_mosi) {
        char mosi_str[128];
        spi_format_value(s->mosi_byte, s->wordsize, s->format, mosi_str, sizeof(mosi_str));
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_MOSI_DATA, mosi_str);
    }
}

static void spi_decode(struct srd_decoder_inst* di)
{
    spi_state* s = (spi_state*)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    int CLK = 0;
    int MISO = 1;
    int MOSI = 2;
    int CS = 3;

    /* Get initial pin states at current position, like Python's self.wait({}) */
    uint64_t cur_sample;
    if (c_cond_wait_current(di, &cur_sample) != SRD_OK)
        return;

    C_ANN_PUT(di, cur_sample, cur_sample, s->out_ann, ANN_ATK_DATA_POINT, "color:#F32FDC");
    C_ANN_PUT(di, cur_sample, cur_sample, s->out_ann, ANN_ATK_RISING_EDGE, "color:#F32FDC");
    C_ANN_PUT(di, cur_sample, cur_sample, s->out_ann, ANN_ATK_FALLING_EDGE, "color:#F32FDC");

    if (!s->have_cs) {
        c_decoder_put_python(di, cur_sample, cur_sample, s->out_python, "CS-CHANGE", NULL, 0);
    }

    if (s->have_cs) {
        int cs = c_decoder_get_pin(di, CS, cur_sample);
        s->cs_active = spi_cs_asserted(s, cs);

        unsigned char cs_data[2];
        cs_data[0] = 0xFF;
        cs_data[1] = (unsigned char)cs;
        c_decoder_put_python(di, cur_sample, cur_sample, s->out_python, "CS-CHANGE", cs_data, 2);

        if (s->cs_active) {
            s->transfer_start = cur_sample;
            s->misobytes_cnt = 0;
            s->mosibytes_cnt = 0;
        }
    }

    while (1) {
        srd_cond_builder* cb = c_cond_new();
        /* Wait for ANY CLK edge, like Python's {0: 'e'}, then check edge type below */
        c_cond_edge(cb, CLK);

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

        int clk = c_decoder_get_pin(di, CLK, samplenum);
        int miso = s->have_miso ? c_decoder_get_pin(di, MISO, samplenum) : 0;
        int mosi = s->have_mosi ? c_decoder_get_pin(di, MOSI, samplenum) : 0;
        int cs = s->have_cs ? c_decoder_get_pin(di, CS, samplenum) : 1;

        int clk_matched = (matched & (1ULL << 0));
        int cs_matched = s->have_cs && (matched & (1ULL << cs_cond_idx));

        s->cs_active = s->have_cs ? spi_cs_asserted(s, cs) : 1;

        if (cs_matched) {
            unsigned char cs_data[2];
            cs_data[0] = (unsigned char)(1 - cs);
            cs_data[1] = (unsigned char)cs;
            c_decoder_put_python(di, samplenum, samplenum, s->out_python, "CS-CHANGE", cs_data, 2);

            if (s->cs_active) {
                s->transfer_start = samplenum;
                s->misobytes_cnt = 0;
                s->mosibytes_cnt = 0;
            } else if (s->transfer_start != (uint64_t)-1) {
                if (s->have_miso) {
                    char transfer_str[4096];
                    spi_format_transfer(s->misobytes_val, s->misobytes_cnt, s->format, s->wordsize, transfer_str, sizeof(transfer_str));
                    C_ANN_PUT(di, s->transfer_start, samplenum, s->out_ann, ANN_MISO_TRANSFER, transfer_str);
                }
                if (s->have_mosi) {
                    char transfer_str[4096];
                    spi_format_transfer(s->mosibytes_val, s->mosibytes_cnt, s->format, s->wordsize, transfer_str, sizeof(transfer_str));
                    C_ANN_PUT(di, s->transfer_start, samplenum, s->out_ann, ANN_MOSI_TRANSFER, transfer_str);
                }
                /* TRANSFER output with byte data */
                {
                    int mosi_cnt = s->have_mosi ? s->mosibytes_cnt : 0;
                    int miso_cnt = s->have_miso ? s->misobytes_cnt : 0;
                    int data_len = 5 + mosi_cnt + miso_cnt;
                    unsigned char* xfer_data = (unsigned char*)g_malloc(data_len);
                    int xpos = 0;
                    xfer_data[xpos++] = (unsigned char)((s->have_mosi ? 1 : 0) | (s->have_miso ? 2 : 0));
                    xfer_data[xpos++] = (unsigned char)(mosi_cnt & 0xFF);
                    xfer_data[xpos++] = (unsigned char)((mosi_cnt >> 8) & 0xFF);
                    xfer_data[xpos++] = (unsigned char)(miso_cnt & 0xFF);
                    xfer_data[xpos++] = (unsigned char)((miso_cnt >> 8) & 0xFF);
                    for (int i = 0; i < mosi_cnt; i++)
                        xfer_data[xpos++] = (unsigned char)s->mosibytes_val[i];
                    for (int i = 0; i < miso_cnt; i++)
                        xfer_data[xpos++] = (unsigned char)s->misobytes_val[i];
                    c_decoder_put_python(di, s->transfer_start, samplenum, s->out_python, "TRANSFER", xfer_data, xpos);
                    g_free(xfer_data);
                }
            }

            spi_reset_word(s);
        }

        if (s->have_cs && !s->cs_active)
            continue;

        if (!clk_matched)
            continue;

        /* Skip first CLK edge (phantom edge at sample 0), matching Python's
         * find_clk_edge(first=True) which returns early on the first sample */
        if (s->first_edge) {
            s->first_edge = 0;
            continue;
        }

        int mode;
        if (s->cpol == 0 && s->cpha == 0)
            mode = 0;
        else if (s->cpol == 0 && s->cpha == 1)
            mode = 1;
        else if (s->cpol == 1 && s->cpha == 0)
            mode = 2;
        else
            mode = 3;

        int correct_edge = 0;
        if ((mode == 0 && clk == 1) || (mode == 3 && clk == 1))
            correct_edge = 1;
        else if ((mode == 1 && clk == 0) || (mode == 2 && clk == 0))
            correct_edge = 2;

        if (!correct_edge)
            continue;

        if (s->show_data_point) {
            char dp_str[8];
            if (correct_edge == 1) {
                snprintf(dp_str, sizeof(dp_str), "%d", CLK);
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_ATK_RISING_EDGE, dp_str);
            } else {
                snprintf(dp_str, sizeof(dp_str), "%d", CLK);
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_ATK_FALLING_EDGE, dp_str);
            }
            snprintf(dp_str, sizeof(dp_str), "%d", MISO);
            C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_ATK_DATA_POINT, dp_str);
            snprintf(dp_str, sizeof(dp_str), "%d", MOSI);
            C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_ATK_DATA_POINT, dp_str);
        }

        if (s->bit_count == 0) {
            s->start_sample = samplenum;
            s->cs_was_deasserted = s->have_cs ? !spi_cs_asserted(s, cs) : 0;
        }

        if (s->bit_count > 0) {
            if (s->have_miso && s->bit_count <= s->wordsize)
                s->miso_bits_es[s->bit_count - 1] = samplenum;
            if (s->have_mosi && s->bit_count <= s->wordsize)
                s->mosi_bits_es[s->bit_count - 1] = samplenum;
        }
        s->last_bit_sample = samplenum;

        if (s->have_mosi) {
            if (s->bit_order == 0)
                s->mosi_byte = (s->mosi_byte << 1) | mosi;
            else
                s->mosi_byte |= ((uint64_t)mosi << s->bit_count);

            if (s->bit_count < s->wordsize) {
                s->mosi_bits_ss[s->bit_count] = samplenum;
                s->mosi_bits_es[s->bit_count] = samplenum;
                s->mosi_bits_val[s->bit_count] = mosi;
            }
        }

        if (s->have_miso) {
            if (s->bit_order == 0)
                s->miso_byte = (s->miso_byte << 1) | miso;
            else
                s->miso_byte |= ((uint64_t)miso << s->bit_count);

            if (s->bit_count < s->wordsize) {
                s->miso_bits_ss[s->bit_count] = samplenum;
                s->miso_bits_es[s->bit_count] = samplenum;
                s->miso_bits_val[s->bit_count] = miso;
            }
        }

        s->bit_count++;

        if (s->bit_count != s->wordsize)
            continue;

        spi_put_data(di, s);

        if (s->samplerate > 0) {
            double elapsed = 1.0 / (double)s->samplerate;
            elapsed *= (double)(samplenum - s->start_sample + 1);
            int bitrate = (int)(1.0 / elapsed * s->wordsize);
            c_decoder_put_meta_int(di, s->start_sample, samplenum, s->out_bitrate, bitrate);
        }

        if (s->have_cs && s->cs_was_deasserted) {
            C_ANN_PUT(di, s->start_sample, samplenum, s->out_ann, ANN_WARNING,
                "CS# was deasserted during this data word!");
        }

        spi_reset_word(s);
    }
}

static void spi_destroy(struct srd_decoder_inst* di)
{
    void* priv = c_decoder_get_private(di);
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
    .num_annotation_rows = 8,
    .annotation_rows = spi_ann_rows,
    .inputs = spi_inputs,
    .num_inputs = 1,
    .outputs = spi_outputs,
    .num_outputs = 1,
    .binary = spi_binary,
    .num_binary = 2,
    .tags = spi_tags,
    .num_tags = 1,
    .reset = spi_reset,
    .start = spi_start,
    .decode = spi_decode,
    .destroy = spi_destroy,
    .metadata = spi_metadata,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder* srd_c_decoder_entry(void)
{
    return &spi_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
