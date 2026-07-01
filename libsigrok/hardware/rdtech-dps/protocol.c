/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018 James Churchill <pelrun@gmail.com>
 * Copyright (C) 2019 Frank Stettner <frank-stettner@gmx.net>
 * Copyright (C) 2021 Gerhard Sittig <gerhard.sittig@gmx.net>
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

#include "hardware/compat/compat.h"
#include <errno.h>
#include <math.h>
#include <string.h>
#include "protocol.h"

/* ===========================================================================
 * Local byte-access macros (W8 / R8) used by the Modbus layer.
 * compat_config.h provides WB16 but not W8/R8.
 * =========================================================================== */
#ifndef W8
#define W8(p, x) do { *(uint8_t *)(p) = (uint8_t)(x); } while (0)
#endif
#ifndef R8
#define R8(x) (*(const uint8_t *)(x))
#endif

/* ===========================================================================
 * Local CRC-16 implementation (Modbus polynomial 0xA001).
 * PXView's libsigrok does not provide sr_crc16().
 * =========================================================================== */
static uint16_t rdtech_dps_crc16(uint16_t seed, const uint8_t *data, size_t len)
{
	uint16_t crc = seed;
	size_t i;
	int j;

	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA001;
			else
				crc >>= 1;
		}
	}

	return crc;
}

#define SR_CRC16_DEFAULT_INIT 0xFFFF

/* ===========================================================================
 * Local sr_session_send_meta() replacement.
 * Sends a META packet with a single config key/value pair (same pattern as
 * korad-kaxxxxp/protocol.c).
 * =========================================================================== */
SR_PRIV int sr_session_send_meta(const struct sr_dev_inst *sdi,
		uint32_t key, GVariant *data)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_meta meta;
	struct sr_config *src;

	if (!sdi || !data)
		return SR_ERR_ARG;

	src = sr_config_new((int)key, data);
	if (!src)
		return SR_ERR;

	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_META;
	packet.status = SR_PKT_OK;
	packet.payload = &meta;
	meta.config = g_slist_append(NULL, src);

	ds_data_forward(sdi, &packet);

	sr_config_free(src);
	g_slist_free(meta.config);

	return SR_OK;
}

/* ===========================================================================
 * Local Modbus serial RTU layer.
 * Combines standard sigrok's modbus.c and modbus_serial_rtu.c into a single
 * self-contained implementation. PXView's libsigrok has no Modbus support.
 * =========================================================================== */

#define MODBUS_READ_COILS              0x01
#define MODBUS_READ_HOLDING_REGISTERS  0x03
#define MODBUS_WRITE_COIL              0x05
#define MODBUS_WRITE_MULTIPLE_REGISTERS 0x10

/* --- modbus_serial_rtu.c --- */

static int modbus_serial_rtu_dev_inst_new(void *priv, const char *resource,
		char **params, const char *serialcomm, int modbusaddr)
{
	struct modbus_serial_rtu *modbus = priv;

	(void)params;

	modbus->serial = sr_serial_dev_inst_new(resource, serialcomm);
	modbus->slave_addr = modbusaddr;

	return SR_OK;
}

static int modbus_serial_rtu_open(void *priv)
{
	struct modbus_serial_rtu *modbus = priv;
	struct sr_serial_dev_inst *serial = modbus->serial;

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return SR_ERR;

	return SR_OK;
}

static int modbus_serial_rtu_source_add(void *priv, int events, int timeout,
		sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi)
{
	struct modbus_serial_rtu *modbus = priv;
	struct sr_serial_dev_inst *serial = modbus->serial;

	return serial_source_add(serial, events, timeout, cb, sdi);
}

static int modbus_serial_rtu_source_remove(void *priv)
{
	struct modbus_serial_rtu *modbus = priv;
	struct sr_serial_dev_inst *serial = modbus->serial;

	return serial_source_remove(serial);
}

static int modbus_serial_rtu_send(void *priv,
		const uint8_t *buffer, int buffer_size)
{
	int result;
	struct modbus_serial_rtu *modbus = priv;
	struct sr_serial_dev_inst *serial = modbus->serial;
	uint8_t slave_addr = modbus->slave_addr;
	uint16_t crc;

	result = serial_write_blocking(serial, &slave_addr, sizeof(slave_addr), 0);
	if (result < 0)
		return SR_ERR;

	result = serial_write_blocking(serial, buffer, buffer_size, 0);
	if (result < 0)
		return SR_ERR;

	crc = rdtech_dps_crc16(SR_CRC16_DEFAULT_INIT, &slave_addr, sizeof(slave_addr));
	crc = rdtech_dps_crc16(crc, buffer, buffer_size);

	result = serial_write_blocking(serial, &crc, sizeof(crc), 0);
	if (result < 0)
		return SR_ERR;

	return SR_OK;
}

static int modbus_serial_rtu_read_begin(void *priv, uint8_t *function_code)
{
	struct modbus_serial_rtu *modbus = priv;
	uint8_t slave_addr;
	int ret;

	ret = serial_read_blocking(modbus->serial, &slave_addr, 1, 500);
	if (ret != 1 || slave_addr != modbus->slave_addr)
		return SR_ERR;

	ret = serial_read_blocking(modbus->serial, function_code, 1, 100);
	if (ret != 1)
		return SR_ERR;

	modbus->crc = rdtech_dps_crc16(SR_CRC16_DEFAULT_INIT, &slave_addr, sizeof(slave_addr));
	modbus->crc = rdtech_dps_crc16(modbus->crc, function_code, 1);

	return SR_OK;
}

static int modbus_serial_rtu_read_data(void *priv, uint8_t *buf, int maxlen)
{
	struct modbus_serial_rtu *modbus = priv;
	int ret;

	ret = serial_read_nonblocking(modbus->serial, buf, maxlen);
	if (ret < 0)
		return ret;
	modbus->crc = rdtech_dps_crc16(modbus->crc, buf, ret);
	return ret;
}

static int modbus_serial_rtu_read_end(void *priv)
{
	struct modbus_serial_rtu *modbus = priv;
	uint16_t crc;
	int ret;

	ret = serial_read_blocking(modbus->serial, &crc, sizeof(crc), 100);
	if (ret != 2)
		return SR_ERR;

	if (crc != modbus->crc) {
		sr_err("CRC error (0x%04X vs 0x%04X).", crc, modbus->crc);
		return SR_ERR_DATA;
	}

	return SR_OK;
}

static int modbus_serial_rtu_close(void *priv)
{
	struct modbus_serial_rtu *modbus = priv;

	return serial_close(modbus->serial);
}

static void modbus_serial_rtu_free(void *priv)
{
	struct modbus_serial_rtu *modbus = priv;

	sr_serial_dev_inst_free(modbus->serial);
}

/* The single supported Modbus transport (serial RTU). */
static const struct sr_modbus_dev_inst modbus_serial_rtu_dev = {
	.name          = "serial_rtu",
	.prefix        = "",
	.priv_size     = sizeof(struct modbus_serial_rtu),
	.scan          = NULL,
	.dev_inst_new  = modbus_serial_rtu_dev_inst_new,
	.open          = modbus_serial_rtu_open,
	.source_add    = modbus_serial_rtu_source_add,
	.source_remove = modbus_serial_rtu_source_remove,
	.send          = modbus_serial_rtu_send,
	.read_begin    = modbus_serial_rtu_read_begin,
	.read_data     = modbus_serial_rtu_read_data,
	.read_end      = modbus_serial_rtu_read_end,
	.close         = modbus_serial_rtu_close,
	.free          = modbus_serial_rtu_free,
};

/* --- modbus.c --- */

static struct sr_dev_inst *sr_modbus_scan_resource(const char *resource,
		const char *serialcomm, int modbusaddr,
		struct sr_dev_inst *(*probe_device)(struct sr_modbus_dev_inst *modbus))
{
	struct sr_modbus_dev_inst *modbus;
	struct sr_dev_inst *sdi;

	if (!(modbus = modbus_dev_inst_new(resource, serialcomm, modbusaddr)))
		return NULL;

	if (sr_modbus_open(modbus) != SR_OK) {
		sr_info("Couldn't open Modbus device.");
		sr_modbus_free(modbus);
		return NULL;
	}

	sdi = probe_device(modbus);

	sr_modbus_close(modbus);

	if (!sdi)
		sr_modbus_free(modbus);

	return sdi;
}

SR_PRIV GSList *sr_modbus_scan(struct sr_dev_driver *di, GSList *options,
		struct sr_dev_inst *(*probe_device)(struct sr_modbus_dev_inst *modbus))
{
	GSList *resources, *l, *devices;
	struct sr_dev_inst *sdi;
	const char *resource = NULL;
	const char *serialcomm = NULL;
	int modbusaddr = 1;
	gchar **res;

	(void)di;

	for (l = options; l; l = l->next) {
		struct sr_config *src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			resource = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_MODBUSADDR:
			modbusaddr = g_variant_get_uint64(src->data);
			break;
		}
	}

	devices = NULL;

	/* serial_rtu_dev.scan is NULL, so the resource-listing branch is skipped. */

	if (!devices && resource) {
		sdi = sr_modbus_scan_resource(resource, serialcomm, modbusaddr,
				probe_device);
		if (sdi)
			devices = g_slist_append(NULL, sdi);
	}

	return devices;
}

SR_PRIV struct sr_modbus_dev_inst *modbus_dev_inst_new(const char *resource,
		const char *serialcomm, int modbusaddr)
{
	struct sr_modbus_dev_inst *modbus = NULL;
	const struct sr_modbus_dev_inst *modbus_dev;
	gchar **params;

	/* Only serial RTU is supported. */
	modbus_dev = &modbus_serial_rtu_dev;
	if (!strncmp(resource, modbus_dev->prefix, strlen(modbus_dev->prefix))) {
		sr_dbg("Opening %s device %s.", modbus_dev->name, resource);
		modbus = g_malloc(sizeof(*modbus));
		*modbus = *modbus_dev;
		modbus->priv = g_malloc0(modbus->priv_size);
		modbus->read_timeout_ms = 1000;
		params = g_strsplit(resource, "/", 0);
		if (modbus->dev_inst_new(modbus->priv, resource,
				params, serialcomm, modbusaddr) != SR_OK) {
			sr_modbus_free(modbus);
			modbus = NULL;
		}
		g_strfree(params);
	}

	return modbus;
}

SR_PRIV int sr_modbus_open(struct sr_modbus_dev_inst *modbus)
{
	return modbus->open(modbus->priv);
}

SR_PRIV int sr_modbus_source_add(struct sr_modbus_dev_inst *modbus,
		int events, int timeout, sr_receive_data_callback_t cb,
		const struct sr_dev_inst *sdi)
{
	return modbus->source_add(modbus->priv, events, timeout, cb, sdi);
}

SR_PRIV int sr_modbus_source_remove(struct sr_modbus_dev_inst *modbus)
{
	return modbus->source_remove(modbus->priv);
}

static int sr_modbus_request(struct sr_modbus_dev_inst *modbus,
		uint8_t *request, int request_size)
{
	if (!request || request_size < 1)
		return SR_ERR_ARG;

	return modbus->send(modbus->priv, request, request_size);
}

static int sr_modbus_reply(struct sr_modbus_dev_inst *modbus,
		uint8_t *reply, int reply_size)
{
	int len, ret;
	gint64 laststart;
	unsigned int elapsed_ms;

	if (!reply || reply_size < 2)
		return SR_ERR_ARG;

	laststart = g_get_monotonic_time();

	ret = modbus->read_begin(modbus->priv, reply);
	if (ret != SR_OK)
		return ret;
	if (*reply & 0x80)
		reply_size = 2;

	reply++;
	reply_size--;

	while (reply_size > 0) {
		len = modbus->read_data(modbus->priv, reply, reply_size);
		if (len < 0) {
			sr_err("Incompletely read Modbus response.");
			return SR_ERR;
		} else if (len > 0) {
			laststart = g_get_monotonic_time();
		}
		reply += len;
		reply_size -= len;
		elapsed_ms = (g_get_monotonic_time() - laststart) / 1000;
		if (elapsed_ms >= (unsigned int)modbus->read_timeout_ms) {
			sr_err("Timed out waiting for Modbus response.");
			return SR_ERR;
		}
	}

	ret = modbus->read_end(modbus->priv);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

static int sr_modbus_request_reply(struct sr_modbus_dev_inst *modbus,
		uint8_t *request, int request_size, uint8_t *reply, int reply_size)
{
	int ret;
	ret = sr_modbus_request(modbus, request, request_size);
	if (ret != SR_OK)
		return ret;
	return sr_modbus_reply(modbus, reply, reply_size);
}

static int sr_modbus_error_check(const uint8_t *reply)
{
	const char *function = "UNKNOWN";
	const char *error = NULL;
	char buf[8];

	if (!(reply[0] & 0x80))
		return FALSE;

	switch (reply[0] & ~0x80) {
	case MODBUS_READ_COILS:
		function = "MODBUS_READ_COILS";
		break;
	case MODBUS_READ_HOLDING_REGISTERS:
		function = "READ_HOLDING_REGISTERS";
		break;
	case MODBUS_WRITE_COIL:
		function = "WRITE_COIL";
		break;
	case MODBUS_WRITE_MULTIPLE_REGISTERS:
		function = "WRITE_MULTIPLE_REGISTERS";
		break;
	}

	switch (reply[1]) {
	case 0x01: error = "ILLEGAL FUNCTION"; break;
	case 0x02: error = "ILLEGAL DATA ADDRESS"; break;
	case 0x03: error = "ILLEGAL DATA VALUE"; break;
	case 0x04: error = "SLAVE DEVICE FAILURE"; break;
	case 0x05: error = "ACKNOWLEDGE"; break;
	case 0x06: error = "SLAVE DEVICE BUSY"; break;
	case 0x08: error = "MEMORY PARITY ERROR"; break;
	case 0x0A: error = "GATEWAY PATH UNAVAILABLE"; break;
	case 0x0B: error = "GATEWAY TARGET DEVICE FAILED TO RESPOND"; break;
	}
	if (!error) {
		snprintf(buf, sizeof(buf), "0x%X", reply[1]);
		error = buf;
	}

	sr_err("%s error executing %s function.", error, function);

	return TRUE;
}

SR_PRIV int sr_modbus_read_holding_registers(struct sr_modbus_dev_inst *modbus,
		int address, int nb_registers, uint16_t *registers)
{
	uint8_t request[5], reply[2 + (2 * nb_registers)];
	int ret;

	if (address < -1 || address > 0xFFFF
	    || nb_registers < 1 || nb_registers > 125)
		return SR_ERR_ARG;

	W8(request + 0, MODBUS_READ_HOLDING_REGISTERS);
	WB16(request + 1, address);
	WB16(request + 3, nb_registers);

	if (address >= 0) {
		ret = sr_modbus_request(modbus, request, sizeof(request));
		if (ret != SR_OK)
			return ret;
	}

	if (registers) {
		ret = sr_modbus_reply(modbus, reply, sizeof(reply));
		if (ret != SR_OK)
			return ret;
		if (sr_modbus_error_check(reply))
			return SR_ERR_DATA;
		if (reply[0] != request[0] || R8(reply + 1) != (uint8_t)(2 * nb_registers))
			return SR_ERR_DATA;
		memcpy(registers, reply + 2, 2 * nb_registers);
	}

	return SR_OK;
}

SR_PRIV int sr_modbus_write_multiple_registers(struct sr_modbus_dev_inst *modbus,
		int address, int nb_registers, uint16_t *registers)
{
	uint8_t request[6 + (2 * nb_registers)], reply[5];
	int ret;

	if (address < 0 || address > 0xFFFF
	    || nb_registers < 1 || nb_registers > 123 || !registers)
		return SR_ERR_ARG;

	W8(request + 0, MODBUS_WRITE_MULTIPLE_REGISTERS);
	WB16(request + 1, address);
	WB16(request + 3, nb_registers);
	W8(request + 5, 2 * nb_registers);
	memcpy(request + 6, registers, 2 * nb_registers);

	ret = sr_modbus_request_reply(modbus, request, sizeof(request),
			reply, sizeof(reply));
	if (ret != SR_OK)
		return ret;
	if (sr_modbus_error_check(reply))
		return SR_ERR_DATA;
	if (memcmp(request, reply, sizeof(reply)))
		return SR_ERR_DATA;

	return SR_OK;
}

SR_PRIV int sr_modbus_close(struct sr_modbus_dev_inst *modbus)
{
	return modbus->close(modbus->priv);
}

SR_PRIV void sr_modbus_free(struct sr_modbus_dev_inst *modbus)
{
	modbus->free(modbus->priv);
	g_free(modbus->priv);
	g_free(modbus);
}

/* ===========================================================================
 * Original rdtech-dps protocol logic (adapted for PXView compat layer).
 * =========================================================================== */

/* These are the Modbus RTU registers for the DPS family of devices. */
enum rdtech_dps_register {
	REG_DPS_USET       = 0x00, /* Mirror of 0x50 */
	REG_DPS_ISET       = 0x01, /* Mirror of 0x51 */
	REG_DPS_UOUT       = 0x02,
	REG_DPS_IOUT       = 0x03,
	REG_DPS_POWER      = 0x04,
	REG_DPS_UIN        = 0x05,
	REG_DPS_LOCK       = 0x06,
	REG_DPS_PROTECT    = 0x07,
	REG_DPS_CV_CC      = 0x08,
	REG_DPS_ENABLE     = 0x09,
	REG_DPS_BACKLIGHT  = 0x0A, /* Mirror of 0x55 */
	REG_DPS_MODEL      = 0x0B,
	REG_DPS_VERSION    = 0x0C,

	REG_DPS_PRESET     = 0x23, /* Loads a preset into preset 0. */

	/*
	 * Add (preset * 0x10) to each of the following, for preset 1-9.
	 * Preset 0 regs below are the active output settings.
	 */
	PRE_DPS_USET       = 0x50,
	PRE_DPS_ISET       = 0x51,
	PRE_DPS_OVPSET     = 0x52,
	PRE_DPS_OCPSET     = 0x53,
	PRE_DPS_OPPSET     = 0x54,
	PRE_DPS_BACKLIGHT  = 0x55,
	PRE_DPS_DISABLE    = 0x56, /* Disable output if 0 is copied here from a preset (1 is no change). */
	PRE_DPS_BOOT       = 0x57, /* Enable output at boot if 1. */
};
#define PRE_DPS_STRIDE 0x10

enum rdtech_dps_protect_state {
	STATE_NORMAL = 0,
	STATE_OVP    = 1,
	STATE_OCP    = 2,
	STATE_OPP    = 3,
};

enum rdtech_dps_regulation_mode {
	MODE_CV      = 0,
	MODE_CC      = 1,
};

/*
 * These are the Modbus RTU registers for the RD family of devices.
 * Some registers are device specific, like REG_RD_RANGE of RD6012P
 * which could be battery related in other devices.
 */
enum rdtech_rd_register {
	REG_RD_MODEL = 0, /* u16 */
	REG_RD_SERIAL = 1, /* u32 */
	REG_RD_FIRMWARE = 3, /* u16 */
	REG_RD_TEMP_INT = 4, /* 2x u16 */
	REG_RD_TEMP_INT_F = 6, /* 2x u16 */
	REG_RD_VOLT_TGT = 8, /* u16 */
	REG_RD_CURR_LIM = 9, /* u16 */
	REG_RD_VOLTAGE = 10, /* u16 */
	REG_RD_CURRENT = 11, /* u16 */
	REG_RD_ENERGY = 12, /* u16 */
	REG_RD_POWER = 13, /* u16 */
	REG_RD_VOLT_IN = 14, /* u16 */
	REG_RD_PROTECT = 16, /* u16 */
	REG_RD_REGULATION = 17, /* u16 */
	REG_RD_ENABLE = 18, /* u16 */
	REG_RD_PRESET = 19, /* u16 */
	REG_RD_RANGE = 20, /* u16 */
	/*
	 * Battery at 32 == 0x20 pp:
	 * Mode, voltage, temperature, capacity, energy.
	 */
	/*
	 * Date/time at 48 == 0x30 pp:
	 * Year, month, day, hour, minute, second.
	 */
	/* Backlight at 72 == 0x48. */
	REG_RD_OVP_THR = 82, /* 0x52 */
	REG_RD_OCP_THR = 83, /* 0x53 */
	/* One "live" slot and 9 "memory" positions. */
	REG_RD_START_MEM = 84, /* 0x54 */
};

/* Retries failed modbus read attempts for improved reliability. */
static int rdtech_dps_read_holding_registers(struct sr_modbus_dev_inst *modbus,
	int address, int nb_registers, uint16_t *registers)
{
	size_t retries;
	int ret;

	retries = 3;
	while (retries--) {
		ret = sr_modbus_read_holding_registers(modbus,
			address, nb_registers, registers);
		if (ret == SR_OK)
			return ret;
	}

	return ret;
}

/* Set one 16bit register. LE format for DPS devices. */
static int rdtech_dps_set_reg(const struct sr_dev_inst *sdi,
	uint16_t address, uint16_t value)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	uint16_t registers[1];
	int ret;
	uint8_t *wrptr;

	devc = sdi->priv;
	modbus = sdi->conn;

	wrptr = (void *)registers;
	write_u16be(wrptr, value);

	g_mutex_lock(&devc->rw_mutex);
	ret = sr_modbus_write_multiple_registers(modbus, address,
		ARRAY_SIZE(registers), registers);
	g_mutex_unlock(&devc->rw_mutex);

	return ret;
}

/* Set one 16bit register. BE format for RD devices. */
static int rdtech_rd_set_reg(const struct sr_dev_inst *sdi,
	uint16_t address, uint16_t value)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	uint16_t registers[1];
	int ret;
	uint8_t *wrptr;

	devc = sdi->priv;
	modbus = sdi->conn;

	wrptr = (void *)registers;
	write_u16be(wrptr, value);

	g_mutex_lock(&devc->rw_mutex);
	ret = sr_modbus_write_multiple_registers(modbus, address,
		ARRAY_SIZE(registers), registers);
	g_mutex_unlock(&devc->rw_mutex);

	return ret;
}

/* Get DPS model number and firmware version from a connected device. */
SR_PRIV int rdtech_dps_get_model_version(struct sr_modbus_dev_inst *modbus,
	enum rdtech_dps_model_type model_type,
	uint16_t *model, uint16_t *version, uint32_t *serno)
{
	uint16_t registers[4];
	int ret;
	const uint8_t *rdptr;

	/*
	 * No mutex here because when the routine executes then the
	 * device instance was not created yet (probe phase).
	 */
	switch (model_type) {
	case MODEL_DPS:
		/* Get the MODEL and VERSION registers. */
		ret = rdtech_dps_read_holding_registers(modbus,
			REG_DPS_MODEL, 2, registers);
		if (ret != SR_OK)
			return ret;
		rdptr = (void *)registers;
		*model = read_u16be_inc(&rdptr);
		*version = read_u16be_inc(&rdptr);
		*serno = 0;
		sr_info("RDTech DPS/DPH model: %u version: %u",
			*model, *version);
		return SR_OK;
	case MODEL_RD:
		/* Get the MODEL, SERIAL, and FIRMWARE registers. */
		ret = rdtech_dps_read_holding_registers(modbus,
			REG_RD_MODEL, 4, registers);
		if (ret != SR_OK)
			return ret;
		rdptr = (void *)registers;
		*model = read_u16be_inc(&rdptr);
		*serno = read_u32be_inc(&rdptr);
		*version = read_u16be_inc(&rdptr);
		sr_info("RDTech RD model: %u version: %u, serno %u",
			*model, *version, *serno);
		return SR_OK;
	default:
		sr_err("Unexpected RDTech PSU device type. Programming error?");
		return SR_ERR_ARG;
	}
	/* UNREACH */
}

SR_PRIV void rdtech_dps_update_multipliers(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	const struct rdtech_dps_range *range;

	devc = sdi->priv;
	range = &devc->model->ranges[devc->curr_range];
	devc->current_multiplier = pow(10.0, range->current_digits);
	devc->voltage_multiplier = pow(10.0, range->voltage_digits);
}

/*
 * Determine range of connected device. Don't do anything once
 * acquisition has started (since the range will then be tracked).
 */
SR_PRIV int rdtech_dps_update_range(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	uint16_t range;
	int ret;

	devc = sdi->priv;

	/*
	 * Only update range if there are multiple ranges and data
	 * acquisition hasn't started.
	 */
	if (devc->model->n_ranges <= 1 || devc->acquisition_started)
		return SR_OK;
	if (devc->model->model_type != MODEL_RD)
		return SR_ERR;

	ret = rdtech_dps_read_holding_registers(sdi->conn,
		REG_RD_RANGE, 1, &range);
	if (ret != SR_OK)
		return ret;
	range = range ? 1 : 0;
	devc->curr_range = range;
	rdtech_dps_update_multipliers(sdi);

	return SR_OK;
}

/* Send a measured value to the session feed. */
static int send_value(const struct sr_dev_inst *sdi,
	struct sr_channel *ch, float value,
	enum sr_mq mq, enum sr_mqflag mqflags,
	enum sr_unit unit, int digits)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	int ret;

	sr_analog_init(&analog, &encoding, &meaning, &spec, digits);
	analog.meaning->channels = g_slist_append(NULL, ch);
	analog.num_samples = 1;
	analog.data = &value;
	analog.meaning->mq = mq;
	analog.meaning->mqflags = mqflags;
	analog.meaning->unit = unit;

	packet.type = SR_DF_ANALOG;
	packet.payload = &analog;
	ret = sr_session_send(sdi, &packet);

	g_slist_free(analog.meaning->channels);

	return ret;
}

/*
 * Get the device's current state. Exhaustively, relentlessly.
 * Concentrate all details of communication in the physical transport,
 * register layout interpretation, and potential model dependency in
 * this central spot, to simplify maintenance.
 */
SR_PRIV int rdtech_dps_get_state(const struct sr_dev_inst *sdi,
	struct rdtech_dps_state *state, enum rdtech_dps_state_context reason)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	gboolean get_config, get_init_state, get_curr_meas;
	uint16_t registers[14];
	int ret;
	const uint8_t *rdptr;
	uint16_t uset_raw, iset_raw, uout_raw, iout_raw, power_raw;
	uint16_t reg_val, reg_state, out_state, ovpset_raw, ocpset_raw;
	gboolean is_lock, is_out_enabled, is_reg_cc;
	gboolean uses_ovp, uses_ocp;
	gboolean have_range;
	uint16_t range;
	float volt_target, curr_limit;
	float ovp_threshold, ocp_threshold;
	float curr_voltage, curr_current, curr_power;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;
	devc = sdi->priv;
	modbus = sdi->conn;
	if (!state)
		return SR_ERR_ARG;

	/* Determine the requested level of response detail. */
	get_config = FALSE;
	get_init_state = FALSE;
	get_curr_meas = FALSE;
	switch (reason) {
	case ST_CTX_CONFIG:
		get_config = TRUE;
		get_init_state = TRUE;
		get_curr_meas = TRUE;
		break;
	case ST_CTX_PRE_ACQ:
		get_init_state = TRUE;
		get_curr_meas = TRUE;
		break;
	case ST_CTX_IN_ACQ:
		get_curr_meas = TRUE;
		break;
	default:
		/* EMPTY */
		break;
	}
	/*
	 * TODO Make use of this information to reduce the transfer
	 * volume, especially on low bitrate serial connections. Though
	 * the device firmware's samplerate is probably more limiting
	 * than communication bandwidth is.
	 */
	(void)get_config;
	(void)get_init_state;
	(void)get_curr_meas;

	have_range = devc->model->n_ranges > 1;
	if (!have_range)
		range = 0;

	switch (devc->model->model_type) {
	case MODEL_DPS:
		g_mutex_lock(&devc->rw_mutex);
		ret = rdtech_dps_read_holding_registers(modbus,
			REG_DPS_USET, REG_DPS_ENABLE - REG_DPS_USET + 1,
			registers);
		g_mutex_unlock(&devc->rw_mutex);
		if (ret != SR_OK)
			return ret;

		/* Interpret the registers' values. */
		rdptr = (const void *)registers;
		uset_raw = read_u16be_inc(&rdptr);
		volt_target = uset_raw / devc->voltage_multiplier;
		iset_raw = read_u16be_inc(&rdptr);
		curr_limit = iset_raw / devc->current_multiplier;
		uout_raw = read_u16be_inc(&rdptr);
		curr_voltage = uout_raw / devc->voltage_multiplier;
		iout_raw = read_u16be_inc(&rdptr);
		curr_current = iout_raw / devc->current_multiplier;
		power_raw = read_u16be_inc(&rdptr);
		curr_power = power_raw / 100.0f;
		(void)read_u16be_inc(&rdptr); /* UIN */
		reg_val = read_u16be_inc(&rdptr); /* LOCK */
		is_lock = reg_val != 0;
		reg_val = read_u16be_inc(&rdptr); /* PROTECT */
		uses_ovp = reg_val == STATE_OVP;
		uses_ocp = reg_val == STATE_OCP;
		reg_state = read_u16be_inc(&rdptr); /* CV_CC */
		is_reg_cc = reg_state == MODE_CC;
		out_state = read_u16be_inc(&rdptr); /* ENABLE */
		is_out_enabled = out_state != 0;

		/* Transfer another chunk of registers in a single call. */
		g_mutex_lock(&devc->rw_mutex);
		ret = rdtech_dps_read_holding_registers(modbus,
			PRE_DPS_OVPSET, 2, registers);
		g_mutex_unlock(&devc->rw_mutex);
		if (ret != SR_OK)
			return ret;

		/* Interpret the second registers chunk's values. */
		rdptr = (const void *)registers;
		ovpset_raw = read_u16be_inc(&rdptr); /* PRE OVPSET */
		ovp_threshold = ovpset_raw * devc->voltage_multiplier;
		ocpset_raw = read_u16be_inc(&rdptr); /* PRE OCPSET */
		ocp_threshold = ocpset_raw * devc->current_multiplier;

		break;

	case MODEL_RD:
		/* Retrieve a set of adjacent registers. */
		g_mutex_lock(&devc->rw_mutex);
		ret = rdtech_dps_read_holding_registers(modbus,
			REG_RD_VOLT_TGT,
			devc->model->n_ranges > 1
				? REG_RD_RANGE - REG_RD_VOLT_TGT + 1
				: REG_RD_ENABLE - REG_RD_VOLT_TGT + 1,
			registers);
		g_mutex_unlock(&devc->rw_mutex);
		if (ret != SR_OK)
			return ret;

		/* Interpret the registers' raw content. */
		rdptr = (const void *)registers;
		uset_raw = read_u16be_inc(&rdptr); /* USET */
		volt_target = uset_raw / devc->voltage_multiplier;
		iset_raw = read_u16be_inc(&rdptr); /* ISET */
		curr_limit = iset_raw / devc->current_multiplier;
		uout_raw = read_u16be_inc(&rdptr); /* UOUT */
		curr_voltage = uout_raw / devc->voltage_multiplier;
		iout_raw = read_u16be_inc(&rdptr); /* IOUT */
		curr_current = iout_raw / devc->current_multiplier;
		(void)read_u16be_inc(&rdptr); /* ENERGY */
		power_raw = read_u16be_inc(&rdptr); /* POWER */
		curr_power = power_raw / 100.0f;
		(void)read_u16be_inc(&rdptr); /* VOLT_IN */
		(void)read_u16be_inc(&rdptr);
		reg_val = read_u16be_inc(&rdptr); /* PROTECT */
		uses_ovp = reg_val == STATE_OVP;
		uses_ocp = reg_val == STATE_OCP;
		reg_state = read_u16be_inc(&rdptr); /* REGULATION */
		is_reg_cc = reg_state == MODE_CC;
		out_state = read_u16be_inc(&rdptr); /* ENABLE */
		is_out_enabled = out_state != 0;
		if (have_range) {
			(void)read_u16be_inc(&rdptr); /* PRESET */
			range = read_u16be_inc(&rdptr) ? 1 : 0; /* RANGE */
		}

		/* Retrieve a set of adjacent registers. */
		g_mutex_lock(&devc->rw_mutex);
		ret = rdtech_dps_read_holding_registers(modbus,
			REG_RD_OVP_THR, 2, registers);
		g_mutex_unlock(&devc->rw_mutex);
		if (ret != SR_OK)
			return ret;

		/* Interpret the registers' raw content. */
		rdptr = (const void *)registers;
		ovpset_raw = read_u16be_inc(&rdptr); /* OVP THR */
		ovp_threshold = ovpset_raw / devc->voltage_multiplier;
		ocpset_raw = read_u16be_inc(&rdptr); /* OCP THR */
		ocp_threshold = ocpset_raw / devc->current_multiplier;

		/* Details which we cannot query from the device. */
		is_lock = FALSE;

		break;

	default:
		/* ShouldNotHappen(TM). Probe should have failed. */
		return SR_ERR_ARG;
	}

	/*
	 * Store gathered details in the high level container.
	 */
	memset(state, 0, sizeof(*state));
	state->lock = is_lock;
	state->mask |= STATE_LOCK;
	state->output_enabled = is_out_enabled;
	state->mask |= STATE_OUTPUT_ENABLED;
	state->regulation_cc = is_reg_cc;
	state->mask |= STATE_REGULATION_CC;
	state->protect_ovp = uses_ovp;
	state->mask |= STATE_PROTECT_OVP;
	state->protect_ocp = uses_ocp;
	state->mask |= STATE_PROTECT_OCP;
	state->protect_enabled = TRUE;
	state->mask |= STATE_PROTECT_ENABLED;
	state->voltage_target = volt_target;
	state->mask |= STATE_VOLTAGE_TARGET;
	state->current_limit = curr_limit;
	state->mask |= STATE_CURRENT_LIMIT;
	state->ovp_threshold = ovp_threshold;
	state->mask |= STATE_OVP_THRESHOLD;
	state->ocp_threshold = ocp_threshold;
	state->mask |= STATE_OCP_THRESHOLD;
	state->voltage = curr_voltage;
	state->mask |= STATE_VOLTAGE;
	state->current = curr_current;
	state->mask |= STATE_CURRENT;
	state->power = curr_power;
	state->mask |= STATE_POWER;
	if (have_range) {
		state->range = range;
		state->mask |= STATE_RANGE;
	}

	return SR_OK;
}

/* Setup device's parameters. Selectively, from caller specs. */
SR_PRIV int rdtech_dps_set_state(const struct sr_dev_inst *sdi,
	struct rdtech_dps_state *state)
{
	struct dev_context *devc;
	uint16_t reg_value;
	int ret;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!state)
		return SR_ERR_ARG;

	/* Only a subset of known values is settable. */
	if (state->mask & STATE_OUTPUT_ENABLED) {
		reg_value = state->output_enabled ? 1 : 0;
		switch (devc->model->model_type) {
		case MODEL_DPS:
			ret = rdtech_dps_set_reg(sdi, REG_DPS_ENABLE, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		case MODEL_RD:
			ret = rdtech_rd_set_reg(sdi, REG_RD_ENABLE, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		default:
			return SR_ERR_ARG;
		}
	}
	if (state->mask & STATE_VOLTAGE_TARGET) {
		reg_value = state->voltage_target * devc->voltage_multiplier;
		switch (devc->model->model_type) {
		case MODEL_DPS:
			ret = rdtech_dps_set_reg(sdi, REG_DPS_USET, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		case MODEL_RD:
			ret = rdtech_rd_set_reg(sdi, REG_RD_VOLT_TGT, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		default:
			return SR_ERR_ARG;
		}
	}
	if (state->mask & STATE_CURRENT_LIMIT) {
		reg_value = state->current_limit * devc->current_multiplier;
		switch (devc->model->model_type) {
		case MODEL_DPS:
			ret = rdtech_dps_set_reg(sdi, REG_DPS_ISET, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		case MODEL_RD:
			ret = rdtech_rd_set_reg(sdi, REG_RD_CURR_LIM, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		default:
			return SR_ERR_ARG;
		}
	}
	if (state->mask & STATE_OVP_THRESHOLD) {
		reg_value = state->ovp_threshold * devc->voltage_multiplier;
		switch (devc->model->model_type) {
		case MODEL_DPS:
			ret = rdtech_dps_set_reg(sdi, PRE_DPS_OVPSET, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		case MODEL_RD:
			ret = rdtech_rd_set_reg(sdi, REG_RD_OVP_THR, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		default:
			return SR_ERR_ARG;
		}
	}
	if (state->mask & STATE_OCP_THRESHOLD) {
		reg_value = state->ocp_threshold * devc->current_multiplier;
		switch (devc->model->model_type) {
		case MODEL_DPS:
			ret = rdtech_dps_set_reg(sdi, PRE_DPS_OCPSET, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		case MODEL_RD:
			ret = rdtech_rd_set_reg(sdi, REG_RD_OCP_THR, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		default:
			return SR_ERR_ARG;
		}
	}
	if (state->mask & STATE_LOCK) {
		switch (devc->model->model_type) {
		case MODEL_DPS:
			reg_value = state->lock ? 1 : 0;
			ret = rdtech_dps_set_reg(sdi, REG_DPS_LOCK, reg_value);
			if (ret != SR_OK)
				return ret;
			break;
		case MODEL_RD:
			/* Do nothing, _and_ silently succeed. */
			break;
		default:
			return SR_ERR_ARG;
		}
	}
	if (state->mask & STATE_RANGE) {
		reg_value = state->range;
		switch (devc->model->model_type) {
		case MODEL_DPS:
			/* DPS models don't support current ranges at all. */
			if (reg_value > 0)
				return SR_ERR_ARG;
			break;
		case MODEL_RD:
			/*
			 * Reject unsupported range indices.
			 * Need not set the range when the device only
			 * supports a single fixed range.
			 */
			if (reg_value >= devc->model->n_ranges)
				return SR_ERR_NA;
			if (devc->model->n_ranges <= 1)
				return SR_OK;
			ret = rdtech_rd_set_reg(sdi, REG_RD_RANGE, reg_value);
			if (ret != SR_OK)
				return ret;
			/*
			 * Immediately update internal state outside of
			 * an acquisition. Assume that in-acquisition
			 * activity will update internal state. This is
			 * essential for meta package emission.
			 */
			if (!devc->acquisition_started) {
				devc->curr_range = reg_value;
				rdtech_dps_update_multipliers(sdi);
			}
			break;
		default:
			return SR_ERR_ARG;
		}
	}

	return SR_OK;
}

/* Get the current state when acquisition starts. */
SR_PRIV int rdtech_dps_seed_receive(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct rdtech_dps_state state;
	int ret;

	if (!sdi || !sdi->priv)
		return SR_ERR_ARG;
	devc = sdi->priv;

	ret = rdtech_dps_get_state(sdi, &state, ST_CTX_PRE_ACQ);
	if (ret != SR_OK)
		return ret;

	if (state.mask & STATE_PROTECT_OVP)
		devc->curr_ovp_state = state.protect_ovp;
	if (state.mask & STATE_PROTECT_OCP)
		devc->curr_ocp_state = state.protect_ocp;
	if (state.mask & STATE_REGULATION_CC)
		devc->curr_cc_state = state.regulation_cc;
	if (state.mask & STATE_OUTPUT_ENABLED)
		devc->curr_out_state = state.output_enabled;
	if (state.mask & STATE_RANGE) {
		devc->curr_range = state.range;
		rdtech_dps_update_multipliers(sdi);
	}

	return SR_OK;
}

/* Get measurements, track state changes during acquisition. */
SR_PRIV int rdtech_dps_receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct rdtech_dps_state state;
	int ret;
	struct sr_channel *ch;
	const char *regulation_text, *range_text;

	(void)fd;
	(void)revents;

	if (!sdi)
		return TRUE;
	devc = sdi->priv;

	/* Get the device's current state. */
	ret = rdtech_dps_get_state(sdi, &state, ST_CTX_IN_ACQ);
	if (ret != SR_OK)
		return ret;


	/* Submit measurement data to the session feed. */
	std_session_send_df_frame_begin(sdi);
	ch = g_slist_nth_data(sdi->channels, 0);
	send_value(sdi, ch, state.voltage,
		SR_MQ_VOLTAGE, SR_MQFLAG_DC, SR_UNIT_VOLT,
		devc->model->ranges[devc->curr_range].voltage_digits);
	ch = g_slist_nth_data(sdi->channels, 1);
	send_value(sdi, ch, state.current,
		SR_MQ_CURRENT, SR_MQFLAG_DC, SR_UNIT_AMPERE,
		devc->model->ranges[devc->curr_range].current_digits);
	ch = g_slist_nth_data(sdi->channels, 2);
	send_value(sdi, ch, state.power,
		SR_MQ_POWER, 0, SR_UNIT_WATT, 2);
	std_session_send_df_frame_end(sdi);

	/* Check for state changes. */
	if (devc->curr_ovp_state != state.protect_ovp) {
		(void)sr_session_send_meta(sdi,
			SR_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE,
			g_variant_new_boolean(state.protect_ovp));
		devc->curr_ovp_state = state.protect_ovp;
	}
	if (devc->curr_ocp_state != state.protect_ocp) {
		(void)sr_session_send_meta(sdi,
			SR_CONF_OVER_CURRENT_PROTECTION_ACTIVE,
			g_variant_new_boolean(state.protect_ocp));
		devc->curr_ocp_state = state.protect_ocp;
	}
	if (devc->curr_cc_state != state.regulation_cc) {
		regulation_text = state.regulation_cc ? "CC" : "CV";
		(void)sr_session_send_meta(sdi, SR_CONF_REGULATION,
			g_variant_new_string(regulation_text));
		devc->curr_cc_state = state.regulation_cc;
	}
	if (devc->curr_out_state != state.output_enabled) {
		(void)sr_session_send_meta(sdi, SR_CONF_ENABLED,
			g_variant_new_boolean(state.output_enabled));
		devc->curr_out_state = state.output_enabled;
	}
	if (devc->curr_range != state.range) {
		range_text = devc->model->ranges[state.range].range_str;
		(void)sr_session_send_meta(sdi, SR_CONF_RANGE,
			g_variant_new_string(range_text));
		devc->curr_range = state.range;
		rdtech_dps_update_multipliers(sdi);
	}

	/* Check optional acquisition limits. */
	sr_sw_limits_update_samples_read(&devc->limits, 1);
	if (sr_sw_limits_check(&devc->limits)) {
		sr_dev_acquisition_stop(sdi);
		return TRUE;
	}

	return TRUE;
}
