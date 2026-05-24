/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2017 Ryan "Izzy" Bales <izzy84075@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_FRAME_NAME = 0,
    ANN_FRAME_ERROR,
    ANN_FRAME_BIT_NAME,
    ANN_FRAME_BITS_DATA,
    ANN_BLOCK_ERROR,
    ANN_BLOCK_DATA,
    NUM_ANN,
};

/* Block type lookup table */
typedef struct {
    uint8_t code;
    const char *name;
} btype_entry;

static const btype_entry btype_table[] = {
    {0x01, "PRIORITY-UPDATE"},
    {0x02, "TAGGER-STATUS"},
    {0x18, "COUNT-DOWN"},
    {0x20, "VARIABLE-CONTENTS"},
    {0x22, "GAME-CONTENTS"},
    {0xA0, "READ-VARIABLE"},
    {0xC0, "WRITE-VARIABLE"},
    {0xC2, "WRITE-GAME"},
};
#define BTYPE_TABLE_SIZE (sizeof(btype_table) / sizeof(btype_table[0]))

/* Weapon mode lookup */
typedef struct {
    uint8_t code;
    const char *name;
} weapmode_entry;

static const weapmode_entry weapmode_table[] = {
    {0x01, "Semi-Automatic"},
    {0x02, "Full-Automatic"},
};
#define WEAPMODE_TABLE_SIZE (sizeof(weapmode_table) / sizeof(weapmode_table[0]))

/* Shield status lookup */
typedef struct {
    uint8_t code;
    const char *name;
} shieldstatus_entry;

static const shieldstatus_entry shieldstatus_table[] = {
    {0x00, "Ready"},
    {0x01, "Active"},
    {0x02, "Cooldown"},
};
#define SHIELDSTATUS_TABLE_SIZE (sizeof(shieldstatus_table) / sizeof(shieldstatus_table[0]))

/* Hunting direction lookup */
typedef struct {
    uint8_t code;
    const char *name;
} huntingdir_entry;

static const huntingdir_entry huntingdir_table[] = {
    {0x00, "Normal"},
    {0x01, "Reversed"},
};
#define HUNTINGDIR_TABLE_SIZE (sizeof(huntingdir_table) / sizeof(huntingdir_table[0]))

#define LTAR_SD_DEC_MAX_FRAMES 16
#define LTAR_SD_DEC_BITS_PER_FRAME 10

typedef struct {
    int out_ann;
} ltar_sd_dec_state;

static const char *ltar_sd_dec_inputs[] = {"ltar_smartdevice", NULL};
static const char *ltar_sd_dec_outputs[] = {"ltar_smartdevice_decode", NULL};
static const char *ltar_sd_dec_tags[] = {"Embedded/industrial", NULL};

static const char *ltar_sd_dec_ann_labels[][3] = {
    {"", "frame-name", "Frame Name"},
    {"", "frame-error", "Frame Error"},
    {"", "frame-bit-name", "Frame Bit Name"},
    {"", "frame-bits-data", "Frame Bits Data"},
    {"", "block-error", "Block Errors"},
    {"", "block-data", "Block Data"},
};

static const int ltar_sd_dec_row_frame_names_classes[] = {ANN_FRAME_NAME, -1};
static const int ltar_sd_dec_row_frame_errors_classes[] = {ANN_FRAME_ERROR, -1};
static const int ltar_sd_dec_row_frame_bit_names_classes[] = {ANN_FRAME_BIT_NAME, -1};
static const int ltar_sd_dec_row_frame_bits_datas_classes[] = {ANN_FRAME_BITS_DATA, -1};
static const int ltar_sd_dec_row_block_errors_classes[] = {ANN_BLOCK_ERROR, -1};

static const struct srd_c_ann_row ltar_sd_dec_ann_rows[] = {
    {"frame-names", "Frame name", ltar_sd_dec_row_frame_names_classes, 1},
    {"frame-errors", "Frame errors", ltar_sd_dec_row_frame_errors_classes, 1},
    {"frame-bit-names", "Frame bit names", ltar_sd_dec_row_frame_bit_names_classes, 1},
    {"frame-bits-datas", "Frame bits data", ltar_sd_dec_row_frame_bits_datas_classes, 1},
    {"block-errors", "Block errors", ltar_sd_dec_row_block_errors_classes, 1},
};

static const char *ltar_sd_dec_lookup_btype(uint8_t code)
{
    for (int i = 0; i < (int)BTYPE_TABLE_SIZE; i++) {
        if (btype_table[i].code == code)
            return btype_table[i].name;
    }
    return NULL;
}

static const char *ltar_sd_dec_lookup_weapmode(uint8_t code)
{
    for (int i = 0; i < (int)WEAPMODE_TABLE_SIZE; i++) {
        if (weapmode_table[i].code == code)
            return weapmode_table[i].name;
    }
    return NULL;
}

static const char *ltar_sd_dec_lookup_shieldstatus(uint8_t code)
{
    for (int i = 0; i < (int)SHIELDSTATUS_TABLE_SIZE; i++) {
        if (shieldstatus_table[i].code == code)
            return shieldstatus_table[i].name;
    }
    return NULL;
}

static const char *ltar_sd_dec_lookup_huntingdir(uint8_t code)
{
    for (int i = 0; i < (int)HUNTINGDIR_TABLE_SIZE; i++) {
        if (huntingdir_table[i].code == code)
            return huntingdir_table[i].name;
    }
    return "Normal";
}

static void ltar_sd_dec_check_block_length(struct srd_decoder_inst *di,
    ltar_sd_dec_state *s, uint8_t btype, int length,
    uint64_t block_ss, uint64_t block_es)
{
    if (btype == 0x02 && length != 11) {
        C_ANN_PUT(di, block_ss, block_es, s->out_ann, ANN_BLOCK_ERROR,
                  "Invalid block length");
    }
}

static void ltar_sd_dec_check_block_csum(struct srd_decoder_inst *di,
    ltar_sd_dec_state *s,
    const uint8_t *byte_values, int num_frames,
    const uint64_t *frame_ss, const uint64_t *frame_es,
    uint64_t block_ss, uint64_t block_es)
{
    int temp = 0xFF;
    for (int i = 0; i < num_frames; i++)
        temp -= byte_values[i];
    temp = temp & 0xFF;

    if (temp != 0) {
        C_ANN_PUT(di, block_ss, block_es, s->out_ann, ANN_BLOCK_ERROR,
                  "Invalid block checksum");
    } else if (num_frames > 0) {
        C_ANN_PUT(di, frame_ss[num_frames - 1], frame_es[num_frames - 1],
                  s->out_ann, ANN_FRAME_BITS_DATA, "Valid Checksum");
    }
}

static void ltar_sd_dec_put_block_type(struct srd_decoder_inst *di,
    ltar_sd_dec_state *s, uint8_t btype,
    uint64_t frame_ss, uint64_t frame_es)
{
    C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_NAME, "Block Type");
    C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME, "Block Type");

    const char *name = ltar_sd_dec_lookup_btype(btype);
    char buf[64];
    if (name)
        snprintf(buf, sizeof(buf), "%s (0x%02X)", name, btype);
    else
        snprintf(buf, sizeof(buf), "Unknown Block Type (0x%02X)", btype);
    C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, buf);
}

static void ltar_sd_dec_put_csum(struct srd_decoder_inst *di,
    ltar_sd_dec_state *s,
    uint64_t frame_ss, uint64_t frame_es)
{
    C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_NAME, "Block Checksum");
    C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME, "Block Checksum");
}

static void ltar_sd_dec_put_data(struct srd_decoder_inst *di,
    ltar_sd_dec_state *s, uint8_t btype, int index, uint8_t value,
    uint64_t frame_ss, uint64_t frame_es)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "Block Data %d", index);
    C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_NAME, buf);

    if (btype == 0x02) {
        /* TAGGER-STATUS */
        switch (index) {
        case 0: {
            /* Player Number (bits 0-2) */
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Player Number");
            snprintf(buf, sizeof(buf), "%d", value & 0x07);
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, buf);
            /* Team Number (bits 3-4) */
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Team Number");
            snprintf(buf, sizeof(buf), "%d", (value & 0x18) >> 3);
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, buf);
            break;
        }
        case 1: {
            /* Weapon Mode (bits 0-1) */
            uint8_t wm = value & 0x03;
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Weapon Mode");
            const char *wm_name = ltar_sd_dec_lookup_weapmode(wm);
            if (wm_name)
                C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, wm_name);
            else
                C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, "Unknown");
            /* Shield State (bits 2-3) */
            uint8_t ss = (value & 0x0C) >> 2;
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Shield State");
            const char *ss_name = ltar_sd_dec_lookup_shieldstatus(ss);
            if (ss_name)
                C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, ss_name);
            else
                C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, "Unknown");
            /* Hunting Direction (bit 5) */
            uint8_t hd = (value & 0x20) >> 5;
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Hunting Direction");
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA,
                      ltar_sd_dec_lookup_huntingdir(hd));
            break;
        }
        case 2:
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Health Remaining");
            snprintf(buf, sizeof(buf), "%d", value);
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, buf);
            break;
        case 3:
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Loaded Ammo");
            snprintf(buf, sizeof(buf), "%d", value);
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, buf);
            break;
        case 4:
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Remaining Ammo, Low");
            snprintf(buf, sizeof(buf), "%d", value);
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, buf);
            break;
        case 5:
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Remaining Ammo, High");
            snprintf(buf, sizeof(buf), "%d", value << 8);
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, buf);
            break;
        case 6:
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Shield Time");
            snprintf(buf, sizeof(buf), "%d", value);
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, buf);
            break;
        case 7:
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Game Time, Minutes");
            snprintf(buf, sizeof(buf), "%d", value);
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, buf);
            break;
        case 8:
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BIT_NAME,
                      "Game Time, Seconds");
            snprintf(buf, sizeof(buf), "%d", value);
            C_ANN_PUT(di, frame_ss, frame_es, s->out_ann, ANN_FRAME_BITS_DATA, buf);
            break;
        }
    }
}

static void ltar_sd_dec_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ltar_sd_dec_state *s = (ltar_sd_dec_state *)c_decoder_get_private(di);
    if (!s)
        return;

    if (strcmp(cmd, "BLOCK") != 0)
        return;

    /* Data format from ltar_smartdevice_c:
       data[0] = frame_count (uint8_t)
       Each frame: [byte_value(1B)][bit_count(1B)][per bit: value(1B)+ss(8B LE)+es(8B LE)] */
    if (data_len < 1)
        return;

    int num_frames = data[0];
    if (num_frames <= 0 || num_frames > LTAR_SD_DEC_MAX_FRAMES)
        return;

    /* Parse frames */
    uint8_t *byte_values = (uint8_t *)g_malloc(num_frames);
    uint64_t *frame_ss = (uint64_t *)g_malloc(num_frames * sizeof(uint64_t));
    uint64_t *frame_es = (uint64_t *)g_malloc(num_frames * sizeof(uint64_t));

    const unsigned char *p = data + 1;
    for (int i = 0; i < num_frames; i++) {
        if (p + 2 > data + data_len) break;
        byte_values[i] = p[0];
        uint8_t bit_count = p[1];
        /* Skip bit data to find frame boundaries */
        if (bit_count > 0 && p + 2 + (size_t)bit_count * 17 <= data + data_len) {
            /* Frame start = first bit's ss */
            memcpy(&frame_ss[i], p + 3, 8);
            /* Frame end = last bit's es */
            memcpy(&frame_es[i], p + 2 + (bit_count - 1) * 17 + 9, 8);
            p += 2 + bit_count * 17;
        } else {
            frame_ss[i] = start_sample;
            frame_es[i] = end_sample;
            p += 2;
        }
    }

    uint8_t btype = byte_values[0];

    /* Check block length */
    ltar_sd_dec_check_block_length(di, s, btype, num_frames, start_sample, end_sample);

    /* Check block checksum */
    ltar_sd_dec_check_block_csum(di, s, byte_values, num_frames,
                                 frame_ss, frame_es, start_sample, end_sample);

    /* Process frames */
    for (int i = 0; i < num_frames; i++) {
        if (i == 0) {
            /* Block Type */
            ltar_sd_dec_put_block_type(di, s, byte_values[i], frame_ss[i], frame_es[i]);
        } else if (i == num_frames - 1) {
            /* Checksum */
            ltar_sd_dec_put_csum(di, s, frame_ss[i], frame_es[i]);
        } else {
            /* Data */
            int data_count = i - 1;
            ltar_sd_dec_put_data(di, s, btype, data_count, byte_values[i],
                                 frame_ss[i], frame_es[i]);
        }
    }

    g_free(byte_values);
    g_free(frame_ss);
    g_free(frame_es);
}

static void ltar_sd_dec_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(ltar_sd_dec_state)));
    }
    ltar_sd_dec_state *s = (ltar_sd_dec_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(ltar_sd_dec_state));
}

static void ltar_sd_dec_start(struct srd_decoder_inst *di)
{
    ltar_sd_dec_state *s = (ltar_sd_dec_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "ltar_smartdevice_decode");
}

static void ltar_sd_dec_decode(struct srd_decoder_inst *di)
{
    (void)di;
}

static void ltar_sd_dec_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder ltar_smartdevice_decode_c_decoder = {
    .id = "ltar_smartdevice_decode_c",
    .name = "LTAR SmartDevice Decode(C)",
    .longname = "LTAR SmartDevice Decode (C)",
    .desc = "A decoder for the LTAR SmartDevice protocol. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = ltar_sd_dec_ann_labels,
    .num_annotation_rows = 5,
    .annotation_rows = ltar_sd_dec_ann_rows,
    .inputs = ltar_sd_dec_inputs,
    .num_inputs = 1,
    .outputs = ltar_sd_dec_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = ltar_sd_dec_tags,
    .num_tags = 1,
    .reset = ltar_sd_dec_reset,
    .start = ltar_sd_dec_start,
    .decode = ltar_sd_dec_decode,
    .destroy = ltar_sd_dec_destroy,
    .recv_proto = ltar_sd_dec_recv_proto,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &ltar_smartdevice_decode_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
