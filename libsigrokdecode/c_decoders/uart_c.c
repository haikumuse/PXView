#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum uart_state {
    WAIT_FOR_START_BIT,
    GET_START_BIT,
    GET_DATA_BITS,
    GET_PARITY_BIT,
    GET_STOP_BITS,
};

enum uart_parity {
    PARITY_NONE,
    PARITY_ODD,
    PARITY_EVEN,
    PARITY_ZERO,
    PARITY_ONE,
    PARITY_IGNORE,
};

enum uart_ann {
    RX_DATA = 0,
    TX_DATA,
    RX_START,
    TX_START,
    RX_PARITY_OK,
    TX_PARITY_OK,
    RX_PARITY_ERR,
    TX_PARITY_ERR,
    RX_STOP,
    TX_STOP,
    RX_WARN,
    TX_WARN,
    RX_DATA_BIT,
    TX_DATA_BIT,
    RX_BREAK,
    TX_BREAK,
    RX_PACKET,
    TX_PACKET,
    RX_SAMPLES,
    TX_SAMPLES,
    ATK_POINT,
    NUM_ANN,
};

#define RX 0
#define TX 1

typedef struct {
    enum uart_state state[2];
    int state_num[2];
    uint64_t frame_start[2];
    int frame_valid[2];
    int datavalue[2];
    int paritybit[2];
    int databit_count[2];
    int stopbit_count[2];

    uint64_t samplerate;
    double baudrate;
    int data_bits;
    double stop_bits;
    enum uart_parity parity_type;
    int bit_order_msb;
    int format;
    int invert_rx;
    int invert_tx;

    double bit_width;
    double half_bit_width;
    double bit_samplenum;

    double frame_len_samples;
    int has_rx;
    int has_tx;

    int out_ann;
    int out_python;
    int show_data_point;
} uart_state;

static struct srd_channel uart_optional_channels[] = {
    {"rx", "RX", "UART receive line", 0, SRD_CHANNEL_SDATA, NULL},
    {"tx", "TX", "UART transmit line", 1, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option uart_options[] = {
    {"baudrate", NULL, "Baud rate(\xe6\xb3\xa2\xe7\x89\xb9\xe7\x8e\x87)", NULL, NULL},
    {"data_bits", NULL, "Data bits(\xe6\x95\xb0\xe6\x8d\xae\xe4\xbd\x8d\xe6\x95\xb0)", NULL, NULL},
    {"stop_bits", NULL, "Stop bits(\xe5\x81\x9c\xe6\xad\xa2\xe4\xbd\x8d)", NULL, NULL},
    {"parity", NULL, "Parity(\xe6\xa0\xa1\xe9\xaa\x8c\xe4\xbd\x8d)", NULL, NULL},
    {"bit_order", NULL, "Bit order(\xe4\xbd\x8d\xe5\xba\x8f)", NULL, NULL},
    {"format", NULL, "Data format(\xe6\x95\xb0\xe6\x8d\xae\xe6\xa0\xbc\xe5\xbc\x8f)", NULL, NULL},
    {"invert_rx", NULL, "Invert RX(\xe5\x8f\x8d\xe8\xbd\xacRX)", NULL, NULL},
    {"invert_tx", NULL, "Invert TX(\xe5\x8f\x8d\xe8\xbd\xacTX)", NULL, NULL},
    {"show_data_point", NULL, "Show data point(\xe6\x95\xb0\xe6\x8d\xae\xe7\x82\xb9\xe6\x98\xbe\xe7\xa4\xba)", NULL, NULL},
};

static const char *uart_ann_labels[][3] = {
    {"", "rx-data", "RX data"},
    {"", "tx-data", "TX data"},
    {"", "rx-start", "RX start bit"},
    {"", "tx-start", "TX start bit"},
    {"", "rx-parity-ok", "RX parity OK bit"},
    {"", "tx-parity-ok", "TX parity OK bit"},
    {"", "rx-parity-err", "RX parity error bit"},
    {"", "tx-parity-err", "TX parity error bit"},
    {"", "rx-stop", "RX stop bit"},
    {"", "tx-stop", "TX stop bit"},
    {"", "rx-warning", "RX warning"},
    {"", "tx-warning", "TX warning"},
    {"", "rx-data-bit", "RX data bit"},
    {"", "tx-data-bit", "TX data bit"},
    {"", "rx-break", "RX break"},
    {"", "tx-break", "TX break"},
    {"", "rx-packet", "RX packet"},
    {"", "tx-packet", "TX packet"},
    {"", "rx-sample", "RX sample"},
    {"", "tx-sample", "TX sample"},
    {"", "atk-data-point", "ATK Data point"},
};

static const int uart_row_rx_bits_classes[] = {RX_DATA_BIT, -1};
static const int uart_row_rx_samples_classes[] = {RX_SAMPLES, -1};
static const int uart_row_rx_data_classes[] = {RX_DATA, -1};
static const int uart_row_rx_warn_classes[] = {RX_WARN, RX_PARITY_ERR, -1};
static const int uart_row_rx_break_classes[] = {RX_BREAK, -1};
static const int uart_row_rx_packet_classes[] = {RX_PACKET, -1};
static const int uart_row_tx_bits_classes[] = {TX_DATA_BIT, -1};
static const int uart_row_tx_samples_classes[] = {TX_SAMPLES, -1};
static const int uart_row_tx_data_classes[] = {TX_DATA, -1};
static const int uart_row_tx_warn_classes[] = {TX_WARN, TX_PARITY_ERR, -1};
static const int uart_row_tx_break_classes[] = {TX_BREAK, -1};
static const int uart_row_tx_packet_classes[] = {TX_PACKET, -1};
static const int uart_row_atk_classes[] = {ATK_POINT, -1};

static const struct srd_c_ann_row uart_ann_rows[] = {
    {"rx-data-bits", "RX bits", uart_row_rx_bits_classes, 1},
    {"rx-samples", "RX samples", uart_row_rx_samples_classes, 1},
    {"rx-data-vals", "RX data", uart_row_rx_data_classes, 1},
    {"rx-warnings", "RX warnings", uart_row_rx_warn_classes, 2},
    {"rx-breaks", "RX breaks", uart_row_rx_break_classes, 1},
    {"rx-packets", "RX packets", uart_row_rx_packet_classes, 1},
    {"tx-data-bits", "TX bits", uart_row_tx_bits_classes, 1},
    {"tx-samples", "TX samples", uart_row_tx_samples_classes, 1},
    {"tx-data-vals", "TX data", uart_row_tx_data_classes, 1},
    {"tx-warnings", "TX warnings", uart_row_tx_warn_classes, 2},
    {"tx-breaks", "TX breaks", uart_row_tx_break_classes, 1},
    {"tx-packets", "TX packets", uart_row_tx_packet_classes, 1},
    {"atk-signs", "ATK signs", uart_row_atk_classes, 1},
};

static const char *uart_inputs[] = {"logic"};
static const char *uart_outputs[] = {"uart"};
static const char *uart_tags[] = {"Embedded/industrial"};

static int parity_ok(enum uart_parity ptype, int parity_bit, int data, int data_bits)
{
    if (ptype == PARITY_NONE)
        return 1;
    if (ptype == PARITY_IGNORE)
        return 1;
    if (ptype == PARITY_ZERO)
        return (parity_bit == 0);
    if (ptype == PARITY_ONE)
        return (parity_bit == 1);

    int ones = 0;
    for (int i = 0; i < data_bits; i++) {
        if (data & (1 << i))
            ones++;
    }
    ones += parity_bit;

    if (ptype == PARITY_ODD)
        return (ones % 2) == 1;
    if (ptype == PARITY_EVEN)
        return (ones % 2) == 0;

    return 1;
}

static void uart_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(uart_state)));
    }
    uart_state *s = (uart_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(uart_state));

    s->state[RX] = WAIT_FOR_START_BIT;
    s->state[TX] = WAIT_FOR_START_BIT;
    s->state_num[RX] = 0;
    s->state_num[TX] = 0;
    s->frame_valid[RX] = 1;
    s->frame_valid[TX] = 1;
    s->baudrate = 115200;
    s->data_bits = 8;
    s->stop_bits = 1.0;
    s->parity_type = PARITY_NONE;
    s->bit_order_msb = 0;
    s->format = 0;
    s->invert_rx = 0;
    s->invert_tx = 0;
    s->out_ann = 0;
    s->out_python = -1;
    s->show_data_point = 1;
}

static void uart_start(struct srd_decoder_inst *di)
{
    uart_state *s = (uart_state *)c_decoder_get_private(di);

    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "uart");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "uart");

    s->baudrate = (double)c_decoder_get_option_int(di, "baudrate", 115200);
    s->data_bits = (int)c_decoder_get_option_int(di, "data_bits", 8);
    s->stop_bits = c_decoder_get_option_double(di, "stop_bits", 1.0);

    const char *parity_str = c_decoder_get_option_string(di, "parity", "none");
    if (parity_str && strcmp(parity_str, "odd") == 0)
        s->parity_type = PARITY_ODD;
    else if (parity_str && strcmp(parity_str, "even") == 0)
        s->parity_type = PARITY_EVEN;
    else if (parity_str && strcmp(parity_str, "zero") == 0)
        s->parity_type = PARITY_ZERO;
    else if (parity_str && strcmp(parity_str, "one") == 0)
        s->parity_type = PARITY_ONE;
    else if (parity_str && strcmp(parity_str, "ignore") == 0)
        s->parity_type = PARITY_IGNORE;
    else
        s->parity_type = PARITY_NONE;

    const char *show_dp_str = c_decoder_get_option_string(di, "show_data_point", "yes");
    s->show_data_point = (strcmp(show_dp_str, "yes") == 0) ? 1 : 0;

    const char *bit_order_str = c_decoder_get_option_string(di, "bit_order", "lsb-first");
    s->bit_order_msb = (strcmp(bit_order_str, "msb-first") == 0) ? 1 : 0;

    const char *format_str = c_decoder_get_option_string(di, "format", "hex");
    if (format_str && strcmp(format_str, "ascii") == 0)
        s->format = 1;
    else if (format_str && strcmp(format_str, "dec") == 0)
        s->format = 2;
    else if (format_str && strcmp(format_str, "oct") == 0)
        s->format = 3;
    else if (format_str && strcmp(format_str, "bin") == 0)
        s->format = 4;
    else
        s->format = 0;

    const char *inv_rx_str = c_decoder_get_option_string(di, "invert_rx", "no");
    s->invert_rx = (strcmp(inv_rx_str, "yes") == 0) ? 1 : 0;

    const char *inv_tx_str = c_decoder_get_option_string(di, "invert_tx", "no");
    s->invert_tx = (strcmp(inv_tx_str, "yes") == 0) ? 1 : 0;

    s->has_rx = c_decoder_has_channel(di, 0);
    s->has_tx = c_decoder_has_channel(di, 1);

    s->samplerate = c_decoder_get_samplerate(di);
    if (s->samplerate > 0 && s->baudrate > 0) {
        s->bit_width = (double)s->samplerate / s->baudrate;
        s->half_bit_width = s->bit_width * 0.5;
        s->bit_samplenum = s->bit_width * 0.5;

        double frame_samples = 1.0;
        frame_samples += s->data_bits;
        frame_samples += (s->parity_type != PARITY_NONE) ? 1.0 : 0.0;
        frame_samples += s->stop_bits;
        frame_samples *= s->bit_width;
        s->frame_len_samples = frame_samples;
    }
}

static uint64_t get_bit_sample_point_for_rxtx(uart_state *s, int rxtx, int bit_num)
{
    return (uint64_t)(s->frame_start[rxtx] + (uint64_t)round(bit_num * s->bit_width + s->bit_samplenum));
}

static uint64_t get_bit_start(uart_state *s, int rxtx, int bit_num)
{
    return (uint64_t)(s->frame_start[rxtx] + (uint64_t)round(bit_num * s->bit_width));
}

static uint64_t get_bit_end(uart_state *s, int rxtx, int bit_num)
{
    return (uint64_t)(s->frame_start[rxtx] + (uint64_t)round((bit_num + 1) * s->bit_width));
}

static void handle_data_complete(struct srd_decoder_inst *di, int rxtx)
{
    uart_state *s = (uart_state *)c_decoder_get_private(di);

    int data = s->datavalue[rxtx];

    uint64_t ss = get_bit_start(s, rxtx, 1);
    uint64_t es = get_bit_end(s, rxtx, s->data_bits);

    char val_str[32];
    if (s->format == 1) {
        if (data >= 0x20 && data <= 0x7E)
            snprintf(val_str, sizeof(val_str), "%c", data);
        else
            snprintf(val_str, sizeof(val_str), "\\x%02X", data);
    } else if (s->format == 2) {
        snprintf(val_str, sizeof(val_str), "%d", data);
    } else if (s->format == 3) {
        snprintf(val_str, sizeof(val_str), "%o", data);
    } else if (s->format == 4) {
        int pos = 0;
        for (int i = s->data_bits - 1; i >= 0 && pos < (int)sizeof(val_str) - 1; i--) {
            val_str[pos++] = (data & (1 << i)) ? '1' : '0';
        }
        val_str[pos] = '\0';
    } else {
        if (s->data_bits <= 8)
            snprintf(val_str, sizeof(val_str), "%02X", data);
        else
            snprintf(val_str, sizeof(val_str), "%03X", data);
    }

    C_ANN_PUT(di, ss, es, s->out_ann, RX_DATA + rxtx, val_str);

    unsigned char data_byte = (unsigned char)data;
    c_decoder_put_python(di, ss, es, s->out_python, "DATA", &data_byte, 1);

    if (s->show_data_point) {
        C_ANN_PUT(di, ss, es, s->out_ann, ATK_POINT, "color:#F32FDC");
    }
}

static void handle_frame_complete(struct srd_decoder_inst *di, int rxtx)
{
    uart_state *s = (uart_state *)c_decoder_get_private(di);

    if (!s->frame_valid[rxtx]) {
        uint64_t ss = s->frame_start[rxtx];
        uint64_t es = ss + (uint64_t)round(s->frame_len_samples);
        C_ANN_PUT(di, ss, es, s->out_ann, RX_WARN + rxtx, "Frame err");
    }
}

static int get_rxtx_pin(uart_state *s, struct srd_decoder_inst *di, int rxtx, int ch, uint64_t samplenum)
{
    int val = c_decoder_get_pin(di, ch, samplenum);
    if ((rxtx == RX && s->invert_rx) || (rxtx == TX && s->invert_tx))
        val = val ? 0 : 1;
    return val;
}

static void process_rxtx(struct srd_decoder_inst *di, int rxtx, uint64_t samplenum)
{
    uart_state *s = (uart_state *)c_decoder_get_private(di);
    int ch = rxtx;
    int signal = get_rxtx_pin(s, di, rxtx, ch, samplenum);

    switch (s->state[rxtx]) {
    case WAIT_FOR_START_BIT:
        if (signal == 0) {
            s->frame_start[rxtx] = samplenum;
            s->frame_valid[rxtx] = 1;
            s->datavalue[rxtx] = 0;
            s->paritybit[rxtx] = -1;
            s->databit_count[rxtx] = 0;
            s->stopbit_count[rxtx] = 0;
            s->state[rxtx] = GET_START_BIT;
        }
        break;

    case GET_START_BIT:
        {
            uint64_t sample_point = get_bit_sample_point_for_rxtx(s, rxtx, 0);
            if (samplenum >= sample_point) {
                int start_bit = get_rxtx_pin(s, di, rxtx, ch, sample_point);
                uint64_t ss = get_bit_start(s, rxtx, 0);
                uint64_t es = get_bit_end(s, rxtx, 0);

                if (start_bit != 0) {
                    C_ANN_PUT(di, ss, samplenum, s->out_ann, RX_WARN + rxtx, "Start err");
                    c_decoder_put_python(di, ss, samplenum, s->out_python, "INVALID STARTBIT", NULL, 0);
                    s->frame_valid[rxtx] = 0;
                    handle_frame_complete(di, rxtx);
                    s->state[rxtx] = WAIT_FOR_START_BIT;
                } else {
                    C_ANN_PUT(di, ss, es, s->out_ann, RX_START + rxtx, "Start bit");
                    c_decoder_put_python(di, ss, es, s->out_python, "STARTBIT", NULL, 0);
                    s->state[rxtx] = GET_DATA_BITS;
                    s->databit_count[rxtx] = 0;
                }
            }
        }
        break;

    case GET_DATA_BITS:
        {
            int bit_idx = s->databit_count[rxtx];
            uint64_t sample_point = get_bit_sample_point_for_rxtx(s, rxtx, bit_idx + 1);
            if (samplenum >= sample_point) {
                int bit_val = get_rxtx_pin(s, di, rxtx, ch, sample_point);
                uint64_t ss = get_bit_start(s, rxtx, bit_idx + 1);
                uint64_t es = get_bit_end(s, rxtx, bit_idx + 1);

                char bit_str[4];
                snprintf(bit_str, sizeof(bit_str), "%d", bit_val);
                C_ANN_PUT(di, ss, es, s->out_ann, RX_DATA_BIT + rxtx, bit_str);

                if (s->bit_order_msb) {
                    s->datavalue[rxtx] |= (bit_val << (s->data_bits - 1 - bit_idx));
                } else {
                    s->datavalue[rxtx] |= (bit_val << bit_idx);
                }

                s->databit_count[rxtx]++;

                if (s->databit_count[rxtx] >= s->data_bits) {
                    handle_data_complete(di, rxtx);

                    if (s->parity_type != PARITY_NONE) {
                        s->state[rxtx] = GET_PARITY_BIT;
                    } else {
                        s->state[rxtx] = GET_STOP_BITS;
                        s->stopbit_count[rxtx] = 0;
                    }
                }
            }
        }
        break;

    case GET_PARITY_BIT:
        {
            int parity_bit_num = 1 + s->data_bits;
            uint64_t sample_point = get_bit_sample_point_for_rxtx(s, rxtx, parity_bit_num);
            if (samplenum >= sample_point) {
                int parity_val = get_rxtx_pin(s, di, rxtx, ch, sample_point);
                s->paritybit[rxtx] = parity_val;
                uint64_t ss = get_bit_start(s, rxtx, parity_bit_num);
                uint64_t es = get_bit_end(s, rxtx, parity_bit_num);

                if (parity_ok(s->parity_type, parity_val, s->datavalue[rxtx], s->data_bits)) {
                    C_ANN_PUT(di, ss, es, s->out_ann, RX_PARITY_OK + rxtx, "Parity bit");
                    unsigned char pval = (unsigned char)parity_val;
                    c_decoder_put_python(di, ss, es, s->out_python, "PARITYBIT", &pval, 1);
                } else {
                    C_ANN_PUT(di, ss, es, s->out_ann, RX_PARITY_ERR + rxtx, "Parity err");
                    unsigned char pval = (unsigned char)parity_val;
                    c_decoder_put_python(di, ss, es, s->out_python, "PARITYBIT", &pval, 1);
                    c_decoder_put_python(di, ss, es, s->out_python, "PARITY ERROR", NULL, 0);
                    s->frame_valid[rxtx] = 0;
                }

                s->state[rxtx] = GET_STOP_BITS;
                s->stopbit_count[rxtx] = 0;
            }
        }
        break;

    case GET_STOP_BITS:
        {
            int stop_bit_num = 1 + s->data_bits;
            if (s->parity_type != PARITY_NONE)
                stop_bit_num++;

            stop_bit_num += s->stopbit_count[rxtx];

            uint64_t sample_point;
            int is_half_stop = 0;

            double remaining_stop = s->stop_bits - s->stopbit_count[rxtx];
            if (remaining_stop > 0.4 && remaining_stop < 0.6) {
                is_half_stop = 1;
            }

            if (is_half_stop) {
                sample_point = s->frame_start[rxtx] +
                    (uint64_t)round(stop_bit_num * s->bit_width + s->bit_samplenum * 0.5);
            } else {
                sample_point = get_bit_sample_point_for_rxtx(s, rxtx, stop_bit_num);
            }

            if (samplenum >= sample_point) {
                int stop_val = get_rxtx_pin(s, di, rxtx, ch, sample_point);
                uint64_t ss, es;

                if (is_half_stop) {
                    ss = s->frame_start[rxtx] + (uint64_t)round(stop_bit_num * s->bit_width);
                    es = ss + (uint64_t)round(s->bit_width * 0.5);
                } else {
                    ss = get_bit_start(s, rxtx, stop_bit_num);
                    es = get_bit_end(s, rxtx, stop_bit_num);
                }

                if (stop_val != 1) {
                    C_ANN_PUT(di, ss, es, s->out_ann, RX_WARN + rxtx, "Stop err");
                    c_decoder_put_python(di, ss, es, s->out_python, "INVALID STOPBIT", NULL, 0);
                    s->frame_valid[rxtx] = 0;
                } else {
                    C_ANN_PUT(di, ss, es, s->out_ann, RX_STOP + rxtx, "Stop bit");
                    unsigned char sval = (unsigned char)stop_val;
                    c_decoder_put_python(di, ss, es, s->out_python, "STOPBIT", &sval, 1);
                }

                s->stopbit_count[rxtx]++;

                double total_stop_counted = s->stopbit_count[rxtx];
                int all_stop_bits_done = 0;

                if (s->stop_bits == 0.5) {
                    all_stop_bits_done = (s->stopbit_count[rxtx] >= 1);
                } else if (s->stop_bits == 1.0) {
                    all_stop_bits_done = (s->stopbit_count[rxtx] >= 1);
                } else if (s->stop_bits == 1.5) {
                    all_stop_bits_done = (s->stopbit_count[rxtx] >= 2);
                } else if (s->stop_bits == 2.0) {
                    all_stop_bits_done = (s->stopbit_count[rxtx] >= 2);
                } else {
                    all_stop_bits_done = (total_stop_counted >= s->stop_bits);
                }

                if (all_stop_bits_done) {
                    handle_frame_complete(di, rxtx);
                    uint64_t frame_ss = s->frame_start[rxtx];
                    uint64_t frame_es = frame_ss + (uint64_t)round(s->frame_len_samples);
                    unsigned char data_byte = (unsigned char)s->datavalue[rxtx];
                    c_decoder_put_python(di, frame_ss, frame_es, s->out_python, "FRAME", &data_byte, 1);
                    s->state[rxtx] = WAIT_FOR_START_BIT;
                }
            }
        }
        break;
    }
}

static void uart_decode(struct srd_decoder_inst *di)
{
    uart_state *s = (uart_state *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;

    if (s->samplerate == 0 || s->baudrate == 0 || s->bit_width == 0)
        return;

    if (!s->has_rx && !s->has_tx)
        return;

    C_ANN_PUT(di, 0, 0, s->out_ann, ATK_POINT, "color:#F32FDC");

    while (1) {
        srd_cond_builder *b = c_cond_new();
        int has_cond = 0;

        if (s->state[RX] == WAIT_FOR_START_BIT && s->has_rx) {
            c_cond_fall(b, 0);
            c_cond_or(b);
            has_cond = 1;
        }

        if (s->state[TX] == WAIT_FOR_START_BIT && s->has_tx) {
            c_cond_fall(b, 1);
            c_cond_or(b);
            has_cond = 1;
        }

        if (s->state[RX] != WAIT_FOR_START_BIT && s->has_rx) {
            int bit_num;
            switch (s->state[RX]) {
            case GET_START_BIT: bit_num = 0; break;
            case GET_DATA_BITS: bit_num = 1 + s->databit_count[RX]; break;
            case GET_PARITY_BIT: bit_num = 1 + s->data_bits; break;
            case GET_STOP_BITS:
                bit_num = 1 + s->data_bits;
                if (s->parity_type != PARITY_NONE) bit_num++;
                bit_num += s->stopbit_count[RX];
                break;
            default: bit_num = 0; break;
            }

            uint64_t target_sample;
            if (s->state[RX] == GET_STOP_BITS) {
                double remaining_stop = s->stop_bits - s->stopbit_count[RX];
                int is_half = (remaining_stop > 0.4 && remaining_stop < 0.6);
                if (is_half) {
                    target_sample = s->frame_start[RX] +
                        (uint64_t)round(bit_num * s->bit_width + s->bit_samplenum * 0.5);
                } else {
                    target_sample = get_bit_sample_point_for_rxtx(s, RX, bit_num);
                }
            } else {
                target_sample = get_bit_sample_point_for_rxtx(s, RX, bit_num);
            }

            if (target_sample > samplenum || s->state[RX] == GET_START_BIT) {
                uint64_t skip_count = (target_sample > samplenum) ? (target_sample - samplenum) : 0;
                c_cond_skip(b, skip_count);
                c_cond_or(b);
                has_cond = 1;
            }
        }

        if (s->state[TX] != WAIT_FOR_START_BIT && s->has_tx) {
            int bit_num;
            switch (s->state[TX]) {
            case GET_START_BIT: bit_num = 0; break;
            case GET_DATA_BITS: bit_num = 1 + s->databit_count[TX]; break;
            case GET_PARITY_BIT: bit_num = 1 + s->data_bits; break;
            case GET_STOP_BITS:
                bit_num = 1 + s->data_bits;
                if (s->parity_type != PARITY_NONE) bit_num++;
                bit_num += s->stopbit_count[TX];
                break;
            default: bit_num = 0; break;
            }

            uint64_t target_sample;
            if (s->state[TX] == GET_STOP_BITS) {
                double remaining_stop = s->stop_bits - s->stopbit_count[TX];
                int is_half = (remaining_stop > 0.4 && remaining_stop < 0.6);
                if (is_half) {
                    target_sample = s->frame_start[TX] +
                        (uint64_t)round(bit_num * s->bit_width + s->bit_samplenum * 0.5);
                } else {
                    target_sample = get_bit_sample_point_for_rxtx(s, TX, bit_num);
                }
            } else {
                target_sample = get_bit_sample_point_for_rxtx(s, TX, bit_num);
            }

            if (target_sample > samplenum || s->state[TX] == GET_START_BIT) {
                uint64_t skip_count = (target_sample > samplenum) ? (target_sample - samplenum) : 0;
                c_cond_skip(b, skip_count);
                c_cond_or(b);
                has_cond = 1;
            }
        }

        if (!has_cond) {
            c_cond_skip(b, 1);
        }

        int ret = c_cond_wait(b, di, &samplenum, &matched);
        c_cond_free(b);

        if (ret != SRD_OK)
            return;

        if (s->has_rx)
            process_rxtx(di, RX, samplenum);
        if (s->has_tx)
            process_rxtx(di, TX, samplenum);
    }
}

static void uart_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder uart_c_decoder = {
    .id = "uart_c",
    .name = "UART(C)",
    .longname = "Universal Asynchronous Receiver/Transmitter (C)",
    .desc = "UART protocol decoder (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = uart_optional_channels,
    .num_optional_channels = 2,
    .options = uart_options,
    .num_options = 9,
    .num_annotations = NUM_ANN,
    .ann_labels = uart_ann_labels,
    .num_annotation_rows = 13,
    .annotation_rows = uart_ann_rows,
    .inputs = uart_inputs,
    .num_inputs = 1,
    .outputs = uart_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = uart_tags,
    .num_tags = 1,
    .reset = uart_reset,
    .start = uart_start,
    .decode = uart_decode,
    .destroy = uart_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &uart_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
