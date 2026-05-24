#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

/* 1-Wire network layer decoder */

enum {
    ANN_RESET_PRESENCE = 0,
    ANN_ROM_CMD,
    ANN_ROM,
    ANN_DATA,
    ANN_WARN,
    NUM_ANN,
};

enum ow_net_state {
    STATE_COMMAND,
    STATE_GET_ROM,
    STATE_SEARCH_ROM,
    STATE_TRANSPORT,
    STATE_COMMAND_ERROR,
};

/* ROM command table */
struct rom_cmd {
    uint8_t code;
    const char *name;
    enum ow_net_state next_state;
};

static const struct rom_cmd rom_commands[] = {
    {0x33, "Read ROM",               STATE_GET_ROM},
    {0x0f, "Conditional read ROM",   STATE_GET_ROM},
    {0xcc, "Skip ROM",               STATE_TRANSPORT},
    {0x55, "Match ROM",              STATE_GET_ROM},
    {0xf0, "Search ROM",             STATE_SEARCH_ROM},
    {0xec, "Conditional search ROM", STATE_SEARCH_ROM},
    {0x3c, "Overdrive skip ROM",     STATE_TRANSPORT},
    {0x69, "Overdrive match ROM",    STATE_GET_ROM},
    {0xa5, "Resume",                 STATE_TRANSPORT},
    {0x96, "DS2408: Disable Test Mode", STATE_GET_ROM},
};

#define NUM_ROM_COMMANDS (sizeof(rom_commands) / sizeof(rom_commands[0]))

typedef struct {
    enum ow_net_state state;
    int bit_cnt;
    uint8_t data;
    uint64_t ss_block;
    uint64_t es_block;
    int out_ann;
    int out_python;
} ow_net_state;

static const char *ownet_inputs[] = {"onewire_link", NULL};
static const char *ownet_outputs[] = {"onewire_network", NULL};
static const char *ownet_tags[] = {"Embedded/industrial", NULL};

static const char *ownet_ann_labels[][3] = {
    {"", "reset-presence", "Reset/presence"},
    {"", "rom-command", "ROM command"},
    {"", "rom", "ROM address"},
    {"", "data", "Transport data"},
    {"", "warnings", "Warnings"},
};

static const int ownet_row_cmds_classes[] = {ANN_ROM_CMD, ANN_ROM, ANN_DATA, -1};
static const int ownet_row_reset_classes[] = {ANN_RESET_PRESENCE, -1};
static const int ownet_row_warnings_classes[] = {ANN_WARN, -1};

static const struct srd_c_ann_row ownet_ann_rows[] = {
    {"commands", "Commands", ownet_row_cmds_classes, 3},
    {"reset", "Reset/Presence", ownet_row_reset_classes, 1},
    {"warnings", "Warnings", ownet_row_warnings_classes, 1},
};

static const struct rom_cmd *find_rom_command(uint8_t code)
{
    for (size_t i = 0; i < NUM_ROM_COMMANDS; i++) {
        if (rom_commands[i].code == code)
            return &rom_commands[i];
    }
    return NULL;
}

static void ownet_put_proto(struct srd_decoder_inst *di, ow_net_state *s,
    uint64_t ss, uint64_t es, const char *cmd, const unsigned char *data, uint64_t data_len)
{
    if (s->out_python >= 0)
        c_decoder_put_python(di, ss, es, s->out_python, cmd, data, data_len);
}

static void ownet_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ow_net_state *s = (ow_net_state *)c_decoder_get_private(di);
    if (!s)
        return;

    uint8_t val = (data && data_len > 0) ? data[0] : 0;

    if (strcmp(cmd, "RESET/PRESENCE") == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Reset/presence: %s", val ? "true" : "false");
        C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_RESET_PRESENCE, buf);
        ownet_put_proto(di, s, start_sample, end_sample, "RESET/PRESENCE", data, data_len);
        s->state = STATE_COMMAND;
        s->bit_cnt = 0;
        s->data = 0;
        return;
    }

    if (strcmp(cmd, "BIT") != 0)
        return;

    if (s->bit_cnt == 0)
        s->ss_block = start_sample;

    s->data |= (val << s->bit_cnt);
    s->bit_cnt++;

    if (s->state == STATE_COMMAND) {
        if (s->bit_cnt < 8)
            return;
        s->es_block = end_sample;
        s->data &= 0xff;

        const struct rom_cmd *c = find_rom_command(s->data);
        if (c) {
            char buf[128];
            snprintf(buf, sizeof(buf), "ROM command: 0x%02x '%s'", s->data, c->name);
            C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_ROM_CMD, buf);
            s->state = c->next_state;
        } else {
            char buf[128];
            snprintf(buf, sizeof(buf), "ROM command: 0x%02x '%s'", s->data, "unrecognized");
            C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_ROM_CMD, buf);
            s->state = STATE_COMMAND_ERROR;
        }
        s->bit_cnt = 0;
        s->data = 0;
    } else if (s->state == STATE_GET_ROM) {
        if (s->bit_cnt < 64)
            return;
        s->es_block = end_sample;
        uint64_t rom = 0;
        for (int i = 0; i < 8; i++) {
            rom |= ((uint64_t)((s->data >> (i * 8)) & 0xff)) << (i * 8);
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "ROM: 0x%016llx", (unsigned long long)rom);
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_ROM, buf);
        unsigned char rom_data[8];
        for (int i = 0; i < 8; i++)
            rom_data[i] = (rom >> (i * 8)) & 0xff;
        ownet_put_proto(di, s, s->ss_block, s->es_block, "ROM", rom_data, 8);
        s->state = STATE_TRANSPORT;
        s->bit_cnt = 0;
        s->data = 0;
    } else if (s->state == STATE_SEARCH_ROM) {
        /* Search ROM: each bit takes 3 BIT packets (original, complement, direction) */
        /* Simplified: collect 64 bits from direction phase */
        if (s->bit_cnt < 64)
            return;
        s->es_block = end_sample;
        uint64_t rom = 0;
        for (int i = 0; i < 8; i++) {
            rom |= ((uint64_t)((s->data >> (i * 8)) & 0xff)) << (i * 8);
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "ROM: 0x%016llx", (unsigned long long)rom);
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_ROM, buf);
        unsigned char rom_data[8];
        for (int i = 0; i < 8; i++)
            rom_data[i] = (rom >> (i * 8)) & 0xff;
        ownet_put_proto(di, s, s->ss_block, s->es_block, "ROM", rom_data, 8);
        s->state = STATE_TRANSPORT;
        s->bit_cnt = 0;
        s->data = 0;
    } else if (s->state == STATE_TRANSPORT) {
        if (s->bit_cnt < 8)
            return;
        s->es_block = end_sample;
        s->data &= 0xff;
        char buf[32];
        snprintf(buf, sizeof(buf), "Data: 0x%02x", s->data);
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_DATA, buf);
        ownet_put_proto(di, s, s->ss_block, s->es_block, "DATA", &s->data, 1);
        s->bit_cnt = 0;
        s->data = 0;
    } else if (s->state == STATE_COMMAND_ERROR) {
        if (s->bit_cnt < 8)
            return;
        s->es_block = end_sample;
        s->data &= 0xff;
        char buf[64];
        snprintf(buf, sizeof(buf), "ROM error data: 0x%02x", s->data);
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_WARN, buf);
        s->bit_cnt = 0;
        s->data = 0;
    }
}

static void ownet_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(ow_net_state)));
    ow_net_state *s = (ow_net_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(ow_net_state));
    s->state = STATE_COMMAND;
}

static void ownet_start(struct srd_decoder_inst *di)
{
    ow_net_state *s = (ow_net_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "onewire_network");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "onewire_network");
}

static void ownet_decode(struct srd_decoder_inst *di)
{
    (void)di;
}

static void ownet_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder onewire_network_c_decoder = {
    .id = "onewire_network_c",
    .name = "OneWire network(C)",
    .longname = "1-Wire serial communication bus (network layer)(C)",
    .desc = "1-Wire network layer: ROM commands, device addressing and enumeration. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = ownet_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = ownet_ann_rows,
    .inputs = ownet_inputs,
    .num_inputs = 1,
    .outputs = ownet_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = ownet_tags,
    .num_tags = 1,
    .reset = ownet_reset,
    .start = ownet_start,
    .decode = ownet_decode,
    .destroy = ownet_destroy,
    .recv_proto = ownet_recv_proto,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &onewire_network_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
