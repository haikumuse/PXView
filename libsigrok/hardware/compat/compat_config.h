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

#endif
