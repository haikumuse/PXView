/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 * 
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#pragma once

#include "../config.h"
#include <stdint.h>

#define countof(x) (sizeof(x)/sizeof(x[0]))

#define begin_element(x) (&x[0])
#define end_element(x) (&x[countof(x)])

enum View_type {
    TIME_VIEW,
    FFT_VIEW,
    ALL_VIEW
};

enum DEVICE_COLLECT_MODE {
    COLLECT_SINGLE = 0,
    COLLECT_REPEAT = 1,
    COLLECT_LOOP = 2,
};

enum DEVICE_STATUS_TYPE {
  ST_INIT = 0,
  ST_RUNNING = 1,
  ST_STOPPED = 2,
};

// --- PXView-local device handle types ---
// These were previously defined in PXView's fork libsigrok.h. After migrating
// to upstream libsigrok 0.6.0 (libsigrok), they are defined locally here.
// ds_device_handle is an opaque token used by SigSession's device-list API.
// The actual device identity is carried by struct sr_dev_inst* in DeviceAgent.
typedef uint64_t ds_device_handle;
#define NULL_HANDLE ((ds_device_handle)0)

// Device list entry returned by SigSession::get_device_list().
// The handle is an opaque token; name is a UTF-8 display string.
struct ds_device_base_info {
    ds_device_handle handle;
    char name[150];
};

// Extended device info used internally by DeviceAgent.
struct ds_device_full_info {
    ds_device_handle handle;
    char name[150];
    char path[256];
    char driver_name[20];
    int dev_type;
    int actived_times;
    struct sr_dev_inst *sdi;
};

// Device type classification (mirrors upstream sr_dev_inst_type but kept
// as a PXView-local enum for backward compat with existing code paths).
enum sr_device_type {
    DEV_TYPE_UNKOWN = 0,
    DEV_TYPE_DEMO,
    DEV_TYPE_FILELOG,
    DEV_TYPE_USB,
    DEV_TYPE_SERIAL,
};

// Work mode (logic analyzer / oscilloscope / analog).
// Note: DSO mode is deprecated (DSCope hardware dropped), but the enum
// value is retained for TriggerConfig/UI compatibility.
enum {
    LOGIC = 0,
    DSO = 1,
    ANALOG = 2,
};

// Trigger mode (UI retained for Adv/Serial; only Simple is synced to driver).
enum {
    SIMPLE_TRIGGER = 0,
    ADV_TRIGGER = 1,
    SERIAL_TRIGGER = 2,
};

// Fork libsigrok trigger position struct (binary-compatible with pxlogic
// driver's pxlogic_trigger_pos). Used by SR_DF_TRIGGER payload in the fork
// data feed. Upstream SR_DF_TRIGGER has NO payload, so this is only used
// internally for trigger position tracking.
struct ds_trigger_pos {
    uint16_t check_id;
    uint64_t real_pos;
    uint32_t ram_saddr;
    uint16_t remain_cnt_l;
    uint16_t remain_cnt_h;
    uint8_t status;
};

// Fork libsigrok DSO status struct (50 fields). DSO mode is deprecated
// (DSCope hardware dropped); this struct is retained as a stub so
// dso_measure.cpp / deviceagent.cpp / storesession.cpp compile until the
// DSO code paths are fully removed. Fields ch0_*/ch1_* are read by
// storesession.cpp export-proc to write DSO measurement metadata.
struct sr_status {
    uint8_t trig_glitch;
    uint8_t trig_ch;
    int16_t vpos_rms;
    int16_t vpos_max;
    int16_t vpos_min;
    int16_t vpos_cyc_rms;
    int16_t vpos_mean;
    int16_t vpos_cyc_mean;
    uint32_t vlen;
    uint8_t trig_flag;
    uint16_t trig_offset;
    uint8_t measure_valid;
    // Per-channel DSO measurement fields (fork). ch0_* for channel 0, ch1_*
    // for channel 1. Storesession.cpp reads these to populate the .dsd file
    // metadata header. All zeroed by default — DSO measure code is deprecated.
    uint32_t ch0_cyc_tlen;
    uint32_t ch0_cyc_cnt;
    uint8_t  ch0_max;
    uint8_t  ch0_min;
    uint32_t ch0_cyc_plen;
    uint32_t ch0_cyc_llen;
    uint8_t  ch0_level_valid;
    uint8_t  ch0_plevel;
    uint8_t  ch0_low_level;
    uint8_t  ch0_high_level;
    uint32_t ch0_cyc_rlen;
    uint32_t ch0_cyc_flen;
    uint64_t ch0_acc_square;
    uint64_t ch0_acc_mean;
    uint32_t ch1_cyc_tlen;
    uint32_t ch1_cyc_cnt;
    uint8_t  ch1_max;
    uint8_t  ch1_min;
    uint32_t ch1_cyc_plen;
    uint32_t ch1_cyc_llen;
    uint8_t  ch1_level_valid;
    uint8_t  ch1_plevel;
    uint8_t  ch1_low_level;
    uint8_t  ch1_high_level;
    uint32_t ch1_cyc_rlen;
    uint32_t ch1_cyc_flen;
    uint64_t ch1_acc_square;
    uint64_t ch1_acc_mean;
    // Padding for remaining fork fields not yet referenced.
    uint8_t _pad[32];
};

// Fork libsigrok DSO datafeed payload. DSO mode is deprecated.
struct sr_datafeed_dso {
    void *data;
    uint32_t num_samples;
    uint8_t trig_flag;
    uint8_t trig_ch;
    uint8_t en_ch_num;
    uint8_t sample_bits;
    int16_t trig_offset;
    uint32_t packet_len;
    uint32_t samplerate_tog;
};

// Fork libsigrok max probe count constants (used to size fixed envelope arrays).
// DSO mode is deprecated; values kept conservative so legacy fixed-size arrays
// in DsoSnapshot/AnalogSnapshot compile without modification.
#define DS_MAX_DSO_PROBES_NUM     16
#define DS_MAX_ANALOG_PROBES_NUM  16

// Fork libsigrok ds_min / ds_max helper macros. Upstream libsigrok does not
// provide these; alias to standard min/max so DSO envelope code compiles.
#define ds_min(a, b)  (((a) < (b)) ? (a) : (b))
#define ds_max(a, b)  (((a) > (b)) ? (a) : (b))

// Fork libsigrok channel type extensions (upstream only has LOGIC/ANALOG).
// These are PXView-local channel types used by SignalModel for non-hardware
// signal categories (decoder/FFT/Lissajous/Math/Group). Values 10003+ avoid
// collision with upstream SR_CHANNEL_LOGIC(10000)/SR_CHANNEL_DSO(10001, fork)
// /SR_CHANNEL_ANALOG(10002).
#define SR_CHANNEL_DECODER    10003
#define SR_CHANNEL_FFT        10004
#define SR_CHANNEL_LISSAJOUS  10005
#define SR_CHANNEL_MATH       10006
#define SR_CHANNEL_GROUP      10007

// Fork libsigrok probe-enable config key (upstream uses SR_CONF_PROBE_EN
// differently / does not expose it). Defined here so SignalModel::commit_to_device
// and set_probe_enabled compile until the DSO/probe paths are cleaned up.
#define SR_CONF_PROBE_EN  60049

// Backward-compat alias: upstream libsigrok renamed sr_input_format to
// sr_input_module. Code referencing the old name is updated gradually.
#define sr_input_format sr_input_module

// Fork libsigrok DSO vertical-division count (used by DSO/Analog/Math/Spectrum
// scaling). Standard oscilloscope displays have 8 vertical divisions.
// DSO mode is deprecated; value retained for stub compatibility.
#define DS_CONF_DSO_VDIVS  8

// Fork libsigrok DSO horizontal-division count. Standard oscilloscope
// displays have 10 horizontal divisions. Used by DSO time-base math
// (sigsession.cpp / samplingbar.cpp / dsosignal.cpp).
#define DS_CONF_DSO_HDIVS  10

// Fork libsigrok hardware operation modes (returned by
// SR_CONF_OPERATION_MODE). LO_OP_BUFFER = buffered capture, LO_OP_STREAM =
// streaming capture. PXLogic hardware uses these to select capture mode.
#define LO_OP_BUFFER  0
#define LO_OP_STREAM  1
#define LO_OP_INTEST  2

// Fork libsigrok sample alignment mask. Used by samplingbar.cpp to align
// sample counts to DMA buffer boundaries (64-byte alignment).
#define SAMPLES_ALIGN  63

// Fork libsigrok trigger constants. Used by triggerdock.cpp for trigger stage
// count and probe count. TriggerStages = max advanced trigger stages (4).
// TriggerProbes = max trigger probe count (16). DS_MAX_TRIG_PERCENT = max
// trigger position percentage (100).
#define TriggerStages       4
#define TriggerProbes       16
#define DS_MAX_TRIG_PERCENT 100

// Fork libsigrok sr_list_item struct. Used by deviceoptions.cpp /
// deviceoptionsdock.cpp to iterate channel mode lists returned by
// SR_CONF_CHANNEL_MODE config_list. Array terminated by entry with id < 0.
struct sr_list_item {
    int id;
    const char *name;
};

// Fork libsigrok DSO trigger source flag. DSO_TRIGGER_AUTO = auto-trigger
// (no external trigger). Used by capturemanager.cpp / viewport_painter.cpp /
// dsotriggerdock.cpp. DSO mode is deprecated.
#define DSO_TRIGGER_AUTO  0

// Fork libsigrok time-unit conversion macro. SR_SEC(x) converts x seconds to
// the internal time-base unit (nanoseconds). Used by sigsession.cpp /
// samplingbar.cpp for time-base math: total_time = timebase * HDIVS / SR_SEC(1).
#define SR_SEC(x)  ((uint64_t)(x) * 1000000000ULL)

// Fork libsigrok DSO config keys not in upstream libsigrok. Used by
// capturemanager.cpp (wait-for-upload) and storesession.cpp (max/min
// timebase). Defined as 60050+ to avoid conflict with 60040-60049 DSO keys.
#define SR_CONF_WAIT_UPLOAD    60050
#define SR_CONF_MAX_TIMEBASE   60051
#define SR_CONF_MIN_TIMEBASE   60052

// Fork libsigrok DSO trigger config keys not in upstream libsigrok. Used by
// session_service.cpp DSO trigger get/set paths. DSO mode is deprecated; values
// defined as 60053+ to avoid conflict with other stubs.
#define SR_CONF_TRIGGER_HOLDOFF  60053
#define SR_CONF_TRIGGER_MARGIN   60054
#define SR_CONF_TRIGGER_CHANNEL  60055

// Fork libsigrok config keys not in upstream libsigrok. Used by
// view_data_sync.cpp (actual samples / calibration) and mainwindow.cpp
// (file version). Defined as 60056+ to avoid conflict with other stubs.
#define SR_CONF_ACTUAL_SAMPLES   60056
#define SR_CONF_CALI             60057
#define SR_CONF_FILE_VERSION     60058

// Fork libsigrok channel/zero config keys. Used by triggerdock.cpp
// (total channel num), deviceoptions.cpp / deviceoptionsdock.cpp (zero/have_zero),
// samplingbar.cpp (RLE support, max DSO samplerate).
#define SR_CONF_TOTAL_CH_NUM       60062
#define SR_CONF_ZERO               60063
#define SR_CONF_HAVE_ZERO          60064
#define SR_CONF_RLE_SUPPORT        60065
#define SR_CONF_MAX_DSO_SAMPLERATE 60066

// Fork libsigrok analog probe mapping config keys. Used by analogsignal.cpp
// and probeoptions.cpp for analog channel unit/min/max mapping. Upstream
// libsigrok does not expose these — defined as 60059+.
#define SR_CONF_PROBE_MAP_UNIT   60059
#define SR_CONF_PROBE_MAP_MIN    60060
#define SR_CONF_PROBE_MAP_MAX    60061

// Fork libsigrok DSO/zero calibration config keys. Used by calibration.cpp
// (VGAIN/PREOFF/COMB_COMP), waitingdialog.cpp (ZERO_SET/ZERO_LOAD/ZERO_COMB),
// fftoptions.cpp (MAX_DSO_SAMPLELIMITS). Defined as 60067+ to avoid conflict.
#define SR_CONF_MAX_DSO_SAMPLELIMITS   60067
#define SR_CONF_ZERO_SET               60068
#define SR_CONF_ZERO_LOAD              60069
#define SR_CONF_ZERO_COMB              60070
#define SR_CONF_ZERO_COMB_FGAIN        60071
#define SR_CONF_ZERO_DEFAULT           60072
#define SR_CONF_PROBE_COMB_COMP        60073
#define SR_CONF_PROBE_COMB_COMP_EN     60074
#define SR_CONF_PROBE_VGAIN            60075
#define SR_CONF_PROBE_VGAIN_DEFAULT    60076
#define SR_CONF_PROBE_VGAIN_RANGE      60077
#define SR_CONF_PROBE_PREOFF           60078
#define SR_CONF_PROBE_PREOFF_MARGIN    60079

// Fork libsigrok USB config key. Upstream libsigrok only exposes
// SR_CONF_USB_SPEED / SR_CONF_USB30_SUPPORT. SR_CONF_USB is PXView-fork only.
// (SR_CONF_STREAM / STREAM_BUFF / STREAM_MEM_BUFF / DISK_CACHE_ENABLE /
// DISK_CACHE_PATH / HW_DEPTH / DEVICE_SESSIONS / USB_SPEED already exist
// upstream — do NOT redefine.)
#define SR_CONF_USB                 60088

// Fork libsigrok misc config keys not in upstream libsigrok. Used by
// probeoptions.cpp (PROBE_CONFIGS), deviceoptions.cpp (STATUS/CLOCK_TYPE/
// BANDWIDTH_LIMIT/BANDWIDTH). Defined as 60080+ in the free range between
// the DSO calibration keys (60067-60079) and SR_CONF_USB (60088).
#define SR_CONF_PROBE_CONFIGS       60080
#define SR_CONF_STATUS              60081
#define SR_CONF_CLOCK_TYPE          60082
#define SR_CONF_BANDWIDTH_LIMIT     60083
#define SR_CONF_BANDWIDTH           60084

// Fork libsigrok DSO trigger source/type enum values. Used by dsotriggerdock.cpp
// (QButtonGroup IDs), dsosignal.cpp (slope comparison), capturemanager.cpp,
// viewport_painter.cpp. DSO mode is deprecated; values are button-group IDs
// so actual integers only need to be distinct within each group.
// Source group (DSO_TRIGGER_*): AUTO=0, CH0=1, CH1=2, CH0A1=3, CH0O1=4.
// Type group: RISING=0, FALLING=1.
#define DSO_TRIGGER_CH0    1
#define DSO_TRIGGER_CH1    2
#define DSO_TRIGGER_CH0A1  3
#define DSO_TRIGGER_CH0O1  4
#define DSO_TRIGGER_RISING   0
#define DSO_TRIGGER_FALLING  1

// Fork libsigrok coupling enum values. Upstream libsigrok only exposes
// SR_CONF_COUPLING as an integer config key (no enum). Used by probeoptions.cpp
// and dsosignal.cpp to decode the coupling value returned by
// get_config_int16(SR_CONF_PROBE_COUPLING). Typical oscilloscope coupling modes.
#define SR_GND_COUPLING  0
#define SR_DC_COUPLING   1
#define SR_AC_COUPLING   2

// Fork libsigrok SI unit multiplier macros. Used by samplingbar.h
// (SR_GB/SR_Mn/SR_US for sample-depth constants) and lissajousoptions.h
// (SR_Kn). Upstream libsigrok does not provide these — define as simple
// uint64_t multipliers.
#define SR_NS(x)  ((uint64_t)(x))
#define SR_US(x)  ((uint64_t)(x) * 1000ULL)
#define SR_MS(x)  ((uint64_t)(x) * 1000000ULL)
#define SR_Kn(x)  ((uint64_t)(x) * 1000ULL)
#define SR_KB(x)  ((uint64_t)(x) * 1000ULL)
#define SR_Mn(x)  ((uint64_t)(x) * 1000000ULL)
#define SR_GB(x)  ((uint64_t)(x) * 1000000000ULL)

// Fork libsigrok time unit macros (duration in nanoseconds). Used by
// samplingbar.cpp for time-base calculation. SR_SEC already defined above
// as ((uint64_t)(x) * 1000000000ULL).
#define SR_MIN(x)  ((uint64_t)(x) * 60ULL * 1000000000ULL)
#define SR_HOUR(x) ((uint64_t)(x) * 3600ULL * 1000000000ULL)
#define SR_DAY(x)  ((uint64_t)(x) * 86400ULL * 1000000000ULL)

// Fork libsigrok time string formatter. Upstream libsigrok provides
// sr_samplerate_string / sr_voltage_string but not sr_time_string. Stub
// implementation in deviceagent.cpp formats duration (nanoseconds) as a
// human-readable string. Caller must g_free() the returned pointer.
char *sr_time_string(uint64_t duration);

// Fork libsigrok sr_datatype enum extensions. Upstream sr_datatype enum
// (libsigrok.h) defines SR_T_UINT64=10000 through SR_T_UINT32=10011. Fork
// libsigrok added SR_T_UINT8/INT16/CHAR/LIST for DSO config variants. These
// never appear in upstream driver config_info, so comparisons are always
// false (safe stub). Defined as 10012+ to avoid conflict.
#define SR_T_UINT8  10012
#define SR_T_INT16  10013
#define SR_T_CHAR   10014
#define SR_T_LIST   10015

// Fork libsigrok DSO measurement type enum. Used by DsoMeasure / DsoSignal /
// ViewStatus for DSO auto-measurements. DSO mode is deprecated; values
// retained so the DSO UI code compiles until full removal.
enum DSO_MEASURE_TYPE {
    DSO_MS_BEGIN = 0,
    DSO_MS_FREQ  = 1,
    DSO_MS_PERD  = 2,
    DSO_MS_PDUT  = 3,
    DSO_MS_NDUT  = 4,
    DSO_MS_PCNT  = 5,
    DSO_MS_RISE  = 6,
    DSO_MS_FALL  = 7,
    DSO_MS_PWDT  = 8,
    DSO_MS_NWDT  = 9,
    DSO_MS_BRST  = 10,
    DSO_MS_AMPT  = 11,
    DSO_MS_VHIG  = 12,
    DSO_MS_VLOW  = 13,
    DSO_MS_VRMS  = 14,
    DSO_MS_VMEA  = 15,
    DSO_MS_VP2P  = 16,
    DSO_MS_VMAX  = 17,
    DSO_MS_VMIN  = 18,
    DSO_MS_POVR  = 19,
    DSO_MS_NOVR  = 20,
    DSO_MS_END   = 21,
};

// Fork libsigrok packet status code.
#define SR_PKT_OK 0

// Fork libsigrok packet types not in upstream.
#define SR_DF_DSO 10007
#define SR_DF_OVERFLOW 10008

// Fork libsigrok channel type for DSO (upstream only has LOGIC/ANALOG).
// Upstream defines SR_CHANNEL_LOGIC=10000, SR_CHANNEL_ANALOG=10001 (enum
// sr_channeltype). PXView fork had DSO=10001 which conflicted with upstream
// SR_CHANNEL_ANALOG — redefined to 10002 to avoid duplicate case values in
// switch statements. DSO mode is deprecated.
#define SR_CHANNEL_DSO 10002

// Backward-compat alias: upstream libsigrok renamed sr_config_info to
// sr_key_info. Callers that reference sr_config_info are updated gradually.
#define sr_config_info sr_key_info

// Fork libsigrok device-mode list entry (fork sr_dev_mode). Upstream
// libsigrok does not expose a device-mode list (SR_CONF_DEVICE_MODE returns
// a GVariant, not a GSList of sr_dev_mode). DeviceAgent::get_device_mode_list()
// already returns nullptr as a stub, so callers iterating the list are no-ops;
// this struct exists only so the (const sr_dev_mode*) casts in
// storesession.cpp / devmode.cpp compile.
struct sr_dev_mode {
    int mode;
    const char *acronym;
};

// --- Fork DSO SR_CONF_* keys (temporary, removed in Task 10 with DSO paths) ---
// These keys existed in PXView's fork libsigrok.h but are absent from upstream
// libsigrok. Defined here as 60040+ (no conflict with 60001-60013/60020+
// PXLogic extension keys) so DSO typed wrappers in DeviceAgent compile until
// the DSO code paths are deleted in Task 10.
#define SR_CONF_PROBE_VDIV        60040
#define SR_CONF_PROBE_COUPLING    60041
#define SR_CONF_PROBE_OFFSET      60042
#define SR_CONF_PROBE_HW_OFFSET   60043
#define SR_CONF_PROBE_MAP_DEFAULT 60044
#define SR_CONF_REF_MIN           60045
#define SR_CONF_REF_MAX           60046
#define SR_CONF_UNIT_BITS         60047
#define SR_CONF_TRIGGER_VALUE     60048

#define DESTROY_OBJECT(p) if((p)){delete (p); p = NULL;} 
#define DESTROY_QT_OBJECT(p) if((p)){((p))->deleteLater(); p = NULL;}
#define DESTROY_QT_LATER(p) ((p))->deleteLater();

#define RELEASE_ARRAY(a)   for (auto ptr : (a)){delete ptr;} (a).clear();

#define ABS_VAL(x) ((x) > 0 ? (x) : -(x))

#define SESSION_FORMAT_VERSION      3
#define HEADER_FORMAT_VERSION       3

namespace DecoderDataFormat
{
    enum _data_format
    {
        hex=0,
        dec=1,       
        oct=2,
        bin=3,
        ascii=4
    };

    int Parse(const char *name);       
}
