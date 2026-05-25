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
    uint8_t rawbits[512];
    int num_rawbits;
    uint8_t bits[512];
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
    uint64_t ss_databytebits[64];
    int num_databytebits;
    uint8_t prev_rx;
    double bit_width;
    double sample_point;
    uint64_t next_sample_point;
    int bit_width_known;
    uint64_t sof_samplenum;
    uint64_t edge_positions[32];
    int num_edge_positions;
    int out_ann;
    int out_python;
} can_state;

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

static void putg(can_state *s, struct srd_decoder_inst *di,
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

static void putx(can_state *s, struct srd_decoder_inst *di,
                 uint64_t samplenum, int ann_class, const char **txts, int num_txts)
{
    putg(s, di, samplenum, samplenum, ann_class, txts, num_txts);
}

static void putb(can_state *s, struct srd_decoder_inst *di,
                 uint64_t ss, uint64_t es, int ann_class, const char **txts, int num_txts)
{
    putg(s, di, ss, es, ann_class, txts, num_txts);
}

static void can_put_python(can_state *s, struct srd_decoder_inst *di)
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

static void reset_variables(can_state *s)
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
    /* Do NOT reset bit_width_known - bitrate/sample_point are persistent,
     * matching Python where they are set in metadata() and never reset. */
    s->next_sample_point = 0;
    s->num_edge_positions = 0;
}

static int is_stuff_bit(can_state *s)
{
    /* Bit stuffing applies to SOF, ID, Control, Data, and CRC fields.
     * It does NOT apply to CRC delimiter, ACK, and EOF.
     * Python decoder uses: if len(self.bits) > self.last_databit + 17: return False
     * last_databit is the last bit of the data field.
     * CRC(15) + CRC Delim(1) + ACK Slot(1) + ACK Delim(1) = 18 bits.
     * So stuff bits stop after the CRC sequence. */
    if (s->num_bits > s->last_databit + 17)
        return 0;

    if (s->num_rawbits < 6)
        return 0;

    /* Check if the 5 preceding bits are identical AND the 6th bit (current)
     * is different, matching Python's check:
     * last_6_bits in ([0,0,0,0,0,1], [1,1,1,1,1,0]) */
    uint8_t *l = &s->rawbits[s->num_rawbits - 6];
    if (l[0] == l[1] && l[0] == l[2] && l[0] == l[3] && l[0] == l[4] && l[5] != l[0]) {
        /* The 6th bit (l[5]) is the stuff bit. */
        return 1;
    }

    return 0;
}

static void dom_edge_seen(can_state *s, uint64_t samplenum)
{
    s->dom_edge_snum = samplenum;
    s->dom_edge_bcount = s->curbit;
}

static uint64_t get_sample_point(can_state *s, int bitnum)
{
    double offset = (double)(bitnum - s->dom_edge_bcount) * s->bit_width;
    return (uint64_t)((double)s->dom_edge_snum + offset + s->sample_point);
}

static int decode_frame_end(can_state *s, struct srd_decoder_inst *di,
                           uint8_t can_rx, int bitnum, uint64_t samplenum)
{
    if (bitnum == s->last_databit + 1) {
        s->ss_block = samplenum;
        s->crc_len = 15;
    }
    if (bitnum == s->last_databit + s->crc_len) {
        int x = s->last_databit + 1;
        if (x + s->crc_len + 1 <= s->num_bits)
            s->crc = bitpack_msb(&s->bits[x], s->crc_len + 1);
        char t1[64], t2[48], t3[16];
        snprintf(t1, sizeof(t1), "CRC-15 sequence: 0x%04x", s->crc);
        snprintf(t2, sizeof(t2), "CRC-15: 0x%04x", s->crc);
        snprintf(t3, sizeof(t3), "0x%04x", s->crc);
        const char *txts[] = {t1, t2, t3, NULL};
        putb(s, di, s->ss_block, samplenum, ANN_CRC_SEQ, txts, 3);
    }
    if (bitnum == s->last_databit + s->crc_len + 1) {
        char t1[32], t2[32], t3[16];
        snprintf(t1, sizeof(t1), "CRC delimiter: %d", can_rx);
        snprintf(t2, sizeof(t2), "CRC d: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_CRC_DEL, txts, 3);
        if (can_rx != 1) {
            const char *warn[] = {"CRC delimiter must be a recessive bit", NULL};
            putx(s, di, samplenum, ANN_WARNING, warn, 1);
        }
    }
    if (bitnum == s->last_databit + s->crc_len + 2) {
        const char *ack = (can_rx == 0) ? "ACK" : "NACK";
        char t1[32], t2[32], t3[16];
        snprintf(t1, sizeof(t1), "ACK slot: %s", ack);
        snprintf(t2, sizeof(t2), "ACK s: %s", ack);
        snprintf(t3, sizeof(t3), "%s", ack);
        const char *txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_ACK_SLOT, txts, 3);
    }
    if (bitnum == s->last_databit + s->crc_len + 3) {
        char t1[32], t2[32], t3[16];
        snprintf(t1, sizeof(t1), "ACK delimiter: %d", can_rx);
        snprintf(t2, sizeof(t2), "ACK d: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_ACK_DEL, txts, 3);
        if (can_rx != 1) {
            const char *warn[] = {"ACK delimiter must be a recessive bit", NULL};
            putx(s, di, samplenum, ANN_WARNING, warn, 1);
        }
    }
    if (bitnum == s->last_databit + s->crc_len + 4)
        s->ss_block = samplenum;

    if (bitnum == s->last_databit + s->crc_len + 10) {
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
        can_put_python(s, di);
        reset_variables(s);
        return 1;
    }
    return 0;
}

static void decode_data_field(can_state *s, struct srd_decoder_inst *di,
                              int bitnum, uint64_t samplenum)
{
    if (bitnum > s->dlc_start + 3 && bitnum < s->last_databit) {
        if (s->num_databytebits < 64)
            s->ss_databytebits[s->num_databytebits++] = samplenum;
    }
    if (bitnum == s->last_databit && s->dlc > 0) {
        if (s->num_databytebits < 64)
            s->ss_databytebits[s->num_databytebits++] = samplenum;
        s->num_frame_bytes = 0;
        for (int i = 0; i < dlc2len(s->dlc) && i < 8; i++) {
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
                const char *txts[] = {t1, t2, t3, NULL};
                putg(s, di, s->ss_databytebits[i * 8],
                     s->ss_databytebits[(i + 1) * 8 - 1], ANN_DATA, txts, 3);
            }
        }
        s->num_databytebits = 0;
    }
}

static int decode_standard_frame(can_state *s, struct srd_decoder_inst *di,
                                uint8_t can_rx, int bitnum, uint64_t samplenum)
{
    if (bitnum == 14) {
        char t1[32], t2[32], t3[16];
        snprintf(t1, sizeof(t1), "Reserved bit 0: %d", can_rx);
        snprintf(t2, sizeof(t2), "RB0: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *rb0_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, rb0_txts, 3);

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
    if (bitnum == s->dlc_start)
        s->ss_block = samplenum;

    if (bitnum == s->dlc_start + 3) {
        if (s->dlc_start + 4 <= s->num_bits)
            s->dlc = bitpack_msb(&s->bits[s->dlc_start], 4);
        char t1[32], t2[32], t3[16];
        snprintf(t1, sizeof(t1), "Data length code: %d", s->dlc);
        snprintf(t2, sizeof(t2), "DLC: %d", s->dlc);
        snprintf(t3, sizeof(t3), "%d", s->dlc);
        const char *dlc_txts[] = {t1, t2, t3, NULL};
        putb(s, di, s->ss_block, samplenum, ANN_DLC, dlc_txts, 3);

        if (s->dlc != 0 && s->rtr_type == 1) {
            const char *warn[] = {"Data length code (DLC) != 0 is not allowed", NULL};
            putb(s, di, s->ss_block, samplenum, ANN_WARNING, warn, 1);
            s->dlc = 0;
        } else if (s->dlc > 8) {
            const char *warn[] = {"Data length code (DLC) > 8 is not allowed", NULL};
            putb(s, di, s->ss_block, samplenum, ANN_WARNING, warn, 1);
            s->dlc = 8;
        }
        s->last_databit = s->dlc_start + 3 + (dlc2len(s->dlc) * 8);
    }

    decode_data_field(s, di, bitnum, samplenum);

    if (bitnum > s->last_databit)
        return decode_frame_end(s, di, can_rx, bitnum, samplenum);
    return 0;
}

static int decode_extended_frame(can_state *s, struct srd_decoder_inst *di,
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

        char st1[48], st2[32], st3[16];
        snprintf(st1, sizeof(st1), "Substitute remote request: %d", s->bits[12]);
        snprintf(st2, sizeof(st2), "SRR: %d", s->bits[12]);
        snprintf(st3, sizeof(st3), "%d", s->bits[12]);
        const char *srr_txts[] = {st1, st2, st3, NULL};
        putg(s, di, s->ss_bit12, s->ss_bit12, ANN_SRR, srr_txts, 3);
    }
    if (bitnum == 32) {
        s->ss_bit32 = samplenum;
        const char *rtr = (can_rx == 1) ? "remote" : "data";
        char t1[64], t2[48], t3[16];
        snprintf(t1, sizeof(t1), "Remote transmission request: %s frame", rtr);
        snprintf(t2, sizeof(t2), "RTR: %s frame", rtr);
        snprintf(t3, sizeof(t3), "%s", rtr);
        const char *rtr_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RTR, rtr_txts, 3);
        s->rtr_type = (can_rx == 1) ? 1 : 0;
    }
    if (bitnum == 33) {
        char t1[32], t2[32], t3[16];
        snprintf(t1, sizeof(t1), "Reserved bit 1: %d", can_rx);
        snprintf(t2, sizeof(t2), "RB1: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, txts, 3);
    }
    if (bitnum == 34) {
        char t1[32], t2[32], t3[16];
        snprintf(t1, sizeof(t1), "Reserved bit 0: %d", can_rx);
        snprintf(t2, sizeof(t2), "RB0: %d", can_rx);
        snprintf(t3, sizeof(t3), "%d", can_rx);
        const char *txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_RESERVED_BIT, txts, 3);
    }
    if (bitnum == s->dlc_start)
        s->ss_block = samplenum;

    if (bitnum == s->dlc_start + 3) {
        if (s->dlc_start + 4 <= s->num_bits)
            s->dlc = bitpack_msb(&s->bits[s->dlc_start], 4);
        char t1[32], t2[32], t3[16];
        snprintf(t1, sizeof(t1), "Data length code: %d", s->dlc);
        snprintf(t2, sizeof(t2), "DLC: %d", s->dlc);
        snprintf(t3, sizeof(t3), "%d", s->dlc);
        const char *dlc_txts[] = {t1, t2, t3, NULL};
        putb(s, di, s->ss_block, samplenum, ANN_DLC, dlc_txts, 3);

        if (s->dlc != 0 && s->rtr_type == 1) {
            s->dlc = 0;
        } else if (s->dlc > 8) {
            s->dlc = 8;
        }
        s->last_databit = s->dlc_start + 3 + (dlc2len(s->dlc) * 8);
    }

    decode_data_field(s, di, bitnum, samplenum);

    if (bitnum > s->last_databit)
        return decode_frame_end(s, di, can_rx, bitnum, samplenum);
    return 0;
}

static void handle_bit(can_state *s, struct srd_decoder_inst *di,
                       uint8_t can_rx, uint64_t samplenum)
{
    if (s->num_rawbits < 512) s->rawbits[s->num_rawbits++] = can_rx;
    if (s->num_bits < 512) s->bits[s->num_bits++] = can_rx;

    if (is_stuff_bit(s)) {
        char text[4];
        snprintf(text, sizeof(text), "%d", can_rx);
        const char *txts[] = {text, NULL};
        putx(s, di, samplenum, ANN_STUFF_BIT, txts, 1);
        s->num_bits--;
        s->curbit++;
        return;
    }

    char bit_text[4];
    snprintf(bit_text, sizeof(bit_text), "%d", can_rx);
    const char *bit_txts[] = {bit_text, NULL};
    putx(s, di, samplenum, ANN_BIT, bit_txts, 1);

    int bitnum = s->num_bits - 1;

    if (bitnum == 0) {
        s->ss_packet = samplenum;
        const char *sof_txts[] = {"Start of frame", "SOF", "S", NULL};
        putx(s, di, samplenum, ANN_SOF, sof_txts, 3);
        if (can_rx != 0) {
            const char *warn[] = {"Start of frame (SOF) must be a dominant bit", NULL};
            putx(s, di, samplenum, ANN_WARNING, warn, 1);
        }
    }
    if (bitnum == 1)
        s->ss_block = samplenum;

    if (bitnum == 11) {
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
    }
    if (bitnum == 12)
        s->ss_bit12 = samplenum;

    if (bitnum == 13) {
        const char *ide = (can_rx == 0) ? "standard" : "extended";
        s->frame_type = (can_rx == 0) ? FRAME_STANDARD : FRAME_EXTENDED;
        char t1[64], t2[48], t3[32];
        snprintf(t1, sizeof(t1), "Identifier extension bit: %s frame", ide);
        snprintf(t2, sizeof(t2), "IDE: %s frame", ide);
        snprintf(t3, sizeof(t3), "%s", ide);
        const char *ide_txts[] = {t1, t2, t3, NULL};
        putx(s, di, samplenum, ANN_IDE, ide_txts, 3);
    }
    if (bitnum >= 14) {
        int done = 0;
        if (s->frame_type == FRAME_STANDARD)
            done = decode_standard_frame(s, di, can_rx, bitnum, samplenum);
        else if (s->frame_type == FRAME_EXTENDED)
            done = decode_extended_frame(s, di, can_rx, bitnum, samplenum);
        if (done) return;
    }
    s->curbit++;
}

static void try_estimate_bit_width(can_state *s)
{
    if (s->bit_width_known || s->num_edge_positions < 2)
        return;

    uint64_t min_dist = 0;
    int first = 1;
    for (int i = 1; i < s->num_edge_positions; i++) {
        uint64_t d = s->edge_positions[i] - s->edge_positions[i - 1];
        if (d > 0 && (first || d < min_dist)) {
            min_dist = d;
            first = 0;
        }
    }
    uint64_t first_d = s->edge_positions[0] - s->sof_samplenum;
    if (first_d > 0 && (first || first_d < min_dist))
        min_dist = first_d;

    if (!first && min_dist > 1) {
        s->bit_width = (double)min_dist;
        s->sample_point = s->bit_width * 0.7;
        s->bit_width_known = 1;
        s->next_sample_point = get_sample_point(s, s->curbit);
    }
}

static struct srd_channel can_channels[] = {
    {"can_rx", "CAN", "CAN bus line", 0, SRD_CHANNEL_SDATA, "dec_can_chan_can_rx"},
};

static const char *can_ann_labels[][3] = {
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

static struct srd_decoder_option can_options_arr[2];

static const char *can_inputs[] = {"logic"};
static const char *can_outputs[] = {"can"};
static const char *can_tags[] = {"Automotive"};

static const int can_row_bits_classes[] = {ANN_STUFF_BIT, ANN_BIT};
static const int can_row_fields_classes[] = {ANN_DATA, ANN_SOF, ANN_EOF, ANN_ID, ANN_EXT_ID, ANN_FULL_ID, ANN_IDE, ANN_RESERVED_BIT, ANN_RTR, ANN_SRR, ANN_DLC, ANN_CRC_SEQ, ANN_CRC_DEL, ANN_ACK_SLOT, ANN_ACK_DEL};
static const int can_row_warnings_classes[] = {ANN_WARNING};
static const struct srd_c_ann_row can_ann_rows[] = {
    {"bits", "Bits", can_row_bits_classes, 2},
    {"fields", "Fields", can_row_fields_classes, 15},
    {"warnings", "Warnings", can_row_warnings_classes, 1},
};

static void can_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(can_state)));
    can_state *s = (can_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(can_state));
    s->prev_rx = 1;
    s->last_databit = 999;
    s->crc_len = 15;
    s->frame_type = -1;
    s->out_ann = 0;
    s->out_python = -1;
}

static void can_start(struct srd_decoder_inst *di)
{
    can_state *s = (can_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "can");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "can");
}

static void can_decode(struct srd_decoder_inst *di)
{
    can_state *s = (can_state *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;
    int CAN_RX = 0;

    uint64_t samplerate = c_decoder_get_samplerate(di);
    if (samplerate == 0)
        return;

    int64_t bitrate = c_decoder_get_option_int(di, "bitrate", 1000000);
    double sample_point_pct = c_decoder_get_option_double(di, "sample_point", 70.0);

    if (samplerate > 0 && bitrate > 0) {
        s->bit_width = (double)samplerate / (double)bitrate;
        s->sample_point = (s->bit_width / 100.0) * sample_point_pct;
        s->bit_width_known = 1;
    }

    while (1) {
        if (s->state == STATE_IDLE) {
            /* Wait for falling edge (SOF), matching Python's self.wait({0: 'f'}) */
            srd_cond_builder *cb = c_cond_new();
            c_cond_fall(cb, CAN_RX);
            int ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            /* SOF detected - falling edge means dominant bit.
               Python does NOT output any annotations here - it just records
               the edge position and enters GET BITS state. The SOF annotation
               will be output by handle_bit() when the first sample point is
               reached. */
            s->state = STATE_GET_BITS;
            s->sof_samplenum = samplenum;
            dom_edge_seen(s, samplenum);
            s->curbit = 0;
            s->num_rawbits = 0;
            s->num_bits = 0;
            s->num_databytebits = 0;
            s->num_edge_positions = 0;
            s->frame_type = -1;
            s->last_databit = 999;
            s->crc_len = 15;
            s->dlc = 0;
            s->rtr_type = 0;
            s->num_frame_bytes = 0;

            if (s->bit_width_known)
                s->next_sample_point = get_sample_point(s, s->curbit);

            s->prev_rx = c_decoder_get_pin(di, CAN_RX, samplenum);

        } else if (s->state == STATE_GET_BITS) {
            /* Wait for either sample point OR falling edge, matching Python's
               self.wait([{'skip': pos - self.samplenum}, {0: 'f'}]) */
            srd_cond_builder *cb = c_cond_new();
            if (s->bit_width_known) {
                uint64_t skip_amount = 0;
                if (s->next_sample_point > samplenum)
                    skip_amount = s->next_sample_point - samplenum;
                c_cond_skip(cb, skip_amount);
                c_cond_or(cb);
            }
            c_cond_fall(cb, CAN_RX);
            int ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            uint8_t can_rx = c_decoder_get_pin(di, CAN_RX, samplenum);

            /* Check which condition matched */
            int fell = (can_rx == 0 && s->prev_rx == 1);
            int at_sample_point = 0;

            if (s->bit_width_known && samplenum >= s->next_sample_point) {
                at_sample_point = 1;
            }

            if (fell) {
                dom_edge_seen(s, samplenum);
                if (s->bit_width_known)
                    s->next_sample_point = get_sample_point(s, s->curbit);
            }

            if (at_sample_point) {
                uint8_t bit_val = c_decoder_get_pin(di, CAN_RX, s->next_sample_point);
                handle_bit(s, di, bit_val, s->next_sample_point);
                if (s->state != STATE_GET_BITS) {
                    s->prev_rx = can_rx;
                    continue;
                }
                s->next_sample_point = get_sample_point(s, s->curbit);
            }

            s->prev_rx = can_rx;
        }
    }
}

static void can_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder can_c_decoder = {
    .id = "can_c",
    .name = "CAN(C)",
    .longname = "Controller Area Network (C)",
    .desc = "CAN bus protocol decoder (C implementation)",
    .license = "gplv2+",
    .channels = can_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = can_options_arr,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = can_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = can_ann_rows,
    .reset = can_reset,
    .start = can_start,
    .decode = can_decode,
    .destroy = can_destroy,
    .inputs = can_inputs,
    .num_inputs = 1,
    .outputs = can_outputs,
    .num_outputs = 1,
    .tags = can_tags,
    .num_tags = 1,
    .binary = NULL,
    .num_binary = 0,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    can_options_arr[0].id = "bitrate";
    can_options_arr[0].idn = "dec_can_opt_bitrate";
    can_options_arr[0].desc = "bitrate (bits/s)";
    can_options_arr[0].def = g_variant_new_int64(1000000);
    can_options_arr[0].values = NULL;

    can_options_arr[1].id = "sample_point";
    can_options_arr[1].idn = "dec_can_opt_sample_point";
    can_options_arr[1].desc = "Sample point (%)";
    can_options_arr[1].def = g_variant_new_double(70.0);
    can_options_arr[1].values = NULL;

    return &can_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
