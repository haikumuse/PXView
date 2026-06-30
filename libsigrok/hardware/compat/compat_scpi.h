/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2025 Compat Layer Authors
 * Based on standard sigrok SCPI implementation
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

#ifndef LIBSIGROK_COMPAT_SCPI_H
#define LIBSIGROK_COMPAT_SCPI_H

/**
 * @file
 *
 * SCPI (Standard Commands for Programmable Instruments) communication
 * layer compat implementation.
 *
 * Supports three transport types:
 * - TCP SCPI (Raw TCP and Rigol TCP modes)
 * - USBTMC SCPI (USB Test and Measurement Class via libusb)
 * - Serial SCPI (RS-232/RS-485 serial ports)
 */

#include <glib.h>
#include <stdint.h>
#include <libsigrok/libsigrok.h>

/*--- SCPI Commands ---------------------------------------------------------*/

#define SCPI_CMD_IDN "*IDN?"
#define SCPI_CMD_OPC "*OPC?"

/*--- SCPI Transport Types -------------------------------------------------*/

enum scpi_transport_layer {
	SCPI_TRANSPORT_LIBGPIB,
	SCPI_TRANSPORT_SERIAL,
	SCPI_TRANSPORT_RAW_TCP,
	SCPI_TRANSPORT_RIGOL_TCP,
	SCPI_TRANSPORT_USBTMC,
	SCPI_TRANSPORT_VISA,
	SCPI_TRANSPORT_VXI,
};

/*--- SCPI Command Table ---------------------------------------------------*/

struct scpi_command {
	int command;
	const char *string;
};

/*--- SCPI Hardware Info ---------------------------------------------------*/

struct sr_scpi_hw_info {
	char *manufacturer;
	char *model;
	char *serial_number;
	char *firmware_version;
};

/*--- TCP Device Instance --------------------------------------------------*/

struct sr_tcp_dev_inst {
	char *host_addr;
	char *tcp_port;
	int sock_fd;
};

/*--- SCPI Device Instance -------------------------------------------------*/

struct sr_scpi_dev_inst {
	const char *name;
	const char *prefix;
	enum scpi_transport_layer transport;
	int priv_size;
	GSList *(*scan)(struct drv_context *drvc);
	int (*dev_inst_new)(void *priv, struct drv_context *drvc,
		const char *resource, char **params, const char *serialcomm);
	int (*open)(struct sr_scpi_dev_inst *scpi);
	int (*connection_id)(struct sr_scpi_dev_inst *scpi, char **connection_id);
	int (*source_add)(struct sr_session *session, void *priv, int events,
		int timeout, sr_receive_data_callback_t cb, void *cb_data);
	int (*source_remove)(struct sr_session *session, void *priv);
	int (*send)(void *priv, const char *command);
	int (*read_begin)(void *priv);
	int (*read_data)(void *priv, char *buf, int maxlen);
	int (*write_data)(void *priv, char *buf, int len);
	int (*read_complete)(void *priv);
	int (*close)(struct sr_scpi_dev_inst *scpi);
	void (*free)(void *priv);
	unsigned int read_timeout_us;
	void *priv;
	/* Firmware version for quirks (e.g., Rigol DS1000 series) */
	uint64_t firmware_version;
	GMutex scpi_mutex;
	char *actual_channel_name;
	gboolean no_opc_command;
};

/*--- SCPI Core Functions --------------------------------------------------*/

SR_PRIV GSList *sr_scpi_scan(struct drv_context *drvc, GSList *options,
		struct sr_dev_inst *(*probe_device)(struct sr_scpi_dev_inst *scpi));

SR_PRIV struct sr_scpi_dev_inst *scpi_dev_inst_new(struct drv_context *drvc,
		const char *resource, const char *serialcomm);

SR_PRIV int sr_scpi_open(struct sr_scpi_dev_inst *scpi);

SR_PRIV int sr_scpi_connection_id(struct sr_scpi_dev_inst *scpi,
		char **connection_id);

SR_PRIV int sr_scpi_source_add(struct sr_session *session,
		struct sr_scpi_dev_inst *scpi, int events, int timeout,
		sr_receive_data_callback_t cb, void *cb_data);

SR_PRIV int sr_scpi_source_remove(struct sr_session *session,
		struct sr_scpi_dev_inst *scpi);

SR_PRIV int sr_scpi_send(struct sr_scpi_dev_inst *scpi,
		const char *format, ...);

SR_PRIV int sr_scpi_send_variadic(struct sr_scpi_dev_inst *scpi,
		const char *format, va_list args);

SR_PRIV int sr_scpi_read_begin(struct sr_scpi_dev_inst *scpi);

SR_PRIV int sr_scpi_read_data(struct sr_scpi_dev_inst *scpi,
		char *buf, int maxlen);

SR_PRIV int sr_scpi_write_data(struct sr_scpi_dev_inst *scpi,
		char *buf, int len);

SR_PRIV int sr_scpi_read_complete(struct sr_scpi_dev_inst *scpi);

SR_PRIV int sr_scpi_close(struct sr_scpi_dev_inst *scpi);

SR_PRIV void sr_scpi_free(struct sr_scpi_dev_inst *scpi);

/*--- SCPI Response Functions ----------------------------------------------*/

SR_PRIV int sr_scpi_read_response(struct sr_scpi_dev_inst *scpi,
		GString *response, gint64 abs_timeout_us);

SR_PRIV int sr_scpi_get_string(struct sr_scpi_dev_inst *scpi,
		const char *command, char **scpi_response);

SR_PRIV int sr_scpi_get_bool(struct sr_scpi_dev_inst *scpi,
		const char *command, gboolean *scpi_response);

SR_PRIV int sr_scpi_get_int(struct sr_scpi_dev_inst *scpi,
		const char *command, int *scpi_response);

SR_PRIV int sr_scpi_get_float(struct sr_scpi_dev_inst *scpi,
		const char *command, float *scpi_response);

SR_PRIV int sr_scpi_get_double(struct sr_scpi_dev_inst *scpi,
		const char *command, double *scpi_response);

SR_PRIV int sr_scpi_get_opc(struct sr_scpi_dev_inst *scpi);

SR_PRIV int sr_scpi_get_floatv(struct sr_scpi_dev_inst *scpi,
		const char *command, GArray **scpi_response);

SR_PRIV int sr_scpi_get_uint8v(struct sr_scpi_dev_inst *scpi,
		const char *command, GArray **scpi_response);

SR_PRIV int sr_scpi_get_data(struct sr_scpi_dev_inst *scpi,
		const char *command, GString **scpi_response);

SR_PRIV int sr_scpi_get_block(struct sr_scpi_dev_inst *scpi,
		const char *command, GByteArray **scpi_response);

SR_PRIV int sr_scpi_get_hw_id(struct sr_scpi_dev_inst *scpi,
		struct sr_scpi_hw_info **scpi_response);

SR_PRIV void sr_scpi_hw_info_free(struct sr_scpi_hw_info *hw_info);

/*--- SCPI Utility Functions -----------------------------------------------*/

SR_PRIV const char *sr_scpi_unquote_string(char *s);

SR_PRIV const char *sr_vendor_alias(const char *raw_vendor);

SR_PRIV const char *sr_scpi_cmd_get(const struct scpi_command *cmdtable,
		int command);

SR_PRIV int sr_scpi_cmd(const struct sr_dev_inst *sdi,
		const struct scpi_command *cmdtable,
		int channel_command, const char *channel_name,
		int command, ...);

SR_PRIV int sr_scpi_cmd_resp(const struct sr_dev_inst *sdi,
		const struct scpi_command *cmdtable,
		int channel_command, const char *channel_name,
		GVariant **gvar, const GVariantType *gvtype, int command, ...);

/*--- TCP Transport Functions ----------------------------------------------*/

SR_PRIV struct sr_tcp_dev_inst *sr_tcp_dev_inst_new(
		const char *host_addr, const char *tcp_port);

SR_PRIV void sr_tcp_dev_inst_free(struct sr_tcp_dev_inst *tcp);

SR_PRIV int sr_tcp_get_port_path(struct sr_tcp_dev_inst *tcp,
		const char *prefix, char separator, char *path, size_t path_len);

SR_PRIV int sr_tcp_connect(struct sr_tcp_dev_inst *tcp);

SR_PRIV int sr_tcp_disconnect(struct sr_tcp_dev_inst *tcp);

SR_PRIV int sr_tcp_write_bytes(struct sr_tcp_dev_inst *tcp,
		const uint8_t *data, size_t dlen);

SR_PRIV int sr_tcp_read_bytes(struct sr_tcp_dev_inst *tcp,
		uint8_t *data, size_t dlen, gboolean nonblocking);

SR_PRIV int sr_tcp_source_add(struct sr_session *session,
		struct sr_tcp_dev_inst *tcp, int events, int timeout,
		sr_receive_data_callback_t cb, void *cb_data);

SR_PRIV int sr_tcp_source_remove(struct sr_session *session,
		struct sr_tcp_dev_inst *tcp);

/*--- SCPI Device Declarations ---------------------------------------------*/

extern const struct sr_scpi_dev_inst scpi_tcp_raw_dev;
extern const struct sr_scpi_dev_inst scpi_tcp_rigol_dev;
extern const struct sr_scpi_dev_inst scpi_usbtmc_libusb_dev;
extern const struct sr_scpi_dev_inst scpi_serial_dev;

#endif /* LIBSIGROK_COMPAT_SCPI_H */