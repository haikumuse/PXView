#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_DATA = 0,
    ANN_SOF,
    ANN_EOF,
    ANN_ID,
    ANN_EXT_ID,
    ANN_FULL_ID,
    ANN_IDE,
    ANN_RESERVED_BIT,
    ANN_RTR,
    ANN_SRR,
    ANN_DLC,
    ANN_CRC_SEQ,
    ANN_CRC_DEL,
    ANN_ACK_SLOT,
    ANN_ACK_DEL,
    ANN_STUFF_BIT,
    ANN_WARNING,
    ANN_BIT,
    NUM_ANN,
};

enum {
    STATE_IDLE,
    STATE_GET_BITS,
};

enum {
    FRAME_STANDARD = 0,
    FRAME_EXTENDED = 1,
};

typedef struct {
    int state;
    uint8_t rawbits[1024];
    int num_rawbits;
    uint8_t bits[1024];
    int num_bits;
    int curbit;
    int frame_type;
    uint32_t ident;
    uint32_t eid;
    uint32_t fullid;
    int rtr_type;
    int dlc;
    int last_databit;
    int dlc_start;
    uint8_t frame_bytes[64];
    int num_frame_bytes;
    int crc_len;
    uint32_t crc;
    uint64_t dom_edge_snum;
    int dom_edge_bcount;
    uint64_t ss_block;
    uint64_t ss_bit12;
    uint64_t ss_bit32;
    uint64_t ss_packet;
    uint64_t es_packet;
    uint64_t ss_databytebits[512];
    int num_databytebits;
    uint8_t prev_rx;
    double bit_width;
    double sample_point;
    uint64_t next_sample_point;
    int bit_width_known;
    uint64_t sof_samplenum;
    int fd;
    int rtr;
    int stuff_count;
    uint64_t stuff_count_start;
    uint8_t crc_data[256];
    int num_crc_data;
    int out_ann;
    int out_python;
    int64_t nominal_bitrate;
    int64_t fast_bitrate;
    double sample_point_pct;
    uint64_t samplerate;
} canfd_state;

static int dlc2len(int dlc)
{
    static const int table[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
    if (dlc < 0 || dlc > 15) return 0;
    return table[dlc];
}

static uint32_t bitpack_msb(uint8_t *bits, int count)
{
    uint32_t val = 0;
    for (int i = 0; i < count && i < 32; i++)
        val = (val << 1) | (bits[i] & 1);
    return val;
}

static int ParityCheck(int value)
{
    static const int parity[] = {0, 3, 5, 6, 9, 10, 12, 15};
    for (int i = 0; i < 8; i++) {
        if (parity[i] == value)
            return 1;
    }
    return 0;
}

static uint32_t gray_to_binary(uint8_t *gray_bits, int count)
{
    uint32_t gray = 0;
    for (int i = 0; i < count && i < 32; i++)
        gray = (gray << 1) | (gray_bits[i] & 1);
    uint32_t binary = gray;
    uint32_t mask = binary >> 1;
    while (mask) {
        binary ^= mask;
        mask >>= 1;
    }
    return binary;
}

static void putg(canfd_state *s, struct srd_decoder_inst *di,
                 uint64_t ss, uint64_t es, int ann_class, const char **txts, int num_txts)
{
    (void)num_txts;
    int left = (int)s->sample_point;
    int right = (int)(s->bit_width - s->sample_point);
    uint64_t new_ss = (ss > (uint64_t)left) ? (ss - left) : 0;
    uint64_t new_es = es + right;

    struct srd_c_annotation ann;
    ann.ann_class = ann_class;
    ann.ann_type = 0;
    ann.ann_text = (char **)txts;
    c_decoder_put(di, new_ss, new_es, s->out_ann, &ann);
}

static void putx(canfd_state *s, struct srd_decoder_inst *di,
                 uint64_t samplenum, int ann_class, const char **txts, int num_txts)
{
    putg(s, di, samplenum, samplenum, ann_class, txts, num_txts);
}

static void putb(canfd_state *s, struct srd_decoder_inst *di,
                 uint64_t ss, uint64_t es, int ann_class, const char **txts, int num_txts)
{
    putg(s, di, ss, es, ann_class, txts, num_txts);
}

static void canfd_put_python(canfd_state *s, struct srd_decoder_inst *di)
{
    if (s->out_python < 0)
        return;

    const char *frame_type_str = (s->frame_type == FRAME_STANDARD) ? "standard" : "extended";
    const char *rtr_type_str = (s->rtr_type == 1) ? "remote" : "data";
    char text[1024];
    int pos = snprintf(text, sizeof(text), "%s,%u,%s,%d", frame_type_str, s->fullid, rtr_type_str, s->dlc);
    if (s->num_frame_bytes > 0) {
        pos += snprintf(text + pos, sizeof(text) - pos, ",");
        for (int i = 0; i < s->num_frame_bytes && pos < (int)(sizeof(text) - 4); i++)
            pos += snprintf(text + pos, sizeof(text) - pos, "%s0x%02x", (i > 0) ? "," : "[", s->frame_bytes[i]);
        if (pos < (int)(sizeof(text) - 2))
            snprintf(text + pos, sizeof(text) - pos, "]");
    }
    c_decoder_put_python(di, s->ss_packet, s->es_packet, s->out_python, text, NULL, 0);
}

static void reset_variables(canfd_state *s)
{
    s->state = STATE_IDLE;
    s->num_rawbits = 0;
    s->num_bits = 0;
    s->curbit = 0;
    s->frame_type = -1;
    s->ident = 0;
    s->eid = 0;
    s->fullid = 0;
    s->rtr_type = 0;
    s->dlc = 0;
    s->last_databit = 999;
    s->dlc_start = 0;
    s->crc_len = 15;
    s->crc = 0;
    memset(s->frame_bytes, 0, sizeof(s->frame_bytes));
    s->num_frame_bytes = 0;
    s->ss_block = 0;
    s->ss_bit12 = 0;
    s->ss_bit32 = 0;
    s->ss_packet = 0;
    s->es_packet = 0;
    s->num_databytebits = 0;
    s->bit_width_known = 0;
    s->next_sample_point = 0;
    s->fd = 0;
    s->rtr = 0;
    s->stuff_count = 0;
    s->stuff_count_start = 0;
    s->num_crc_data = 0;
}

static int is_stuff_bit(canfd_state *s)
{
    if (s->num_bits > s->last_databit + 17)
        return 0;

    if (s->fd && s->num_bits > s->last_databit + 1)
        return 0;

    if (s->num_rawbits < 6)
        return 0;

    uint8_t *l = &s->rawbits[s->num_rawbits - 6];
    if (l[0]==0 && l[1]==0 && l[2]==0 && l[3]==0 && l[4]==0 && l[5]==1) {
        s->stuff_count++;
        return 1;
    }
    if (l[0]==1 && l[1]==1 && l[2]==1 && l[3]==1 && l[4]==1 && l[5]==0) {
        s->stuff_count++;
        return 1;
    }
    return 0;
}

static void dom_edge_seen(canfd_state *s, uint64_t samplenum)
{
    s->dom_edge_snum = samplenum;
    s->dom_edge_bcount = s->curbit;
}

static uint64_t get_sample_point(canfd_state *s, int bitnum)
{
    double offset = (double)(bitnum - s->dom_edge_bcount) * s->bit_width;
    return (uint64_t)((double)s->dom_edge_snum + offset + s->sample_point);
}

static void set_bit_rate(canfd_state *s, int64_t bitrate)
{
    if (s->samplerate > 0 && bitrate > 0) {
        s->bit_width = (double)s->samplerate / (double)bitrate;
        s->sample_point = (s->bit_width / 100.0) * s->sample_point_pct;
    }
}

static void set_nominal_bitrate(canfd_state *s)
{
    set_bit_rate(s, s->nominal_bitrate);
}

static void set_fast_bitrate(canfd_state *s)
{
    set_bit_rate(s, s->fast_bitrate);
}

static int decode_frame_end(canfd_state *s, struct srd_decoder_inst *di,
                            uint8_t can_rx, int bitnum, uint64_t samplenum)
{
    if (bitnum == s->last_databit + 1) {
        s->ss_block = samplenum;
        if (s->fd) {
            if (dlc2len(s->dlc) <= 16)
                s->crc_len = 27;
            else
                s->crc_len = 32;
            char t1[64], t2[32], t3[8];
            snprintf(t1, sizeof(t1), "Fixed stuff bit: %d", can_rx);
            snprintf(t2, sizeof(t2), "FSB: %d", can_rx);
            snprintf(t3, sizeof(t3), "%d", can_rx);
            const char *txts[] = {t1, t2, t3, NULL};
            putx(s, di, samplenum, ANN_STUFF_BIT, txts, 3);
        } else {
            s->crc_len = 15;
        }
    } else if (s->fd && bitnum == s->last_databit + 2) {
        s->ss_block = samplenum;
        s->stuff_count_start = samplenum;
    } else if (s->fd && bitnum == s->last_databit + 4) {
        int stuff_bits_start = s->num_bits - 3;
        uint8_t stuff_bits_arr[3];
        for (int i = 0; i < 3 && (stuff_bits_start + i) < s->num_bits; i++)
            stuff_bits_arr[i] = s->bits[stuff_bits_start + i];
        uint32_t stuff_count_val = bitpack_msb(stuff_bits_arr, 3);
        uint32_t stuff_count_bin = gray_to_binary(stuff_bits_arr, 3);

        if (s->stuff_count % 8 != (int)stuff_count_bin) {
            const char *gce[] = {"Gray Code Error", "GCE", "GCE", NULL};
            putb(s, di, s->ss_block, samplenum, ANN_WARNING, gce, 3);
        }
        s->stuff_count = 0;

        char sc_str[4];
        for (int b = 2; b >= 0; b--)
            sc_str[2 - b] = ((stuff_count_val >> b) & 1) ? '1' : '0';
        sc_str[3] = '\0';
        char t1[64], t2[64];
        snprintf(t1, sizeof(t1), "Stuff count: %s", sc_str);
        snprintf(t2, sizeof(t2), "Stuff count: %s", sc_str);
        const char *sc_txts[] = {t1, t2, sc_str, NULL};
        putb(s, di, s->ss_block, samplenum, ANN_CRC_SEQ, sc_txts, 3);
        s->ss_block = samplenum + (uint64_t)s->bit_width;
    } else if (s->fd && bitnum == s->last_databit + 5) {
        int sp_start = s->num_bits - 4;
        uint8_t sp_bits[4];
        for (int i = 0; i < 4 && (sp_start + i) < s->num_bits; i++)
            sp_bits[i] = s->bits[sp_start + i];
        uint32_t stuff_and_parity = bitpack_msb(sp_bits, 4);
        if (!ParityCheck((int)stuff_and_parity)) {
            const char *pce[] = {"Parity Check Error", "PCE", "PCE", NULL};
            putg(s, di, s->stuff_count_start, samplenum, ANN_WARNING, pce, 3);
        }
        char t1[32], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Parity: %d", can_rx);
        snprintf(t2, sizeof(t2), "P: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *par_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_CRC_SEQ, par_txts, 3);
        s->ss_block = samplenum + (uint64_t)s->bit_width;
    } else if (s->fd && bitnum > s->last_databit + 5 && bitnum < s->last_databit + s->crc_len) {
        int index = bitnum - (s->last_databit + 5);
        if ((index - 1) % 5 == 0) {
            char t1[64], t2[32], t3[8];
            snprintf(t1, sizeof(t1), "Fixed stuff bit: %d", can_rx);
            snprintf(t2, sizeof(t2), "FSB: %d", can_rx);
            snprintf(t3, sizeof(t3), "%d", can_rx);
            const char *fsb_txts[] = {t1, t2, t3, NULL};
            putx(s, di, samplenum, ANN_STUFF_BIT, fsb_txts, 3);
        }
        if (index % 5 == 0) {
            int cd_start = s->num_bits - 4;
            for (int i = 0; i < 4 && (cd_start + i) < s->num_bits; i++) {
                if (s->num_crc_data < 256)
                    s->crc_data[s->num_crc_data++] = s->bits[cd_start + i];
            }
        }
    } else if (bitnum == s->last_databit + s->crc_len) {
        const char *crc_type;
        if (s->fd) {
            if (dlc2len(s->dlc) <= 16)
                crc_type = "CRC-17";
            else
                crc_type = "CRC-21";
        } else {
            crc_type = "CRC-15";
        }

        int x = s->last_databit + 1;
        if (s->fd) {
            if (s->num_crc_data < 256)
                s->crc_data[s->num_crc_data++] = can_rx;
            s->crc = bitpack_msb(s->crc_data, s->num_crc_data > 32 ? 32 : s->num_crc_data);
        } else {
            if (x + s->crc_len + 1 <= s->num_bits)
                s->crc = bitpack_msb(&s->bits[x], s->crc_len + 1);
        }

        char t1[64], t2[48], t3[16];
        snprintf(t1, sizeof(t1), "%s sequence: 0x%04x", crc_type, s->crc);
        snprintf(t2, sizeof(t2), "%s: 0x%04x", crc_type, s->crc);
        snprintf(t3, sizeof(t3), "0x%04x", s->crc);
        const char *crc_txts[] = {t1, t2, t3, NULL};
        putb(s, di, s->ss_block, samplenum, ANN_CRC_SEQ, crc_txts, 3);
    } else if (bitnum == s->last_databit + s->crc_len + 1) {
        char t1[32], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "CRC delimiter: %d", can_rx);
        snprintf(t2, sizeof(t2), "CRC d: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *cd_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_CRC_DEL, cd_txts, 3);
        if (can_rx != 1) {
            const char *warn[] = {"CRC delimiter must be a recessive bit", NULL};
            putx(s, di, samplenum, ANN_WARNING, warn, 1);
        }
        if (s->fd) {
            set_nominal_bitrate(s);
            if (s->bit_width_known)
                s->next_sample_point = get_sample_point(s, s->curbit);
        }
    } else if (bitnum == s->last_databit + s->crc_len + 2) {
        const char *ack = (can_rx == 0) ? "ACK" : "NACK";
        char t1[32], t2[32], t3[16];
        snprintf(t1, sizeof(t1), "ACK slot: %s", ack);
        snprintf(t2, sizeof(t2), "ACK s: %s", ack);
        snprintf(t3, sizeof(t3), "%s", ack);
        const char *ack_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_ACK_SLOT, ack_txts, 3);
    } else if (bitnum == s->last_databit + s->crc_len + 3) {
        char t1[32], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "ACK delimiter: %d", can_rx);
        snprintf(t2, sizeof(t2), "ACK d: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *ad_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_ACK_DEL, ad_txts, 3);
        if (can_rx != 1) {
            const char *warn[] = {"ACK delimiter must be a recessive bit", NULL};
            putx(s, di, samplenum, ANN_WARNING, warn, 1);
        }
    } else if (bitnum == s->last_databit + s->crc_len + 4) {
        s->ss_block = samplenum;
    } else if (bitnum == s->last_databit + s->crc_len + 10) {
        const char *eof_txts[] = {"End of frame", "EOF", "E", NULL};
        putb(s, di, s->ss_block, samplenum, ANN_EOF, eof_txts, 3);
        if (s->num_rawbits >= 7) {
            uint8_t *last7 = &s->rawbits[s->num_rawbits - 7];
            if (!(last7[0]==1 && last7[1]==1 && last7[2]==1 && last7[3]==1 &&
                  last7[4]==1 && last7[5]==1 && last7[6]==1)) {
                const char *warn[] = {"End of frame (EOF) must be 7 recessive bits", NULL};
                putb(s, di, s->ss_block, samplenum, ANN_WARNING, warn, 1);
            }
        }
        s->es_packet = samplenum;
        canfd_put_python(s, di);
        reset_variables(s);
        return 1;
    }
    return 0;
}

static void decode_data_field(canfd_state *s, struct srd_decoder_inst *di,
                              int bitnum, uint64_t samplenum)
{
    if (bitnum > s->dlc_start + 3 && bitnum < s->last_databit) {
        if (s->num_databytebits < 512)
            s->ss_databytebits[s->num_databytebits++] = samplenum;
    }
    if (bitnum == s->last_databit && s->dlc > 0) {
        if (s->num_databytebits < 512)
            s->ss_databytebits[s->num_databytebits++] = samplenum;
        s->num_frame_bytes = 0;
        int num_bytes = dlc2len(s->dlc);
        if (num_bytes > 64) num_bytes = 64;
        for (int i = 0; i < num_bytes; i++) {
            int x = s->dlc_start + 4 + (8 * i);
            uint8_t b = 0;
            if (x + 8 <= s->num_bits)
                b = (uint8_t)bitpack_msb(&s->bits[x], 8);
            s->frame_bytes[s->num_frame_bytes++] = b;
            if (i * 8 < s->num_databytebits && (i + 1) * 8 - 1 < s->num_databytebits) {
                char t1[64], t2[48], t3[16];
                snprintf(t1, sizeof(t1), "Data byte %d: 0x%02x", i, b);
                snprintf(t2, sizeof(t2), "DB %d: 0x%02x", i, b);
                snprintf(t3, sizeof(t3), "0x%02x", b);
                const char *db_txts[] = {t1, t2, t3, NULL};
                putg(s, di, s->ss_databytebits[i * 8],
                     s->ss_databytebits[(i + 1) * 8 - 1], ANN_DATA, db_txts, 3);
            }
        }
        s->num_databytebits = 0;
    }
}

static int decode_standard_frame(canfd_state *s, struct srd_decoder_inst *di,
                                uint8_t can_rx, int bitnum, uint64_t samplenum)
{
    if (bitnum == 14) {
        s->fd = can_rx ? 1 : 0;
        char t1[64], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Flexible data format: %d", can_rx);
        snprintf(t2, sizeof(t2), "FDF: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *fdf_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, fdf_txts, 3);

        if (s->fd) {
            const char *rrs_txts[] = {"Remote request subtitution", "RRS", NULL};
            putg(s, di, s->ss_bit12, s->ss_bit12, ANN_RTR, rrs_txts, 2);
            s->dlc_start = 18;
        } else {
            const char *rtr = (s->bits[12] == 1) ? "remote" : "data";
            char rt1[64], rt2[48], rt3[16];
            snprintf(rt1, sizeof(rt1), "Remote transmission request: %s frame", rtr);
            snprintf(rt2, sizeof(rt2), "RTR: %s frame", rtr);
            snprintf(rt3, sizeof(rt3), "%s", rtr);
            const char *rtr_txts[] = {rt1, rt2, rt3, NULL};
            putg(s, di, s->ss_bit12, s->ss_bit12, ANN_RTR, rtr_txts, 3);
            s->rtr_type = (s->bits[12] == 1) ? 1 : 0;
            s->dlc_start = 15;
        }
    }

    if (bitnum == 15 && s->fd) {
        char t1[32], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Reserved bit 0: %d", can_rx);
        snprintf(t2, sizeof(t2), "RB0: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *rb0_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, rb0_txts, 3);
    }

    if (bitnum == 16 && s->fd) {
        char t1[48], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Bit rate switch: %d", can_rx);
        snprintf(t2, sizeof(t2), "BRS: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *brs_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, brs_txts, 3);
    }

    if (bitnum == 17 && s->fd) {
        char t1[48], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Error state indicator: %d", can_rx);
        snprintf(t2, sizeof(t2), "ESI: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *esi_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, esi_txts, 3);
    }

    if (bitnum == s->dlc_start)
        s->ss_block = samplenum;

    if (bitnum == s->dlc_start + 3) {
        if (s->dlc_start + 4 <= s->num_bits)
            s->dlc = bitpack_msb(&s->bits[s->dlc_start], 4);
        char t1[32], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Data length code: %d", s->dlc);
        snprintf(t2, sizeof(t2), "DLC: %d", s->dlc);
        snprintf(t3, sizeof(t3), "%d", s->dlc);
        const char *dlc_txts[] = {t1, t2, t3, NULL};
        putb(s, di, s->ss_block, samplenum, ANN_DLC, dlc_txts, 3);

        s->last_databit = s->dlc_start + 3 + (dlc2len(s->dlc) * 8);
        if (s->dlc != 0 && s->rtr_type == 1) {
            const char *warn[] = {"Data length code (DLC) != 0 is not allowed", NULL};
            putb(s, di, s->ss_block, samplenum, ANN_WARNING, warn, 1);
            s->dlc = 0;
            s->last_databit = s->dlc_start + 3 + (dlc2len(s->dlc) * 8);
        } else if (s->dlc > 8 && !s->fd) {
            const char *warn[] = {"CAN Data length code (DLC) > 8 is not allowed", NULL};
            putb(s, di, s->ss_block, samplenum, ANN_WARNING, warn, 1);
            s->dlc = 8;
            s->last_databit = s->dlc_start + 3 + (dlc2len(s->dlc) * 8);
        }
    }

    decode_data_field(s, di, bitnum, samplenum);

    if (bitnum > s->last_databit)
        return decode_frame_end(s, di, can_rx, bitnum, samplenum);
    return 0;
}

static int decode_extended_frame(canfd_state *s, struct srd_decoder_inst *di,
                                uint8_t can_rx, int bitnum, uint64_t samplenum)
{
    if (bitnum == 14) {
        s->ss_block = samplenum;
        s->dlc_start = 35;
    }

    if (bitnum == 31) {
        if (14 < s->num_bits)
            s->eid = bitpack_msb(&s->bits[14], s->num_bits - 14);
        s->fullid = s->ident << 18 | s->eid;

        char s_eid[32], s_full[32];
        snprintf(s_eid, sizeof(s_eid), "%d (0x%x)", s->eid, s->eid);
        snprintf(s_full, sizeof(s_full), "%d (0x%x)", s->fullid, s->fullid);

        char et1[64], et2[48], et3[32], et4[32];
        snprintf(et1, sizeof(et1), "Extended Identifier: %s", s_eid);
        snprintf(et2, sizeof(et2), "Extended ID: %s", s_eid);
        snprintf(et3, sizeof(et3), "Extended ID");
        snprintf(et4, sizeof(et4), "%s", s_eid);
        const char *eid_txts[] = {et1, et2, et3, et4, NULL};
        putb(s, di, s->ss_block, samplenum, ANN_EXT_ID, eid_txts, 4);

        char ft1[64], ft2[48], ft3[32];
        snprintf(ft1, sizeof(ft1), "Full Identifier: %s", s_full);
        snprintf(ft2, sizeof(ft2), "Full ID: %s", s_full);
        snprintf(ft3, sizeof(ft3), "%s", s_full);
        const char *fid_txts[] = {ft1, ft2, ft3, NULL};
        putb(s, di, s->ss_block, samplenum, ANN_FULL_ID, fid_txts, 3);

        char st1[48], st2[32], st3[8];
        snprintf(st1, sizeof(st1), "Substitute remote request: %d", s->bits[12]);
        snprintf(st2, sizeof(st2), "SRR: %d", s->bits[12]);
        snprintf(st3, sizeof(st3), "%d", s->bits[12]);
        const char *srr_txts[] = {st1, st2, st3, NULL};
        putg(s, di, s->ss_bit12, s->ss_bit12, ANN_SRR, srr_txts, 3);
    }

    if (bitnum == 32) {
        s->ss_bit32 = samplenum;
        s->rtr = can_rx;
    } else if (bitnum == 33) {
        s->fd = can_rx ? 1 : 0;
        char t1[64], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Flexible data format: %d", can_rx);
        snprintf(t2, sizeof(t2), "FDF: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *fdf_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, fdf_txts, 3);

        if (s->fd) {
            char rt1[64], rt2[32], rt3[8];
            snprintf(rt1, sizeof(rt1), "Remote requset substituion: %d", s->rtr);
            snprintf(rt2, sizeof(rt2), "RRS: %d", s->rtr);
            snprintf(rt3, sizeof(rt3), "%d", s->rtr);
            const char *rrs_txts[] = {rt1, rt2, rt3, NULL};
            putg(s, di, s->ss_bit32, s->ss_bit32, ANN_RESERVED_BIT, rrs_txts, 3);
            s->dlc_start = 37;
        } else {
            const char *rtr = (s->rtr == 1) ? "remote" : "data";
            s->rtr_type = (s->rtr == 1) ? 1 : 0;
            char rt1[64], rt2[48], rt3[16];
            snprintf(rt1, sizeof(rt1), "Remote transmission request: %s frame", rtr);
            snprintf(rt2, sizeof(rt2), "RTR: %s frame", rtr);
            snprintf(rt3, sizeof(rt3), "%s", rtr);
            const char *rtr_txts[] = {rt1, rt2, rt3, NULL};
            putg(s, di, s->ss_bit32, s->ss_bit32, ANN_RTR, rtr_txts, 3);
            s->dlc_start = 35;
        }
    }

    if (bitnum == 34) {
        char t1[32], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Reserved bit 0: %d", can_rx);
        snprintf(t2, sizeof(t2), "RB0: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *rb0_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, rb0_txts, 3);
    }

    if (bitnum == 35 && s->fd) {
        char t1[48], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Bit rate switch: %d", can_rx);
        snprintf(t2, sizeof(t2), "BRS: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *brs_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, brs_txts, 3);
    }

    if (bitnum == 36 && s->fd) {
        char t1[48], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Error state indicator: %d", can_rx);
        snprintf(t2, sizeof(t2), "ESI: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *esi_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, esi_txts, 3);
    }

    if (bitnum == s->dlc_start)
        s->ss_block = samplenum;

    if (bitnum == s->dlc_start + 3) {
        if (s->dlc_start + 4 <= s->num_bits)
            s->dlc = bitpack_msb(&s->bits[s->dlc_start], 4);
        char t1[32], t2[32], t3[8];
        snprintf(t1, sizeof(t1), "Data length code: %d", s->dlc);
        snprintf(t2, sizeof(t2), "DLC: %d", s->dlc);
        snprintf(t3, sizeof(t3), "%d", s->dlc);
        const char *dlc_txts[] = {t1, t2, t3, NULL};
        putb(s, di, s->ss_block, samplenum, ANN_DLC, dlc_txts, 3);

        s->last_databit = s->dlc_start + 3 + (dlc2len(s->dlc) * 8);
        if (s->dlc != 0 && s->rtr_type == 1) {
            s->dlc = 0;
            s->last_databit = s->dlc_start + 3 + (dlc2len(s->dlc) * 8);
        } else if (s->dlc > 8 && !s->fd) {
            s->dlc = 8;
            s->last_databit = s->dlc_start + 3 + (dlc2len(s->dlc) * 8);
        }
    }

    decode_data_field(s, di, bitnum, samplenum);

    if (bitnum > s->last_databit)
        return decode_frame_end(s, di, can_rx, bitnum, samplenum);
    return 0;
}

static void handle_bit(canfd_state *s, struct srd_decoder_inst *di,
                       uint8_t can_rx, uint64_t samplenum)
{
    if (s->num_rawbits < 1024) s->rawbits[s->num_rawbits++] = can_rx;
    if (s->num_bits < 1024) s->bits[s->num_bits++] = can_rx;

    int bitnum = s->num_bits - 1;

    if (s->fd && can_rx) {
        if ((bitnum == 16 && s->frame_type == FRAME_STANDARD) ||
            (bitnum == 35 && s->frame_type == FRAME_EXTENDED)) {
            dom_edge_seen(s, samplenum);
            set_fast_bitrate(s);
            if (s->bit_width_known)
                s->next_sample_point = get_sample_point(s, s->curbit);
        }
    }

    if (is_stuff_bit(s)) {
        char text[4];
        snprintf(text, sizeof(text), "%d", can_rx);
        const char *sb_txts[] = {text, NULL};
        putx(s, di, samplenum, ANN_STUFF_BIT, sb_txts, 1);
        s->num_bits--;
        s->curbit++;
        return;
    }

    char bit_text[4];
    snprintf(bit_text, sizeof(bit_text), "%d", can_rx);
    const char *bit_txts[] = {bit_text, NULL};
    putx(s, di, samplenum, ANN_BIT, bit_txts, 1);

    if (bitnum == 0) {
        s->ss_packet = samplenum;
        const char *sof_txts[] = {"Start of frame", "SOF", "S", NULL};
        putx(s, di, samplenum, ANN_SOF, sof_txts, 3);
        if (can_rx != 0) {
            const char *warn[] = {"Start of frame (SOF) must be a dominant bit", NULL};
            putx(s, di, samplenum, ANN_WARNING, warn, 1);
        }
    } else if (bitnum == 1) {
        s->ss_block = samplenum;
    } else if (bitnum == 11) {
        s->ident = bitpack_msb(&s->bits[1], 11);
        s->fullid = s->ident;
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%d (0x%x)", s->ident, s->ident);
        char t1[64], t2[48], t3[32];
        snprintf(t1, sizeof(t1), "Identifier: %s", id_str);
        snprintf(t2, sizeof(t2), "ID: %s", id_str);
        snprintf(t3, sizeof(t3), "%s", id_str);
        const char *id_txts[] = {t1, t2, t3, NULL};
        putb(s, di, s->ss_block, samplenum, ANN_ID, id_txts, 3);
        if ((s->ident & 0x7f0) == 0x7f0) {
            const char *warn[] = {"Identifier bits 10..4 must not be all recessive", NULL};
            putb(s, di, s->ss_block, samplenum, ANN_WARNING, warn, 1);
        }
    } else if (bitnum == 12) {
        s->ss_bit12 = samplenum;
    } else if (bitnum == 13) {
        const char *ide = (can_rx == 0) ? "standard" : "extended";
        s->frame_type = (can_rx == 0) ? FRAME_STANDARD : FRAME_EXTENDED;
        char t1[64], t2[48], t3[32];
        snprintf(t1, sizeof(t1), "Identifier extension bit: %s frame", ide);
        snprintf(t2, sizeof(t2), "IDE: %s frame", ide);
        snprintf(t3, sizeof(t3), "%s", ide);
        const char *ide_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_IDE, ide_txts, 3);
    } else if (bitnum >= 14) {
        int done = 0;
        if (s->frame_type == FRAME_STANDARD)
            done = decode_standard_frame(s, di, can_rx, bitnum, samplenum);
        else if (s->frame_type == FRAME_EXTENDED)
            done = decode_extended_frame(s, di, can_rx, bitnum, samplenum);
        if (done) return;
    }

    s->curbit++;
}

static struct srd_channel canfd_channels[] = {
    {"can_rx", "CAN FD", "CAN FD bus line", 0, SRD_CHANNEL_SDATA, NULL},
};

static const char *canfd_ann_labels[][3] = {
    {"", "data", "CAN payload data"},
    {"", "sof", "Start of frame"},
    {"", "eof", "End of frame"},
    {"", "id", "Identifier"},
    {"", "ext-id", "Extended identifier"},
    {"", "full-id", "Full identifier"},
    {"", "ide", "Identifier extension bit"},
    {"", "reserved-bit", "Reserved bit 0 and 1"},
    {"", "rtr", "Remote transmission request"},
    {"", "srr", "Substitute remote request"},
    {"", "dlc", "Data length count"},
    {"", "crc-sequence", "CRC sequence"},
    {"", "crc-delimiter", "CRC delimiter"},
    {"", "ack-slot", "ACK slot"},
    {"", "ack-delimiter", "ACK delimiter"},
    {"", "stuff-bit", "Stuff bit"},
    {"", "warnings", "Human-readable warnings"},
    {"", "bit", "Bit"},
};

static struct srd_decoder_option canfd_options[] = {
    {"nominal_bitrate", NULL, "Nominal bitrate (bits/s)", NULL, NULL},
    {"fast_bitrate", NULL, "Fast bitrate (bits/s)", NULL, NULL},
    {"sample_point", NULL, "Sample point (%)", NULL, NULL},
};

static const char *canfd_inputs[] = {"logic"};
static const char *canfd_outputs[] = {"can"};
static const char *canfd_tags[] = {"Automotive"};

static const int canfd_row_bits_classes[] = {ANN_STUFF_BIT, ANN_BIT};
static const int canfd_row_fields_classes[] = {ANN_DATA, ANN_SOF, ANN_EOF, ANN_ID, ANN_EXT_ID, ANN_FULL_ID, ANN_IDE, ANN_RESERVED_BIT, ANN_RTR, ANN_SRR, ANN_DLC, ANN_CRC_SEQ, ANN_CRC_DEL, ANN_ACK_SLOT, ANN_ACK_DEL};
static const int canfd_row_warnings_classes[] = {ANN_WARNING};
static const struct srd_c_ann_row canfd_ann_rows[] = {
    {"bits", "Bits", canfd_row_bits_classes, 2},
    {"fields", "Fields", canfd_row_fields_classes, 15},
    {"warnings", "Warnings", canfd_row_warnings_classes, 1},
};

static void canfd_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(canfd_state)));
    canfd_state *s = (canfd_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(canfd_state));
    s->prev_rx = 1;
    s->last_databit = 999;
    s->crc_len = 15;
    s->frame_type = -1;
    s->out_ann = 0;
    s->out_python = -1;
    s->nominal_bitrate = 1000000;
    s->fast_bitrate = 2000000;
    s->sample_point_pct = 70.0;
}

static void canfd_start(struct srd_decoder_inst *di)
{
    canfd_state *s = (canfd_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "can");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "can");
}

static void canfd_decode(struct srd_decoder_inst *di)
{
    canfd_state *s = (canfd_state *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched = 0;
    int CAN_RX = 0;

    s->samplerate = c_decoder_get_samplerate(di);
    s->nominal_bitrate = c_decoder_get_option_int(di, "nominal_bitrate", 1000000);
    s->fast_bitrate = c_decoder_get_option_int(di, "fast_bitrate", 2000000);
    s->sample_point_pct = c_decoder_get_option_double(di, "sample_point", 70.0);

    if (s->samplerate > 0 && s->nominal_bitrate > 0) {
        set_nominal_bitrate(s);
        s->bit_width_known = 1;
    }

    while (1) {
        if (s->state == STATE_IDLE) {
            srd_cond_builder *cb = c_cond_new();
            c_cond_fall(cb, CAN_RX);
            int ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            s->state = STATE_GET_BITS;
            s->ss_packet = samplenum;
            s->sof_samplenum = samplenum;
            dom_edge_seen(s, samplenum);
            s->curbit = 0;
            s->num_rawbits = 0;
            s->num_bits = 0;
            s->num_databytebits = 0;
            s->frame_type = -1;
            s->last_databit = 999;
            s->crc_len = 15;
            s->dlc = 0;
            s->rtr_type = 0;
            s->num_frame_bytes = 0;
            s->fd = 0;
            s->rtr = 0;
            s->stuff_count = 0;
            s->num_crc_data = 0;

            if (s->bit_width_known)
                s->next_sample_point = get_sample_point(s, s->curbit);

        } else if (s->state == STATE_GET_BITS) {
            uint64_t pos = get_sample_point(s, s->curbit);
            uint64_t skip_count = 0;
            if (pos > samplenum)
                skip_count = pos - samplenum;

            srd_cond_builder *cb = c_cond_new();
            c_cond_skip(cb, skip_count);
            c_cond_or(cb);
            c_cond_fall(cb, CAN_RX);
            int ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            if (matched & (0b1 << 1)) {
                dom_edge_seen(s, samplenum);
                if (s->bit_width_known)
                    s->next_sample_point = get_sample_point(s, s->curbit);
            }

            if (matched & (0b1 << 0)) {
                uint8_t can_rx = c_decoder_get_pin(di, CAN_RX, pos);
                handle_bit(s, di, can_rx, pos);
                if (s->state != STATE_GET_BITS)
                    continue;
                if (s->bit_width_known)
                    s->next_sample_point = get_sample_point(s, s->curbit);
            }
        }
    }
}

static void canfd_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder canfd_c_decoder = {
    .id = "can_fd_c",
    .name = "CAN-FD(C)",
    .longname = "Controller Area Network with Flexible Data rate (C)",
    .desc = "CAN-FD bus protocol decoder (C implementation)",
    .license = "gplv2+",
    .channels = canfd_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = canfd_options,
    .num_options = 3,
    .num_annotations = NUM_ANN,
    .ann_labels = canfd_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = canfd_ann_rows,
    .reset = canfd_reset,
    .start = canfd_start,
    .decode = canfd_decode,
    .destroy = canfd_destroy,
    .inputs = canfd_inputs,
    .num_inputs = 1,
    .outputs = canfd_outputs,
    .num_outputs = 1,
    .tags = canfd_tags,
    .num_tags = 1,
    .binary = NULL,
    .num_binary = 0,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    canfd_options[0].def = g_variant_new_int64(1000000);
    canfd_options[1].def = g_variant_new_int64(2000000);
    canfd_options[2].def = g_variant_new_double(70.0);
    return &canfd_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
