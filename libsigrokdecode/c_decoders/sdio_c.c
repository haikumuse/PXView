/*
 * SDIO C decoder
 * Ported from Python sdio decoder
 */

#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKEN_BITS 136
#define MAX_DATA_RECV 8192

enum sdio_ann {
    ANN_CMD0 = 0,  /* 0-63: CMD0..CMD63 */
    /* ANN_CMD1..ANN_CMD63 = 1..63 */
    ANN_ACMD0 = 64, /* 64-127: ACMD0..ACMD63 */
    /* ANN_ACMD1..ANN_ACMD63 = 65..127 */
    ANN_BITS = 128,
    ANN_FIELD_START = 129,
    ANN_FIELD_TRANSMISSION = 130,
    ANN_FIELD_CMD = 131,
    ANN_FIELD_ARG = 132,
    ANN_FIELD_CRC = 133,
    ANN_FIELD_END = 134,
    ANN_DECODED_BIT = 135,
    ANN_DECODED_FIELD = 136,
    ANN_DATA = 137,
    ANN_DATA_FIELD = 138,
    ANN_DATA_BUSY = 139,
    ANN_DATA_FIELD_ERROR = 140,
    ANN_MESSAGE = 141,
    NUM_ANN = 142,
};

enum sdio_state {
    STATE_GET_COMMAND_TOKEN = 0,
    STATE_GET_RESPONSE_R1,
    STATE_GET_RESPONSE_R1b,
    STATE_GET_RESPONSE_R2,
    STATE_GET_RESPONSE_R3,
    STATE_GET_RESPONSE_R4,
    STATE_GET_RESPONSE_R5,
    STATE_GET_RESPONSE_R6,
    STATE_GET_RESPONSE_R7,
    STATE_HANDLE_CMD,
};

enum sdio_data_state {
    DATA_STATE_IDLE = 0,
    DATA_STATE_WAIT_FOR_START,
    DATA_STATE_DATA,
    DATA_STATE_CRC,
    DATA_STATE_CARD_BUSY,
};

/* CRC functions from sd_crc.py */
static int crc_BIT(int data, int n)
{
    return (data & (1 << n)) != 0 ? 1 : 0;
}

static uint8_t crc7(const int *bin_array, int len)
{
    int data = 0;
    for (int i = 0; i < len; i++) {
        int di = bin_array[i] ^ crc_BIT(data, 6);
        data = (data & 0x07) | ((data & 0x38) << 1) | ((di ^ crc_BIT(data, 2)) << 3);
        data = (data & 0x78) | ((data & 0x03) << 1) | di;
    }
    return (uint8_t)data;
}

static uint16_t crc16(const int *bin_array, int len)
{
    int data = 0;
    for (int i = 0; i < len; i++) {
        int di = bin_array[i] ^ crc_BIT(data, 15);
        data = (data & 0x0FFF) | ((data & 0x7000) << 1) | ((di ^ crc_BIT(data, 11)) << 12);
        data = (data & 0xF01F) | ((data & 0x07E0) << 1) | ((di ^ crc_BIT(data, 4)) << 5);
        data = (data & 0xFFE0) | ((data & 0x000F) << 1) | di;
    }
    return (uint16_t)data;
}

/* Command name lookup */
static const char *cmd_names[] = {
    "GO_IDLE_STATE", NULL, "ALL_SEND_CID", "SEND_RELATIVE_ADDR",
    "SET_DSR", "IO_SEND_OP_COND", "SWITCH_FUNC", "SELECT/DESELECT_CARD",
    "SEND_IF_COND", "SEND_CSD", "SEND_CID", "VOLTAGE_SWITCH",
    "STOP_TRANSMISSION", "SEND_STATUS", NULL, "GO_INACTIVE_STATE",
    "SET_BLOCKLEN", "READ_SINGLE_BLOCK", "READ_MULTIPLE_BLOCK",
    "SEND_TUNING_BLOCK", "SPEED_CLASS_CONTROL", NULL, NULL,
    "SET_BLOCK_COUNT", "WRITE_BLOCK", "WRITE_MULTIPLE_BLOCK",
    "Reserved for manufacturer", "PROGRAM_CSD", "SET_WRITE_PROT",
    "CLR_WRITE_PROT", "SEND_WRITE_PROT", NULL, "ERASE_WR_BLK_START",
    "ERASE_WR_BLK_END", "Reserved for CMD6", "Reserved for CMD6",
    "Reserved for CMD6", "Reserved for CMD6", "ERASE", NULL,
    "Reserved for security specification", NULL, "LOCK_UNLOCK",
    NULL, NULL, NULL, NULL, NULL,
    "Reserved for CMD6", NULL, "IO_RW_DIRECT", "IO_RW_EXTENDED",
    "Unknown", "APP_CMD", "GEN_CMD", "Reserved for CMD6",
    NULL, NULL, "Reserved for manufacturer", "Reserved for manufacturer",
    "Reserved for manufacturer", "Reserved for manufacturer",
};

static const char *acmd_names[] = {
    NULL, NULL, NULL, NULL, NULL, NULL, "SET_BUS_WIDTH",
    NULL, NULL, NULL, NULL, NULL, NULL, "SD_STATUS",
    "Reserved for Security Application", "Reserved for Security Application",
    "Reserved for Security Application", NULL,
    "Reserved for SD security applications", NULL, NULL, NULL,
    "SEND_NUM_WR_BLOCKS", "SET_WR_BLK_ERASE_COUNT", NULL,
    "Reserved for SD security applications", "Reserved for SD security applications",
    "Reserved for security specification", "Reserved for security specification",
    NULL, "Reserved for security specification", "Reserved for security specification",
    "Reserved for security specification", "Reserved for security specification",
    "Reserved for security specification", "Reserved for security specification",
    NULL, NULL, "Reserved for SD security applications", NULL, NULL,
    "SD_SEND_OP_COND", "SET_CLR_CARD_DETECT",
    "Reserved for SD security applications", "Reserved for SD security applications",
    "Reserved for SD security applications", "Reserved for SD security applications",
    "Reserved for SD security applications", "Reserved for SD security applications",
    "Reserved for SD security applications", "Reserved for SD security applications",
    "Unknown", "SEND_SCR",
    "Reserved for security specification", "Reserved for security specification",
    "Reserved for security specification", "Non-existant",
    "Reserved for security specification", "Reserved for security specification",
    "Reserved for security specification", "Reserved for security specification",
    "Unknown", "Unknown", "Unknown", "Unknown",
};

static const char *get_cmd_name(int cmd, int is_acmd)
{
    if (cmd < 0 || cmd > 63) return "Unknown";
    const char **names = is_acmd ? acmd_names : cmd_names;
    return names[cmd] ? names[cmd] : "Unknown";
}

static const int cmd_list[] = {0, 2, 3, 5, 6, 7, 8, 9, 10, 13, 16, 52, 53, 55, -1};
static const int acmd_list[] = {6, 13, 41, 51, -1};

static int is_in_list(const int *list, int val)
{
    for (int i = 0; list[i] >= 0; i++)
        if (list[i] == val) return 1;
    return 0;
}

typedef struct {
    int four_line;
    int rise_sample;
    int io_block_len;

    int state;
    int is_acmd;
    int cmd;
    uint32_t arg;
    int cmd_str_is_acmd;

    uint64_t token_ss[MAX_TOKEN_BITS];
    uint64_t token_es[MAX_TOKEN_BITS];
    int token_val[MAX_TOKEN_BITS];
    int token_count;

    int data_state;
    int data_bytes_required;
    int data_crc_resp;

    uint64_t data_recv_ss[MAX_DATA_RECV];
    int data_recv_pins[MAX_DATA_RECV][4];
    int data_recv_count;

    uint16_t crc_value[4];

    int out_ann;
} sdio_state;

/* Generate ann_labels - 142 entries */
#define ANN_LABEL_CMD_START 0
#define ANN_LABEL_ACMD_START 64

static char *sdio_ann_labels_buf[NUM_ANN][3];

static void init_ann_labels(void)
{
    static int initialized = 0;
    if (initialized) return;
    initialized = 1;

    for (int i = 0; i < 64; i++) {
        static char cmd_ids[64][16], cmd_names_str[64][16];
        snprintf(cmd_ids[i], sizeof(cmd_ids[i]), "cmd%d", i);
        snprintf(cmd_names_str[i], sizeof(cmd_names_str[i]), "CMD%d", i);
        sdio_ann_labels_buf[i][0] = "";
        sdio_ann_labels_buf[i][1] = cmd_ids[i];
        sdio_ann_labels_buf[i][2] = cmd_names_str[i];
    }
    for (int i = 0; i < 64; i++) {
        static char acmd_ids[64][16], acmd_names_str[64][16];
        snprintf(acmd_ids[i], sizeof(acmd_ids[i]), "acmd%d", i);
        snprintf(acmd_names_str[i], sizeof(acmd_names_str[i]), "ACMD%d", i);
        sdio_ann_labels_buf[64 + i][0] = "";
        sdio_ann_labels_buf[64 + i][1] = acmd_ids[i];
        sdio_ann_labels_buf[64 + i][2] = acmd_names_str[i];
    }
    sdio_ann_labels_buf[128][0] = ""; sdio_ann_labels_buf[128][1] = "bits"; sdio_ann_labels_buf[128][2] = "Bits";
    sdio_ann_labels_buf[129][0] = ""; sdio_ann_labels_buf[129][1] = "field-start"; sdio_ann_labels_buf[129][2] = "Start bit";
    sdio_ann_labels_buf[130][0] = ""; sdio_ann_labels_buf[130][1] = "field-transmission"; sdio_ann_labels_buf[130][2] = "Transmission bit";
    sdio_ann_labels_buf[131][0] = ""; sdio_ann_labels_buf[131][1] = "field-cmd"; sdio_ann_labels_buf[131][2] = "Command";
    sdio_ann_labels_buf[132][0] = ""; sdio_ann_labels_buf[132][1] = "field-arg"; sdio_ann_labels_buf[132][2] = "Argument";
    sdio_ann_labels_buf[133][0] = ""; sdio_ann_labels_buf[133][1] = "field-crc"; sdio_ann_labels_buf[133][2] = "CRC";
    sdio_ann_labels_buf[134][0] = ""; sdio_ann_labels_buf[134][1] = "field-end"; sdio_ann_labels_buf[134][2] = "End bit";
    sdio_ann_labels_buf[135][0] = ""; sdio_ann_labels_buf[135][1] = "decoded-bit"; sdio_ann_labels_buf[135][2] = "Decoded bit";
    sdio_ann_labels_buf[136][0] = ""; sdio_ann_labels_buf[136][1] = "decoded-field"; sdio_ann_labels_buf[136][2] = "Decoded field";
    sdio_ann_labels_buf[137][0] = ""; sdio_ann_labels_buf[137][1] = "data"; sdio_ann_labels_buf[137][2] = "Data";
    sdio_ann_labels_buf[138][0] = ""; sdio_ann_labels_buf[138][1] = "data-field"; sdio_ann_labels_buf[138][2] = "Data fields";
    sdio_ann_labels_buf[139][0] = ""; sdio_ann_labels_buf[139][1] = "data-busy"; sdio_ann_labels_buf[139][2] = "Data busy";
    sdio_ann_labels_buf[140][0] = ""; sdio_ann_labels_buf[140][1] = "data-field-error"; sdio_ann_labels_buf[140][2] = "Data fields (Error)";
    sdio_ann_labels_buf[141][0] = ""; sdio_ann_labels_buf[141][1] = "message"; sdio_ann_labels_buf[141][2] = "Messages";
}

static struct srd_channel sdio_channels[] = {
    { "cmd", "CMD", "Command", 0, SRD_CHANNEL_SDATA, "dec_sdio_chan_cmd" },
    { "clk", "CLK", "Clock", 1, SRD_CHANNEL_SCLK, "dec_sdio_chan_clk" },
};

static struct srd_channel sdio_optional_channels[] = {
    { "dat0", "DAT0", "Data pin 0", 2, SRD_CHANNEL_SDATA, "dec_sdio_opt_chan_dat0" },
    { "dat1", "DAT1", "Data pin 1", 3, SRD_CHANNEL_SDATA, "dec_sdio_opt_chan_dat1" },
    { "dat2", "DAT2", "Data pin 2", 4, SRD_CHANNEL_SDATA, "dec_sdio_opt_chan_dat2" },
    { "dat3", "DAT3", "Data pin 3", 5, SRD_CHANNEL_SDATA, "dec_sdio_opt_chan_dat3" },
};

static struct srd_decoder_option sdio_options[] = {
    { "lines", NULL, "Lines used", NULL, NULL },
    { "io_block_len", NULL, "Block size of SDIO", NULL, NULL },
    { "polarity", NULL, "Sample edge", NULL, NULL },
};

static int sdio_row_datas_classes[] = { 137, 138, 139, 140, -1 };
static int sdio_row_raw_bits_classes[] = { 128, -1 };
static int sdio_row_decoded_bits_classes[] = { 135, -1 };
static int sdio_row_decoded_fields_classes[] = { 136, -1 };
static int sdio_row_fields_classes[] = { 129, 130, 131, 132, 133, 134, -1 };
static int sdio_row_cmd_classes[129]; /* 0..128, -1 terminated */
static int sdio_row_msg_classes[] = { 141, -1 };

static struct srd_c_ann_row sdio_ann_rows[7];

static void init_ann_rows(void)
{
    static int rows_initialized = 0;
    if (rows_initialized) return;
    rows_initialized = 1;

    for (int i = 0; i < 128; i++)
        sdio_row_cmd_classes[i] = i;
    sdio_row_cmd_classes[128] = -1;

    sdio_ann_rows[0] = (struct srd_c_ann_row){ "datas", "Datas Line", sdio_row_datas_classes, 4 };
    sdio_ann_rows[1] = (struct srd_c_ann_row){ "raw-bits", "Raw bits", sdio_row_raw_bits_classes, 1 };
    sdio_ann_rows[2] = (struct srd_c_ann_row){ "decoded-bits", "Decoded bits", sdio_row_decoded_bits_classes, 1 };
    sdio_ann_rows[3] = (struct srd_c_ann_row){ "decoded-fields", "Decoded fields", sdio_row_decoded_fields_classes, 1 };
    sdio_ann_rows[4] = (struct srd_c_ann_row){ "fields", "Fields", sdio_row_fields_classes, 6 };
    sdio_ann_rows[5] = (struct srd_c_ann_row){ "cmd", "Commands", sdio_row_cmd_classes, 128 };
    sdio_ann_rows[6] = (struct srd_c_ann_row){ "msg", "Messages", sdio_row_msg_classes, 1 };
}

static const char *sdio_inputs[] = { "logic" };
static const char *sdio_outputs[] = { "sdio" };
static const char *sdio_tags[] = { "Memory" };

static int get_token_bits(sdio_state *s, uint64_t samplenum, int cmd_val, int n)
{
    if (s->token_count > 0) {
        s->token_es[s->token_count - 1] = samplenum;
    }
    if (s->token_count < MAX_TOKEN_BITS) {
        s->token_ss[s->token_count] = samplenum;
        s->token_es[s->token_count] = samplenum;
        s->token_val[s->token_count] = cmd_val;
    }
    s->token_count++;
    if (s->token_count < n) return 0;
    if (s->token_count >= 2) {
        s->token_es[n - 1] += s->token_ss[n - 1] - s->token_ss[n - 2];
    }
    return 1;
}

static uint32_t get_token_data(sdio_state *s, int start, int end)
{
    uint32_t val = 0;
    for (int i = start; i <= end && i < s->token_count; i++) {
        val = (val << 1) | s->token_val[i];
    }
    return val;
}

static void sdio_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(sdio_state)));
    }
    sdio_state *s = (sdio_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(sdio_state));
    s->state = STATE_GET_COMMAND_TOKEN;
    s->data_state = DATA_STATE_IDLE;
    s->data_bytes_required = 512;
    s->out_ann = -1;
}

static void sdio_start(struct srd_decoder_inst *di)
{
    sdio_state *s = (sdio_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "sdio");

    const char *lines_str = c_decoder_get_option_string(di, "lines", "1-line");
    s->four_line = (strcmp(lines_str, "4-line") == 0) ? 1 : 0;

    s->io_block_len = (int)c_decoder_get_option_int(di, "io_block_len", 512);

    const char *pol_str = c_decoder_get_option_string(di, "polarity", "risedge");
    s->rise_sample = (strcmp(pol_str, "risedge") == 0) ? 1 : 0;
}

/* Helper annotation functions */
static void putf(sdio_state *s, struct srd_decoder_inst *di, int start, int end, int ann_class, const char *text, const char *short_text)
{
    if (start >= s->token_count || end >= s->token_count) return;
    if (short_text)
        C_ANN_PUT(di, s->token_ss[start], s->token_es[end], s->out_ann, ann_class, text, short_text);
    else
        C_ANN_PUT(di, s->token_ss[start], s->token_es[end], s->out_ann, ann_class, text);
}

static void puta(sdio_state *s, struct srd_decoder_inst *di, int start, int end, int ann_class, const char *text, const char *short_text)
{
    /* puta maps to token[47-8-e]..token[47-8-s] */
    int real_start = 47 - 8 - end;
    int real_end = 47 - 8 - start;
    if (real_start < 0 || real_end >= s->token_count) return;
    putf(s, di, real_start, real_end, ann_class, text, short_text);
}

static void putt(sdio_state *s, struct srd_decoder_inst *di, int ann_class, const char *text, const char *short_text)
{
    if (s->token_count < 48) return;
    if (short_text)
        C_ANN_PUT(di, s->token_ss[0], s->token_es[47], s->out_ann, ann_class, text, short_text);
    else
        C_ANN_PUT(di, s->token_ss[0], s->token_es[47], s->out_ann, ann_class, text);
}

static void handle_common_token_fields(sdio_state *s, struct srd_decoder_inst *di)
{
    /* Individual bit annotations */
    for (int bit = 0; bit < s->token_count && bit < MAX_TOKEN_BITS; bit++) {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", s->token_val[bit]);
        putf(s, di, bit, bit, ANN_BITS, bit_str, NULL);
    }

    /* Start bit */
    putf(s, di, 0, 0, ANN_FIELD_START, "Start bit", "Start");

    /* Transmission bit */
    if (s->token_count > 1) {
        const char *t = (s->token_val[1] == 1) ? "host" : "card";
        char t_str[64];
        snprintf(t_str, sizeof(t_str), "Transmission: %s", t);
        putf(s, di, 1, 1, ANN_FIELD_TRANSMISSION, t_str, t);
    }

    /* Command index */
    if (s->token_count > 7) {
        s->cmd = (int)get_token_data(s, 2, 7);
        const char *name = get_cmd_name(s->cmd, s->is_acmd);
        char c_str[128];
        snprintf(c_str, sizeof(c_str), "Command: %s (%d)", name, s->cmd);
        char c_short[64];
        snprintf(c_short, sizeof(c_short), "Cmd: %s (%d)", name, s->cmd);
        putf(s, di, 2, 7, ANN_FIELD_CMD, c_str, c_short);
    }

    /* Argument */
    if (s->token_count > 39)
        putf(s, di, 8, 39, ANN_FIELD_ARG, "Argument", "Arg");

    /* CRC */
    if (s->token_count > 46) {
        uint8_t crc_cal = crc7(s->token_val, 40);
        uint8_t crc_recv = (uint8_t)get_token_data(s, 40, 46);
        if (crc_cal != crc_recv) {
            char crc_str[64];
            snprintf(crc_str, sizeof(crc_str), "CRC Error: 0x%x(should be 0x%x)", crc_recv, crc_cal);
            putf(s, di, 40, 46, ANN_FIELD_CRC, crc_str, "CRC Error");
        } else {
            char crc_str[32];
            snprintf(crc_str, sizeof(crc_str), "CRC: 0x%x", crc_recv);
            putf(s, di, 40, 46, ANN_FIELD_CRC, crc_str, "CRC");
        }
    }

    /* End bit */
    if (s->token_count > 47)
        putf(s, di, 47, 47, ANN_FIELD_END, "End bit", "End");
}

static void cal_arg(sdio_state *s)
{
    s->arg = 0;
    for (int i = 0; i < 32 && (i + 8) < s->token_count; i++) {
        s->arg = (s->arg << 1) + s->token_val[i + 8];
    }
}

static void putc_cmd(sdio_state *s, struct srd_decoder_inst *di, int ann_class, const char *desc)
{
    char cmd_str[128];
    const char *prefix = s->cmd_str_is_acmd ? "A" : "";
    snprintf(cmd_str, sizeof(cmd_str), "%sCMD%d (%s): %s", prefix, s->cmd, get_cmd_name(s->cmd, s->is_acmd), desc);
    char short_str[64];
    snprintf(short_str, sizeof(short_str), "%sCMD%d", prefix, s->cmd);
    putt(s, di, ann_class, cmd_str, short_str);
}

static void handle_response_r1(sdio_state *s, struct srd_decoder_inst *di, int cmd_val)
{
    if (!get_token_bits(s, 0, cmd_val, 48)) return;
    handle_common_token_fields(s, di);
    putt(s, di, ANN_DECODED_FIELD, "Reply: R1", NULL);
    puta(s, di, 0, 31, ANN_DECODED_FIELD, "Card status", "Status");
    s->token_count = 0;
    s->state = STATE_GET_COMMAND_TOKEN;
}

static void handle_response_r1b(sdio_state *s, struct srd_decoder_inst *di, int cmd_val)
{
    if (!get_token_bits(s, 0, cmd_val, 48)) return;
    handle_common_token_fields(s, di);
    puta(s, di, 0, 31, ANN_DECODED_FIELD, "Card status", "Status");
    putt(s, di, ANN_DECODED_FIELD, "Reply: R1b", NULL);
    s->token_count = 0;
    s->state = STATE_GET_COMMAND_TOKEN;
}

static void handle_response_r2(sdio_state *s, struct srd_decoder_inst *di, int cmd_val)
{
    if (!get_token_bits(s, 0, cmd_val, 136)) return;
    for (int bit = 0; bit < s->token_count && bit < MAX_TOKEN_BITS; bit++) {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", s->token_val[bit]);
        putf(s, di, bit, bit, ANN_BITS, bit_str, NULL);
    }
    putf(s, di, 0, 0, ANN_FIELD_START, "Start bit", "Start");
    if (s->token_count > 1) {
        const char *t = (s->token_val[1] == 1) ? "host" : "card";
        char t_str[64];
        snprintf(t_str, sizeof(t_str), "Transmission: %s", t);
        putf(s, di, 1, 1, ANN_FIELD_TRANSMISSION, t_str, t);
    }
    putf(s, di, 2, 7, ANN_FIELD_CMD, "Reserved", "Res");
    putf(s, di, 8, 134, ANN_FIELD_ARG, "Argument", "Arg");
    putf(s, di, 135, 135, ANN_FIELD_END, "End bit", "End");
    putf(s, di, 8, 134, ANN_DECODED_FIELD, "CID/CSD register", "CID/CSD");
    s->token_count = 0;
    s->state = STATE_GET_COMMAND_TOKEN;
}

static void handle_response_r3(sdio_state *s, struct srd_decoder_inst *di, int cmd_val)
{
    if (!get_token_bits(s, 0, cmd_val, 48)) return;
    putt(s, di, ANN_DECODED_FIELD, "Reply: R3", NULL);
    for (int bit = 0; bit < s->token_count && bit < MAX_TOKEN_BITS; bit++) {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", s->token_val[bit]);
        putf(s, di, bit, bit, ANN_BITS, bit_str, NULL);
    }
    putf(s, di, 0, 0, ANN_FIELD_START, "Start bit", "Start");
    if (s->token_count > 1) {
        const char *t = (s->token_val[1] == 1) ? "host" : "card";
        char t_str[64];
        snprintf(t_str, sizeof(t_str), "Transmission: %s", t);
        putf(s, di, 1, 1, ANN_FIELD_TRANSMISSION, t_str, t);
    }
    putf(s, di, 2, 7, ANN_FIELD_CMD, "Reserved", "Res");
    putf(s, di, 8, 39, ANN_FIELD_ARG, "Argument", "Arg");
    putf(s, di, 40, 46, ANN_FIELD_CRC, "Reserved", "Res");
    putf(s, di, 47, 47, ANN_FIELD_END, "End bit", "End");
    puta(s, di, 0, 31, ANN_DECODED_FIELD, "OCR register", "OCR reg");
    s->token_count = 0;
    s->state = STATE_GET_COMMAND_TOKEN;
}

static void handle_response_r4(sdio_state *s, struct srd_decoder_inst *di, int cmd_val)
{
    if (!get_token_bits(s, 0, cmd_val, 48)) return;
    putt(s, di, ANN_DECODED_FIELD, "Reply: R4", NULL);
    for (int bit = 0; bit < s->token_count && bit < MAX_TOKEN_BITS; bit++) {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", s->token_val[bit]);
        putf(s, di, bit, bit, ANN_BITS, bit_str, NULL);
    }
    putf(s, di, 0, 0, ANN_FIELD_START, "Start bit", "Start");
    if (s->token_count > 1) {
        const char *t = (s->token_val[1] == 1) ? "host" : "card";
        char t_str[64];
        snprintf(t_str, sizeof(t_str), "Transmission: %s", t);
        putf(s, di, 1, 1, ANN_FIELD_TRANSMISSION, t_str, t);
    }
    putf(s, di, 2, 7, ANN_FIELD_CMD, "Reserved", "Res");
    putf(s, di, 8, 39, ANN_FIELD_ARG, "Argument", "Arg");
    putf(s, di, 40, 46, ANN_FIELD_CRC, "Reserved", "Res");
    putf(s, di, 47, 47, ANN_FIELD_END, "End bit", "End");
    puta(s, di, 31, 31, ANN_DECODED_FIELD, "Card ready", "ready");
    puta(s, di, 28, 30, ANN_DECODED_FIELD, "Number of I/O functions", "n.o. I/O functions");
    puta(s, di, 27, 27, ANN_DECODED_FIELD, "Memory present", "MP");
    puta(s, di, 25, 26, ANN_DECODED_FIELD, "Stuff bits", "SB");
    puta(s, di, 24, 24, ANN_DECODED_FIELD, "Switching to 1.8V accepted", "Switch to 1.8V");
    puta(s, di, 0, 23, ANN_DECODED_FIELD, "I/O operating conditions register", "I/O OCR");
    s->token_count = 0;
    s->state = STATE_GET_COMMAND_TOKEN;
}

static void handle_response_r5(sdio_state *s, struct srd_decoder_inst *di, int cmd_val)
{
    if (!get_token_bits(s, 0, cmd_val, 48)) return;
    cal_arg(s);
    handle_common_token_fields(s, di);
    puta(s, di, 0, 7, ANN_DECODED_FIELD, "Read or write data", "Data");
    puta(s, di, 8, 15, ANN_DECODED_FIELD, "Response flags", "Response");
    puta(s, di, 16, 31, ANN_DECODED_FIELD, "Stuff bits", "SB");
    putt(s, di, ANN_DECODED_FIELD, "Reply: R5", NULL);
    s->token_count = 0;
    s->state = STATE_GET_COMMAND_TOKEN;
}

static void handle_response_r6(sdio_state *s, struct srd_decoder_inst *di, int cmd_val)
{
    if (!get_token_bits(s, 0, cmd_val, 48)) return;
    handle_common_token_fields(s, di);
    puta(s, di, 0, 15, ANN_DECODED_FIELD, "Card status bits", "Status");
    uint32_t rca = get_token_data(s, 8, 23);
    char rca_str[64];
    snprintf(rca_str, sizeof(rca_str), "Relative card address 0x%X", rca);
    puta(s, di, 16, 31, ANN_DECODED_FIELD, rca_str, "Relative card address");
    putt(s, di, ANN_DECODED_FIELD, "Reply: R6", NULL);
    s->token_count = 0;
    s->state = STATE_GET_COMMAND_TOKEN;
}

static void handle_response_r7(sdio_state *s, struct srd_decoder_inst *di, int cmd_val)
{
    if (!get_token_bits(s, 0, cmd_val, 48)) return;
    handle_common_token_fields(s, di);
    putt(s, di, ANN_DECODED_FIELD, "Reply: R7", NULL);
    puta(s, di, 12, 31, ANN_DECODED_FIELD, "Reserved", "Res");
    puta(s, di, 8, 11, ANN_DECODED_FIELD, "Voltage accepted", "Voltage");
    puta(s, di, 0, 7, ANN_DECODED_FIELD, "Echo-back of check pattern", "Echo");
    s->token_count = 0;
    s->state = STATE_GET_COMMAND_TOKEN;
}

static void sdio_decode(struct srd_decoder_inst *di)
{
    sdio_state *s = (sdio_state *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    int CMD = 0, CLK = 1;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        if (s->rise_sample)
            c_cond_rise(cb, CLK);
        else
            c_cond_fall(cb, CLK);

        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;

        int cmd = c_decoder_get_pin(di, CMD, samplenum);
        int dat0 = c_decoder_has_channel(di, 2) ? c_decoder_get_pin(di, 2, samplenum) : 0;
        int dat1 = c_decoder_has_channel(di, 3) ? c_decoder_get_pin(di, 3, samplenum) : 0;
        int dat2 = c_decoder_has_channel(di, 4) ? c_decoder_get_pin(di, 4, samplenum) : 0;
        int dat3 = c_decoder_has_channel(di, 5) ? c_decoder_get_pin(di, 5, samplenum) : 0;
        int pins[6] = {cmd, 0/*clk*/, dat0, dat1, dat2, dat3};

        /* Handle data lines */
        if (s->data_state == DATA_STATE_IDLE || s->data_state == DATA_STATE_WAIT_FOR_START) {
            /* Check for data start */
            int is_pending;
            if (s->four_line)
                is_pending = (dat0 + dat1 + dat2 + dat3 >= 4);
            else
                is_pending = (dat0 == 1);

            if (s->data_recv_count == 0) {
                if (!is_pending) {
                    s->data_recv_ss[0] = samplenum;
                    s->data_recv_pins[0][0] = dat0;
                    s->data_recv_pins[0][1] = dat1;
                    s->data_recv_pins[0][2] = dat2;
                    s->data_recv_pins[0][3] = dat3;
                    s->data_recv_count = 1;
                }
            } else {
                if (s->data_recv_count > 0 && s->data_recv_count < MAX_DATA_RECV) {
                    s->data_recv_ss[s->data_recv_count] = samplenum;
                    s->data_recv_pins[s->data_recv_count][0] = dat0;
                    s->data_recv_pins[s->data_recv_count][1] = dat1;
                    s->data_recv_pins[s->data_recv_count][2] = dat2;
                    s->data_recv_pins[s->data_recv_count][3] = dat3;
                    s->data_recv_count++;
                }
                s->data_state = DATA_STATE_DATA;
            }
        } else if (s->data_state == DATA_STATE_DATA) {
            if (s->data_recv_count < MAX_DATA_RECV) {
                s->data_recv_ss[s->data_recv_count] = samplenum;
                s->data_recv_pins[s->data_recv_count][0] = dat0;
                s->data_recv_pins[s->data_recv_count][1] = dat1;
                s->data_recv_pins[s->data_recv_count][2] = dat2;
                s->data_recv_pins[s->data_recv_count][3] = dat3;
                s->data_recv_count++;
            }
            int samples_required = s->four_line ? s->data_bytes_required * 2 : s->data_bytes_required * 8;
            if (s->data_recv_count > samples_required) {
                /* Output data bytes */
                for (int i = 0; i < s->data_bytes_required; i++) {
                    int value;
                    if (s->four_line) {
                        int hi = s->data_recv_pins[i * 2][3] * 8 + s->data_recv_pins[i * 2][2] * 4 +
                                 s->data_recv_pins[i * 2][1] * 2 + s->data_recv_pins[i * 2][0];
                        int lo = s->data_recv_pins[i * 2 + 1][3] * 8 + s->data_recv_pins[i * 2 + 1][2] * 4 +
                                 s->data_recv_pins[i * 2 + 1][1] * 2 + s->data_recv_pins[i * 2 + 1][0];
                        value = (hi << 4) | lo;
                    } else {
                        value = 0;
                        for (int j = 0; j < 8; j++)
                            value |= s->data_recv_pins[i * 8 + j][0] << (7 - j);
                    }
                    char hex_str[16];
                    snprintf(hex_str, sizeof(hex_str), "0x%02X", value);
                    C_ANN_PUT(di, s->data_recv_ss[s->four_line ? i * 2 : i * 8],
                              s->data_recv_ss[s->four_line ? (i + 1) * 2 : (i + 1) * 8],
                              s->out_ann, ANN_DATA, hex_str);
                }
                s->data_state = DATA_STATE_CRC;
                /* Keep last sample for CRC */
                s->data_recv_count = 1;
                s->data_recv_ss[0] = samplenum;
                s->data_recv_pins[0][0] = dat0;
                s->data_recv_pins[0][1] = dat1;
                s->data_recv_pins[0][2] = dat2;
                s->data_recv_pins[0][3] = dat3;
            }
        } else if (s->data_state == DATA_STATE_CRC) {
            if (s->data_recv_count < MAX_DATA_RECV) {
                s->data_recv_ss[s->data_recv_count] = samplenum;
                s->data_recv_pins[s->data_recv_count][0] = dat0;
                s->data_recv_pins[s->data_recv_count][1] = dat1;
                s->data_recv_pins[s->data_recv_count][2] = dat2;
                s->data_recv_pins[s->data_recv_count][3] = dat3;
                s->data_recv_count++;
            }
            if (s->data_recv_count > 17) {
                C_ANN_PUT(di, s->data_recv_ss[0], s->data_recv_ss[15],
                          s->out_ann, ANN_DATA_FIELD, "CRC", "C");
                C_ANN_PUT(di, s->data_recv_ss[15], s->data_recv_ss[16],
                          s->out_ann, ANN_DATA_FIELD, "End", "E");
                if (s->data_crc_resp) {
                    s->data_state = DATA_STATE_CARD_BUSY;
                    s->data_recv_count = 1;
                } else {
                    s->data_state = DATA_STATE_IDLE;
                    s->data_recv_count = 0;
                }
            }
        } else if (s->data_state == DATA_STATE_CARD_BUSY) {
            if (dat0 == 1) {
                C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_DATA_BUSY, "Card Busy", "Busy", "B");
                s->data_state = DATA_STATE_IDLE;
                s->data_recv_count = 0;
            }
        }

        /* CMD line state machine */
        if (s->state >= STATE_GET_RESPONSE_R1 && s->state <= STATE_GET_RESPONSE_R7) {
            if (s->token_count == 0) {
                if (cmd != 0) continue;
                if (!get_token_bits(s, samplenum, cmd, 2)) continue;
            } else if (s->token_count < 2) {
                if (!get_token_bits(s, samplenum, cmd, 2)) continue;
                if (s->token_val[1] == 1) {
                    s->state = STATE_GET_COMMAND_TOKEN;
                    s->token_count = 0;
                    continue;
                }
            } else {
                switch (s->state) {
                case STATE_GET_RESPONSE_R1: handle_response_r1(s, di, cmd); break;
                case STATE_GET_RESPONSE_R1b: handle_response_r1b(s, di, cmd); break;
                case STATE_GET_RESPONSE_R2: handle_response_r2(s, di, cmd); break;
                case STATE_GET_RESPONSE_R3: handle_response_r3(s, di, cmd); break;
                case STATE_GET_RESPONSE_R4: handle_response_r4(s, di, cmd); break;
                case STATE_GET_RESPONSE_R5: handle_response_r5(s, di, cmd); break;
                case STATE_GET_RESPONSE_R6: handle_response_r6(s, di, cmd); break;
                case STATE_GET_RESPONSE_R7: handle_response_r7(s, di, cmd); break;
                default: break;
                }
            }
        } else {
            /* GET COMMAND TOKEN */
            if (s->token_count == 0) {
                if (cmd != 0) continue;
            }
            if (!get_token_bits(s, samplenum, cmd, 48)) continue;

            handle_common_token_fields(s, di);

            s->cmd_str_is_acmd = s->is_acmd;
            const char *prefix = s->is_acmd ? "A" : "";

            if (!s->is_acmd) {
                if (is_in_list(cmd_list, s->cmd)) {
                    char desc[128];
                    snprintf(desc, sizeof(desc), "%sCMD%d (%s)", prefix, s->cmd, get_cmd_name(s->cmd, 0));
                    putc_cmd(s, di, s->cmd, get_cmd_name(s->cmd, 0));
                } else {
                    char desc[32];
                    snprintf(desc, sizeof(desc), "CMD%d", s->cmd);
                    putc_cmd(s, di, s->cmd, desc);
                }
            } else {
                if (is_in_list(acmd_list, s->cmd)) {
                    putc_cmd(s, di, 64 + s->cmd, get_cmd_name(s->cmd, 1));
                } else {
                    char desc[32];
                    snprintf(desc, sizeof(desc), "ACMD%d", s->cmd);
                    putc_cmd(s, di, 64 + s->cmd, desc);
                }
            }

            s->data_state = DATA_STATE_WAIT_FOR_START;
            s->data_recv_count = 0;
            s->data_bytes_required = 4;
            s->data_crc_resp = 0;
            cal_arg(s);

            /* Handle specific commands */
            switch (s->cmd) {
            case 0:
                puta(s, di, 0, 31, ANN_DECODED_FIELD, "Stuff bits", "Stuff");
                s->token_count = 0;
                s->state = STATE_GET_COMMAND_TOKEN;
                break;
            case 2:
                puta(s, di, 0, 31, ANN_DECODED_FIELD, "Stuff bits", "Stuff");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R2;
                break;
            case 3:
                puta(s, di, 0, 31, ANN_DECODED_FIELD, "Stuff bits", "Stuff");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R6;
                break;
            case 5:
                puta(s, di, 25, 31, ANN_DECODED_FIELD, "Stuff bits", "Stuff");
                puta(s, di, 24, 24, ANN_DECODED_FIELD, "Switching to 1.8V Request", "Switch to 1.8V");
                puta(s, di, 0, 23, ANN_DECODED_FIELD, "Operation Conditions Register", "I/O OCR");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R4;
                break;
            case 6:
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R1;
                break;
            case 7: {
                uint32_t rca = get_token_data(s, 8, 23);
                char rca_str[64];
                snprintf(rca_str, sizeof(rca_str), "Relative card address 0x%X", rca);
                puta(s, di, 16, 31, ANN_DECODED_FIELD, rca_str, "Relative card address");
                puta(s, di, 0, 15, ANN_DECODED_FIELD, "Stuff bits", "Stuff");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R1b;
                break;
            }
            case 8:
                puta(s, di, 12, 31, ANN_DECODED_FIELD, "Reserved", "Res");
                puta(s, di, 8, 11, ANN_DECODED_FIELD, "Supply voltage", "Voltage");
                puta(s, di, 0, 7, ANN_DECODED_FIELD, "Check pattern", "Check pat");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R7;
                break;
            case 9:
                puta(s, di, 16, 31, ANN_DECODED_FIELD, "RCA", "R");
                puta(s, di, 0, 15, ANN_DECODED_FIELD, "Stuff bits", "Stuff");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R2;
                break;
            case 10:
                puta(s, di, 16, 31, ANN_DECODED_FIELD, "RCA", "R");
                puta(s, di, 0, 15, ANN_DECODED_FIELD, "Stuff bits", "Stuff");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R2;
                break;
            case 13:
                puta(s, di, 16, 31, ANN_DECODED_FIELD, "RCA", "R");
                puta(s, di, 0, 15, ANN_DECODED_FIELD, "Stuff bits", "Stuff");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R1;
                break;
            case 16:
                puta(s, di, 0, 31, ANN_DECODED_FIELD, "Block length", "Blocklen");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R1;
                break;
            case 52:
                puta(s, di, 31, 31, ANN_DECODED_FIELD, "R/W flag", "Write");
                puta(s, di, 28, 30, ANN_DECODED_FIELD, "Function number", "Function");
                puta(s, di, 27, 27, ANN_DECODED_FIELD, "RAW flag", "RAW");
                puta(s, di, 9, 25, ANN_DECODED_FIELD, "Register address", "Address");
                puta(s, di, 0, 7, ANN_DECODED_FIELD, "Write data or stuff bits", "Write data");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R5;
                break;
            case 53:
                puta(s, di, 31, 31, ANN_DECODED_FIELD, "R/W flag", "Write");
                puta(s, di, 28, 30, ANN_DECODED_FIELD, "Function number", "Function");
                puta(s, di, 27, 27, ANN_DECODED_FIELD, "Block mode", "Block");
                puta(s, di, 26, 26, ANN_DECODED_FIELD, "OP code (increasing addr)", "OP code");
                puta(s, di, 9, 25, ANN_DECODED_FIELD, "Register address", "Address");
                puta(s, di, 0, 8, ANN_DECODED_FIELD, "Byte/Block count", "Count");
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R5;
                if (!(s->arg & 0x08000000))
                    s->data_bytes_required = s->arg & 0x1ff;
                else
                    s->data_bytes_required = s->io_block_len;
                s->data_crc_resp = (s->arg & 0x80000000) != 0;
                break;
            case 55:
                puta(s, di, 16, 31, ANN_DECODED_FIELD, "RCA", "R");
                puta(s, di, 0, 15, ANN_DECODED_FIELD, "Stuff bits", "Stuff");
                s->is_acmd = 1;
                s->token_count = 0;
                s->state = STATE_GET_RESPONSE_R1;
                break;
            default:
                /* Handle ACMDs */
                if (s->is_acmd) {
                    switch (s->cmd) {
                    case 6:
                        s->token_count = 0;
                        s->state = STATE_GET_RESPONSE_R1;
                        break;
                    case 13:
                        puta(s, di, 0, 31, ANN_DECODED_FIELD, "Stuff bits", "Stuff");
                        s->token_count = 0;
                        s->state = STATE_GET_RESPONSE_R1;
                        break;
                    case 41:
                        puta(s, di, 0, 23, ANN_DECODED_FIELD, "VDD voltage window", "VDD volt");
                        puta(s, di, 30, 30, ANN_DECODED_FIELD, "Host capacity support info", "Host capacity");
                        s->token_count = 0;
                        s->state = STATE_GET_RESPONSE_R3;
                        break;
                    case 51:
                        s->token_count = 0;
                        s->state = STATE_GET_RESPONSE_R1;
                        s->data_bytes_required = 8;
                        s->data_crc_resp = 0;
                        break;
                    default:
                        s->token_count = 0;
                        s->state = STATE_GET_RESPONSE_R1;
                        break;
                    }
                } else {
                    s->token_count = 0;
                    s->state = STATE_GET_RESPONSE_R1;
                }
                break;
            }

            /* Leave ACMD mode after first non-55/63 command */
            if (s->is_acmd && s->cmd != 55 && s->cmd != 63)
                s->is_acmd = 0;
        }
    }
}

static void sdio_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder sdio_c_decoder = {
    .id = "sdio_c",
    .name = "SDIO(C)",
    .longname = "Secure Digital I/O (C)",
    .desc = "Secure Digital I/O low-level protocol. (C implementation)",
    .license = "gplv2+",
    .channels = sdio_channels,
    .num_channels = 2,
    .optional_channels = sdio_optional_channels,
    .num_optional_channels = 4,
    .options = sdio_options,
    .num_options = 3,
    .num_annotations = NUM_ANN,
    .ann_labels = (const char *(*)[3])sdio_ann_labels_buf,
    .num_annotation_rows = 7,
    .annotation_rows = sdio_ann_rows,
    .inputs = sdio_inputs,
    .num_inputs = 1,
    .outputs = sdio_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = sdio_tags,
    .num_tags = 1,
    .reset = sdio_reset,
    .start = sdio_start,
    .decode = sdio_decode,
    .destroy = sdio_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    init_ann_labels();
    init_ann_rows();

    sdio_options[0].def = g_variant_new_string("1-line");
    sdio_options[1].def = g_variant_new_string("512");
    sdio_options[2].def = g_variant_new_string("risedge");

    GSList *lines_vals = NULL;
    lines_vals = g_slist_append(lines_vals, g_variant_new_string("1-line"));
    lines_vals = g_slist_append(lines_vals, g_variant_new_string("4-line"));
    sdio_options[0].values = lines_vals;

    GSList *block_vals = NULL;
    block_vals = g_slist_append(block_vals, g_variant_new_string("128"));
    block_vals = g_slist_append(block_vals, g_variant_new_string("256"));
    block_vals = g_slist_append(block_vals, g_variant_new_string("512"));
    block_vals = g_slist_append(block_vals, g_variant_new_string("1024"));
    sdio_options[1].values = block_vals;

    GSList *pol_vals = NULL;
    pol_vals = g_slist_append(pol_vals, g_variant_new_string("risedge"));
    pol_vals = g_slist_append(pol_vals, g_variant_new_string("falledge"));
    sdio_options[2].values = pol_vals;

    return &sdio_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
