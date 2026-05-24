#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_WARNINGS = 0,
    ANN_CONTROL_CODE,
    ANN_ADDRESS_PIN,
    ANN_RW_BIT,
    ANN_WORD_ADDR_BYTE,
    ANN_DATA_BYTE,
    ANN_CONTROL_WORD,
    ANN_WORD_ADDR,
    ANN_DATA,
    ANN_BYTE_WRITE,
    ANN_PAGE_WRITE,
    ANN_CUR_ADDR_READ,
    ANN_RANDOM_READ,
    ANN_SEQ_RANDOM_READ,
    ANN_SEQ_CUR_ADDR_READ,
    ANN_ACK_POLLING,
    ANN_SET_BANK_ADDR,
    ANN_READ_BANK_ADDR,
    ANN_SET_WP,
    ANN_CLEAR_ALL_WP,
    ANN_READ_WP,
    NUM_ANN,
};

enum eeprom24xx_state {
    EEPROM24XX_WAIT_FOR_START,
    EEPROM24XX_GET_CONTROL_WORD,
    EEPROM24XX_R_GET_ACK_NACK_AFTER_CW,
    EEPROM24XX_R_GET_WORD_ADDR_OR_BYTE,
    EEPROM24XX_R_GET_ACK_NACK_AFTER_WORD_ADDR_OR_BYTE,
    EEPROM24XX_R_GET_RESTART,
    EEPROM24XX_R_READ_BYTE,
    EEPROM24XX_R_GET_ACK_NACK_AFTER_BYTE_READ,
    EEPROM24XX_W_GET_ACK_NACK_AFTER_CW,
    EEPROM24XX_W_GET_WORD_ADDR,
    EEPROM24XX_W_GET_ACK_AFTER_WORD_ADDR,
    EEPROM24XX_W_DETERMINE_READ_OR_WRITE,
    EEPROM24XX_W_WRITE_BYTE,
    EEPROM24XX_W_GET_ACK_NACK_AFTER_BYTE_WRITTEN,
    EEPROM24XX_R2_GET_CONTROL_WORD,
    EEPROM24XX_R2_GET_ACK_AFTER_ADDR_READ,
    EEPROM24XX_R2_READ_BYTE,
    EEPROM24XX_R2_GET_ACK_NACK_AFTER_BYTE_READ,
    EEPROM24XX_GET_STOP_AFTER_LAST_BYTE,
};

#define EEPROM24XX_MAX_PACKETS 1024

typedef struct {
    uint64_t ss;
    uint64_t es;
    uint8_t databyte;
} eeprom24xx_packet;

typedef struct {
    const char *key;
    const char *vendor;
    const char *model;
    int size;
    int page_size;
    int page_wraparound;
    int addr_bytes;
    int addr_pins;
    int max_speed;
} eeprom24xx_chip;

typedef struct {
    enum eeprom24xx_state state;
    eeprom24xx_packet packets[EEPROM24XX_MAX_PACKETS];
    int num_packets;
    uint8_t bytebuf[EEPROM24XX_MAX_PACKETS];
    int num_bytebuf;
    int is_cur_addr_read;
    int is_random_access_read;
    int is_seq_random_read;
    int is_byte_write;
    int is_page_write;
    int addr_counter;
    int chip_idx;
    uint64_t ss, es;
    uint64_t ss_block, es_block;
    int out_ann;
    int out_binary;
} eeprom24xx_state;

static const eeprom24xx_chip chip_table[] = {
    {"generic", "", "Generic", 128, 8, 1, 1, 3, 400},
    {"microchip_24aa65", "Microchip", "24AA65", 8192, 64, 1, 2, 3, 400},
    {"microchip_24lc65", "Microchip", "24LC65", 8192, 64, 1, 2, 3, 400},
    {"microchip_24c65", "Microchip", "24C65", 8192, 64, 1, 2, 3, 400},
    {"microchip_24aa64", "Microchip", "24AA64", 8192, 32, 1, 2, 3, 400},
    {"microchip_24lc64", "Microchip", "24LC64", 8192, 32, 1, 2, 3, 400},
    {"microchip_24aa02uid", "Microchip", "24AA02UID", 256, 8, 1, 1, 0, 400},
    {"microchip_24aa025uid", "Microchip", "24AA025UID", 256, 16, 1, 1, 3, 400},
    {"microchip_24aa025uid_sot23", "Microchip", "24AA025UID (SOT-23)", 256, 16, 1, 1, 2, 400},
    {"onsemi_cat24c256", "ON Semiconductor", "CAT24C256", 32768, 64, 1, 2, 3, 1000},
    {"onsemi_cat24m01", "ON Semiconductor", "CAT24M01", 131072, 256, 1, 2, 2, 1000},
    {"siemens_slx_24c01", "Siemens", "SLx 24C01", 128, 8, 1, 1, 0, 400},
    {"siemens_slx_24c02", "Siemens", "SLx 24C02", 256, 8, 1, 1, 0, 400},
    {"st_m24c01", "ST", "M24C01", 128, 16, 1, 1, 3, 400},
    {"st_m24c02", "ST", "M24C02", 256, 16, 1, 1, 3, 400},
    {"xicor_x24c02", "Xicor", "X24C02", 256, 4, 1, 1, 3, 100},
};
#define NUM_CHIPS (sizeof(chip_table) / sizeof(chip_table[0]))

static const char *eeprom24xx_inputs[] = {"i2c", NULL};
static const char *eeprom24xx_tags[] = {"IC", "Memory", NULL};

static struct srd_decoder_option eeprom24xx_options[] = {
    {"chip", "dec_eeprom24xx_opt_chip", "Chip", NULL, NULL},
    {"addr_counter", "dec_eeprom24xx_opt_addr_counter", "Initial address counter value", NULL, NULL},
};

static const char *eeprom24xx_ann_labels[][3] = {
    {"", "warnings", "Warnings"},                     /* 0 */
    {"", "control-code", "Control code"},              /* 1 */
    {"", "address-pin", "Address pin (A0/A1/A2)"},    /* 2 */
    {"", "rw-bit", "Read/write bit"},                  /* 3 */
    {"", "word-addr-byte", "Word address byte"},       /* 4 */
    {"", "data-byte", "Data byte"},                    /* 5 */
    {"", "control-word", "Control word"},              /* 6 */
    {"", "word-addr", "Word address"},                 /* 7 */
    {"", "data", "Data"},                              /* 8 */
    {"", "byte-write", "Byte write"},                  /* 9 */
    {"", "page-write", "Page write"},                  /* 10 */
    {"", "cur-addr-read", "Current address read"},     /* 11 */
    {"", "random-read", "Random read"},                /* 12 */
    {"", "seq-random-read", "Sequential random read"}, /* 13 */
    {"", "seq-cur-addr-read", "Sequential current address read"}, /* 14 */
    {"", "ack-polling", "Acknowledge polling"},         /* 15 */
    {"", "set-bank-addr", "Set bank address"},          /* 16 */
    {"", "read-bank-addr", "Read bank address"},        /* 17 */
    {"", "set-wp", "Set write protection"},             /* 18 */
    {"", "clear-all-wp", "Clear all write protection"}, /* 19 */
    {"", "read-wp", "Read write protection status"},    /* 20 */
};

static const struct srd_decoder_binary eeprom24xx_binary[] = {
    {0, "binary", "Binary"},
};

static const int eeprom24xx_row_bits_bytes_classes[] = {ANN_CONTROL_CODE, ANN_ADDRESS_PIN, ANN_RW_BIT, ANN_WORD_ADDR_BYTE, ANN_DATA_BYTE, -1};
static const int eeprom24xx_row_fields_classes[] = {ANN_CONTROL_WORD, ANN_WORD_ADDR, ANN_DATA, -1};
static const int eeprom24xx_row_ops_classes[] = {ANN_BYTE_WRITE, ANN_PAGE_WRITE, ANN_CUR_ADDR_READ, ANN_RANDOM_READ, ANN_SEQ_RANDOM_READ, ANN_SEQ_CUR_ADDR_READ, ANN_ACK_POLLING, ANN_SET_BANK_ADDR, ANN_READ_BANK_ADDR, ANN_SET_WP, ANN_CLEAR_ALL_WP, ANN_READ_WP, -1};
static const int eeprom24xx_row_warnings_classes[] = {ANN_WARNINGS, -1};

static const struct srd_c_ann_row eeprom24xx_ann_rows[] = {
    {"bits-bytes", "Bits/bytes", eeprom24xx_row_bits_bytes_classes, 5},
    {"fields", "Fields", eeprom24xx_row_fields_classes, 3},
    {"ops", "Operations", eeprom24xx_row_ops_classes, 12},
    {"warnings", "Warnings", eeprom24xx_row_warnings_classes, 1},
};

static const eeprom24xx_chip *eeprom24xx_get_chip(eeprom24xx_state *s)
{
    if (s->chip_idx >= 0 && s->chip_idx < (int)NUM_CHIPS)
        return &chip_table[s->chip_idx];
    return &chip_table[0];
}

static void eeprom24xx_reset_variables(eeprom24xx_state *s)
{
    s->state = EEPROM24XX_WAIT_FOR_START;
    s->num_packets = 0;
    s->num_bytebuf = 0;
    s->is_cur_addr_read = 0;
    s->is_random_access_read = 0;
    s->is_seq_random_read = 0;
    s->is_byte_write = 0;
    s->is_page_write = 0;
}

static void eeprom24xx_packet_append(eeprom24xx_state *s, uint8_t databyte)
{
    if (s->num_packets < EEPROM24XX_MAX_PACKETS) {
        s->packets[s->num_packets].ss = s->ss;
        s->packets[s->num_packets].es = s->es;
        s->packets[s->num_packets].databyte = databyte;
        s->num_packets++;
    }
    s->bytebuf[s->num_bytebuf++] = databyte;
}

static void eeprom24xx_putb(struct srd_decoder_inst *di, eeprom24xx_state *s, int cls, const char *text)
{
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, cls, text);
}

static void eeprom24xx_put_warning(struct srd_decoder_inst *di, eeprom24xx_state *s, const char *msg)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "Warning: %s", msg);
    eeprom24xx_putb(di, s, ANN_WARNINGS, buf);
}

static void eeprom24xx_put_word_addr(struct srd_decoder_inst *di, eeprom24xx_state *s)
{
    const eeprom24xx_chip *chip = eeprom24xx_get_chip(s);
    eeprom24xx_packet *p = s->packets;
    int n = s->num_packets;

    if (chip->addr_bytes == 1) {
        if (n >= 2) {
            uint8_t a = p[1].databyte;
            char buf[128];
            snprintf(buf, sizeof(buf), "Word address byte: %02X", a);
            C_ANN_PUT(di, p[1].ss, p[1].es, s->out_ann, ANN_WORD_ADDR_BYTE, buf);
            C_ANN_PUT(di, p[1].ss, p[1].es, s->out_ann, ANN_WORD_ADDR, "Word address", "Word addr", "Addr", "A");
            s->addr_counter = a;
        }
    } else {
        if (n >= 3) {
            uint8_t ah = p[1].databyte;
            uint8_t al = p[2].databyte;
            char buf[128];
            snprintf(buf, sizeof(buf), "Word address high byte: %02X", ah);
            C_ANN_PUT(di, p[1].ss, p[1].es, s->out_ann, ANN_WORD_ADDR_BYTE, buf);
            snprintf(buf, sizeof(buf), "Word address low byte: %02X", al);
            C_ANN_PUT(di, p[2].ss, p[2].es, s->out_ann, ANN_WORD_ADDR_BYTE, buf);
            C_ANN_PUT(di, p[1].ss, p[2].es, s->out_ann, ANN_WORD_ADDR, "Word address", "Word addr", "Addr", "A");
            s->addr_counter = (ah << 8) | al;
        }
    }
}

static void eeprom24xx_put_data_byte(struct srd_decoder_inst *di, eeprom24xx_state *s, eeprom24xx_packet *p)
{
    const eeprom24xx_chip *chip = eeprom24xx_get_chip(s);
    char addr_buf[16];
    if (chip->addr_bytes == 1)
        snprintf(addr_buf, sizeof(addr_buf), "%02X", s->addr_counter);
    else
        snprintf(addr_buf, sizeof(addr_buf), "%04X", s->addr_counter);

    char buf[128];
    snprintf(buf, sizeof(buf), "Data byte %s: %02X", addr_buf, p->databyte);
    C_ANN_PUT(di, p->ss, p->es, s->out_ann, ANN_DATA_BYTE, buf);
}

static void eeprom24xx_put_data_bytes(struct srd_decoder_inst *di, eeprom24xx_state *s, int idx, int cls, const char *op_name)
{
    const eeprom24xx_chip *chip = eeprom24xx_get_chip(s);
    int n = s->num_packets;

    /* Output individual data byte annotations */
    for (int i = idx; i < n; i++) {
        eeprom24xx_put_data_byte(di, s, &s->packets[i]);
        s->addr_counter++;
    }

    /* Output data field annotation */
    if (idx < n) {
        C_ANN_PUT(di, s->packets[idx].ss, s->packets[n - 1].es, s->out_ann, ANN_DATA, "Data", "D");
    }

    /* Output operation annotation */
    char addr_str[16];
    if (chip->addr_bytes == 1)
        snprintf(addr_str, sizeof(addr_str), "%02X", s->bytebuf[0]);
    else
        snprintf(addr_str, sizeof(addr_str), "%02X%02X", s->bytebuf[0], s->bytebuf[1]);

    int num_data_bytes = s->num_bytebuf - chip->addr_bytes;
    char len_str[32];
    snprintf(len_str, sizeof(len_str), "%d byte%s", num_data_bytes, (num_data_bytes != 1) ? "s" : "");

    char buf[256];
    snprintf(buf, sizeof(buf), "%s (addr=%s, %s): ", op_name, addr_str, len_str);
    int pos = strlen(buf);
    for (int i = chip->addr_bytes; i < s->num_bytebuf && pos < (int)sizeof(buf) - 10; i++)
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X ", s->bytebuf[i]);

    eeprom24xx_putb(di, s, cls, buf);

    /* Output binary data */
    if (s->num_bytebuf > chip->addr_bytes) {
        c_decoder_put_binary(di, s->ss_block, s->es_block, s->out_binary, 0,
            s->num_bytebuf - chip->addr_bytes,
            &s->bytebuf[chip->addr_bytes]);
    }
}

static void eeprom24xx_decide_on_seq_or_rnd_read(eeprom24xx_state *s)
{
    if (s->num_bytebuf < 2) {
        eeprom24xx_reset_variables(s);
        return;
    }
    if (s->num_bytebuf == 2)
        s->is_random_access_read = 1;
    else
        s->is_seq_random_read = 1;
}

static void eeprom24xx_put_operation(struct srd_decoder_inst *di, eeprom24xx_state *s)
{
    const eeprom24xx_chip *chip = eeprom24xx_get_chip(s);
    int idx = 1 + chip->addr_bytes;

    if (s->is_byte_write) {
        eeprom24xx_put_word_addr(di, s);
        eeprom24xx_put_data_bytes(di, s, idx, ANN_BYTE_WRITE, "Byte write");
    } else if (s->is_page_write) {
        eeprom24xx_put_word_addr(di, s);
        int initial_addr = s->addr_counter;
        eeprom24xx_put_data_bytes(di, s, idx, ANN_PAGE_WRITE, "Page write");
        int num_bytes_to_write = s->num_packets - idx;
        if (num_bytes_to_write > chip->page_size) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Wrote %d bytes but page size is only %d bytes!",
                     num_bytes_to_write, chip->page_size);
            eeprom24xx_put_warning(di, s, buf);
        }
        int page1 = initial_addr / chip->page_size;
        int page2 = (s->addr_counter - 1) / chip->page_size;
        if (page1 != page2) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Page write crossed page boundary from page %d to %d!", page1, page2);
            eeprom24xx_put_warning(di, s, buf);
        }
    } else if (s->is_cur_addr_read) {
        if (s->num_packets >= 2) {
            eeprom24xx_put_data_byte(di, s, &s->packets[1]);
            C_ANN_PUT(di, s->packets[1].ss, s->packets[s->num_packets - 1].es, s->out_ann, ANN_DATA, "Data", "D");
            char buf[64];
            snprintf(buf, sizeof(buf), "Current address read: %02X", s->bytebuf[0]);
            eeprom24xx_putb(di, s, ANN_CUR_ADDR_READ, buf);
            c_decoder_put_binary(di, s->ss_block, s->es_block, s->out_binary, 0, 1, &s->bytebuf[0]);
            s->addr_counter++;
        }
    } else if (s->is_random_access_read) {
        eeprom24xx_put_word_addr(di, s);
        eeprom24xx_put_data_bytes(di, s, idx + 1, ANN_RANDOM_READ, "Random access read");
    } else if (s->is_seq_random_read) {
        eeprom24xx_put_word_addr(di, s);
        eeprom24xx_put_data_bytes(di, s, idx + 1, ANN_SEQ_RANDOM_READ, "Sequential random read");
    }
}

static void eeprom24xx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    eeprom24xx_state *s = (eeprom24xx_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (strcmp(cmd, "BITS") == 0)
        return;

    uint8_t databyte = (data && data_len > 0) ? data[0] : 0;

    switch (s->state) {
    case EEPROM24XX_WAIT_FOR_START:
        if (strcmp(cmd, "START") == 0 || strcmp(cmd, "START REPEAT") == 0) {
            s->ss_block = start_sample;
            s->state = EEPROM24XX_GET_CONTROL_WORD;
        }
        break;

    case EEPROM24XX_GET_CONTROL_WORD:
        if (strcmp(cmd, "ADDRESS READ") == 0 || strcmp(cmd, "ADDRESS WRITE") == 0) {
            eeprom24xx_packet_append(s, databyte);
            /* Output control word annotation */
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_CONTROL_WORD, "Control word", "Control", "CW", "C");
            if (strcmp(cmd, "ADDRESS READ") == 0)
                s->state = EEPROM24XX_R_GET_ACK_NACK_AFTER_CW;
            else
                s->state = EEPROM24XX_W_GET_ACK_NACK_AFTER_CW;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_R_GET_ACK_NACK_AFTER_CW:
        if (strcmp(cmd, "ACK") == 0) {
            s->state = EEPROM24XX_R_GET_WORD_ADDR_OR_BYTE;
        } else if (strcmp(cmd, "NACK") == 0) {
            s->es_block = end_sample;
            eeprom24xx_put_warning(di, s, "No reply from slave!");
            eeprom24xx_reset_variables(s);
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_R_GET_WORD_ADDR_OR_BYTE:
        if (strcmp(cmd, "STOP") == 0) {
            s->es_block = end_sample;
            eeprom24xx_put_warning(di, s, "Slave replied, but master aborted!");
            eeprom24xx_reset_variables(s);
        } else if (strcmp(cmd, "DATA READ") == 0) {
            eeprom24xx_packet_append(s, databyte);
            s->state = EEPROM24XX_R_GET_ACK_NACK_AFTER_WORD_ADDR_OR_BYTE;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_R_GET_ACK_NACK_AFTER_WORD_ADDR_OR_BYTE:
        if (strcmp(cmd, "ACK") == 0) {
            s->state = EEPROM24XX_R_GET_RESTART;
        } else if (strcmp(cmd, "NACK") == 0) {
            s->is_cur_addr_read = 1;
            s->state = EEPROM24XX_GET_STOP_AFTER_LAST_BYTE;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_R_GET_RESTART:
        if (strcmp(cmd, "START REPEAT") == 0) {
            s->state = EEPROM24XX_R_READ_BYTE;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_R_READ_BYTE:
        if (strcmp(cmd, "DATA READ") == 0) {
            eeprom24xx_packet_append(s, databyte);
            s->state = EEPROM24XX_R_GET_ACK_NACK_AFTER_BYTE_READ;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_R_GET_ACK_NACK_AFTER_BYTE_READ:
        if (strcmp(cmd, "ACK") == 0) {
            s->state = EEPROM24XX_R_READ_BYTE;
        } else if (strcmp(cmd, "NACK") == 0) {
            s->state = EEPROM24XX_GET_STOP_AFTER_LAST_BYTE;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_W_GET_ACK_NACK_AFTER_CW:
        if (strcmp(cmd, "ACK") == 0) {
            s->state = EEPROM24XX_W_GET_WORD_ADDR;
        } else if (strcmp(cmd, "NACK") == 0) {
            s->es_block = end_sample;
            eeprom24xx_put_warning(di, s, "No reply from slave!");
            eeprom24xx_reset_variables(s);
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_W_GET_WORD_ADDR:
        if (strcmp(cmd, "STOP") == 0) {
            s->es_block = end_sample;
            eeprom24xx_put_warning(di, s, "Slave replied, but master aborted!");
            eeprom24xx_reset_variables(s);
        } else if (strcmp(cmd, "DATA WRITE") == 0) {
            eeprom24xx_packet_append(s, databyte);
            s->state = EEPROM24XX_W_GET_ACK_AFTER_WORD_ADDR;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_W_GET_ACK_AFTER_WORD_ADDR:
        if (strcmp(cmd, "ACK") == 0) {
            s->state = EEPROM24XX_W_DETERMINE_READ_OR_WRITE;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_W_DETERMINE_READ_OR_WRITE:
        if (strcmp(cmd, "START REPEAT") == 0) {
            s->state = EEPROM24XX_R2_GET_CONTROL_WORD;
        } else if (strcmp(cmd, "DATA WRITE") == 0) {
            eeprom24xx_packet_append(s, databyte);
            s->state = EEPROM24XX_W_GET_ACK_NACK_AFTER_BYTE_WRITTEN;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_W_WRITE_BYTE:
        if (strcmp(cmd, "DATA WRITE") == 0) {
            eeprom24xx_packet_append(s, databyte);
            s->state = EEPROM24XX_W_GET_ACK_NACK_AFTER_BYTE_WRITTEN;
        } else if (strcmp(cmd, "STOP") == 0) {
            if (s->num_bytebuf < 2) {
                eeprom24xx_reset_variables(s);
                break;
            }
            s->es_block = end_sample;
            if (s->num_bytebuf == 2)
                s->is_byte_write = 1;
            else
                s->is_page_write = 1;
            eeprom24xx_put_operation(di, s);
            eeprom24xx_reset_variables(s);
        } else if (strcmp(cmd, "START REPEAT") == 0) {
            s->state = EEPROM24XX_R2_GET_CONTROL_WORD;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_W_GET_ACK_NACK_AFTER_BYTE_WRITTEN:
        if (strcmp(cmd, "ACK") == 0) {
            s->state = EEPROM24XX_W_WRITE_BYTE;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_R2_GET_CONTROL_WORD:
        if (strcmp(cmd, "ADDRESS READ") == 0) {
            eeprom24xx_packet_append(s, databyte);
            s->state = EEPROM24XX_R2_GET_ACK_AFTER_ADDR_READ;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_R2_GET_ACK_AFTER_ADDR_READ:
        if (strcmp(cmd, "ACK") == 0) {
            s->state = EEPROM24XX_R2_READ_BYTE;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_R2_READ_BYTE:
        if (strcmp(cmd, "DATA READ") == 0) {
            eeprom24xx_packet_append(s, databyte);
            s->state = EEPROM24XX_R2_GET_ACK_NACK_AFTER_BYTE_READ;
        } else if (strcmp(cmd, "STOP") == 0) {
            eeprom24xx_decide_on_seq_or_rnd_read(s);
            s->es_block = end_sample;
            eeprom24xx_put_warning(di, s, "STOP expected after a NACK (not ACK)");
            eeprom24xx_put_operation(di, s);
            eeprom24xx_reset_variables(s);
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_R2_GET_ACK_NACK_AFTER_BYTE_READ:
        if (strcmp(cmd, "ACK") == 0) {
            s->state = EEPROM24XX_R2_READ_BYTE;
        } else if (strcmp(cmd, "NACK") == 0) {
            eeprom24xx_decide_on_seq_or_rnd_read(s);
            s->state = EEPROM24XX_GET_STOP_AFTER_LAST_BYTE;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;

    case EEPROM24XX_GET_STOP_AFTER_LAST_BYTE:
        if (strcmp(cmd, "STOP") == 0) {
            s->es_block = end_sample;
            eeprom24xx_put_operation(di, s);
            eeprom24xx_reset_variables(s);
        } else if (strcmp(cmd, "START REPEAT") == 0) {
            s->es_block = end_sample;
            eeprom24xx_put_warning(di, s, "STOP expected (not RESTART)");
            eeprom24xx_put_operation(di, s);
            eeprom24xx_reset_variables(s);
            s->ss_block = start_sample;
            s->state = EEPROM24XX_GET_CONTROL_WORD;
        } else {
            eeprom24xx_reset_variables(s);
        }
        break;
    }

    /* Global STOP handler */
    if (strcmp(cmd, "STOP") == 0) {
        if (s->state != EEPROM24XX_WAIT_FOR_START) {
            eeprom24xx_reset_variables(s);
        }
    }
}

static void eeprom24xx_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(eeprom24xx_state)));
    }
    eeprom24xx_state *s = (eeprom24xx_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(eeprom24xx_state));
    s->state = EEPROM24XX_WAIT_FOR_START;
    s->chip_idx = 0;
    s->addr_counter = 0;
}

static void eeprom24xx_start(struct srd_decoder_inst *di)
{
    eeprom24xx_state *s = (eeprom24xx_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "eeprom24xx");
    s->out_binary = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "eeprom24xx");

    const char *chip_key = c_decoder_get_option_string(di, "chip", "generic");
    s->chip_idx = 0;
    for (int i = 0; i < (int)NUM_CHIPS; i++) {
        if (strcmp(chip_key, chip_table[i].key) == 0) {
            s->chip_idx = i;
            break;
        }
    }

    s->addr_counter = (int)c_decoder_get_option_int(di, "addr_counter", 0);
}

static void eeprom24xx_decode(struct srd_decoder_inst *di)
{
    (void)di;
}

static void eeprom24xx_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder eeprom24xx_c_decoder = {
    .id = "eeprom24xx_c",
    .name = "24xx EEPROM(C)",
    .longname = "24xx I2C EEPROM (C)",
    .desc = "24xx series I2C EEPROM protocol. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = eeprom24xx_options,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = eeprom24xx_ann_labels,
    .num_annotation_rows = 4,
    .annotation_rows = eeprom24xx_ann_rows,
    .inputs = eeprom24xx_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = eeprom24xx_binary,
    .num_binary = 1,
    .tags = eeprom24xx_tags,
    .num_tags = 2,
    .reset = eeprom24xx_reset,
    .start = eeprom24xx_start,
    .decode = eeprom24xx_decode,
    .destroy = eeprom24xx_destroy,
    .recv_proto = eeprom24xx_recv_proto,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    eeprom24xx_options[0].def = g_variant_new_string("generic");
    GSList *chip_vals = NULL;
    for (int i = 0; i < (int)NUM_CHIPS; i++)
        chip_vals = g_slist_append(chip_vals, g_variant_new_string(chip_table[i].key));
    eeprom24xx_options[0].values = chip_vals;

    eeprom24xx_options[1].def = g_variant_new_int64(0);

    return &eeprom24xx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
