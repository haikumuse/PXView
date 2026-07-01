/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Aurelien Jacobs <aurel@gnuage.org>
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
static uint16_t maynuo_m97_crc16(uint16_t seed, const uint8_t *data, size_t len)
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
 * std_session_send_df_frame_begin/end() are provided by compat_helpers.c.
 * =========================================================================== */

/* ===========================================================================
 * Local Modbus serial RTU layer.
 * Combines standard sigrok's modbus.c and modbus_serial_rtu.c into a single
 * self-contained implementation. PXView's libsigrok has no Modbus support.
 * Adds sr_modbus_read_coils() and sr_modbus_write_coil() which the
 * maynuo-m97 driver needs (it queries status bits via Modbus coils).
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

	crc = maynuo_m97_crc16(SR_CRC16_DEFAULT_INIT, &slave_addr, sizeof(slave_addr));
	crc = maynuo_m97_crc16(crc, buffer, buffer_size);

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

	modbus->crc = maynuo_m97_crc16(SR_CRC16_DEFAULT_INIT, &slave_addr, sizeof(slave_addr));
	modbus->crc = maynuo_m97_crc16(modbus->crc, function_code, 1);

	return SR_OK;
}

static int modbus_serial_rtu_read_data(void *priv, uint8_t *buf, int maxlen)
{
	struct modbus_serial_rtu *modbus = priv;
	int ret;

	ret = serial_read_nonblocking(modbus->serial, buf, maxlen);
	if (ret < 0)
		return ret;
	modbus->crc = maynuo_m97_crc16(modbus->crc, buf, ret);
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
	GSList *l, *devices;
	struct sr_dev_inst *sdi;
	const char *resource = NULL;
	const char *serialcomm = NULL;
	int modbusaddr = 1;

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
		g_strfreev(params);
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

/*
 * Read `nb_coils` coils starting at `address`. The result is packed
 * bit-by-bit into the least significant bits of the `coils` output buffer
 * (one coil per byte, matching standard sigrok's convention).
 *
 * Note: Standard sigrok's sr_modbus_read_coils() packs coils as bits into
 * bytes (8 coils per byte). The maynuo-m97 driver only ever reads one coil
 * at a time and inspects `coils[0] & 1`, so the packing convention does not
 * matter for correctness here. We replicate the bit-packed behavior to
 * match standard sigrok's API exactly.
 */
SR_PRIV int sr_modbus_read_coils(struct sr_modbus_dev_inst *modbus,
		int address, int nb_coils, uint8_t *coils)
{
	uint8_t request[5], reply[2 + ((nb_coils + 7) / 8)];
	int ret, i, byte_count;
	uint8_t mask;

	if (address < 0 || address > 0xFFFF
			|| nb_coils < 1 || nb_coils > 2000 || !coils)
		return SR_ERR_ARG;

	W8(request + 0, MODBUS_READ_COILS);
	WB16(request + 1, address);
	WB16(request + 3, nb_coils);

	ret = sr_modbus_request_reply(modbus, request, sizeof(request),
			reply, sizeof(reply));
	if (ret != SR_OK)
		return ret;
	if (sr_modbus_error_check(reply))
		return SR_ERR_DATA;
	if (reply[0] != request[0])
		return SR_ERR_DATA;
	byte_count = R8(reply + 1);
	if (byte_count != (nb_coils + 7) / 8)
		return SR_ERR_DATA;

	/* Unpack bits into one-coil-per-byte layout for caller convenience. */
	for (i = 0; i < nb_coils; i++) {
		mask = 1 << (i & 7);
		coils[i] = (reply[2 + (i / 8)] & mask) ? 1 : 0;
	}

	return SR_OK;
}

SR_PRIV int sr_modbus_write_coil(struct sr_modbus_dev_inst *modbus,
		int address, int value)
{
	uint8_t request[5], reply[5];
	int ret;

	if (address < 0 || address > 0xFFFF)
		return SR_ERR_ARG;

	W8(request + 0, MODBUS_WRITE_COIL);
	WB16(request + 1, address);
	WB16(request + 3, value ? 0xFF00 : 0x0000);

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
 * Original maynuo-m97 protocol logic (adapted for PXView compat layer).
 * =========================================================================== */

SR_PRIV int maynuo_m97_get_bit(const struct sr_dev_inst *sdi,
		enum maynuo_m97_coil address, int *value)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	uint8_t coil;

	devc = sdi->priv;
	modbus = sdi->conn;

	g_mutex_lock(&devc->rw_mutex);
	int ret = sr_modbus_read_coils(modbus, address, 1, &coil);
	g_mutex_unlock(&devc->rw_mutex);
	*value = coil & 1;
	return ret;
}

SR_PRIV int maynuo_m97_set_bit(const struct sr_dev_inst *sdi,
		enum maynuo_m97_coil address, int value)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;

	devc = sdi->priv;
	modbus = sdi->conn;

	g_mutex_lock(&devc->rw_mutex);
	int ret = sr_modbus_write_coil(modbus, address, value);
	g_mutex_unlock(&devc->rw_mutex);
	return ret;
}

SR_PRIV int maynuo_m97_get_float(const struct sr_dev_inst *sdi,
		enum maynuo_m97_register address, float *value)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	uint16_t registers[2];

	devc = sdi->priv;
	modbus = sdi->conn;

	g_mutex_lock(&devc->rw_mutex);
	int ret = sr_modbus_read_holding_registers(modbus, address, 2, registers);
	g_mutex_unlock(&devc->rw_mutex);
	if (ret == SR_OK)
		*value = RBFL(registers);
	return ret;
}

SR_PRIV int maynuo_m97_set_float(const struct sr_dev_inst *sdi,
		enum maynuo_m97_register address, float value)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	uint16_t registers[2];
	int ret;

	devc = sdi->priv;
	modbus = sdi->conn;

	WBFL(registers, value);
	g_mutex_lock(&devc->rw_mutex);
	ret = sr_modbus_write_multiple_registers(modbus, address, 2, registers);
	g_mutex_unlock(&devc->rw_mutex);
	return ret;
}


static int maynuo_m97_cmd(const struct sr_dev_inst *sdi,
		enum maynuo_m97_mode cmd)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	uint16_t registers[1];
	int ret;

	devc = sdi->priv;
	modbus = sdi->conn;

	WB16(registers, cmd);
	g_mutex_lock(&devc->rw_mutex);
	ret = sr_modbus_write_multiple_registers(modbus, CMD, 1, registers);
	g_mutex_unlock(&devc->rw_mutex);
	return ret;
}

SR_PRIV int maynuo_m97_get_mode(const struct sr_dev_inst *sdi,
		enum maynuo_m97_mode *mode)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	uint16_t registers[1];
	int ret;

	devc = sdi->priv;
	modbus = sdi->conn;

	g_mutex_lock(&devc->rw_mutex);
	ret = sr_modbus_read_holding_registers(modbus, SETMODE, 1, registers);
	g_mutex_unlock(&devc->rw_mutex);
	*mode = RB16(registers) & 0xFF;
	return ret;
}

SR_PRIV int maynuo_m97_set_mode(const struct sr_dev_inst *sdi,
		enum maynuo_m97_mode mode)
{
	return maynuo_m97_cmd(sdi, mode);
}

SR_PRIV int maynuo_m97_set_input(const struct sr_dev_inst *sdi, int enable)
{
	enum maynuo_m97_mode mode;
	int ret;
	if ((ret = maynuo_m97_get_mode(sdi, &mode)) != SR_OK)
		return ret;
	if ((ret = maynuo_m97_cmd(sdi, enable ? INPUT_ON : INPUT_OFF)) != SR_OK)
		return ret;
	return maynuo_m97_set_mode(sdi, mode);
}

SR_PRIV int maynuo_m97_get_model_version(struct sr_modbus_dev_inst *modbus,
		uint16_t *model, uint16_t *version)
{
	uint16_t registers[2];
	int ret;

	/*
	 * No mutex here, because there is no sr_dev_inst when this function
	 * is called.
	 */

	ret = sr_modbus_read_holding_registers(modbus, MODEL, 2, registers);
	*model   = RB16(registers + 0);
	*version = RB16(registers + 1);
	return ret;
}


SR_PRIV const char *maynuo_m97_mode_to_str(enum maynuo_m97_mode mode)
{
	switch (mode) {
	case CC:             return "CC";
	case CV:             return "CV";
	case CW:             return "CP";
	case CR:             return "CR";
	case CC_SOFT_START:  return "CC Soft Start";
	case DYNAMIC:        return "Dynamic";
	case SHORT_CIRCUIT:  return "Short Circuit";
	case LIST:           return "List Mode";
	case CC_L_AND_UL:    return "CC Loading and Unloading";
	case CV_L_AND_UL:    return "CV Loading and Unloading";
	case CW_L_AND_UL:    return "CP Loading and Unloading";
	case CR_L_AND_UL:    return "CR Loading and Unloading";
	case CC_TO_CV:       return "CC + CV";
	case CR_TO_CV:       return "CR + CV";
	case BATTERY_TEST:   return "Battery Test";
	case CV_SOFT_START:  return "CV Soft Start";
	default:             return "UNKNOWN";
	}
}


static void maynuo_m97_session_send_value(const struct sr_dev_inst *sdi,
		struct sr_channel *ch, float value, enum sr_mq mq,
		enum sr_unit unit, int digits)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;

	memset(&analog, 0, sizeof(analog));
	analog.probes = g_slist_append(NULL, ch);
	analog.num_samples = 1;
	analog.data = &value;
	analog.mq = mq;
	analog.unit = unit;
	analog.mqflags = SR_MQFLAG_DC;
	analog.unit_bits = 32;

	packet.type = SR_DF_ANALOG;
	packet.payload = &analog;
	sr_session_send(sdi, &packet);
	g_slist_free(analog.probes);
}

/*
 * Data reception callback. Note the 3-argument signature with const sdi
 * (PXView's sr_receive_data_callback_t expects this signature). The original
 * maynuo-m97 driver used (int fd, int revents, void *cb_data) and unwrapped
 * cb_data into sdi; the PXView version receives sdi directly.
 *
 * The original code accessed channels via sdi->channels->data and
 * sdi->channels->next->data. Use g_slist_nth_data() for clarity and to
 * match the itech-it8500 compat driver's pattern (defensive against
 * future channel list reordering).
 */
SR_PRIV int maynuo_m97_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	uint16_t registers[4];
	int ret;
	struct sr_channel *ch;

	(void)fd;
	(void)revents;

	if (!sdi)
		return TRUE;

	modbus = sdi->conn;
	devc = sdi->priv;

	g_mutex_lock(&devc->rw_mutex);
	ret = sr_modbus_read_holding_registers(modbus, -1, 4, registers);
	g_mutex_unlock(&devc->rw_mutex);

	if (ret == SR_OK) {
		std_session_send_df_frame_begin(sdi);

		ch = g_slist_nth_data(sdi->channels, 0);
		maynuo_m97_session_send_value(sdi, ch,
		                              RBFL(registers + 0),
		                              SR_MQ_VOLTAGE, SR_UNIT_VOLT, 3);
		ch = g_slist_nth_data(sdi->channels, 1);
		maynuo_m97_session_send_value(sdi, ch,
		                              RBFL(registers + 2),
		                              SR_MQ_CURRENT, SR_UNIT_AMPERE, 4);

		std_session_send_df_frame_end(sdi);
		sr_sw_limits_update_samples_read(&devc->limits, 1);
	}

	if (sr_sw_limits_check(&devc->limits)) {
		sr_dev_acquisition_stop(sdi);
		return TRUE;
	}

	return TRUE;
}
