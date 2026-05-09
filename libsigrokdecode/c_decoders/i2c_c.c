#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum i2c_state {
    STATE_FIND_START,
    STATE_FIND_ADDRESS,
    STATE_FIND_DATA,
    STATE_FIND_ACK,
};

typedef struct {
    enum i2c_state state;
    int bit_count;
    uint8_t databyte;
    uint64_t byte_start_sample;
    int is_write;
    int is_repeat_start;
    uint64_t bitwidth;
    uint64_t last_bit_sample;
} i2c_decoder_state;

#define SCL 0
#define SDA 1

#define ANN_START         0
#define ANN_STOP          1
#define ANN_ACK           2
#define ANN_NACK          3
#define ANN_ADDRESS_READ  4
#define ANN_ADDRESS_WRITE 5
#define ANN_DATA_READ     6
#define ANN_DATA_WRITE    7

static struct srd_channel i2c_channels[] = {
    {"scl", "SCL", "Serial clock line", 0, SRD_CHANNEL_SCLK, NULL},
    {"sda", "SDA", "Serial data line", 1, SRD_CHANNEL_SDATA, NULL},
};

static const char *i2c_ann_labels[][3] = {
    {"", "START", "Start condition"},
    {"", "STOP", "Stop condition"},
    {"", "ACK", "ACK"},
    {"", "NACK", "NACK"},
    {"", "ADDRESS READ", "Address read"},
    {"", "ADDRESS WRITE", "Address write"},
    {"", "DATA READ", "Data read"},
    {"", "DATA WRITE", "Data write"},
};

static const int i2c_row_bits_classes[] = {ANN_START, ANN_STOP, -1};
static const int i2c_row_data_classes[] = {ANN_ADDRESS_READ, ANN_ADDRESS_WRITE, ANN_DATA_READ, ANN_DATA_WRITE, -1};
static const int i2c_row_ack_classes[] = {ANN_ACK, ANN_NACK, -1};
static const struct srd_c_ann_row i2c_ann_rows[] = {
    {"bits", "Bits", i2c_row_bits_classes, 3},
    {"data", "Data", i2c_row_data_classes, 4},
    {"ack", "ACK/NACK", i2c_row_ack_classes, 2},
};

static const char *i2c_inputs[] = {"logic", NULL};
static const char *i2c_outputs[] = {"i2c", NULL};
static const char *i2c_tags[] = {"Embedded/industrial", NULL};

static void i2c_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(i2c_decoder_state)));
    }
    i2c_decoder_state *s = (i2c_decoder_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(i2c_decoder_state));
    s->state = STATE_FIND_START;
    s->is_write = -1;
    s->is_repeat_start = 0;
    s->bitwidth = 0;
}

static void i2c_start(struct srd_decoder_inst *di)
{
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "i2c");
}

static void i2c_decode(struct srd_decoder_inst *di)
{
    i2c_decoder_state *s = (i2c_decoder_state *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        srd_cond_builder *cb;
        int ret;

        switch (s->state) {

        case STATE_FIND_START: {
            cb = c_cond_new();
            c_cond_fall(cb, SDA);
            c_cond_high(cb, SCL);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            C_ANN_PUT(di, samplenum, samplenum, 0, ANN_START, "Start", "S");

            s->state = STATE_FIND_ADDRESS;
            s->bit_count = 0;
            s->databyte = 0;
            s->is_write = -1;
            s->is_repeat_start = 1;
            s->bitwidth = 0;
            break;
        }

        case STATE_FIND_ADDRESS: {
            cb = c_cond_new();
            c_cond_rise(cb, SCL);
            c_cond_or(cb);
            c_cond_fall(cb, SDA);
            c_cond_high(cb, SCL);
            c_cond_or(cb);
            c_cond_rise(cb, SDA);
            c_cond_high(cb, SCL);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            if (matched & (1ULL << 0)) {
                int sda_val = c_decoder_get_pin(di, SDA, samplenum);

                if (s->bit_count == 0)
                    s->byte_start_sample = samplenum;

                s->databyte = (s->databyte << 1) | sda_val;
                s->bit_count++;

                if (s->bit_count > 1)
                    s->bitwidth = samplenum - s->last_bit_sample;
                s->last_bit_sample = samplenum;

                if (s->bit_count == 8) {
                    uint8_t d = s->databyte;
                    s->is_write = (d & 1) ? 0 : 1;
                    uint8_t addr = d >> 1;

                    uint64_t byte_end = samplenum
                        + (s->bitwidth > 0 ? s->bitwidth : 1);

                    char val_str[16];
                    char long_str[32];
                    snprintf(val_str, sizeof(val_str), "%02X", addr);

                    if (s->is_write) {
                        snprintf(long_str, sizeof(long_str),
                                 "Address write: %s", val_str);
                        C_ANN_PUT(di, s->byte_start_sample, byte_end, 0, ANN_ADDRESS_WRITE, long_str, "AW", val_str);
                    } else {
                        snprintf(long_str, sizeof(long_str),
                                 "Address read: %s", val_str);
                        C_ANN_PUT(di, s->byte_start_sample, byte_end, 0, ANN_ADDRESS_READ, long_str, "AR", val_str);
                    }

                    s->bit_count = 0;
                    s->databyte = 0;
                    s->state = STATE_FIND_ACK;
                }
            } else if (matched & (1ULL << 1)) {
                C_ANN_PUT(di, samplenum, samplenum, 0, ANN_START, "Start repeat", "Sr");

                s->state = STATE_FIND_ADDRESS;
                s->bit_count = 0;
                s->databyte = 0;
                s->is_write = -1;
            } else if (matched & (1ULL << 2)) {
                C_ANN_PUT(di, samplenum, samplenum, 0, ANN_STOP, "Stop", "P");

                s->state = STATE_FIND_START;
                s->bit_count = 0;
                s->databyte = 0;
                s->is_repeat_start = 0;
                s->is_write = -1;
            }
            break;
        }

        case STATE_FIND_ACK: {
            cb = c_cond_new();
            c_cond_rise(cb, SCL);
            c_cond_or(cb);
            c_cond_rise(cb, SDA);
            c_cond_high(cb, SCL);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            if (matched & (1ULL << 0)) {
                int sda_val = c_decoder_get_pin(di, SDA, samplenum);
                uint64_t ack_end = samplenum
                    + (s->bitwidth > 0 ? s->bitwidth : 1);

                if (sda_val == 0) {
                    C_ANN_PUT(di, samplenum, ack_end, 0, ANN_ACK, "ACK", "A");
                } else {
                    C_ANN_PUT(di, samplenum, ack_end, 0, ANN_NACK, "NACK", "N");
                }

                s->state = STATE_FIND_DATA;
            } else if (matched & (1ULL << 1)) {
                C_ANN_PUT(di, samplenum, samplenum, 0, ANN_STOP, "Stop", "P");

                s->state = STATE_FIND_START;
                s->bit_count = 0;
                s->databyte = 0;
                s->is_repeat_start = 0;
                s->is_write = -1;
            }
            break;
        }

        case STATE_FIND_DATA: {
            cb = c_cond_new();
            c_cond_rise(cb, SCL);
            c_cond_or(cb);
            c_cond_fall(cb, SDA);
            c_cond_high(cb, SCL);
            c_cond_or(cb);
            c_cond_rise(cb, SDA);
            c_cond_high(cb, SCL);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            if (matched & (1ULL << 0)) {
                int sda_val = c_decoder_get_pin(di, SDA, samplenum);

                if (s->bit_count == 0)
                    s->byte_start_sample = samplenum;

                s->databyte = (s->databyte << 1) | sda_val;
                s->bit_count++;

                if (s->bit_count > 1)
                    s->bitwidth = samplenum - s->last_bit_sample;
                s->last_bit_sample = samplenum;

                if (s->bit_count == 8) {
                    uint8_t d = s->databyte;
                    uint64_t byte_end = samplenum
                        + (s->bitwidth > 0 ? s->bitwidth : 1);

                    char val_str[16];
                    char long_str[32];
                    snprintf(val_str, sizeof(val_str), "%02X", d);

                    if (s->is_write) {
                        snprintf(long_str, sizeof(long_str),
                                 "Data write: %s", val_str);
                        C_ANN_PUT(di, s->byte_start_sample, byte_end, 0, ANN_DATA_WRITE, long_str, "DW", val_str);
                    } else {
                        snprintf(long_str, sizeof(long_str),
                                 "Data read: %s", val_str);
                        C_ANN_PUT(di, s->byte_start_sample, byte_end, 0, ANN_DATA_READ, long_str, "DR", val_str);
                    }

                    s->bit_count = 0;
                    s->databyte = 0;
                    s->state = STATE_FIND_ACK;
                }
            } else if (matched & (1ULL << 1)) {
                C_ANN_PUT(di, samplenum, samplenum, 0, ANN_START, "Start repeat", "Sr");

                s->state = STATE_FIND_ADDRESS;
                s->bit_count = 0;
                s->databyte = 0;
                s->is_write = -1;
            } else if (matched & (1ULL << 2)) {
                C_ANN_PUT(di, samplenum, samplenum, 0, ANN_STOP, "Stop", "P");

                s->state = STATE_FIND_START;
                s->bit_count = 0;
                s->databyte = 0;
                s->is_repeat_start = 0;
                s->is_write = -1;
            }
            break;
        }

        }
    }
}

static void i2c_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder i2c_c_decoder = {
    .id = "i2c_c",
    .name = "I2C(C)",
    .longname = "Inter-Integrated Circuit (C)",
    .desc = "I2C protocol decoder (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = i2c_channels,
    .num_channels = 2,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = 8,
    .ann_labels = i2c_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = i2c_ann_rows,
    .inputs = i2c_inputs,
    .num_inputs = 1,
    .outputs = i2c_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = i2c_tags,
    .num_tags = 1,
    .reset = i2c_reset,
    .start = i2c_start,
    .decode = i2c_decode,
    .destroy = i2c_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &i2c_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
