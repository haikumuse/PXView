#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_DATETIME = 0,
    ANN_REGISTER,
    ANN_ALARM,
    ANN_CONTROL,
    ANN_TEMPERATURE,
    ANN_READ_DATETIME,
    ANN_WRITE_DATETIME,
    ANN_READ_ALARM,
    ANN_WRITE_ALARM,
    ANN_READ_TEMP,
    ANN_WARN,
    NUM_ANN,
};

typedef struct {
    int out_ann;
} ds3231_state;

static struct srd_decoder_option ds3231_options[] = {
    {"day0", NULL, "First day of week", NULL, NULL},
};

static const char *ds3231_inputs[] = {"i2c", NULL};
static const char *ds3231_outputs[] = {"ds3231", NULL};
static const char *ds3231_tags[] = {"Clock/timing", "IC", NULL};

static const char *ds3231_ann_labels[][3] = {
    {"", "datetime", "Date/Time"},
    {"", "register", "Register"},
    {"", "alarm", "Alarm"},
    {"", "control", "Control/Status"},
    {"", "temperature", "Temperature"},
    {"", "read-datetime", "Read date/time"},
    {"", "write-datetime", "Write date/time"},
    {"", "read-alarm", "Read alarm"},
    {"", "write-alarm", "Write alarm"},
    {"", "read-temp", "Read temperature"},
    {"", "warning", "Warning"},
};

static const int ds3231_row_datetime_classes[] = {ANN_DATETIME, ANN_READ_DATETIME, ANN_WRITE_DATETIME, -1};
static const int ds3231_row_register_classes[] = {ANN_REGISTER, -1};
static const int ds3231_row_alarm_classes[] = {ANN_ALARM, ANN_READ_ALARM, ANN_WRITE_ALARM, -1};
static const int ds3231_row_control_classes[] = {ANN_CONTROL, -1};
static const int ds3231_row_temperature_classes[] = {ANN_TEMPERATURE, ANN_READ_TEMP, -1};
static const int ds3231_row_actions_classes[] = {ANN_READ_DATETIME, ANN_WRITE_DATETIME, ANN_READ_ALARM, ANN_WRITE_ALARM, ANN_READ_TEMP, -1};
static const int ds3231_row_warnings_classes[] = {ANN_WARN, -1};
static const struct srd_c_ann_row ds3231_ann_rows[] = {
    {"datetime", "Date/Time", ds3231_row_datetime_classes, 3},
    {"registers", "Registers", ds3231_row_register_classes, 1},
    {"alarms", "Alarms", ds3231_row_alarm_classes, 3},
    {"control", "Control/Status", ds3231_row_control_classes, 1},
    {"temperature", "Temperature", ds3231_row_temperature_classes, 2},
    {"actions", "Actions", ds3231_row_actions_classes, 5},
    {"warnings", "Warnings", ds3231_row_warnings_classes, 1},
};

static void ds3231_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(ds3231_state)));
    }
    ds3231_state *s = (ds3231_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(ds3231_state));
}

static void ds3231_start(struct srd_decoder_inst *di)
{
    ds3231_state *s = (ds3231_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "ds3231");
}

static void ds3231_decode(struct srd_decoder_inst *di)
{
    uint64_t samplenum;
    uint64_t matched;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;
    }
}

static void ds3231_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder ds3231_c_decoder = {
    .id = "ds3231_c",
    .name = "Ds3231(C)",
    .longname = "Maxim DS3231 (C)",
    .desc = "Maxim DS3231 realtime clock module protocol. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = ds3231_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = ds3231_ann_labels,
    .num_annotation_rows = 7,
    .annotation_rows = ds3231_ann_rows,
    .inputs = ds3231_inputs,
    .num_inputs = 1,
    .outputs = ds3231_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = ds3231_tags,
    .num_tags = 2,
    .reset = ds3231_reset,
    .start = ds3231_start,
    .decode = ds3231_decode,
    .destroy = ds3231_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    ds3231_options[0].def = g_variant_new_int64(0);
    return &ds3231_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
