#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_DATA = 0,
    ANN_CONTROL,
    ANN_ERROR,
    ANN_INLINE_ERROR,
    NUM_ANN,
};

enum {
    FIND_BREAK,
    SYNC,
    PID,
    DATA,
    CHECKSUM,
};

struct lin_priv {
    int state;
    uint64_t samplerate;
    int baudrate;
    int bit_time;
    int version;
    uint64_t ss_break;
    uint64_t ss_sync;
    uint64_t ss_pid;
    uint64_t ss_data;
    uint64_t ss_checksum;
    uint8_t pid;
    int data_len;
    uint8_t data[8];
    int data_cnt;
    uint8_t checksum;
    int out_ann;
};

static int pid_to_data_len(uint8_t pid)
{
    int id = pid & 0x3F;
    if (id >= 48) return 8;
    if (id >= 32) return 4;
    return 2;
}

static uint8_t calc_parity(uint8_t pid)
{
    int id0 = (pid >> 0) & 1;
    int id1 = (pid >> 1) & 1;
    int id2 = (pid >> 2) & 1;
    int id3 = (pid >> 3) & 1;
    int id4 = (pid >> 4) & 1;
    int id5 = (pid >> 5) & 1;
    int p0 = id0 ^ id1 ^ id2 ^ id4;
    int p1 = (~(id1 ^ id3 ^ id4 ^ id5)) & 1;
    return (p0 << 6) | (p1 << 7);
}

static uint8_t lin_checksum_compute(uint8_t pid, uint8_t *data, int len, int enhanced)
{
    uint16_t sum = 0;
    if (enhanced)
        sum += pid;
    for (int i = 0; i < len; i++)
        sum += data[i];
    while (sum > 0xFF)
        sum = (sum & 0xFF) + (sum >> 8);
    return (~sum) & 0xFF;
}

static int read_uart_byte(struct srd_decoder_inst *di, struct lin_priv *priv,
                          uint64_t *byte_ss, uint64_t *byte_es, uint8_t *val)
{
    srd_cond_builder *cb;
    uint64_t samplenum;
    uint64_t matched;
    int ret;

    cb = c_cond_new();
    c_cond_fall(cb, 0);
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return -1;

    *byte_ss = samplenum;
    *val = 0;

    for (int i = 0; i < 8; i++) {
        uint64_t target = *byte_ss + (uint64_t)priv->bit_time * (i + 1) + priv->bit_time / 2;
        uint64_t skip = (target > samplenum) ? (target - samplenum) : 1;
        cb = c_cond_new();
        c_cond_skip(cb, skip);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return -1;
        int bit = c_decoder_get_pin(di, 0, samplenum);
        *val |= (bit << i);
    }

    uint64_t stop_target = *byte_ss + (uint64_t)priv->bit_time * 9 + priv->bit_time / 2;
    uint64_t skip = (stop_target > samplenum) ? (stop_target - samplenum) : 1;
    cb = c_cond_new();
    c_cond_skip(cb, skip);
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return -1;

    int stop_bit = c_decoder_get_pin(di, 0, samplenum);
    if (stop_bit != 1) return -1;

    *byte_es = *byte_ss + (uint64_t)priv->bit_time * 10;
    return 0;
}

static struct srd_channel lin_channels[] = {
    {"rx", "RX", "LIN data line", 0, SRD_CHANNEL_SDATA, "dec_lin_chan_rx"},
};

static struct srd_decoder_option lin_options_arr[2];

static const char *lin_ann_labels[][3] = {
    {"", "data", "LIN data"},
    {"", "control", "Protocol info"},
    {"", "error", "Error descriptions"},
    {"", "inline_error", "Protocol violations and errors"},
};

static const int lin_row_data_classes[] = {ANN_DATA, ANN_CONTROL, ANN_INLINE_ERROR};
static const int lin_row_error_classes[] = {ANN_ERROR};
static const struct srd_c_ann_row lin_ann_rows[] = {
    {"data", "Data", lin_row_data_classes, 3},
    {"error", "Error", lin_row_error_classes, 1},
};

static const char *lin_inputs[] = {"logic"};
static const char *lin_outputs[] = {NULL};
static const char *lin_tags[] = {"Automotive"};

static void lin_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct lin_priv)));
    struct lin_priv *priv = (struct lin_priv *)c_decoder_get_private(di);
    memset(priv, 0, sizeof(struct lin_priv));
    priv->state = FIND_BREAK;
    priv->out_ann = 0;
}

static void lin_start(struct srd_decoder_inst *di)
{
    struct lin_priv *priv = (struct lin_priv *)c_decoder_get_private(di);
    priv->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "lin");
    priv->samplerate = c_decoder_get_samplerate(di);
    priv->baudrate = (int)c_decoder_get_option_int(di, "baudrate", 9600);
    priv->version = (int)c_decoder_get_option_int(di, "version", 2);
    if (priv->samplerate > 0 && priv->baudrate > 0)
        priv->bit_time = (int)(priv->samplerate / (uint64_t)priv->baudrate);
}

static void lin_decode(struct srd_decoder_inst *di)
{
    struct lin_priv *priv = (struct lin_priv *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;
    srd_cond_builder *cb;
    int ret;

    if (priv->samplerate == 0 || priv->baudrate == 0 || priv->bit_time == 0)
        return;

    while (1) {
        switch (priv->state) {

        case FIND_BREAK: {
            cb = c_cond_new();
            c_cond_fall(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;

            priv->ss_break = samplenum;

            cb = c_cond_new();
            c_cond_rise(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;

            uint64_t break_duration = samplenum - priv->ss_break;
            uint64_t break_threshold = (uint64_t)priv->bit_time * 13;

            if (break_duration >= break_threshold) {
                C_ANN_PUT(di, priv->ss_break, samplenum, priv->out_ann, ANN_CONTROL,
                          "Break condition", "Break", "Brk", "B");
                priv->state = SYNC;
            } else {
                C_ANN_PUT(di, priv->ss_break, samplenum, priv->out_ann, ANN_ERROR,
                          "Break too short");
            }
            break;
        }

        case SYNC: {
            uint64_t byte_ss, byte_es;
            uint8_t byte_val;

            ret = read_uart_byte(di, priv, &byte_ss, &byte_es, &byte_val);
            if (ret != 0) {
                priv->state = FIND_BREAK;
                break;
            }

            priv->ss_sync = byte_ss;

            if (byte_val == 0x55) {
                C_ANN_PUT(di, byte_ss, byte_es, priv->out_ann, ANN_DATA,
                          "Sync", "S");
                priv->state = PID;
            } else {
                C_ANN_PUT(di, byte_ss, byte_es, priv->out_ann, ANN_DATA,
                          "Sync", "S");
                C_ANN_PUT(di, byte_ss, byte_es, priv->out_ann, ANN_ERROR,
                          "Sync is not 0x55", "Not 0x55", "!= 0x55");
                priv->state = FIND_BREAK;
            }
            break;
        }

        case PID: {
            uint64_t byte_ss, byte_es;
            uint8_t byte_val;

            ret = read_uart_byte(di, priv, &byte_ss, &byte_es, &byte_val);
            if (ret != 0) {
                priv->state = FIND_BREAK;
                break;
            }

            priv->ss_pid = byte_ss;
            priv->pid = byte_val;
            priv->data_len = pid_to_data_len(byte_val);
            priv->data_cnt = 0;
            memset(priv->data, 0, sizeof(priv->data));

            uint8_t expected_parity = calc_parity(byte_val);
            uint8_t actual_parity = byte_val & 0xC0;
            int parity_ok = (expected_parity == actual_parity);
            int parity = (byte_val >> 6) & 0x3;
            int expected_p = 0;
            expected_p |= ((expected_parity >> 6) & 1);
            expected_p |= (((expected_parity >> 7) & 1) << 1);
            int id = byte_val & 0x3F;

            char t1[64], t2[32], t3[16];
            const char *pstr = parity_ok ? "ok" : "bad";
            int ann_cls = parity_ok ? ANN_DATA : ANN_INLINE_ERROR;
            snprintf(t1, sizeof(t1), "ID: %02X Parity: %d (%s)", id, parity, pstr);
            snprintf(t2, sizeof(t2), "ID: 0x%02X", id);
            snprintf(t3, sizeof(t3), "I: %d", id);
            C_ANN_PUT(di, byte_ss, byte_es, priv->out_ann, ann_cls, t1, t2, t3);

            if (!parity_ok) {
                char pt[16];
                snprintf(pt, sizeof(pt), "P != %d", expected_p);
                C_ANN_PUT(di, byte_ss, byte_es, priv->out_ann, ANN_ERROR, pt);
            }

            priv->state = DATA;
            break;
        }

        case DATA: {
            uint64_t byte_ss, byte_es;
            uint8_t byte_val;

            ret = read_uart_byte(di, priv, &byte_ss, &byte_es, &byte_val);
            if (ret != 0) {
                priv->state = FIND_BREAK;
                break;
            }

            if (priv->data_cnt == 0)
                priv->ss_data = byte_ss;

            if (priv->data_cnt < 8)
                priv->data[priv->data_cnt] = byte_val;
            priv->data_cnt++;

            char t1[32], t2[16];
            snprintf(t1, sizeof(t1), "Data: 0x%02X", byte_val);
            snprintf(t2, sizeof(t2), "D: 0x%02X", byte_val);
            C_ANN_PUT(di, byte_ss, byte_es, priv->out_ann, ANN_DATA, t1, t2);

            if (priv->data_cnt >= priv->data_len)
                priv->state = CHECKSUM;
            break;
        }

        case CHECKSUM: {
            uint64_t byte_ss, byte_es;
            uint8_t byte_val;

            ret = read_uart_byte(di, priv, &byte_ss, &byte_es, &byte_val);
            if (ret != 0) {
                priv->state = FIND_BREAK;
                break;
            }

            priv->ss_checksum = byte_ss;
            priv->checksum = byte_val;

            int id = priv->pid & 0x3F;
            int enhanced = (priv->version >= 2) ? (id != 60 && id != 61) : 0;
            uint8_t expected = lin_checksum_compute(priv->pid, priv->data, priv->data_len, enhanced);
            int checksum_ok = (byte_val == expected);

            int ann_cls = checksum_ok ? ANN_DATA : ANN_INLINE_ERROR;
            char t1[32], t2[16], t3[8];
            snprintf(t1, sizeof(t1), "Checksum: 0x%02X", byte_val);
            snprintf(t2, sizeof(t2), "Checksum");
            snprintf(t3, sizeof(t3), "Chk");
            C_ANN_PUT(di, byte_ss, byte_es, priv->out_ann, ann_cls, t1, t2, t3);

            if (!checksum_ok) {
                C_ANN_PUT(di, byte_ss, byte_es, priv->out_ann, ANN_ERROR,
                          "Checksum invalid");
            }

            priv->state = FIND_BREAK;
            break;
        }

        }
    }
}

static void lin_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder lin_c_decoder = {
    .id = "lin_c",
    .name = "LIN",
    .longname = "Local Interconnect Network",
    .desc = "Local Interconnect Network (LIN) protocol.",
    .license = "gplv2+",
    .channels = lin_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = lin_options_arr,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = lin_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = lin_ann_rows,
    .inputs = lin_inputs,
    .num_inputs = 1,
    .outputs = lin_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = lin_tags,
    .num_tags = 1,
    .reset = lin_reset,
    .start = lin_start,
    .decode = lin_decode,
    .destroy = lin_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    GSList *version_list = NULL;
    version_list = g_slist_append(version_list, g_variant_new_int64(1));
    version_list = g_slist_append(version_list, g_variant_new_int64(2));
    lin_options_arr[0].id = "baudrate";
    lin_options_arr[0].idn = "dec_lin_opt_baudrate";
    lin_options_arr[0].desc = "Baud rate";
    lin_options_arr[0].def = g_variant_new_int64(9600);
    lin_options_arr[0].values = NULL;

    lin_options_arr[1].id = "version";
    lin_options_arr[1].idn = "dec_lin_opt_version";
    lin_options_arr[1].desc = "Protocol version";
    lin_options_arr[1].def = g_variant_new_int64(2);
    lin_options_arr[1].values = version_list;

    return &lin_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
