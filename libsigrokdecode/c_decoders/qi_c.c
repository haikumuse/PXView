#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum qi_state {
    STATE_IDLE,
    STATE_DATA,
};

enum qi_ann {
    ANN_BITS = 0,
    ANN_BIT_ERRORS,
    ANN_START_BITS,
    ANN_INFO_BITS,
    ANN_DATA_BYTES,
    ANN_PACKET_DATA,
    ANN_CHECKSUM_OK,
    ANN_CHECKSUM_ERR,
    NUM_ANN,
};

static const char *end_codes[] = {
    "Unknown", "Charge Complete", "Internal Fault",
    "Over Temperature", "Over Voltage", "Over Current",
    "Battery Failure", "Reconfigure", "No Response"
};

struct qi_priv {
    uint64_t samplerate;
    double bit_width;
    int out_ann;

    int state;
    uint64_t lastbit;

    uint64_t deq[2];
    int deq_len;

    int bits[12];
    uint64_t bitsi[12];
    int bits_len;

    uint64_t bytestart;

    uint8_t packet[32];
    int packet_len_count;
    uint64_t bytesi[32];
    int bytesi_len;

    uint64_t prev_samplenum;
};

static struct srd_channel qi_channels[] = {
    {"qi", "Qi", "Demodulated Qi data line", 0, SRD_CHANNEL_SDATA, "dec_qi_chan_qi"},
};

static const char *qi_ann_labels[][3] = {
    {"", "bits", "Bits"},
    {"", "bytes-errors", "Bit errors"},
    {"", "bytes-start", "Start bits"},
    {"", "bytes-info", "Info bits"},
    {"", "bytes-data", "Data bytes"},
    {"", "packets-data", "Packet data"},
    {"", "packets-checksum-ok", "Packet checksum"},
    {"", "packets-checksum-err", "Packet checksum"},
};

static const int qi_row_bits_classes[] = {ANN_BITS, -1};
static const int qi_row_bytes_classes[] = {ANN_BIT_ERRORS, ANN_START_BITS, ANN_INFO_BITS, ANN_DATA_BYTES, -1};
static const int qi_row_packets_classes[] = {ANN_PACKET_DATA, ANN_CHECKSUM_OK, ANN_CHECKSUM_ERR, -1};

static const struct srd_c_ann_row qi_ann_rows[] = {
    {"bits", "Bits", qi_row_bits_classes, 1},
    {"bytes", "Bytes", qi_row_bytes_classes, 4},
    {"packets", "Packets", qi_row_packets_classes, 3},
};

static const char *qi_inputs[] = {"logic"};
static const char *qi_outputs[] = {};
static const char *qi_tags[] = {"Embedded/industrial", "Wireless/RF"};

static int qi_packet_len(uint8_t byte_val)
{
    if (byte_val <= 0x1f)
        return 1 + (byte_val - 0) / 32;
    else if (byte_val <= 0x7f)
        return 2 + (byte_val - 32) / 16;
    else if (byte_val <= 0xdf)
        return 8 + (byte_val - 128) / 8;
    else
        return 20 + (byte_val - 224) / 4;
}

static uint8_t qi_calc_checksum(const uint8_t *packet, int len)
{
    uint8_t cs = 0;
    for (int i = 0; i < len - 1; i++)
        cs ^= packet[i];
    return cs;
}

static uint32_t qi_bits_to_uint(int *bits, int count)
{
    /* LSB first: bits[0] is LSB */
    uint32_t val = 0;
    for (int i = 0; i < count && i < 32; i++)
        val |= ((uint32_t)(bits[i] & 1) << i);
    return val;
}

static void qi_process_packet(struct srd_decoder_inst *di);
static void qi_process_byte(struct srd_decoder_inst *di);
static void qi_add_bit(struct srd_decoder_inst *di, int bit);
static void qi_handle_transition(struct srd_decoder_inst *di, uint64_t l, int htl);

static void qi_process_packet(struct srd_decoder_inst *di)
{
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);
    uint8_t *p = s->packet;
    int plen = s->packet_len_count;
    char text[256];
    char short_text[64];

    if (p[0] == 0x01) {
        snprintf(text, sizeof(text), "Signal Strength: %d", p[1]);
        snprintf(short_text, sizeof(short_text), "SS: %d", p[1]);
        C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                  s->out_ann, ANN_PACKET_DATA, text, short_text, "SS");
    } else if (p[0] == 0x02) {
        const char *reason = (p[1] < 9) ? end_codes[p[1]] : "Reserved";
        snprintf(text, sizeof(text), "End Power Transfer: %s", reason);
        snprintf(short_text, sizeof(short_text), "EPT: %s", reason);
        C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                  s->out_ann, ANN_PACKET_DATA, text, short_text, "EPT");
    } else if (p[0] == 0x03) {
        int val = (p[1] < 128) ? (int)p[1] : (int)(p[1] & 0x7f) - 128;
        snprintf(text, sizeof(text), "Control Error: %d", val);
        snprintf(short_text, sizeof(short_text), "CE: %d", val);
        C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                  s->out_ann, ANN_PACKET_DATA, text, short_text, "CE");
    } else if (p[0] == 0x04) {
        snprintf(text, sizeof(text), "Received Power: %d", p[1]);
        snprintf(short_text, sizeof(short_text), "RP: %d", p[1]);
        C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                  s->out_ann, ANN_PACKET_DATA, text, short_text, "RP");
    } else if (p[0] == 0x05) {
        snprintf(text, sizeof(text), "Charge Status: %d", p[1]);
        snprintf(short_text, sizeof(short_text), "CS: %d", p[1]);
        C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                  s->out_ann, ANN_PACKET_DATA, text, short_text, "CS");
    } else if (p[0] == 0x06) {
        snprintf(text, sizeof(text), "Power Control Hold-off: %dms", p[1]);
        snprintf(short_text, sizeof(short_text), "PCH: %d", p[1]);
        C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                  s->out_ann, ANN_PACKET_DATA, text, short_text, "PCH");
    } else if (p[0] == 0x51 && plen >= 6) {
        int pc = (p[1] & 0xc0) >> 7;
        int mp = p[1] & 0x3f;
        int prop = (p[3] & 0x80) >> 7;
        int count = p[3] & 0x07;
        int ws = (p[4] & 0xf8) >> 3;
        int wo = p[4] & 0x07;
        snprintf(text, sizeof(text),
                 "Configuration: Power Class = %d, Maximum Power = %d, "
                 "Prop = %d, Count = %d, Window Size = %d, Window Offset = %d",
                 pc, mp, prop, count, ws, wo);
        C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                  s->out_ann, ANN_PACKET_DATA, text);
    } else if (p[0] == 0x71 && plen >= 8) {
        snprintf(text, sizeof(text), "Identification: Version = %d.%d, "
                 "Manufacturer = %02x%02x, Device = %02x%02x%02x%02x",
                 (p[1] & 0xf0) >> 4, p[1] & 0x0f,
                 p[2], p[3], p[4] & ~0x80, p[5], p[6], p[7]);
        C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                  s->out_ann, ANN_PACKET_DATA, text);
    } else if (p[0] == 0x81 && plen >= 8) {
        snprintf(text, sizeof(text), "Extended Identification: %02x%02x%02x%02x%02x%02x%02x%02x",
                 p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                  s->out_ann, ANN_PACKET_DATA, text);
    } else {
        static const uint8_t prop_ids[] = {0x18,0x19,0x28,0x29,0x38,0x48,0x58,0x68,0x78,0x85,0xa4,0xc4,0xe2};
        int is_prop = 0;
        for (int i = 0; i < 13; i++) {
            if (p[0] == prop_ids[i]) { is_prop = 1; break; }
        }
        if (is_prop) {
            C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                      s->out_ann, ANN_PACKET_DATA, "Proprietary", "P");
        } else {
            C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
                      s->out_ann, ANN_PACKET_DATA, "Unknown", "?");
        }
    }

    /* Checksum verification */
    uint8_t cs = 0;
    for (int i = 0; i < plen - 1; i++)
        cs ^= p[i];
    if (cs == p[plen - 1]) {
        C_ANN_PUT(di, s->bytesi[s->bytesi_len - 1],
                  s->bitsi[10],
                  s->out_ann, ANN_CHECKSUM_OK, "Checksum OK", "OK");
    } else {
        C_ANN_PUT(di, s->bytesi[s->bytesi_len - 1],
                  s->bitsi[10],
                  s->out_ann, ANN_CHECKSUM_ERR, "Checksum error", "ERR");
    }
}

static void qi_process_byte(struct srd_decoder_inst *di)
{
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);
    int *b = s->bits;

    /* Check start bit */
    if (b[0] == 0) {
        C_ANN_PUT(di, s->bytestart, s->bitsi[0], s->out_ann, ANN_START_BITS, "Start bit", "Start", "S");
    } else {
        C_ANN_PUT(di, s->bytestart, s->bitsi[0], s->out_ann, ANN_BIT_ERRORS, "Start error", "Start err", "SE");
    }

    /* Extract data bits [1:9], LSB first */
    uint32_t data_val = qi_bits_to_uint(&b[1], 8);

    /* Output data byte */
    char byte_text[16];
    snprintf(byte_text, sizeof(byte_text), "%02x", data_val);
    C_ANN_PUT(di, s->bitsi[0], s->bitsi[8], s->out_ann, ANN_DATA_BYTES, byte_text);

    /* Parity check: odd parity (start with 1, XOR all data bits) */
    int parity = 1;
    for (int i = 1; i <= 8; i++)
        parity ^= b[i];

    /* Check parity bit */
    if (b[9] == parity) {
        C_ANN_PUT(di, s->bitsi[8], s->bitsi[9], s->out_ann, ANN_INFO_BITS, "Parity bit", "Parity", "P");
    } else {
        C_ANN_PUT(di, s->bitsi[8], s->bitsi[9], s->out_ann, ANN_BIT_ERRORS, "Parity error", "Parity err", "PE");
    }

    /* Check stop bit */
    if (b[10] == 1) {
        C_ANN_PUT(di, s->bitsi[9], s->bitsi[10], s->out_ann, ANN_INFO_BITS, "Stop bit", "Stop", "S");
    } else {
        C_ANN_PUT(di, s->bitsi[9], s->bitsi[10], s->out_ann, ANN_BIT_ERRORS, "Stop error", "Stop err", "SE");
    }

    /* Add to packet */
    if (s->packet_len_count < 32) {
        s->packet[s->packet_len_count] = (uint8_t)data_val;
        s->bytesi[s->bytesi_len] = s->bytestart;
        s->bytesi_len++;
        s->packet_len_count++;
    }

    /* Check if packet is complete */
    if (s->packet_len_count >= 2) {
        int expected_len = qi_packet_len(s->packet[0]) + 2;
        if (s->packet_len_count == expected_len) {
            qi_process_packet(di);
            /* Reset packet state */
            s->packet_len_count = 0;
            s->bytesi_len = 0;
        }
    }
}

static void qi_add_bit(struct srd_decoder_inst *di, int bit)
{
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);

    if (s->bits_len < 12) {
        s->bits[s->bits_len] = bit;
        s->bitsi[s->bits_len] = s->prev_samplenum;
        s->bits_len++;
    }

    fprintf(stderr, "DBG add_bit: bit=%d bits_len=%d state=%s prev_sample=%llu\n",
            bit, s->bits_len, s->state == STATE_IDLE ? "IDLE" : "DATA",
            (unsigned long long)s->prev_samplenum);

    /* IDLE state: detect preamble [1,1,1,1,0] */
    if (s->state == STATE_IDLE && s->bits_len >= 5) {
        int *b = s->bits;
        if (b[s->bits_len-5] == 1 && b[s->bits_len-4] == 1 &&
            b[s->bits_len-3] == 1 && b[s->bits_len-2] == 1 &&
            b[s->bits_len-1] == 0) {
            fprintf(stderr, "DBG add_bit: PREAMBLE DETECTED! bitsi[-2]=%llu\n",
                    (unsigned long long)s->bitsi[s->bits_len - 2]);
            s->state = STATE_DATA;
            s->bytestart = s->bitsi[s->bits_len - 2];
            /* Clear bits, set start bit = 0 */
            s->bits[0] = 0;
            s->bitsi[0] = s->prev_samplenum;
            s->bits_len = 1;
            s->packet_len_count = 0;
            s->bytesi_len = 0;
        }
    }
    /* DATA state: accumulate 11 bits */
    else if (s->state == STATE_DATA && s->bits_len == 11) {
        fprintf(stderr, "DBG add_bit: PROCESS_BYTE data_val=0x%02x\n",
                qi_bits_to_uint(&s->bits[1], 8));
        qi_process_byte(di);
        s->bytestart = s->prev_samplenum;
        s->bits_len = 0;
    }

    /* Output bit annotation */
    if (s->state != STATE_IDLE) {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", bit);
        C_ANN_PUT(di, s->lastbit, s->prev_samplenum, s->out_ann, ANN_BITS, bit_str);
    }
    s->lastbit = s->prev_samplenum;
}

static void qi_handle_transition(struct srd_decoder_inst *di, uint64_t l, int htl)
{
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);

    /* Add to deque */
    if (s->deq_len < 2) {
        s->deq[s->deq_len] = l;
        s->deq_len++;
    } else {
        s->deq[0] = s->deq[1];
        s->deq[1] = l;
    }

    double bw = s->bit_width;
    double lo = 0.75 * bw;
    double hi = 1.25 * bw;

    fprintf(stderr, "DBG handle_trans: l=%llu htl=%d deq=[%llu,%llu] deq_len=%d lo=%.1f hi=%.1f\n",
            (unsigned long long)l, htl,
            (unsigned long long)s->deq[0], (unsigned long long)s->deq[1],
            s->deq_len, lo, hi);

    if (s->deq_len >= 2) {
        double sum = (double)(s->deq[s->deq_len-1] + s->deq[s->deq_len-2]);
        fprintf(stderr, "DBG handle_trans: sum=%.1f in_tol=%d\n", sum, (lo < sum && sum < hi));
        if (lo < sum && sum < hi) {
            fprintf(stderr, "DBG handle_trans: -> add_bit(1) via sum\n");
            qi_add_bit(di, 1);
            s->deq_len = 0;
            return;
        }
        if (htl && lo < l * 2.0 && l * 2.0 < hi &&
            (double)s->deq[s->deq_len-2] > hi) {
            fprintf(stderr, "DBG handle_trans: -> add_bit(1) via htl+2x\n");
            qi_add_bit(di, 1);
            s->deq_len = 0;
            return;
        }
    }

    if (lo < (double)l && (double)l < hi) {
        fprintf(stderr, "DBG handle_trans: -> add_bit(0) via in_tolerance\n");
        qi_add_bit(di, 0);
        s->deq_len = 0;
    } else if ((double)l > hi) {
        fprintf(stderr, "DBG handle_trans: -> BACK TO IDLE (l=%.1f > hi=%.1f)\n", (double)l, hi);
        /* Back to IDLE */
        s->state = STATE_IDLE;
        s->bytesi_len = 0;
        s->packet_len_count = 0;
        s->bits_len = 0;
        s->deq_len = 0;
    } else {
        fprintf(stderr, "DBG handle_trans: -> NO ACTION\n");
    }
}

static void qi_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct qi_priv)));
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct qi_priv));
    s->out_ann = -1;
    s->state = STATE_IDLE;
}

static void qi_start(struct srd_decoder_inst *di)
{
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "qi");
}

static void qi_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
        if (value > 0)
            s->bit_width = (double)value / 2000.0;
    }
}

static void qi_decode(struct srd_decoder_inst *di)
{
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched = 0;
    int ret;

    fprintf(stderr, "DBG qi_decode: samplerate=%llu\n", (unsigned long long)s->samplerate);

    if (s->samplerate == 0)
        return;

    s->bit_width = (double)s->samplerate / 2000.0;
    fprintf(stderr, "DBG qi_decode: bit_width=%.1f\n", s->bit_width);

    /* Get initial pin state */
    {
        uint64_t cur_sample;
        ret = c_cond_wait_current(di, &cur_sample);
        fprintf(stderr, "DBG qi_decode: c_cond_wait_current ret=%d cur_sample=%llu\n", ret, (unsigned long long)cur_sample);
        if (ret != SRD_OK)
            return;
        int qi = c_decoder_get_pin(di, 0, cur_sample);
        fprintf(stderr, "DBG qi_decode: initial qi=%d\n", qi);
        s->prev_samplenum = cur_sample;
        qi_handle_transition(di, 0, qi == 0);
    }

    int iter = 0;
    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 0);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) {
            fprintf(stderr, "DBG qi_decode: c_cond_wait returned %d after %d iterations\n", ret, iter);
            return;
        }

        uint64_t l = samplenum - s->prev_samplenum;
        int qi = c_decoder_get_pin(di, 0, samplenum);
        s->prev_samplenum = samplenum;

        qi_handle_transition(di, l, qi == 0);
        iter++;
    }
}

static void qi_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder qi_c_decoder = {
    .id = "qi_c",
    .name = "Qi(C)",
    .longname = "Qi charger protocol (C)",
    .desc = "Protocol used by Qi receiver. (C implementation)",
    .license = "gplv2+",
    .channels = qi_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = qi_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = qi_ann_rows,
    .inputs = qi_inputs,
    .num_inputs = 1,
    .outputs = qi_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = qi_tags,
    .num_tags = 2,
    .reset = qi_reset,
    .start = qi_start,
    .decode = qi_decode,
    .destroy = qi_destroy,
    .metadata = qi_metadata,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &qi_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
