#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum maple_ann {
    ANN_START = 0,
    ANN_END,
    ANN_START_CRC,
    ANN_OCCUPANCY,
    ANN_RESET,
    ANN_BIT,
    ANN_SIZE,
    ANN_SOURCE,
    ANN_DEST,
    ANN_COMMAND,
    ANN_DATA,
    ANN_CHECKSUM,
    ANN_FRAME_ERROR,
    ANN_CHECKSUM_ERROR,
    ANN_SIZE_ERROR,
    NUM_ANN,
};

struct maple_priv {
    uint64_t ss, es;
    int data;
    int length;
    int expected_length;
    int checksum;
    int pending_bit;
    uint64_t pending_bit_pos;
    int out_ann;
    int out_binary;
};

static struct srd_channel maple_channels[] = {
    {"sdcka", "SDCKA", "Data/clock line A", 0, SRD_CHANNEL_SCLK, "dec_maple_bus_chan_sdcka"},
    {"sdckb", "SDCKB", "Data/clock line B", 1, SRD_CHANNEL_SDATA, "dec_maple_bus_chan_sdckb"},
};

static const char *maple_ann_labels[][3] = {
    {"", "start", "Start pattern"},
    {"", "end", "End pattern"},
    {"", "start-with-crc", "Start pattern with CRC"},
    {"", "occupancy", "SDCKB occupancy pattern"},
    {"", "reset", "RESET pattern"},
    {"", "bit", "Bit"},
    {"", "size", "Data size"},
    {"", "source", "Source AP"},
    {"", "dest", "Destination AP"},
    {"", "command", "Command"},
    {"", "data", "Data"},
    {"", "checksum", "Checksum"},
    {"", "frame-error", "Frame error"},
    {"", "checksum-error", "Checksum error"},
    {"", "size-error", "Size error"},
};

static const int maple_row_bits_classes[] = {ANN_START, ANN_END, ANN_START_CRC, ANN_OCCUPANCY, ANN_RESET, ANN_BIT, -1};
static const int maple_row_fields_classes[] = {ANN_SIZE, ANN_SOURCE, ANN_DEST, ANN_COMMAND, ANN_DATA, ANN_CHECKSUM, -1};
static const int maple_row_warnings_classes[] = {ANN_FRAME_ERROR, ANN_CHECKSUM_ERROR, ANN_SIZE_ERROR, -1};

static const struct srd_c_ann_row maple_ann_rows[] = {
    {"bits", "Bits", maple_row_bits_classes, 6},
    {"fields", "Fields", maple_row_fields_classes, 6},
    {"warnings", "Warnings", maple_row_warnings_classes, 3},
};

static const struct srd_decoder_binary maple_binary[] = {
    {0, "size", "Data size"},
    {1, "source", "Source AP"},
    {2, "dest", "Destination AP"},
    {3, "command", "Command code"},
    {4, "data", "Data"},
    {5, "checksum", "Checksum"},
};

static const char *maple_inputs[] = {"logic"};
static const char *maple_tags[] = {"Retro computing"};

static void maple_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct maple_priv)));
    struct maple_priv *s = (struct maple_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct maple_priv));
    s->out_ann = -1;
    s->out_binary = -1;
}

static void maple_start(struct srd_decoder_inst *di)
{
    struct maple_priv *s = (struct maple_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "maple_bus");
    s->out_binary = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "maple_bus");
}

static void byte_annotation(struct srd_decoder_inst *di, struct maple_priv *s,
                            int bintype, uint8_t d)
{
    static const char *ann_long[] = {"Size", "SrcAP", "DstAP", "Cmd", "Data", "Cksum"};
    static const char *ann_mid[] = {"L", "S", "D", "C", "D", "K"};

    char long_str[64], mid_str[32], short_str[8];
    snprintf(long_str, sizeof(long_str), "%s: %02X", ann_long[bintype], d);
    snprintf(mid_str, sizeof(mid_str), "%s: %02X", ann_mid[bintype], d);
    snprintf(short_str, sizeof(short_str), "%02X", d);
    C_ANN_PUT(di, s->ss, s->es, s->out_ann, bintype + 6, long_str, mid_str, short_str);

    /* Binary output */
    c_decoder_put_binary(di, s->ss, s->es, s->out_binary, bintype, 1, &d);
}

/* handle_start: wait for start pattern
   Returns: 1=Start detected, 2=Start with CRC, 0=failed/reset/occupancy */
static int handle_start(struct srd_decoder_inst *di, struct maple_priv *s)
{
    uint64_t samplenum = 0;
    uint64_t matched = 0;

    /* Wait for SDCKA=low, SDCKB=high */
    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 0);
        c_cond_or(cb);
        c_cond_edge(cb, 1);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return 0;

        int sdcka = c_decoder_get_pin(di, 0, samplenum);
        int sdckb = c_decoder_get_pin(di, 1, samplenum);

        if (sdcka == 0 && sdckb == 1)
            break;
    }

    s->ss = samplenum;
    int count = 0;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_fall(cb, 1);    /* SDCKB falling */
        c_cond_or(cb);
        c_cond_rise(cb, 0);    /* SDCKA rising */
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return 0;

        int sdcka = c_decoder_get_pin(di, 0, samplenum);
        int sdckb = c_decoder_get_pin(di, 1, samplenum);

        if (matched & (1ULL << 0)) {
            /* SDCKB fell */
            count++;
        }
        if (matched & (1ULL << 1)) {
            /* SDCKA rose */
            if (sdckb == 1) {
                s->es = samplenum;
                if (count == 4) {
                    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_START, "Start pattern", "Start", "S");
                    return 1;
                } else if (count == 6) {
                    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_START_CRC, "Start pattern with CRC", "Start+CRC", "SC");
                    return 2;
                } else if (count == 8) {
                    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_OCCUPANCY, "Occupancy pattern", "Occupancy", "O");
                    return 0;
                } else if (count >= 14) {
                    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_RESET, "RESET pattern", "RESET", "R");
                    return 0;
                }
            }
            /* Frame error */
            C_ANN_PUT(di, s->ss, samplenum, s->out_ann, ANN_FRAME_ERROR, "Frame error", "ERR", "E");
            return 0;
        }
    }
}

/* handle_byte_or_stop: decode 4 bit-pairs into a byte or detect end pattern
   Returns: 1=byte decoded, 0=end pattern or error */
static int handle_byte_or_stop(struct srd_decoder_inst *di, struct maple_priv *s)
{
    uint64_t samplenum = 0;
    uint64_t matched = 0;
    int data = 0;
    int counta = 0;
    int countb = 0;

    s->ss = 0;

    for (int bitpair = 0; bitpair < 4; bitpair++) {
        /* Wait for SDCKA falling edge or SDCKB falling edge */
        srd_cond_builder *cb = c_cond_new();
        c_cond_fall(cb, 0);    /* SDCKA falling */
        c_cond_or(cb);
        c_cond_fall(cb, 1);    /* SDCKB falling */
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return 0;

        int sdcka = c_decoder_get_pin(di, 0, samplenum);
        int sdckb = c_decoder_get_pin(di, 1, samplenum);

        if (s->ss == 0)
            s->ss = samplenum;

        if (matched & (1ULL << 0)) {
            /* SDCKA fell - read SDCKB value */
            counta++;
            int n = sdckb;
            data = data * 2 + n;
            char bit_str[4];
            snprintf(bit_str, sizeof(bit_str), "%d", n);
            C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_BIT, bit_str);
        }

        if (matched & (1ULL << 1)) {
            /* SDCKB fell - read SDCKA value */
            countb++;
            if (bitpair < 3) {
                /* Wait for SDCKA falling edge */
                cb = c_cond_new();
                c_cond_fall(cb, 0);
                ret = c_cond_wait(cb, di, &samplenum, &matched);
                c_cond_free(cb);
                if (ret != SRD_OK)
                    return 0;

                sdcka = c_decoder_get_pin(di, 0, samplenum);
                sdckb = c_decoder_get_pin(di, 1, samplenum);
                counta++;
                int n = sdckb;
                data = data * 2 + n;
                char bit_str[4];
                snprintf(bit_str, sizeof(bit_str), "%d", n);
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_BIT, bit_str);
            } else {
                /* Last bit pair - check for end pattern */
                if (counta == 1 && countb == 0 && data == 0 && sdckb == 0) {
                    /* End pattern detected */
                    /* Wait for SDCKA rising edge to complete end pattern */
                    cb = c_cond_new();
                    c_cond_rise(cb, 0);
                    ret = c_cond_wait(cb, di, &samplenum, &matched);
                    c_cond_free(cb);
                    if (ret != SRD_OK)
                        return 0;

                    C_ANN_PUT(di, s->ss, samplenum, s->out_ann, ANN_END, "End pattern", "End", "E");
                    return 0;
                }

                /* Wait for SDCKA falling edge */
                cb = c_cond_new();
                c_cond_fall(cb, 0);
                ret = c_cond_wait(cb, di, &samplenum, &matched);
                c_cond_free(cb);
                if (ret != SRD_OK)
                    return 0;

                sdcka = c_decoder_get_pin(di, 0, samplenum);
                sdckb = c_decoder_get_pin(di, 1, samplenum);
                counta++;
                int n = sdckb;
                data = data * 2 + n;
                char bit_str[4];
                snprintf(bit_str, sizeof(bit_str), "%d", n);
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_BIT, bit_str);
            }
        }
    }

    s->es = samplenum;
    s->data = data & 0xFF;
    return 1;
}

static void maple_decode(struct srd_decoder_inst *di)
{
    struct maple_priv *s = (struct maple_priv *)c_decoder_get_private(di);

    while (1) {
        /* Wait for start pattern */
        int start_type = handle_start(di, s);
        if (start_type == 0)
            continue;

        /* Decode bytes */
        s->length = 0;
        s->expected_length = 4;
        s->checksum = 0;
        int has_crc = (start_type == 2);

        while (1) {
            int ret = handle_byte_or_stop(di, s);
            if (ret == 0)
                break;

            uint8_t d = (uint8_t)s->data;
            s->length++;

            if (s->length == 1) {
                /* First byte = size */
                s->expected_length = 4 * (d + 1);
                byte_annotation(di, s, 0, d);
            } else if (s->length == 2) {
                /* Source AP */
                byte_annotation(di, s, 1, d);
            } else if (s->length == 3) {
                /* Destination AP */
                byte_annotation(di, s, 2, d);
            } else if (s->length == 4) {
                /* Command */
                byte_annotation(di, s, 3, d);
            } else if (s->length < s->expected_length + 1) {
                /* Data */
                byte_annotation(di, s, 4, d);
            } else {
                /* Checksum */
                byte_annotation(di, s, 5, d);

                /* Validate size */
                if (s->length != s->expected_length + 1) {
                    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_SIZE_ERROR,
                              "Size error", "Size ERR", "SE");
                }

                /* Validate checksum */
                if (has_crc && d != (s->checksum & 0xFF)) {
                    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_CHECKSUM_ERROR,
                              "Checksum error", "Cksum ERR", "CE");
                }
            }

            s->checksum ^= d;
        }
    }
}

static void maple_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder maple_bus_c_decoder = {
    .id = "maple_bus_c",
    .name = "Maple bus(C)",
    .longname = "SEGA Maple bus (C)",
    .desc = "Maple bus peripheral protocol for SEGA Dreamcast (C implementation)",
    .license = "gplv2+",
    .channels = maple_channels,
    .num_channels = 2,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = maple_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = maple_ann_rows,
    .inputs = maple_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = maple_binary,
    .num_binary = 6,
    .tags = maple_tags,
    .num_tags = 1,
    .reset = maple_reset,
    .start = maple_start,
    .decode = maple_decode,
    .destroy = maple_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &maple_bus_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
