#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum dmx_ann {
    ANN_BIT = 0,
    ANN_BREAK,
    ANN_MAB,
    ANN_STARTBIT,
    ANN_STOPBIT,
    ANN_STARTCODE,
    ANN_CHANNEL,
    ANN_INTERFRAME,
    ANN_INTERPACKET,
    ANN_DATA,
    ANN_ERROR,
    NUM_ANN,
};

enum dmx_state {
    FIND_BREAK,
    MARK_MAB,
    READ_BYTE,
    MARK_IFT,
};

#define DMX_CH 0

struct dmx_priv {
    int state;
    int invert;
    uint64_t samplerate;
    double sample_usec;
    int skip_per_bit;
    uint64_t run_start;
    int channel;
    int bit;
    uint8_t byte_val;
    int out_ann;
};

static struct srd_channel dmx_channels[] = {
    {"dmx", "DMX data", "Any DMX data line", 0, SRD_CHANNEL_SDATA, NULL},
};

static GVariant *invert_values[] = {
    NULL,
};

static struct srd_decoder_option dmx_options[] = {
    {
        .id = "invert",
        .idn = NULL,
        .desc = "Invert Signal?",
        .def = NULL,
        .values = NULL,
    },
};

static const char *dmx_ann_labels[][3] = {
    {"", "bit", "Bit"},
    {"", "break", "Break"},
    {"", "mab", "Mark after break"},
    {"", "startbit", "Start bit"},
    {"", "stopbits", "Stop bit"},
    {"", "startcode", "Start code"},
    {"", "channel", "Channel"},
    {"", "interframe", "Interframe"},
    {"", "interpacket", "Interpacket"},
    {"", "data", "Data"},
    {"", "error", "Error"},
};

static const int dmx_row_name_classes[] = {ANN_BREAK, ANN_MAB, ANN_STARTCODE, ANN_CHANNEL, ANN_INTERFRAME, ANN_INTERPACKET};
static const int dmx_row_data_classes[] = {ANN_DATA};
static const int dmx_row_bits_classes[] = {ANN_BIT, ANN_STARTBIT, ANN_STOPBIT};
static const int dmx_row_errors_classes[] = {ANN_ERROR};
static const struct srd_c_ann_row dmx_ann_rows[] = {
    {"name", "Logical", dmx_row_name_classes, 6},
    {"data", "Data", dmx_row_data_classes, 1},
    {"bits", "Bits", dmx_row_bits_classes, 3},
    {"errors", "Errors", dmx_row_errors_classes, 1},
};

static const char *dmx_inputs[] = {"logic"};
static const char *dmx_outputs[] = {"dmx512"};
static const char *dmx_tags[] = {"Embedded/industrial", "Lighting"};

static void dmx_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct dmx_priv)));
    }
    struct dmx_priv *s = (struct dmx_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct dmx_priv));
    s->state = FIND_BREAK;
    s->samplerate = 0;
    s->sample_usec = 0;
    s->skip_per_bit = 0;
    s->run_start = 0;
    s->channel = 0;
    s->bit = 0;
    s->byte_val = 0;
}

static void dmx_start(struct srd_decoder_inst *di)
{
    struct dmx_priv *s = (struct dmx_priv *)c_decoder_get_private(di);

    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "dmx512");

    const char *invert_str = c_decoder_get_option_string(di, "invert", "no");
    s->invert = (strcmp(invert_str, "yes") == 0) ? 1 : 0;

    s->samplerate = c_decoder_get_samplerate(di);
    if (s->samplerate > 0) {
        s->sample_usec = 1000000.0 / (double)s->samplerate;
        s->skip_per_bit = (int)(4.0 / s->sample_usec);
        if (s->skip_per_bit < 1)
            s->skip_per_bit = 1;
    }
}

static void dmx_decode(struct srd_decoder_inst *di)
{
    struct dmx_priv *s = (struct dmx_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;
    int ret;

    if (s->samplerate == 0 || s->skip_per_bit == 0)
        return;

    {
        srd_cond_builder *b = c_cond_new();
        if (s->invert)
            c_cond_high(b, DMX_CH);
        else
            c_cond_low(b, DMX_CH);
        ret = c_cond_wait(b, di, &samplenum, &matched);
        c_cond_free(b);
        if (ret != SRD_OK)
            return;
        s->run_start = samplenum;
    }

    while (1) {
        switch (s->state) {

        case FIND_BREAK: {
            srd_cond_builder *b = c_cond_new();
            if (s->invert)
                c_cond_fall(b, DMX_CH);
            else
                c_cond_rise(b, DMX_CH);
            ret = c_cond_wait(b, di, &samplenum, &matched);
            c_cond_free(b);
            if (ret != SRD_OK)
                return;

            {
                double runlen = (double)(samplenum - s->run_start) * s->sample_usec;
                if (runlen > 88.0 && runlen < 1000000.0) {
                    C_ANN_PUT(di, s->run_start, samplenum, s->out_ann, ANN_BREAK, "Break");
                    s->state = MARK_MAB;
                    s->channel = 0;
                } else if (runlen >= 1000000.0) {
                    C_ANN_PUT(di, s->run_start, samplenum, s->out_ann, ANN_ERROR, "Invalid break length");
                    srd_cond_builder *b2 = c_cond_new();
                    if (s->invert)
                        c_cond_high(b2, DMX_CH);
                    else
                        c_cond_low(b2, DMX_CH);
                    ret = c_cond_wait(b2, di, &samplenum, &matched);
                    c_cond_free(b2);
                    if (ret != SRD_OK)
                        return;
                    s->run_start = samplenum;
                } else {
                    srd_cond_builder *b2 = c_cond_new();
                    if (s->invert)
                        c_cond_high(b2, DMX_CH);
                    else
                        c_cond_low(b2, DMX_CH);
                    ret = c_cond_wait(b2, di, &samplenum, &matched);
                    c_cond_free(b2);
                    if (ret != SRD_OK)
                        return;
                    s->run_start = samplenum;
                }
            }
            break;
        }

        case MARK_MAB: {
            s->run_start = samplenum;
            {
                srd_cond_builder *b = c_cond_new();
                if (s->invert)
                    c_cond_rise(b, DMX_CH);
                else
                    c_cond_fall(b, DMX_CH);
                ret = c_cond_wait(b, di, &samplenum, &matched);
                c_cond_free(b);
                if (ret != SRD_OK)
                    return;
            }
            C_ANN_PUT(di, s->run_start, samplenum, s->out_ann, ANN_MAB, "MAB");
            s->state = READ_BYTE;
            s->channel = 0;
            s->bit = 0;
            s->byte_val = 0;
            s->run_start = samplenum;
            break;
        }

        case READ_BYTE: {
            int bit_pos[11];
            int bit_val[11];
            int i;

            for (i = 0; i < 11; i++) {
                uint64_t bit_start;
                uint64_t bit_end;
                int bval;

                if (i == 0) {
                    bit_start = samplenum;
                } else {
                    bit_start = s->run_start + (uint64_t)i * s->skip_per_bit;
                }
                bit_end = s->run_start + (uint64_t)(i + 1) * s->skip_per_bit;

                {
                    uint64_t sample_point = s->run_start + (uint64_t)i * s->skip_per_bit + s->skip_per_bit / 2;
                    srd_cond_builder *b = c_cond_new();
                    uint64_t skip_count = 0;
                    if (sample_point > samplenum)
                        skip_count = sample_point - samplenum;
                    c_cond_skip(b, skip_count);
                    ret = c_cond_wait(b, di, &samplenum, &matched);
                    c_cond_free(b);
                    if (ret != SRD_OK)
                        return;

                    {
                        int raw = c_decoder_get_pin(di, DMX_CH, samplenum);
                        bval = s->invert ? (!raw) : raw;
                    }
                }

                bit_pos[i] = i;
                bit_val[i] = bval;

                if (i == 0) {
                    s->byte_val = 0;
                } else if (i >= 9) {
                    if (bval != 1) {
                        if (i == 10) {
                            s->state = FIND_BREAK;
                        }
                    }
                } else {
                    s->byte_val |= (bval << (i - 1));
                }

                if (s->state != READ_BYTE)
                    break;

                if (i < 10) {
                    uint64_t remaining = bit_end - samplenum;
                    if (remaining > 0) {
                        srd_cond_builder *b2 = c_cond_new();
                        c_cond_skip(b2, remaining);
                        ret = c_cond_wait(b2, di, &samplenum, &matched);
                        c_cond_free(b2);
                        if (ret != SRD_OK)
                            return;
                    }
                }
            }

            if (s->state == READ_BYTE) {
                uint64_t byte_start = s->run_start;
                uint64_t byte_end = s->run_start + (uint64_t)11 * s->skip_per_bit;

                for (i = 0; i < 11; i++) {
                    uint64_t bs = s->run_start + (uint64_t)i * s->skip_per_bit;
                    uint64_t be = s->run_start + (uint64_t)(i + 1) * s->skip_per_bit;

                    if (i == 0) {
                        if (bit_val[i] == 0) {
                            C_ANN_PUT(di, bs, be, s->out_ann, ANN_STARTBIT, "Start bit");
                        } else {
                            C_ANN_PUT(di, bs, be, s->out_ann, ANN_ERROR, "Invalid start bit");
                        }
                    } else if (i >= 9) {
                        if (bit_val[i] == 1) {
                            C_ANN_PUT(di, bs, be, s->out_ann, ANN_STOPBIT, "Stop bit");
                        } else {
                            C_ANN_PUT(di, bs, be, s->out_ann, ANN_ERROR, "Invalid stop bit");
                        }
                    } else {
                        char bit_str[4];
                        snprintf(bit_str, sizeof(bit_str), "%d", bit_val[i]);
                        C_ANN_PUT(di, bs, be, s->out_ann, ANN_BIT, bit_str);
                    }
                }

                if (s->channel == 0) {
                    C_ANN_PUT(di, byte_start, byte_end, s->out_ann, ANN_STARTCODE, "Start code");
                } else {
                    char ch_str[32];
                    snprintf(ch_str, sizeof(ch_str), "Channel %d", s->channel);
                    C_ANN_PUT(di, byte_start, byte_end, s->out_ann, ANN_CHANNEL, ch_str);
                }

                {
                    char data_str[32];
                    snprintf(data_str, sizeof(data_str), "%d / 0x%02X", s->byte_val, s->byte_val);
                    uint64_t data_ss = s->run_start + s->skip_per_bit;
                    uint64_t data_es = byte_end - 2 * s->skip_per_bit;
                    C_ANN_PUT(di, data_ss, data_es, s->out_ann, ANN_DATA, data_str);
                }

                s->channel++;
                s->run_start = samplenum;
                s->state = MARK_IFT;
            }
            break;
        }

        case MARK_IFT: {
            s->run_start = samplenum;
            if (s->channel > 65535) {
                srd_cond_builder *b = c_cond_new();
                if (s->invert)
                    c_cond_high(b, DMX_CH);
                else
                    c_cond_low(b, DMX_CH);
                ret = c_cond_wait(b, di, &samplenum, &matched);
                c_cond_free(b);
                if (ret != SRD_OK)
                    return;
                C_ANN_PUT(di, s->run_start, samplenum, s->out_ann, ANN_INTERPACKET, "Interpacket");
                s->state = FIND_BREAK;
                s->run_start = samplenum;
            } else {
                int dmx_val = c_decoder_get_pin(di, DMX_CH, samplenum);
                int line_val = s->invert ? (!dmx_val) : dmx_val;
                if (line_val) {
                    srd_cond_builder *b = c_cond_new();
                    if (s->invert)
                        c_cond_high(b, DMX_CH);
                    else
                        c_cond_low(b, DMX_CH);
                    ret = c_cond_wait(b, di, &samplenum, &matched);
                    c_cond_free(b);
                    if (ret != SRD_OK)
                        return;
                    C_ANN_PUT(di, s->run_start, samplenum, s->out_ann, ANN_INTERFRAME, "Interframe");
                }
                s->state = READ_BYTE;
                s->bit = 0;
                s->byte_val = 0;
                s->run_start = samplenum;
            }
            break;
        }

        }
    }
}

static void dmx_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder dmx512_c_decoder = {
    .id = "dmx512_c",
    .name = "DMX512(C)",
    .longname = "Digital MultipleX 512 (C)",
    .desc = "DMX512 lighting protocol decoder (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = dmx_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = dmx_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = dmx_ann_labels,
    .num_annotation_rows = 4,
    .annotation_rows = dmx_ann_rows,
    .inputs = dmx_inputs,
    .num_inputs = 1,
    .outputs = dmx_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = dmx_tags,
    .num_tags = 2,
    .reset = dmx_reset,
    .start = dmx_start,
    .decode = dmx_decode,
    .destroy = dmx_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &dmx512_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
