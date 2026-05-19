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
    int sda;
    uint64_t ss;
    uint64_t es;
} i2c_bit_entry;

typedef struct {
    enum i2c_state state;
    int bitcount;
    uint8_t databyte;
    uint64_t ss_byte;
    int wr;
    int is_repeat_start;
    uint64_t bitwidth;
    uint64_t pdu_start;
    int pdu_bits;
    uint64_t packet_ss;
    uint64_t packet_es;
    uint64_t packet_part_ss;
    uint8_t address;
    GArray *packet_data;
    char packet_str[2048];
    char packet_str_short[2048];
    i2c_bit_entry bits[8];
    int out_ann;
    int out_binary;
    int out_python;
    int address_shifted;
    int show_data_point;
} i2c_decoder_state;

#define SCL 0
#define SDA 1

#define ANN_START         0
#define ANN_REPEAT_START  1
#define ANN_STOP          2
#define ANN_ACK           3
#define ANN_NACK          4
#define ANN_BIT           5
#define ANN_ADDRESS_READ  6
#define ANN_ADDRESS_WRITE 7
#define ANN_DATA_READ     8
#define ANN_DATA_WRITE    9
#define ANN_PACKET        10
#define ANN_ATK_DATA      11
#define ANN_ATK_RISE      12

#define NUM_ANN 13

static struct srd_channel i2c_channels[] = {
    {"scl", "SCL", "Serial clock line", 0, SRD_CHANNEL_SCLK, NULL},
    {"sda", "SDA", "Serial data line", 1, SRD_CHANNEL_SDATA, NULL},
};

static const char *i2c_ann_labels[][3] = {
    {"", "START", "Start condition"},
    {"", "REPEAT START", "Repeat start condition"},
    {"", "STOP", "Stop condition"},
    {"", "ACK", "ACK"},
    {"", "NACK", "NACK"},
    {"", "BIT", "Data/address bit"},
    {"", "ADDRESS READ", "Address read"},
    {"", "ADDRESS WRITE", "Address write"},
    {"", "DATA READ", "Data read"},
    {"", "DATA WRITE", "Data write"},
    {"", "PACKET", "Packet"},
    {"", "ATK DATA", "ATK Data point"},
    {"", "ATK RISE", "ATK Rising edge"},
};

static const int i2c_row_bits_classes[] = {ANN_BIT, -1};
static const int i2c_row_addr_classes[] = {ANN_START, ANN_REPEAT_START, ANN_STOP, ANN_ACK, ANN_NACK, ANN_ADDRESS_READ, ANN_ADDRESS_WRITE, ANN_DATA_READ, ANN_DATA_WRITE, -1};
static const int i2c_row_pkt_classes[] = {ANN_PACKET, -1};
static const int i2c_row_atk_classes[] = {ANN_ATK_DATA, ANN_ATK_RISE, -1};
static const struct srd_c_ann_row i2c_ann_rows[] = {
    {"bits", "Bits", i2c_row_bits_classes, 1},
    {"addr-data", "Address/data", i2c_row_addr_classes, 9},
    {"packets", "Packets", i2c_row_pkt_classes, 1},
    {"atk-signs", "ATK signs", i2c_row_atk_classes, 2},
};

static struct srd_decoder_option i2c_options[] = {
    {"address_format", NULL, "Displayed slave address format", NULL, NULL},
    {"packets_format", NULL, "Display packets format", NULL, NULL},
    {"show_data_point", NULL, "Show data point", NULL, NULL},
};

static const struct srd_decoder_binary i2c_binary[] = {
    {0, "address-read", "Address read"},
    {1, "address-write", "Address write"},
    {2, "data-read", "Data read"},
    {3, "data-write", "Data write"},
};

static const char *i2c_inputs[] = {"logic", NULL};
static const char *i2c_outputs[] = {"i2c", NULL};
static const char *i2c_tags[] = {"Embedded/industrial", NULL};

static void i2c_reset_packet(i2c_decoder_state *s)
{
    if (s->packet_data)
        g_array_set_size(s->packet_data, 0);
    s->packet_str[0] = '\0';
    s->packet_str_short[0] = '\0';
    s->packet_ss = 0;
    s->packet_es = 0;
    s->packet_part_ss = 0;
    s->address = 0;
}

static void i2c_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(i2c_decoder_state)));
    }
    i2c_decoder_state *s = (i2c_decoder_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(i2c_decoder_state));
    s->state = STATE_FIND_START;
    s->wr = -1;
    s->is_repeat_start = 0;
    s->address_shifted = 1;
    s->show_data_point = 1;
    if (!s->packet_data)
        s->packet_data = g_array_new(FALSE, FALSE, sizeof(uint8_t));
    else
        g_array_set_size(s->packet_data, 0);
}

static void i2c_start(struct srd_decoder_inst *di)
{
    i2c_decoder_state *s = (i2c_decoder_state *)c_decoder_get_private(di);

    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "i2c");
    s->out_binary = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "i2c");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "i2c");

    const char *addr_fmt = c_decoder_get_option_string(di, "address_format", "shifted");
    s->address_shifted = (strcmp(addr_fmt, "shifted") == 0) ? 1 : 0;

    const char *show_dp = c_decoder_get_option_string(di, "show_data_point", "yes");
    s->show_data_point = (strcmp(show_dp, "yes") == 0) ? 1 : 0;
}

static void i2c_format_data_value(uint8_t v, char *out, int out_size)
{
    if (v >= 32 && v <= 126) {
        snprintf(out, out_size, "%c", v);
    } else {
        snprintf(out, out_size, "[%02X]", v);
    }
}

static void i2c_data_array_to_str(struct srd_decoder_inst *di, i2c_decoder_state *s, char *out, int out_size)
{
    const char *pkt_fmt = c_decoder_get_option_string(di, "packets_format", "hex");
    out[0] = '\0';
    int pos = 0;
    for (guint i = 0; i < s->packet_data->len && pos < out_size - 8; i++) {
        uint8_t v = g_array_index(s->packet_data, uint8_t, i);
        char tmp[16];
        if (pkt_fmt && strcmp(pkt_fmt, "hex") == 0) {
            snprintf(tmp, sizeof(tmp), "%02X", v);
        } else if (pkt_fmt && strcmp(pkt_fmt, "dec") == 0) {
            snprintf(tmp, sizeof(tmp), "%d", v);
        } else if (pkt_fmt && strcmp(pkt_fmt, "bin") == 0) {
            snprintf(tmp, sizeof(tmp), "%08b", v);
        } else if (pkt_fmt && strcmp(pkt_fmt, "oct") == 0) {
            snprintf(tmp, sizeof(tmp), "%03o", v);
        } else if (pkt_fmt && strcmp(pkt_fmt, "ascii") == 0) {
            i2c_format_data_value(v, tmp, sizeof(tmp));
        } else {
            snprintf(tmp, sizeof(tmp), "%02x", v);
        }
        if (i > 0 && pos < out_size - 2) {
            out[pos++] = ' ';
        }
        int len = strlen(tmp);
        if (pos + len >= out_size) break;
        memcpy(out + pos, tmp, len);
        pos += len;
    }
    out[pos] = '\0';
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void i2c_format_packet(struct srd_decoder_inst *di, i2c_decoder_state *s, char *pkt_str, int pkt_str_size,
                              char *pkt_short, int pkt_short_size)
{
    char data_str[128];
    i2c_data_array_to_str(di, s, data_str, sizeof(data_str));

    snprintf(pkt_str, pkt_str_size, "0x%02X %s: %s",
             s->address,
             (s->wr == 0) ? "RD" : "WR",
             data_str);

    snprintf(pkt_short, pkt_short_size, "%s", pkt_str + 2);

    if (s->packet_str[0]) {
        char full[4096];
        char full_short[4096];
        snprintf(full, sizeof(full), "%s [SR] %s", s->packet_str, pkt_str);
        snprintf(full_short, sizeof(full_short), "%s [SR] %s", s->packet_str_short, pkt_short);
        snprintf(pkt_str, pkt_str_size, "%s", full);
        snprintf(pkt_short, pkt_short_size, "%s", full_short);
    }
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void i2c_handle_packet(struct srd_decoder_inst *di, i2c_decoder_state *s, int is_start_repeat)
{
    const char *pkt_fmt = c_decoder_get_option_string(di, "packets_format", "hex");
    if (!pkt_fmt || strcmp(pkt_fmt, "none") == 0)
        return;

    if (s->packet_data->len == 0) {
        if (!is_start_repeat)
            i2c_reset_packet(s);
        return;
    }

    char pkt_str[4096];
    char pkt_short[4096];
    i2c_format_packet(di, s, pkt_str, sizeof(pkt_str), pkt_short, sizeof(pkt_short));

    if (is_start_repeat) {
        g_array_set_size(s->packet_data, 0);
        snprintf(s->packet_str, sizeof(s->packet_str), "%s", pkt_str);
        snprintf(s->packet_str_short, sizeof(s->packet_str_short), "%s", pkt_short);
    } else {
        C_ANN_PUT(di, s->packet_ss, s->packet_es, s->out_ann, ANN_PACKET, pkt_str, pkt_short);
        i2c_reset_packet(s);
    }
}
#pragma GCC diagnostic pop

static void i2c_handle_start(struct srd_decoder_inst *di, i2c_decoder_state *s, uint64_t samplenum)
{
    s->pdu_start = samplenum;
    s->pdu_bits = 0;

    if (s->is_repeat_start == 1) {
        C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_REPEAT_START, "Start repeat", "Sr");
        c_decoder_put_python(di, samplenum, samplenum, s->out_python, "START REPEAT", NULL, 0);
        i2c_handle_packet(di, s, 1);
    } else {
        C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_START, "Start", "S");
        c_decoder_put_python(di, samplenum, samplenum, s->out_python, "START", NULL, 0);
        i2c_handle_packet(di, s, 0);
        s->packet_ss = samplenum;
    }
    s->packet_part_ss = samplenum;

    s->state = STATE_FIND_ADDRESS;
    s->bitcount = 0;
    s->databyte = 0;
    s->is_repeat_start = 1;
    s->wr = -1;
}

static void i2c_handle_address_or_data(struct srd_decoder_inst *di, i2c_decoder_state *s,
                                        uint64_t samplenum, int sda_val)
{
    s->pdu_bits++;

    s->databyte = (s->databyte << 1) | sda_val;

    if (s->bitcount == 0)
        s->ss_byte = samplenum;

    for (int i = 7; i > 0; i--)
        s->bits[i] = s->bits[i - 1];
    s->bits[0].sda = sda_val;
    s->bits[0].ss = samplenum;
    s->bits[0].es = samplenum;

    if (s->bitcount > 0)
        s->bits[1].es = samplenum;

    if (s->bitcount == 7) {
        s->bitwidth = s->bits[1].es - s->bits[2].es;
        s->bits[0].es += s->bitwidth;
    }

    if (s->bitcount < 7) {
        s->bitcount++;
        return;
    }

    uint8_t d = s->databyte;
    if (s->state == STATE_FIND_ADDRESS) {
        s->wr = (d & 1) ? 0 : 1;
        if (s->address_shifted)
            d = d >> 1;
    }

    uint64_t byte_end = samplenum + s->bitwidth;

    int bin_class = -1;
    int ann_class = -1;
    char val_str[16];

    if (s->state == STATE_FIND_ADDRESS && s->wr == 1) {
        ann_class = ANN_ADDRESS_WRITE;
        bin_class = 1;
    } else if (s->state == STATE_FIND_ADDRESS && s->wr == 0) {
        ann_class = ANN_ADDRESS_READ;
        bin_class = 0;
    } else if (s->state == STATE_FIND_DATA) {
        if (s->wr == 1) {
            ann_class = ANN_DATA_WRITE;
            bin_class = 3;
        } else {
            ann_class = ANN_DATA_READ;
            bin_class = 2;
        }
        g_array_append_val(s->packet_data, d);
    }

    s->packet_es = byte_end;

    if (bin_class >= 0) {
        c_decoder_put_binary(di, s->ss_byte, byte_end, s->out_binary, bin_class, 1, &d);
    }

    if (s->state == STATE_FIND_ADDRESS) {
        c_decoder_put_python(di, s->ss_byte, byte_end, s->out_python,
            s->wr ? "ADDRESS WRITE" : "ADDRESS READ", &d, 1);
    } else if (s->state == STATE_FIND_DATA) {
        c_decoder_put_python(di, s->ss_byte, byte_end, s->out_python,
            s->wr ? "DATA WRITE" : "DATA READ", &d, 1);
    }

    for (int i = 0; i < 8; i++) {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", s->bits[i].sda);
        C_ANN_PUT(di, s->bits[i].ss, s->bits[i].es, s->out_ann, ANN_BIT, bit_str);
        if (s->show_data_point) {
            C_ANN_PUT(di, s->bits[i].ss, s->bits[i].ss, s->out_ann, ANN_ATK_DATA, "1");
            C_ANN_PUT(di, s->bits[i].ss, s->bits[i].ss, s->out_ann, ANN_ATK_RISE, "0");
        }
    }

    if (ann_class >= 0) {
        snprintf(val_str, sizeof(val_str), "%02X", d);

        if (ann_class == ANN_ADDRESS_WRITE || ann_class == ANN_ADDRESS_READ) {
            const char *w_long, *w_short, *w_tiny;
            if (s->wr) {
                w_long = "Write"; w_short = "Wr"; w_tiny = "W";
            } else {
                w_long = "Read"; w_short = "Rd"; w_tiny = "R";
            }
            C_ANN_PUT(di, samplenum, byte_end, s->out_ann, ann_class, w_long, w_short, w_tiny);

            if (s->show_data_point) {
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_ATK_DATA, "1");
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_ATK_RISE, "0");
            }

            char long_str[64];
            snprintf(long_str, sizeof(long_str), "%s: %s",
                     (ann_class == ANN_ADDRESS_WRITE) ? "Address write" : "Address read", val_str);
            C_ANN_PUT(di, s->ss_byte, samplenum, s->out_ann, ann_class, long_str, val_str, val_str);

            s->address = d;
        } else {
            char long_str[64];
            snprintf(long_str, sizeof(long_str), "%s: %s",
                     (ann_class == ANN_DATA_WRITE) ? "Data write" : "Data read", val_str);
            C_ANN_PUT(di, s->ss_byte, byte_end, s->out_ann, ann_class, long_str, val_str, val_str);
        }
    }

    s->bitcount = 0;
    s->databyte = 0;
    s->state = STATE_FIND_ACK;
}

static void i2c_get_ack(struct srd_decoder_inst *di, i2c_decoder_state *s,
                         uint64_t samplenum, int sda_val)
{
    uint64_t ack_end = samplenum + s->bitwidth;
    s->packet_es = ack_end;

    if (sda_val == 0) {
        C_ANN_PUT(di, samplenum, ack_end, s->out_ann, ANN_ACK, "ACK", "A");
        c_decoder_put_python(di, samplenum, ack_end, s->out_python, "ACK", NULL, 0);
    } else {
        C_ANN_PUT(di, samplenum, ack_end, s->out_ann, ANN_NACK, "NACK", "N");
        c_decoder_put_python(di, samplenum, ack_end, s->out_python, "NACK", NULL, 0);
    }

    s->state = STATE_FIND_DATA;
}

static void i2c_handle_stop(struct srd_decoder_inst *di, i2c_decoder_state *s, uint64_t samplenum)
{
    s->packet_es = samplenum;
    i2c_handle_packet(di, s, 0);
    C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_STOP, "Stop", "P");
    c_decoder_put_python(di, samplenum, samplenum, s->out_python, "STOP", NULL, 0);

    s->state = STATE_FIND_START;
    s->is_repeat_start = 0;
    s->wr = -1;
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

            i2c_handle_start(di, s, samplenum);
            break;
        }

        case STATE_FIND_ADDRESS: {
            cb = c_cond_new();
            c_cond_rise(cb, SCL);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            int sda_val = c_decoder_get_pin(di, SDA, samplenum);
            i2c_handle_address_or_data(di, s, samplenum, sda_val);
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
                i2c_handle_address_or_data(di, s, samplenum, sda_val);
            } else if (matched & (1ULL << 1)) {
                i2c_handle_start(di, s, samplenum);
            } else if (matched & (1ULL << 2)) {
                i2c_handle_stop(di, s, samplenum);
            }
            break;
        }

        case STATE_FIND_ACK: {
            cb = c_cond_new();
            c_cond_rise(cb, SCL);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            int sda_val = c_decoder_get_pin(di, SDA, samplenum);
            i2c_get_ack(di, s, samplenum, sda_val);
            break;
        }

        }
    }
}

static void i2c_destroy(struct srd_decoder_inst *di)
{
    i2c_decoder_state *s = (i2c_decoder_state *)c_decoder_get_private(di);
    if (s) {
        if (s->packet_data)
            g_array_free(s->packet_data, TRUE);
        g_free(s);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder i2c_c_decoder = {
    .id = "i2c_c",
    .name = "I²C(C)",
    .longname = "Inter-Integrated Circuit (C)",
    .desc = "I2C protocol decoder (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = i2c_channels,
    .num_channels = 2,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = i2c_options,
    .num_options = 3,
    .num_annotations = NUM_ANN,
    .ann_labels = i2c_ann_labels,
    .num_annotation_rows = 4,
    .annotation_rows = i2c_ann_rows,
    .inputs = i2c_inputs,
    .num_inputs = 1,
    .outputs = i2c_outputs,
    .num_outputs = 1,
    .binary = i2c_binary,
    .num_binary = 4,
    .tags = i2c_tags,
    .num_tags = 1,
    .reset = i2c_reset,
    .start = i2c_start,
    .decode = i2c_decode,
    .destroy = i2c_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    i2c_options[0].def = g_variant_new_string("shifted");
    i2c_options[1].def = g_variant_new_string("hex");
    i2c_options[2].def = g_variant_new_string("yes");

    GSList *addr_fmt_vals = NULL;
    addr_fmt_vals = g_slist_append(addr_fmt_vals, g_variant_new_string("shifted"));
    addr_fmt_vals = g_slist_append(addr_fmt_vals, g_variant_new_string("unshifted"));
    i2c_options[0].values = addr_fmt_vals;

    GSList *pkt_fmt_vals = NULL;
    pkt_fmt_vals = g_slist_append(pkt_fmt_vals, g_variant_new_string("none"));
    pkt_fmt_vals = g_slist_append(pkt_fmt_vals, g_variant_new_string("hex"));
    pkt_fmt_vals = g_slist_append(pkt_fmt_vals, g_variant_new_string("ascii"));
    pkt_fmt_vals = g_slist_append(pkt_fmt_vals, g_variant_new_string("dec"));
    pkt_fmt_vals = g_slist_append(pkt_fmt_vals, g_variant_new_string("bin"));
    pkt_fmt_vals = g_slist_append(pkt_fmt_vals, g_variant_new_string("oct"));
    i2c_options[1].values = pkt_fmt_vals;

    GSList *sdp_vals = NULL;
    sdp_vals = g_slist_append(sdp_vals, g_variant_new_string("yes"));
    sdp_vals = g_slist_append(sdp_vals, g_variant_new_string("no"));
    i2c_options[2].values = sdp_vals;

    return &i2c_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
