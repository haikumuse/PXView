#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CH_CLK 0
#define CH_DIO 1
#define CH_STB 2

enum tmc_state {
    STATE_FIND_START,
    STATE_FIND_DATA,
    STATE_FIND_ACK,
    STATE_FIND_STOP,
};

enum tmc_ann {
    ANN_START = 0,
    ANN_STOP,
    ANN_ACK,
    ANN_NACK,
    ANN_COMMAND,
    ANN_DATA,
    ANN_BIT,
    ANN_WARN,
    NUM_ANN,
};

struct tmc_priv {
    int state;
    int bustype;       /* 0=WIRE2, 1=WIRE3 */
    int bitcount;
    uint8_t databyte;
    uint64_t ss_byte;
    uint64_t ss_ack;
    uint64_t ss;
    uint64_t es;
    uint64_t pdu_start;
    int pdu_bits;
    int bytecount;
    int radix;         /* 0=Hex, 1=Dec, 2=Oct, 3=Bin */
    uint64_t samplerate;
    int out_ann;
    int out_python;
    int out_binary;
    int out_bitrate;
    struct { int val; uint64_t ss; uint64_t es; } bits[8];
};

static struct srd_channel tmc_channels[] = {
    { "clk", "CLK", "Clock line", 0, SRD_CHANNEL_SCLK, NULL },
    { "dio", "DIO", "Data line", 1, SRD_CHANNEL_SDATA, NULL },
};

static struct srd_channel tmc_optional_channels[] = {
    { "stb", "STB", "Strobe line", 2, SRD_CHANNEL_COMMON, NULL },
};

static struct srd_decoder_option tmc_options[] = {
    { "radix", NULL, "Number format", NULL, NULL },
};

static const char *tmc_ann_labels[][3] = {
    { "", "Start", "S" },
    { "", "Stop", "P" },
    { "", "ACK", "A" },
    { "", "NACK", "N" },
    { "", "Command", "Cmd", "C" },
    { "", "Data", "D" },
    { "", "Bit", "B" },
    { "", "Warnings", "Warn", "W" },
};

static const int tmc_row_bits_classes[] = { ANN_BIT, -1 };
static const int tmc_row_data_classes[] = { ANN_START, ANN_STOP, ANN_ACK, ANN_NACK, ANN_COMMAND, ANN_DATA, -1 };
static const int tmc_row_warnings_classes[] = { ANN_WARN, -1 };

static const struct srd_c_ann_row tmc_ann_rows[] = {
    { "bits", "Bits", tmc_row_bits_classes, 1 },
    { "data", "Cmd/Data", tmc_row_data_classes, 6 },
    { "warnings", "Warnings", tmc_row_warnings_classes, 1 },
};

static const struct srd_decoder_binary tmc_binary[] = {
    { 0, "DATA", "D" },
};

static const char *tmc_inputs[] = { "logic" };
static const char *tmc_outputs[] = { "tmc" };
static const char *tmc_tags[] = { "Embedded/industrial" };

static void tmc_format_value(uint8_t val, int radix, char *out, int out_size)
{
    switch (radix) {
    case 0: snprintf(out, out_size, "0x%02X", val); break;
    case 1: snprintf(out, out_size, "%u", val); break;
    case 2: snprintf(out, out_size, "%03o", val); break;
    case 3: {
        char tmp[9];
        for (int i = 0; i < 8; i++)
            tmp[i] = (val >> (7 - i)) & 1 ? '1' : '0';
        tmp[8] = '\0';
        snprintf(out, out_size, "%s", tmp);
        break;
    }
    default: snprintf(out, out_size, "0x%02X", val); break;
    }
}

static void tmc_clear_data(struct tmc_priv *s)
{
    s->bitcount = 0;
    s->databyte = 0;
    memset(s->bits, 0, sizeof(s->bits));
}

static void tmc_handle_bitrate(struct srd_decoder_inst *di, struct tmc_priv *s, uint64_t samplenum)
{
    if (!s->samplerate || !s->pdu_start)
        return;
    double elapsed = 1.0 / (double)s->samplerate;
    elapsed *= (double)(samplenum - s->pdu_start - 1);
    if (elapsed > 0) {
        int bitrate = (int)(1.0 / elapsed * s->pdu_bits);
        c_decoder_put_meta_int(di, s->ss_byte, samplenum, s->out_bitrate, bitrate);
    }
}

static void tmc_handle_start(struct srd_decoder_inst *di, struct tmc_priv *s, uint64_t samplenum)
{
    s->ss = samplenum;
    s->es = samplenum;
    s->pdu_start = samplenum;
    s->pdu_bits = 0;
    s->bytecount = 0;
    c_decoder_put_python(di, samplenum, samplenum, s->out_python, "START", NULL, 0);
    C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_START, "Start", "S");
    tmc_clear_data(s);
    s->state = STATE_FIND_DATA;
}

static void tmc_handle_data_wire2(struct srd_decoder_inst *di, struct tmc_priv *s, uint64_t samplenum, int dio)
{
    /* Insert bit at front, LSB-first */
    memmove(&s->bits[1], &s->bits[0], sizeof(s->bits[0]) * 7);
    s->bits[0].val = dio;
    s->bits[0].ss = samplenum;
    s->bits[0].es = samplenum;

    /* Register end sample of previous bit and display it */
    if (s->bitcount > 0) {
        s->bits[1].es = samplenum;
        if (s->bitcount <= 8) {
            char bit_str[4];
            snprintf(bit_str, sizeof(bit_str), "%d", s->bits[1].val);
            C_ANN_PUT(di, s->bits[1].ss, s->bits[1].es, s->out_ann, ANN_BIT, bit_str);
        }
    }

    s->bitcount++;
    if (s->bitcount <= 8) {
        s->databyte >>= 1;
        s->databyte |= (dio << 7);
        return;
    }

    /* Display data byte */
    s->ss = s->ss_byte;
    s->es = samplenum;
    int cmd = (s->bytecount == 0) ? ANN_COMMAND : ANN_DATA;

    /* Output BITS python */
    {
        unsigned char bits_data[1 + 8 * 17];
        int bpos = 0;
        bits_data[bpos++] = 8;
        /* bits are stored in reverse order (index 0 is most recent) */
        for (int i = 7; i >= 0; i--) {
            bits_data[bpos++] = (unsigned char)s->bits[i].val;
            uint64_t bv = s->bits[i].ss;
            for (int b = 0; b < 8; b++)
                bits_data[bpos++] = (unsigned char)(bv >> (8 * b));
            bv = s->bits[i].es;
            for (int b = 0; b < 8; b++)
                bits_data[bpos++] = (unsigned char)(bv >> (8 * b));
        }
        c_decoder_put_python(di, s->ss_byte, samplenum, s->out_python, "BITS", bits_data, bpos);
    }

    /* Output COMMAND/DATA python */
    {
        unsigned char byte_data[1];
        byte_data[0] = s->databyte;
        c_decoder_put_python(di, s->ss_byte, samplenum, s->out_python,
            (cmd == ANN_COMMAND) ? "COMMAND" : "DATA", byte_data, 1);
    }

    /* Binary output */
    {
        unsigned char byte_data[1];
        byte_data[0] = s->databyte;
        c_decoder_put_binary(di, s->ss_byte, samplenum, s->out_binary, 0, 1, byte_data);
    }

    /* Annotation */
    {
        char val_str[16];
        tmc_format_value(s->databyte, s->radix, val_str, sizeof(val_str));
        C_ANN_PUT_VAL(di, s->ss_byte, samplenum, s->out_ann, cmd, s->databyte, val_str);
    }

    tmc_clear_data(s);
    s->ss_ack = samplenum;
    s->bytecount++;
    s->state = STATE_FIND_ACK;
}

static void tmc_handle_byte_wire3(struct srd_decoder_inst *di, struct tmc_priv *s, uint64_t samplenum)
{
    if (s->bitcount == 0)
        return;

    /* Update end sample of the last bit */
    s->bits[0].es = samplenum;

    /* Display all bits */
    for (int i = s->bitcount - 1; i >= 0; i--) {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", s->bits[i].val);
        C_ANN_PUT(di, s->bits[i].ss, s->bits[i].es, s->out_ann, ANN_BIT, bit_str);
    }

    /* Display data byte */
    s->ss = s->ss_byte;
    s->es = samplenum;
    int cmd = (s->bytecount == 0) ? ANN_COMMAND : ANN_DATA;

    /* Output BITS python */
    {
        unsigned char bits_data[1 + 8 * 17];
        int bpos = 0;
        bits_data[bpos++] = (unsigned char)s->bitcount;
        for (int i = s->bitcount - 1; i >= 0; i--) {
            bits_data[bpos++] = (unsigned char)s->bits[i].val;
            uint64_t bv = s->bits[i].ss;
            for (int b = 0; b < 8; b++)
                bits_data[bpos++] = (unsigned char)(bv >> (8 * b));
            bv = s->bits[i].es;
            for (int b = 0; b < 8; b++)
                bits_data[bpos++] = (unsigned char)(bv >> (8 * b));
        }
        c_decoder_put_python(di, s->ss_byte, samplenum, s->out_python, "BITS", bits_data, bpos);
    }

    /* Output COMMAND/DATA python */
    {
        unsigned char byte_data[1];
        byte_data[0] = s->databyte;
        c_decoder_put_python(di, s->ss_byte, samplenum, s->out_python,
            (cmd == ANN_COMMAND) ? "COMMAND" : "DATA", byte_data, 1);
    }

    /* Binary output */
    {
        unsigned char byte_data[1];
        byte_data[0] = s->databyte;
        c_decoder_put_binary(di, s->ss_byte, samplenum, s->out_binary, 0, 1, byte_data);
    }

    /* Annotation */
    {
        char val_str[16];
        tmc_format_value(s->databyte, s->radix, val_str, sizeof(val_str));
        C_ANN_PUT_VAL(di, s->ss_byte, samplenum, s->out_ann, cmd, s->databyte, val_str);
    }

    s->bytecount++;
}

static void tmc_handle_data_wire3(struct srd_decoder_inst *di, struct tmc_priv *s, uint64_t samplenum, int dio)
{
    if (s->bitcount >= 8) {
        tmc_handle_byte_wire3(di, s, samplenum);
        tmc_clear_data(s);
        s->ss_byte = samplenum;
    }

    memmove(&s->bits[1], &s->bits[0], sizeof(s->bits[0]) * 7);
    s->bits[0].val = dio;
    s->bits[0].ss = samplenum;
    s->bits[0].es = samplenum;

    s->databyte >>= 1;
    s->databyte |= (dio << 7);

    if (s->bitcount > 0)
        s->bits[1].es = samplenum;

    s->bitcount++;
}

static void tmc_handle_data(struct srd_decoder_inst *di, struct tmc_priv *s, uint64_t samplenum, int dio)
{
    s->pdu_bits++;
    if (s->bitcount == 0)
        s->ss_byte = samplenum;

    if (s->bustype == 0)
        tmc_handle_data_wire2(di, s, samplenum, dio);
    else
        tmc_handle_data_wire3(di, s, samplenum, dio);
}

static void tmc_handle_ack(struct srd_decoder_inst *di, struct tmc_priv *s, uint64_t samplenum, int dio)
{
    s->ss = s->ss_ack;
    s->es = samplenum;
    int cmd = dio ? ANN_NACK : ANN_ACK;
    c_decoder_put_python(di, s->ss_ack, samplenum, s->out_python,
        dio ? "NACK" : "ACK", NULL, 0);
    C_ANN_PUT(di, s->ss_ack, samplenum, s->out_ann, cmd,
        dio ? "NACK" : "ACK", dio ? "N" : "A");
    s->state = STATE_FIND_DATA;
}

static void tmc_handle_stop(struct srd_decoder_inst *di, struct tmc_priv *s, uint64_t samplenum)
{
    tmc_handle_bitrate(di, s, samplenum);

    if (s->bustype == 1) /* wire3: flush last byte */
        tmc_handle_byte_wire3(di, s, samplenum);

    /* Display stop */
    c_decoder_put_python(di, samplenum, samplenum, s->out_python, "STOP", NULL, 0);
    C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_STOP, "Stop", "P");
    tmc_clear_data(s);
    s->state = STATE_FIND_START;
}

static void tmc_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct tmc_priv)));
    struct tmc_priv *s = (struct tmc_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct tmc_priv));
    s->state = STATE_FIND_START;
    s->out_ann = -1;
    s->out_python = -1;
    s->out_binary = -1;
    s->out_bitrate = -1;
}

static void tmc_start(struct srd_decoder_inst *di)
{
    struct tmc_priv *s = (struct tmc_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "tmc");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "tmc");
    s->out_binary = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "tmc");
    s->out_bitrate = c_decoder_register_output_meta(di, SRD_OUTPUT_META,
        "tmc", "int", "Bitrate", "Bitrate from Start bit to Stop bit");

    const char *radix_str = c_decoder_get_option_string(di, "radix", "Hex");
    if (strcmp(radix_str, "Dec") == 0)
        s->radix = 1;
    else if (strcmp(radix_str, "Oct") == 0)
        s->radix = 2;
    else if (strcmp(radix_str, "Bin") == 0)
        s->radix = 3;
    else
        s->radix = 0;
}

static void tmc_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct tmc_priv *s = (struct tmc_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE)
        s->samplerate = value;
}

static void tmc_decode(struct srd_decoder_inst *di)
{
    struct tmc_priv *s = (struct tmc_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched = 0;

    if (!s->samplerate)
        s->samplerate = c_decoder_get_samplerate(di);
    if (!s->samplerate)
        return;

    while (1) {
        srd_cond_builder *cb;
        int ret;

        if (s->state == STATE_FIND_START) {
            cb = c_cond_new();
            c_cond_high(cb, CH_CLK);
            c_cond_fall(cb, CH_STB);
            c_cond_or(cb);
            c_cond_low(cb, CH_CLK);
            c_cond_fall(cb, CH_STB);
            c_cond_or(cb);
            c_cond_high(cb, CH_CLK);
            c_cond_fall(cb, CH_DIO);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            if ((matched & 0b11) || (matched & 0b10)) {
                /* condition 0 or 1: wire3 */
                s->bustype = 1;
                tmc_handle_start(di, s, samplenum);
            } else if (matched & 0b100) {
                /* condition 2: wire2 */
                s->bustype = 0;
                tmc_handle_start(di, s, samplenum);
            }
        } else if (s->state == STATE_FIND_DATA) {
            cb = c_cond_new();
            c_cond_rise(cb, CH_STB);
            c_cond_or(cb);
            c_cond_high(cb, CH_CLK);
            c_cond_rise(cb, CH_DIO);
            c_cond_or(cb);
            c_cond_rise(cb, CH_CLK);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            if ((matched & 0b1) || (matched & 0b10)) {
                /* STOP condition */
                tmc_handle_stop(di, s, samplenum);
            } else if (matched & 0b100) {
                /* CLK rising edge: data */
                int dio = c_decoder_get_pin(di, CH_DIO, samplenum);
                tmc_handle_data(di, s, samplenum, dio);
            }
        } else if (s->state == STATE_FIND_ACK) {
            cb = c_cond_new();
            c_cond_fall(cb, CH_CLK);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            int dio = c_decoder_get_pin(di, CH_DIO, samplenum);
            tmc_handle_ack(di, s, samplenum, dio);
        } else if (s->state == STATE_FIND_STOP) {
            cb = c_cond_new();
            c_cond_rise(cb, CH_STB);
            c_cond_or(cb);
            c_cond_high(cb, CH_CLK);
            c_cond_rise(cb, CH_DIO);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK)
                return;

            if ((matched & 0b1) || (matched & 0b10))
                tmc_handle_stop(di, s, samplenum);
        }
    }
}

static void tmc_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder tmc_c_decoder = {
    .id = "tmc_c",
    .name = "TMC(C)",
    .longname = "Titan Micro Circuit(C)",
    .desc = "Bus for TM1636/37/38 7-segment digital tubes.(C implementation)",
    .license = "gplv2+",
    .channels = tmc_channels,
    .num_channels = 2,
    .optional_channels = tmc_optional_channels,
    .num_optional_channels = 1,
    .options = tmc_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = tmc_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = tmc_ann_rows,
    .inputs = tmc_inputs,
    .num_inputs = 1,
    .outputs = tmc_outputs,
    .num_outputs = 1,
    .binary = tmc_binary,
    .num_binary = 1,
    .tags = tmc_tags,
    .num_tags = 1,
    .reset = tmc_reset,
    .start = tmc_start,
    .decode = tmc_decode,
    .destroy = tmc_destroy,
    .metadata = tmc_metadata,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    tmc_options[0].def = g_variant_new_string("Hex");
    GSList *radix_vals = NULL;
    radix_vals = g_slist_append(radix_vals, g_variant_new_string("Hex"));
    radix_vals = g_slist_append(radix_vals, g_variant_new_string("Dec"));
    radix_vals = g_slist_append(radix_vals, g_variant_new_string("Oct"));
    radix_vals = g_slist_append(radix_vals, g_variant_new_string("Bin"));
    tmc_options[0].values = radix_vals;
    return &tmc_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
