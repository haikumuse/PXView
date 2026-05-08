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

static const char *i2c_ann_labels[][2] = {
    {"START", "Start condition"},
    {"STOP", "Stop condition"},
    {"ACK", "ACK"},
    {"NACK", "NACK"},
    {"ADDRESS READ", "Address read"},
    {"ADDRESS WRITE", "Address write"},
    {"DATA READ", "Data read"},
    {"DATA WRITE", "Data write"},
};

static const char *i2c_inputs[] = {"logic", NULL};
static const char *i2c_outputs[] = {"i2c", NULL};
static const char *i2c_tags[] = {"Embedded/industrial", NULL};

static void i2c_reset(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    if (!di->user_data) {
        di->user_data = g_malloc0(sizeof(i2c_decoder_state));
    }
    i2c_decoder_state *s = (i2c_decoder_state *)di->user_data;
    memset(s, 0, sizeof(i2c_decoder_state));
    s->state = STATE_FIND_START;
    s->is_write = -1;
    s->is_repeat_start = 0;
    s->bitwidth = 0;
}

static void i2c_start(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    c_decoder_register_output(di, SRD_OUTPUT_ANN, "i2c");
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

static void i2c_decode(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    i2c_decoder_state *s = (i2c_decoder_state *)di->user_data;
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        GSList *cond = NULL;
        struct srd_term *t;
        int ret;

        switch (s->state) {

        case STATE_FIND_START: {
            t = g_malloc0(sizeof(struct srd_term));
            t->type = SRD_TERM_FALLING_EDGE;
            t->channel = SDA;
            GSList *term_list = g_slist_append(NULL, t);

            t = g_malloc0(sizeof(struct srd_term));
            t->type = SRD_TERM_HIGH;
            t->channel = SCL;
            term_list = g_slist_append(term_list, t);

            cond = g_slist_append(NULL, term_list);

            ret = c_decoder_wait(di, cond, &samplenum, &matched);
            g_slist_free(cond);
            if (ret != SRD_OK)
                return;

            {
                char *ann_text[] = {"Start", "S", NULL};
                struct srd_c_annotation ann = {ANN_START, ann_text};
                c_decoder_put(di, samplenum, samplenum, 0, &ann);
            }

            s->state = STATE_FIND_ADDRESS;
            s->bit_count = 0;
            s->databyte = 0;
            s->is_write = -1;
            s->is_repeat_start = 1;
            s->bitwidth = 0;
            break;
        }

        case STATE_FIND_ADDRESS: {
            t = g_malloc0(sizeof(struct srd_term));
            t->type = SRD_TERM_RISING_EDGE;
            t->channel = SCL;
            GSList *term_list = g_slist_append(NULL, t);
            cond = g_slist_append(NULL, term_list);

            ret = c_decoder_wait(di, cond, &samplenum, &matched);
            g_slist_free(cond);
            if (ret != SRD_OK)
                return;

            {
                int sda_val = get_pin(di, SDA, samplenum);

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
                        char *ann_text[] = {long_str, "AW", val_str, NULL};
                        struct srd_c_annotation ann = {ANN_ADDRESS_WRITE, ann_text};
                        c_decoder_put(di, s->byte_start_sample, byte_end, 0, &ann);
                    } else {
                        snprintf(long_str, sizeof(long_str),
                                 "Address read: %s", val_str);
                        char *ann_text[] = {long_str, "AR", val_str, NULL};
                        struct srd_c_annotation ann = {ANN_ADDRESS_READ, ann_text};
                        c_decoder_put(di, s->byte_start_sample, byte_end, 0, &ann);
                    }

                    s->bit_count = 0;
                    s->databyte = 0;
                    s->state = STATE_FIND_ACK;
                }
            }
            break;
        }

        case STATE_FIND_ACK: {
            t = g_malloc0(sizeof(struct srd_term));
            t->type = SRD_TERM_RISING_EDGE;
            t->channel = SCL;
            GSList *term_list = g_slist_append(NULL, t);
            cond = g_slist_append(NULL, term_list);

            ret = c_decoder_wait(di, cond, &samplenum, &matched);
            g_slist_free(cond);
            if (ret != SRD_OK)
                return;

            {
                int sda_val = get_pin(di, SDA, samplenum);
                uint64_t ack_end = samplenum
                    + (s->bitwidth > 0 ? s->bitwidth : 1);

                if (sda_val == 0) {
                    char *ann_text[] = {"ACK", "A", NULL};
                    struct srd_c_annotation ann = {ANN_ACK, ann_text};
                    c_decoder_put(di, samplenum, ack_end, 0, &ann);
                } else {
                    char *ann_text[] = {"NACK", "N", NULL};
                    struct srd_c_annotation ann = {ANN_NACK, ann_text};
                    c_decoder_put(di, samplenum, ack_end, 0, &ann);
                }

                s->state = STATE_FIND_DATA;
            }
            break;
        }

        case STATE_FIND_DATA: {
            t = g_malloc0(sizeof(struct srd_term));
            t->type = SRD_TERM_RISING_EDGE;
            t->channel = SCL;
            GSList *term_list0 = g_slist_append(NULL, t);

            t = g_malloc0(sizeof(struct srd_term));
            t->type = SRD_TERM_FALLING_EDGE;
            t->channel = SDA;
            GSList *term_list1 = g_slist_append(NULL, t);
            t = g_malloc0(sizeof(struct srd_term));
            t->type = SRD_TERM_HIGH;
            t->channel = SCL;
            term_list1 = g_slist_append(term_list1, t);

            t = g_malloc0(sizeof(struct srd_term));
            t->type = SRD_TERM_RISING_EDGE;
            t->channel = SDA;
            GSList *term_list2 = g_slist_append(NULL, t);
            t = g_malloc0(sizeof(struct srd_term));
            t->type = SRD_TERM_HIGH;
            t->channel = SCL;
            term_list2 = g_slist_append(term_list2, t);

            cond = g_slist_append(NULL, term_list0);
            cond = g_slist_append(cond, term_list1);
            cond = g_slist_append(cond, term_list2);

            ret = c_decoder_wait(di, cond, &samplenum, &matched);
            g_slist_free(cond);
            if (ret != SRD_OK)
                return;

            if (matched & (1ULL << 0)) {
                int sda_val = get_pin(di, SDA, samplenum);

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
                        char *ann_text[] = {long_str, "DW", val_str, NULL};
                        struct srd_c_annotation ann = {ANN_DATA_WRITE, ann_text};
                        c_decoder_put(di, s->byte_start_sample,
                                      byte_end, 0, &ann);
                    } else {
                        snprintf(long_str, sizeof(long_str),
                                 "Data read: %s", val_str);
                        char *ann_text[] = {long_str, "DR", val_str, NULL};
                        struct srd_c_annotation ann = {ANN_DATA_READ, ann_text};
                        c_decoder_put(di, s->byte_start_sample,
                                      byte_end, 0, &ann);
                    }

                    s->bit_count = 0;
                    s->databyte = 0;
                    s->state = STATE_FIND_ACK;
                }
            } else if (matched & (1ULL << 1)) {
                char *ann_text[] = {"Start repeat", "Sr", NULL};
                struct srd_c_annotation ann = {ANN_START, ann_text};
                c_decoder_put(di, samplenum, samplenum, 0, &ann);

                s->state = STATE_FIND_ADDRESS;
                s->bit_count = 0;
                s->databyte = 0;
                s->is_write = -1;
            } else if (matched & (1ULL << 2)) {
                char *ann_text[] = {"Stop", "P", NULL};
                struct srd_c_annotation ann = {ANN_STOP, ann_text};
                c_decoder_put(di, samplenum, samplenum, 0, &ann);

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

static void i2c_destroy(void *inst)
{
    struct srd_decoder_inst *di = (struct srd_decoder_inst *)inst;
    if (di->user_data) {
        g_free(di->user_data);
        di->user_data = NULL;
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
    .num_annotation_rows = 0,
    .annotation_rows = NULL,
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
