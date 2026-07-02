/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2025 Compat Layer Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_COMPAT_CONFIG_H
#define LIBSIGROK_COMPAT_CONFIG_H

/**
 * @file
 *
 * Compat configuration key definitions and helpers.
 *
 * The actual SR_CONF_* compat keys are added to libsigrok.h directly.
 * This header provides additional compat definitions for config-related
 * constants and macros used by standard sigrok drivers.
 */

/* Standard sigrok config key ranges (for reference) */
#define SR_CONF_SCANOPTS_OFFSET  20000
#define SR_CONF_DRVOPTS_OFFSET   10000
#define SR_CONF_DEVOPTS_OFFSET   30000
#define SR_CONF_ACQOPTS_OFFSET   50000

/* Standard sigrok trigger match types */
enum sr_trigger_match_type {
    SR_TRIGGER_ZERO = 0,
    SR_TRIGGER_ONE = 1,
    SR_TRIGGER_RISING = 2,
    SR_TRIGGER_FALLING = 3,
    SR_TRIGGER_EDGE = 4,
    SR_TRIGGER_OVER = 5,
    SR_TRIGGER_UNDER = 6,
};

/* Standard sigrok channel change flags (for config_channel_set) */
enum sr_channel_change {
    SR_CHANNEL_SET_ENABLED = (1 << 0),
    SR_CHANNEL_SET_NAME    = (1 << 1),
    SR_CHANNEL_SET_TRIGGER = (1 << 2),
    SR_CHANNEL_SET_VDIV    = (1 << 3),
    SR_CHANNEL_SET_COUPLING = (1 << 4),
    SR_CHANNEL_SET_VFACTOR  = (1 << 5),
};

/* Standard sigrok's SR_CONF_GET/Set/LIST capability flags */
#define SR_CONF_GET     (1 << 0)
#define SR_CONF_SET     (1 << 1)
#define SR_CONF_LIST    (1 << 2)

/*
 * SR_CONF_MASK - high bit outside the config-key and capability-flag ranges.
 * Used by drivers' has_devopt() helpers to mask a combined (key | flags)
 * value when probing whether a given capability bit is set on an entry in
 * the devopts[] array. Config keys and GET/SET/LIST flags are all small
 * values, so this single high bit never collides with either.
 */
#define SR_CONF_MASK    0x40000000u

/* Helper macros to build config key with capability flags */
#define SR_CONF_GET_SET(key)       (key | SR_CONF_GET | SR_CONF_SET)
#define SR_CONF_GET_SET_LIST(key)  (key | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST)
#define SR_CONF_GET_LIST(key)      (key | SR_CONF_GET | SR_CONF_LIST)
#define SR_CONF_SET_LIST(key)      (key | SR_CONF_SET | SR_CONF_LIST)

/* Macros to extract key and capability flags from combined value */
#define SR_CONF_KEY_VALUE(val)     ((val) & ~(SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST))
#define SR_CONF_HAS_GET(val)       (((val) & SR_CONF_GET) != 0)
#define SR_CONF_HAS_SET(val)       (((val) & SR_CONF_SET) != 0)
#define SR_CONF_HAS_LIST(val)      (((val) & SR_CONF_LIST) != 0)

/*
 * Standard sigrok DSO config keys that PXView renamed or dropped.
 *
 * SR_CONF_VDIV and SR_CONF_COUPLING are the standard sigrok names for what
 * PXView calls SR_CONF_PROBE_VDIV / SR_CONF_PROBE_COUPLING. They are
 * semantically identical (volts/div and coupling per channel), so we alias
 * them to the PXView names. This lets standard DSO drivers (hantek-dso,
 * hameg-hmo, rigol-ds, ...) use the classic names in their devopts[] arrays
 * and config_get/set/list switch statements without modification.
 *
 * SR_CONF_NUM_HDIV (number of horizontal divisions on screen) has no PXView
 * equivalent, so it gets a unique value in a reserved compat range. The value
 * 30150 sits in the gap between SR_CONF_DEMO_CHANGE (30107) and the
 * acquisition-mode block (50000+), so it cannot collide with any PXView key.
 */
#ifndef SR_CONF_VDIV
#define SR_CONF_VDIV               SR_CONF_PROBE_VDIV
#endif
#ifndef SR_CONF_COUPLING
#define SR_CONF_COUPLING           SR_CONF_PROBE_COUPLING
#endif
#ifndef SR_CONF_NUM_HDIV
#define SR_CONF_NUM_HDIV           30150
#endif
#ifndef SR_CONF_TRIGGER_LEVEL
#define SR_CONF_TRIGGER_LEVEL      30151
#endif
/*
 * Standard sigrok DSO config keys for data source selection and averaging.
 * Used by SCPI oscilloscope drivers (siglent-sds, rigol-ds, ...). PXView does
 * not have direct equivalents, so they get unique values in the same reserved
 * compat range as SR_CONF_NUM_HDIV / SR_CONF_TRIGGER_LEVEL above.
 */
#ifndef SR_CONF_DATA_SOURCE
#define SR_CONF_DATA_SOURCE        30152
#endif
#ifndef SR_CONF_AVERAGING
#define SR_CONF_AVERAGING          30153
#endif
#ifndef SR_CONF_AVG_SAMPLES
#define SR_CONF_AVG_SAMPLES        30154
#endif

/*
 * Standard sigrok sound pressure level (SPL) config keys and power-off key.
 * Used by sound level meter drivers (pce-322a, tondaj-sl-814). PXView does
 * not have direct equivalents, so they get unique values in the same reserved
 * compat range as SR_CONF_NUM_HDIV / SR_CONF_DATA_SOURCE above.
 */
#ifndef SR_CONF_SPL_WEIGHT_FREQ
#define SR_CONF_SPL_WEIGHT_FREQ    30155
#endif
#ifndef SR_CONF_SPL_WEIGHT_TIME
#define SR_CONF_SPL_WEIGHT_TIME    30156
#endif
#ifndef SR_CONF_SPL_MEASUREMENT_RANGE
#define SR_CONF_SPL_MEASUREMENT_RANGE 30157
#endif
#ifndef SR_CONF_POWER_OFF
#define SR_CONF_POWER_OFF          30158
#endif

/*
 * SR_CONF_SWAP - Standard sigrok key for channel swapping (typical use:
 * between buffered and unbuffered channels). Used by OLS-class drivers
 * (openbench-logic-sniffer, pipistrello-ols). PXView has no direct
 * equivalent, so it gets a unique value in the reserved compat range.
 */
#ifndef SR_CONF_SWAP
#define SR_CONF_SWAP               30159
#endif

/*
 * Standard sigrok DMM config keys for measured quantity and measurement range.
 * Used by DMM drivers (uni-t-ut181a, ...). PXView has no direct equivalents,
 * so they get unique values in the reserved compat range (30160+, immediately
 * following SR_CONF_SWAP at 30159).
 */
#ifndef SR_CONF_MEASURED_QUANTITY
#define SR_CONF_MEASURED_QUANTITY  30160
#endif
#ifndef SR_CONF_RANGE
#define SR_CONF_RANGE              30161
#endif

/*
 * Standard sigrok device-class and enable/disable config keys used by relay
 * / multiplexer drivers (dcttech-usbrelay, hp-59306a, icstation-usbrelay,
 * devantech-eth008, ...). PXView's libsigrok.h does not define either key.
 *
 * SR_CONF_MULTIPLEXER is a drvopt (device class identifier) marking a device
 * as a relay multiplexer. SR_CONF_ENABLED is a get/set devopt that controls
 * whether a relay (or all relays in a channel group) is on or off.
 *
 * Canonical sigrok places SR_CONF_MULTIPLEXER in the 10000 device-class
 * range and SR_CONF_ENABLED in the 30000 config range. Several previously
 * migrated drivers carry local #ifndef-guarded fallbacks for these (e.g.
 * SR_CONF_MULTIPLEXER=10010, SR_CONF_ENABLED=30200). Centralising them here
 * in the 30300+ reserved range lets those local guards defer to the single
 * definition, while keeping the values clear of every PXView-native key
 * (which top out at 30107 in the demo/demo-change block, with 50000+ used
 * only for acquisition options).
 */
#ifndef SR_CONF_MULTIPLEXER
#define SR_CONF_MULTIPLEXER        30300
#endif
#ifndef SR_CONF_ENABLED
#define SR_CONF_ENABLED            30301
#endif

/* Byte order macros for reading/writing little-endian and big-endian values.
 * These are used by standard sigrok drivers for serial/USB communication.
 */
#include <stdint.h>

/* Read/write helper functions - inline implementations */
static inline uint16_t read_u16le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t read_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint16_t read_u16be(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}
static inline uint32_t read_u32be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static inline void write_u16le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
}
static inline void write_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}
static inline void write_u16be(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xff);
    p[1] = (uint8_t)(v & 0xff);
}
static inline void write_u32be(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xff);
    p[1] = (uint8_t)((v >> 16) & 0xff);
    p[2] = (uint8_t)((v >> 8) & 0xff);
    p[3] = (uint8_t)(v & 0xff);
}

/* Read macros - standard sigrok naming convention */
#define RL16(x) read_u16le((const uint8_t *)(x))
#define RL32(x) read_u32le((const uint8_t *)(x))
#define RB16(x) read_u16be((const uint8_t *)(x))
#define RB32(x) read_u32be((const uint8_t *)(x))

/* Write macros - standard sigrok naming convention */
#define WL16(p, x) write_u16le((uint8_t *)(p), (uint16_t)(x))
#define WL32(p, x) write_u32le((uint8_t *)(p), (uint32_t)(x))
#define WB16(p, x) write_u16be((uint8_t *)(p), (uint16_t)(x))
#define WB32(p, x) write_u32be((uint8_t *)(p), (uint32_t)(x))

/* Signed versions */
static inline int16_t read_i16le(const uint8_t *p) {
    return (int16_t)read_u16le(p);
}
static inline int32_t read_i32le(const uint8_t *p) {
    return (int32_t)read_u32le(p);
}
static inline int16_t read_i16be(const uint8_t *p) {
    return (int16_t)read_u16be(p);
}
static inline int32_t read_i32be(const uint8_t *p) {
    return (int32_t)read_u32be(p);
}

#define RL16S(x) read_i16le((const uint8_t *)(x))
#define RL32S(x) read_i32le((const uint8_t *)(x))
#define RB16S(x) read_i16be((const uint8_t *)(x))
#define RB32S(x) read_i32be((const uint8_t *)(x))

/*
 * Increment-pointer variants (compat: PXView core does not provide these).
 *
 * Standard sigrok's upstream drivers use read_u*_inc()/write_u*_inc()
 * helpers that advance the pointer after each read/write operation.
 * PXView's libsigrok (based on upstream 0.2.0) only provides the
 * non-_inc variants above. These wrappers let standard drivers compile
 * and operate without each having to roll its own local copy.
 *
 * Each function is guarded with #ifndef so that drivers with existing
 * local definitions (asix-sigma, asix-omega-rtm-cli, itech-it8500,
 * rdtech-dps) can suppress the compat version by defining the guard
 * macro (e.g. via `#define compat_read_u16le_inc_defined`) before
 * including compat_config.h, or by providing their own `#define`
 * redirection macro. Task 14 will remove the local copies; until then
 * the guards let both coexist in the same translation unit.
 */
#ifndef compat_read_u8_inc_defined
#define compat_read_u8_inc_defined
static inline uint8_t read_u8_inc(const uint8_t **ptr) {
    uint8_t val = **ptr;
    *ptr += 1;
    return val;
}
#endif

#ifndef compat_read_u16le_inc_defined
#define compat_read_u16le_inc_defined
static inline uint16_t read_u16le_inc(const uint8_t **ptr) {
    uint16_t val = read_u16le(*ptr);
    *ptr += 2;
    return val;
}
#endif

#ifndef compat_read_u32le_inc_defined
#define compat_read_u32le_inc_defined
static inline uint32_t read_u32le_inc(const uint8_t **ptr) {
    uint32_t val = read_u32le(*ptr);
    *ptr += 4;
    return val;
}
#endif

#ifndef compat_write_u8_inc_defined
#define compat_write_u8_inc_defined
static inline void write_u8_inc(uint8_t **ptr, uint8_t val) {
    **ptr = val;
    *ptr += 1;
}
#endif

#ifndef compat_write_u16le_inc_defined
#define compat_write_u16le_inc_defined
static inline void write_u16le_inc(uint8_t **ptr, uint16_t val) {
    write_u16le(*ptr, val);
    *ptr += 2;
}
#endif

#ifndef compat_write_u24le_inc_defined
#define compat_write_u24le_inc_defined
static inline void write_u24le_inc(uint8_t **ptr, uint32_t val) {
    (*ptr)[0] = (uint8_t)(val & 0xff);
    (*ptr)[1] = (uint8_t)((val >> 8) & 0xff);
    (*ptr)[2] = (uint8_t)((val >> 16) & 0xff);
    *ptr += 3;
}
#endif

#ifndef compat_write_u32le_inc_defined
#define compat_write_u32le_inc_defined
static inline void write_u32le_inc(uint8_t **ptr, uint32_t val) {
    write_u32le(*ptr, val);
    *ptr += 4;
}
#endif

#ifndef compat_write_u40le_inc_defined
#define compat_write_u40le_inc_defined
static inline void write_u40le_inc(uint8_t **ptr, uint64_t val) {
    (*ptr)[0] = (uint8_t)(val & 0xff);
    (*ptr)[1] = (uint8_t)((val >> 8) & 0xff);
    (*ptr)[2] = (uint8_t)((val >> 16) & 0xff);
    (*ptr)[3] = (uint8_t)((val >> 24) & 0xff);
    (*ptr)[4] = (uint8_t)((val >> 32) & 0xff);
    *ptr += 5;
}
#endif

/*
 * Standard sigrok enum type tags.
 *
 * PXView's libsigrok.h defines the SR_MQ_*, SR_UNIT_* and SR_MQFLAG_*
 * values via anonymous enums (no tag). Standard sigrok drivers, however,
 * use 'enum sr_mq', 'enum sr_unit' and 'enum sr_mqflag' as types in
 * function prototypes, struct fields and local variables.
 *
 * To make these tagged types available as complete types (usable as struct
 * fields and function parameters), we define each enum with a single dummy
 * member. The actual enum values (SR_MQ_VOLTAGE, SR_UNIT_VOLT, etc.) come
 * from PXView's anonymous enum and the #defines below; they are plain int
 * constants that can be assigned to these enum-typed variables in C.
 *
 * The dummy members use value 0, which does not collide with any real
 * SR_MQ_* (>= 10000), SR_UNIT_* (>= 10000) or SR_MQFLAG_* (>= 0x01) value.
 */
enum sr_mq { _SR_MQ_TYPE_SENTINEL = 0 };
enum sr_unit { _SR_UNIT_TYPE_SENTINEL = 0 };
enum sr_mqflag { _SR_MQFLAG_TYPE_SENTINEL = 0 };

/*
 * Standard sigrok SR_MQ_* (measured quantity) constants missing from PXView.
 *
 * PXView's libsigrok.h enum only defines SR_MQ_VOLTAGE through
 * SR_MQ_HARMONIC_RATIO (values 10000-10015). Standard sigrok defines
 * additional measured quantities (canonical values 10016-10034).
 *
 * These are added as #defines (not enum entries) so they don't interfere
 * with PXView's existing anonymous enum. Values match canonical sigrok.
 *
 * Note: PXView defines SR_MQ_HARMONIC_RATIO at value 10015 (canonical
 * sigrok value is 10032). SR_MQ_TIME (canonical 10015) would collide
 * with PXView's SR_MQ_HARMONIC_RATIO, so it uses a reserved value (10100)
 * to avoid duplicate case labels in switch statements.
 */
#ifndef SR_MQ_TIME
#define SR_MQ_TIME                       10100
#endif
#ifndef SR_MQ_WIND_SPEED
#define SR_MQ_WIND_SPEED                 10016
#endif
#ifndef SR_MQ_PRESSURE
#define SR_MQ_PRESSURE                   10017
#endif
#ifndef SR_MQ_PARALLEL_INDUCTANCE
#define SR_MQ_PARALLEL_INDUCTANCE        10018
#endif
#ifndef SR_MQ_PARALLEL_CAPACITANCE
#define SR_MQ_PARALLEL_CAPACITANCE       10019
#endif
#ifndef SR_MQ_PARALLEL_RESISTANCE
#define SR_MQ_PARALLEL_RESISTANCE        10020
#endif
#ifndef SR_MQ_SERIES_INDUCTANCE
#define SR_MQ_SERIES_INDUCTANCE          10021
#endif
#ifndef SR_MQ_SERIES_CAPACITANCE
#define SR_MQ_SERIES_CAPACITANCE         10022
#endif
#ifndef SR_MQ_SERIES_RESISTANCE
#define SR_MQ_SERIES_RESISTANCE          10023
#endif
#ifndef SR_MQ_DISSIPATION_FACTOR
#define SR_MQ_DISSIPATION_FACTOR         10024
#endif
#ifndef SR_MQ_QUALITY_FACTOR
#define SR_MQ_QUALITY_FACTOR             10025
#endif
#ifndef SR_MQ_PHASE_ANGLE
#define SR_MQ_PHASE_ANGLE                10026
#endif
#ifndef SR_MQ_DIFFERENCE
#define SR_MQ_DIFFERENCE                 10027
#endif
#ifndef SR_MQ_COUNT
#define SR_MQ_COUNT                      10028
#endif
#ifndef SR_MQ_POWER_FACTOR
#define SR_MQ_POWER_FACTOR               10029
#endif
#ifndef SR_MQ_APPARENT_POWER
#define SR_MQ_APPARENT_POWER             10030
#endif
#ifndef SR_MQ_MASS
#define SR_MQ_MASS                       10031
#endif
/* SR_MQ_HARMONIC_RATIO already defined by PXView's enum at 10015. */
#ifndef SR_MQ_ENERGY
#define SR_MQ_ENERGY                     10033
#endif
#ifndef SR_MQ_ELECTRIC_CHARGE
#define SR_MQ_ELECTRIC_CHARGE            10034
#endif

/*
 * Standard sigrok SR_UNIT_* (unit of measured quantity) constants missing
 * from PXView. PXView's enum only defines SR_UNIT_VOLT through
 * SR_UNIT_CONCENTRATION (values 10000-10016). Standard sigrok defines
 * additional units (canonical values 10017-10039). No conflicts with
 * PXView's existing enum, so canonical values are used directly.
 */
#ifndef SR_UNIT_REVOLUTIONS_PER_MINUTE
#define SR_UNIT_REVOLUTIONS_PER_MINUTE   10017
#endif
#ifndef SR_UNIT_VOLT_AMPERE
#define SR_UNIT_VOLT_AMPERE              10018
#endif
#ifndef SR_UNIT_WATT
#define SR_UNIT_WATT                     10019
#endif
#ifndef SR_UNIT_WATT_HOUR
#define SR_UNIT_WATT_HOUR                10020
#endif
#ifndef SR_UNIT_METER_SECOND
#define SR_UNIT_METER_SECOND             10021
#endif
#ifndef SR_UNIT_HECTOPASCAL
#define SR_UNIT_HECTOPASCAL              10022
#endif
#ifndef SR_UNIT_HUMIDITY_293K
#define SR_UNIT_HUMIDITY_293K            10023
#endif
#ifndef SR_UNIT_DEGREE
#define SR_UNIT_DEGREE                   10024
#endif
#ifndef SR_UNIT_HENRY
#define SR_UNIT_HENRY                    10025
#endif
#ifndef SR_UNIT_GRAM
#define SR_UNIT_GRAM                     10026
#endif
#ifndef SR_UNIT_CARAT
#define SR_UNIT_CARAT                    10027
#endif
#ifndef SR_UNIT_OUNCE
#define SR_UNIT_OUNCE                    10028
#endif
#ifndef SR_UNIT_TROY_OUNCE
#define SR_UNIT_TROY_OUNCE               10029
#endif
#ifndef SR_UNIT_POUND
#define SR_UNIT_POUND                    10030
#endif
#ifndef SR_UNIT_PENNYWEIGHT
#define SR_UNIT_PENNYWEIGHT              10031
#endif
#ifndef SR_UNIT_GRAIN
#define SR_UNIT_GRAIN                    10032
#endif
#ifndef SR_UNIT_TAEL
#define SR_UNIT_TAEL                     10033
#endif
#ifndef SR_UNIT_MOMME
#define SR_UNIT_MOMME                    10034
#endif
#ifndef SR_UNIT_TOLA
#define SR_UNIT_TOLA                     10035
#endif
#ifndef SR_UNIT_PIECE
#define SR_UNIT_PIECE                    10036
#endif
#ifndef SR_UNIT_JOULE
#define SR_UNIT_JOULE                    10037
#endif
#ifndef SR_UNIT_COULOMB
#define SR_UNIT_COULOMB                  10038
#endif
#ifndef SR_UNIT_AMPERE_HOUR
#define SR_UNIT_AMPERE_HOUR              10039
#endif

/*
 * Standard sigrok SR_MQFLAG_* (measurement quality flags) constants missing
 * from PXView. PXView's enum defines flags up to SR_MQFLAG_SPL_PCT_OVER_ALARM
 * (0x10000). Standard sigrok defines additional flags (canonical values
 * 0x20000-0x200000). No conflicts with PXView's existing enum.
 */
#ifndef SR_MQFLAG_DURATION
#define SR_MQFLAG_DURATION               0x20000
#endif
#ifndef SR_MQFLAG_AVG
#define SR_MQFLAG_AVG                    0x40000
#endif
#ifndef SR_MQFLAG_REFERENCE
#define SR_MQFLAG_REFERENCE              0x80000
#endif
#ifndef SR_MQFLAG_UNSTABLE
#define SR_MQFLAG_UNSTABLE               0x100000
#endif
#ifndef SR_MQFLAG_FOUR_WIRE
#define SR_MQFLAG_FOUR_WIRE              0x200000
#endif

/*
 * Standard sigrok SR_PACKET_* (packet parser validity) constants.
 *
 * These are from standard sigrok's 'enum sr_valid_code' and are used by
 * DMM/LCR packet parser validity checks. PXView does not define them
 * (it has a separate SR_PKT_* enum for different purposes). Values are
 * canonical from standard sigrok's libsigrok.h.
 *
 * Note: Some drivers (e.g. serial-dmm) have local #ifndef-guarded fallback
 * definitions with non-canonical values (0/1/2). Because compat_config.h is
 * included before the driver's own protocol.h, these canonical definitions
 * take precedence, which is the desired behavior for standard sigrok
 * compatibility (negative=invalid, zero=valid, positive=need more data).
 */
#ifndef SR_PACKET_INVALID
#define SR_PACKET_INVALID                (-1)
#endif
#ifndef SR_PACKET_VALID
#define SR_PACKET_VALID                  0
#endif
#ifndef SR_PACKET_NEED_RX
#define SR_PACKET_NEED_RX                1
#endif

/*
 * Standard sigrok error code SR_ERR_CHANNEL_GROUP.
 *
 * Canonical libsigrok (libsigrok.h:76) defines SR_ERR_CHANNEL_GROUP = -9
 * ("no channel group specified"). PXView's libsigrok.h has no equivalent
 * (its error enum stops at SR_ERR_FIRMWARE_NOT_EXIST and friends, all
 * positive). Standard sigrok SCPI drivers (rigol-ds, siglent-sds, hantek-4032l,
 * yokogawa-dlm, atten-pps3xxx, ...) return SR_ERR_CHANNEL_GROUP from
 * config_get/set/list when called with a non-NULL channel group that the
 * driver does not support.
 *
 * Map it to SR_ERR_ARG (the closest PXView equivalent: "invalid argument"),
 * matching the local fallback that hantek-4032l/api.c:32-33 and
 * yokogawa-dlm/api.c:30-31 already use. Guarded with #ifndef so drivers
 * that still carry their own local #define (hantek-4032l, yokogawa-dlm)
 * keep taking precedence and don't get a redefinition warning.
 */
#ifndef SR_ERR_CHANNEL_GROUP
#define SR_ERR_CHANNEL_GROUP             SR_ERR_ARG
#endif

#endif
