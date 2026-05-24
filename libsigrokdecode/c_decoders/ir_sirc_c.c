#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum sirc_ann {
    ANN_BIT = 0,
    ANN_AGC,
    ANN_PAUSE,
    ANN_START,
    ANN_CMD,
    ANN_ADDR,
    ANN_EXT,
    ANN_REMOTE,
    ANN_WARN,
    NUM_ANN,
};

#define IR_CH 0

#define AGC_USEC 2400.0
#define ONE_USEC 1200.0
#define ZERO_USEC 600.0
#define PAUSE_USEC 600.0
#define TOLERANCE 0.30

typedef struct {
    uint64_t samplerate;
    double snum_per_us;
    int active;
    int out_ann;
} sirc_state;

static struct srd_channel sirc_channels[] = {
    { "ir", "IR", "IR data line", 0, SRD_CHANNEL_SDATA, NULL },
};

static struct srd_decoder_option sirc_options[] = {
    { "polarity", NULL, "Polarity", NULL, NULL },
};

static const char* sirc_ann_labels[][3] = {
    { "", "bit", "Bit" },
    { "", "agc", "AGC" },
    { "", "pause", "Pause" },
    { "", "start", "Start" },
    { "", "command", "Command" },
    { "", "address", "Address" },
    { "", "extended", "Extended" },
    { "", "remote", "Remote" },
    { "", "warning", "Warning" },
};

static const int sirc_row_bits_classes[] = { ANN_BIT, ANN_AGC, ANN_PAUSE, -1 };
static const int sirc_row_fields_classes[] = { ANN_START, ANN_CMD, ANN_ADDR, ANN_EXT, -1 };
static const int sirc_row_remotes_classes[] = { ANN_REMOTE, -1 };
static const int sirc_row_warnings_classes[] = { ANN_WARN, -1 };
static const struct srd_c_ann_row sirc_ann_rows[] = {
    { "bits", "Bits", sirc_row_bits_classes, 3 },
    { "fields", "Fields", sirc_row_fields_classes, 4 },
    { "remotes", "Remotes", sirc_row_remotes_classes, 1 },
    { "warnings", "Warnings", sirc_row_warnings_classes, 1 },
};

static const char* sirc_inputs[] = { "logic", NULL };
static const char* sirc_outputs[] = { NULL };
static const char* sirc_tags[] = { "IR", NULL };

static int tolerance_check(sirc_state* s, uint64_t ss, uint64_t es, double expected)
{
    double microseconds = (double)(es - ss) / s->snum_per_us;
    double tol = expected * TOLERANCE;
    return (microseconds > (expected - tol)) && (microseconds < (expected + tol));
}

static uint16_t bitpack_lsb(uint8_t* bits, int count)
{
    uint16_t val = 0;
    int i;
    for (i = 0; i < count; i++)
        val |= ((uint16_t)bits[i] << i);
    return val;
}

static int read_pulse(struct srd_decoder_inst* di, sirc_state* s,
    int high, double time_us, uint64_t pulse_ss,
    uint64_t* pulse_es)
{
    uint64_t samplenum;
    uint64_t matched;
    uint64_t max_samples = (uint64_t)(time_us * 1.30 * s->snum_per_us);

    srd_cond_builder* cb = c_cond_new();
    if (high)
        c_cond_fall(cb, IR_CH);
    else
        c_cond_rise(cb, IR_CH);
    c_cond_or(cb);
    c_cond_skip(cb, max_samples);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK)
        return -1;

    *pulse_es = samplenum;

    if (matched & (1ULL << 1))
        return -2;

    if (!tolerance_check(s, pulse_ss, *pulse_es, time_us))
        return -2;

    return 0;
}

static int read_bit(struct srd_decoder_inst* di, sirc_state* s,
    uint64_t high_ss,
    int* bit_val, uint64_t* bit_ss, uint64_t* bit_es, int* good)
{
    uint64_t samplenum;
    uint64_t matched;
    uint64_t max_high_samples = (uint64_t)(2000.0 * s->snum_per_us);

    srd_cond_builder* cb = c_cond_new();
    if (s->active)
        c_cond_fall(cb, IR_CH);
    else
        c_cond_rise(cb, IR_CH);
    c_cond_or(cb);
    c_cond_skip(cb, max_high_samples);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK)
        return -1;

    uint64_t high_es = samplenum;

    if (matched & (1ULL << 1))
        return -2;

    if (tolerance_check(s, high_ss, high_es, ONE_USEC)) {
        *bit_val = 1;
    } else if (tolerance_check(s, high_ss, high_es, ZERO_USEC)) {
        *bit_val = 0;
    } else {
        return -2;
    }

    uint64_t low_es;
    int pause_ret = read_pulse(di, s, !s->active, PAUSE_USEC, high_es, &low_es);
    if (pause_ret == 0) {
        *good = 1;
        *bit_ss = high_ss;
        *bit_es = low_es;
    } else if (pause_ret == -2) {
        *good = 0;
        *bit_ss = high_ss;
        *bit_es = high_es + (uint64_t)(PAUSE_USEC * s->snum_per_us);
    } else {
        return -1;
    }

    {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", *bit_val);
        C_ANN_PUT(di, *bit_ss, *bit_es, s->out_ann, ANN_BIT, bit_str);
    }

    return 0;
}

static void sirc_metadata(struct srd_decoder_inst* di, int key, uint64_t value)
{
    sirc_state* s = (sirc_state*)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
        s->snum_per_us = (double)value / 1e6;
    }
}

static void sirc_reset(struct srd_decoder_inst* di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(sirc_state)));
    }
    sirc_state* s = (sirc_state*)c_decoder_get_private(di);
    memset(s, 0, sizeof(sirc_state));
}

static void sirc_start(struct srd_decoder_inst* di)
{
    sirc_state* s = (sirc_state*)c_decoder_get_private(di);

    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "ir_sirc");

    const char* polarity = c_decoder_get_option_string(di, "polarity", "active-low");
    if (polarity && strcmp(polarity, "active-high") == 0)
        s->active = 1;
    else
        s->active = 0;

    s->samplerate = c_decoder_get_samplerate(di);
    if (s->samplerate > 0)
        s->snum_per_us = (double)s->samplerate / 1e6;
}

static void sirc_decode(struct srd_decoder_inst* di)
{
    sirc_state* s = (sirc_state*)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    if (!s->samplerate) {
        s->samplerate = c_decoder_get_samplerate(di);
        if (s->samplerate > 0)
            s->snum_per_us = (double)s->samplerate / 1e6;
    }
    if (s->samplerate == 0)
        return;

    while (1) {
        srd_cond_builder* cb;
        int ret;

        cb = c_cond_new();
        if (s->active)
            c_cond_high(cb, IR_CH);
        else
            c_cond_low(cb, IR_CH);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        uint64_t frame_ss = samplenum;

        uint64_t agc_ss = samplenum;
        uint64_t agc_es;
        ret = read_pulse(di, s, s->active, AGC_USEC, agc_ss, &agc_es);
        if (ret != 0)
            continue;

        uint64_t pause_ss = agc_es;
        uint64_t pause_es;
        ret = read_pulse(di, s, !s->active, PAUSE_USEC, pause_ss, &pause_es);
        if (ret != 0)
            continue;

        C_ANN_PUT(di, agc_ss, agc_es, s->out_ann, ANN_AGC, "AGC", "A");
        C_ANN_PUT(di, pause_ss, pause_es, s->out_ann, ANN_PAUSE, "Pause", "P");
        C_ANN_PUT(di, agc_ss, pause_es, s->out_ann, ANN_START, "Start", "S");

        uint8_t bits[21];
        uint64_t bit_ss_arr[21];
        uint64_t bit_es_arr[21];
        int bit_count = 0;
        int error = 0;
        uint64_t next_bit_ss = pause_es;

        while (bit_count <= 20) {
            int bval;
            uint64_t bss, bes;
            int good;

            ret = read_bit(di, s, next_bit_ss, &bval, &bss, &bes, &good);
            if (ret == -1)
                return;
            if (ret == -2) {
                error = 1;
                break;
            }

            bits[bit_count] = bval;
            bit_ss_arr[bit_count] = bss;
            bit_es_arr[bit_count] = bes;
            bit_count++;

            if (!good)
                break;

            next_bit_ss = bes;
        }

        if (error || bit_count > 20) {
            cb = c_cond_new();
            c_cond_skip(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            C_ANN_PUT(di, frame_ss, samplenum, s->out_ann, ANN_WARN,
                "Error: too many bits", "Error", "E");
            continue;
        }

        uint8_t* command_bits;
        uint8_t* address_bits;
        uint8_t* extended_bits;
        int command_count;
        int address_count;
        int extended_count;

        if (bit_count == 12) {
            command_bits = bits;
            command_count = 7;
            address_bits = bits + 7;
            address_count = 5;
            extended_bits = NULL;
            extended_count = 0;
        } else if (bit_count == 15) {
            command_bits = bits;
            command_count = 7;
            address_bits = bits + 7;
            address_count = 8;
            extended_bits = NULL;
            extended_count = 0;
        } else if (bit_count == 20) {
            command_bits = bits;
            command_count = 7;
            address_bits = bits + 7;
            address_count = 5;
            extended_bits = bits + 12;
            extended_count = 8;
        } else {
            char err_str[64];
            snprintf(err_str, sizeof(err_str), "Error: incorrect bits count %d", bit_count);
            cb = c_cond_new();
            c_cond_skip(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            C_ANN_PUT(di, frame_ss, samplenum, s->out_ann, ANN_WARN, err_str, "Error", "E");
            continue;
        }

        uint8_t command_num = (uint8_t)bitpack_lsb(command_bits, command_count);
        uint16_t address_num = bitpack_lsb(address_bits, address_count);

        {
            char cmd_long[32], cmd_mid[16];
            snprintf(cmd_long, sizeof(cmd_long), "Command: 0x%02X", command_num);
            snprintf(cmd_mid, sizeof(cmd_mid), "C:0x%02X", command_num);
            C_ANN_PUT(di, bit_ss_arr[0], bit_es_arr[command_count - 1],
                s->out_ann, ANN_CMD, cmd_long, cmd_mid);
        }

        {
            char addr_long[32], addr_mid[16];
            int addr_hex_width = (address_count + 3) / 4;
            snprintf(addr_long, sizeof(addr_long), "Address: 0x%0*X", addr_hex_width, address_num);
            snprintf(addr_mid, sizeof(addr_mid), "A:0x%0*X", addr_hex_width, address_num);
            C_ANN_PUT(di, bit_ss_arr[command_count], bit_es_arr[command_count + address_count - 1],
                s->out_ann, ANN_ADDR, addr_long, addr_mid);
        }

        if (extended_count > 0 && extended_bits) {
            uint16_t extended_num = bitpack_lsb(extended_bits, extended_count);
            char ext_long[32], ext_mid[16];
            int ext_hex_width = (extended_count + 3) / 4;
            snprintf(ext_long, sizeof(ext_long), "Extended: 0x%0*X", ext_hex_width, extended_num);
            snprintf(ext_mid, sizeof(ext_mid), "E:0x%0*X", ext_hex_width, extended_num);
            C_ANN_PUT(di, bit_ss_arr[command_count + address_count],
                bit_es_arr[command_count + address_count + extended_count - 1],
                s->out_ann, ANN_EXT, ext_long, ext_mid);
        }

        {
            char remote_long[64], remote_mid[32];
            if (extended_count > 0) {
                uint16_t extended_num = bitpack_lsb(extended_bits, extended_count);
                snprintf(remote_long, sizeof(remote_long),
                    "Unknown Device: 0x%02X:0x%02X:0x%02X",
                    (uint8_t)address_num, command_num, (uint8_t)extended_num);
                snprintf(remote_mid, sizeof(remote_mid),
                    "UNK: 0x%02X:0x%02X:0x%02X",
                    (uint8_t)address_num, command_num, (uint8_t)extended_num);
            } else {
                int addr_hex_width = (address_count + 3) / 4;
                snprintf(remote_long, sizeof(remote_long),
                    "Unknown Device: 0x%0*X:0x%02X",
                    addr_hex_width, address_num, command_num);
                snprintf(remote_mid, sizeof(remote_mid),
                    "UNK: 0x%0*X:0x%02X",
                    addr_hex_width, address_num, command_num);
            }
            C_ANN_PUT(di, frame_ss, bit_es_arr[bit_count - 1],
                s->out_ann, ANN_REMOTE, remote_long, remote_mid);
        }
    }
}

static void sirc_destroy(struct srd_decoder_inst* di)
{
    void* priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder ir_sirc_c_decoder = {
    .id = "ir_sirc_c",
    .name = "IR SIRC(C)",
    .longname = "Sony IR (SIRC) (C)",
    .desc = "Sony infrared remote control protocol (SIRC). (C implementation)",
    .license = "gplv2+",
    .channels = sirc_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = sirc_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = sirc_ann_labels,
    .num_annotation_rows = 4,
    .annotation_rows = sirc_ann_rows,
    .inputs = sirc_inputs,
    .num_inputs = 1,
    .outputs = sirc_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = sirc_tags,
    .num_tags = 1,
    .metadata = sirc_metadata,
    .reset = sirc_reset,
    .start = sirc_start,
    .decode = sirc_decode,
    .destroy = sirc_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder* srd_c_decoder_entry(void)
{
    GVariant* polarity_vals[] = {
        g_variant_new_string("active-low"),
        g_variant_new_string("active-high"),
    };
    GSList* polarity_list = NULL;
    polarity_list = g_slist_append(polarity_list, polarity_vals[0]);
    polarity_list = g_slist_append(polarity_list, polarity_vals[1]);
    sirc_options[0].id = "polarity";
    sirc_options[0].idn = NULL;
    sirc_options[0].desc = "Polarity";
    sirc_options[0].def = g_variant_new_string("active-low");
    sirc_options[0].values = polarity_list;

    return &ir_sirc_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
