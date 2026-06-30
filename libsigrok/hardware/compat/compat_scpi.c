/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2025 Compat Layer Authors
 * Based on standard sigrok SCPI implementation by poljar (Damir Jelić)
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

#include "compat.h"
#include "compat_scpi.h"
#include "compat_serial.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef HAVE_LIBUSB_1_0
#include <libusb.h>
#endif

#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#define SHUT_RDWR SD_BOTH
#define close closesocket
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define LOG_PREFIX "scpi"

#define SCPI_READ_RETRIES 100
#define SCPI_READ_RETRY_TIMEOUT_US (10 * 1000)

#define LENGTH_BYTES sizeof(uint32_t)

/*--- Vendor Alias Table ---------------------------------------------------*/

static const char *scpi_vendors[][2] = {
	{ "Agilent Technologies", "Agilent" },
	{ "CHROMA", "Chroma" },
	{ "Chroma ATE", "Chroma" },
	{ "HEWLETT-PACKARD", "HP" },
	{ "Keysight Technologies", "Keysight" },
	{ "PHILIPS", "Philips" },
	{ "RIGOL TECHNOLOGIES", "Rigol" },
	{ "Siglent Technologies", "Siglent" },
};

/*--- Boolean Parsing ------------------------------------------------------*/

static int parse_strict_bool(const char *str, gboolean *ret)
{
	if (!str)
		return SR_ERR_ARG;

	if (!g_strcmp0(str, "1") ||
	    !g_ascii_strncasecmp(str, "y", 1) ||
	    !g_ascii_strncasecmp(str, "t", 1) ||
	    !g_ascii_strncasecmp(str, "yes", 3) ||
	    !g_ascii_strncasecmp(str, "true", 4) ||
	    !g_ascii_strncasecmp(str, "on", 2)) {
		*ret = TRUE;
		return SR_OK;
	} else if (!g_strcmp0(str, "0") ||
		   !g_ascii_strncasecmp(str, "n", 1) ||
		   !g_ascii_strncasecmp(str, "f", 1) ||
		   !g_ascii_strncasecmp(str, "no", 2) ||
		   !g_ascii_strncasecmp(str, "false", 5) ||
		   !g_ascii_strncasecmp(str, "off", 3)) {
		*ret = FALSE;
		return SR_OK;
	}

	return SR_ERR;
}

/*===========================================================================
 * TCP Transport Layer Implementation
 *===========================================================================*/

struct scpi_tcp {
	struct sr_tcp_dev_inst *tcp_dev;
	uint8_t length_buf[LENGTH_BYTES];
	size_t length_bytes_read;
	size_t response_length;
	size_t response_bytes_read;
};

static int scpi_tcp_dev_inst_new(void *priv, struct drv_context *drvc,
		const char *resource, char **params, const char *serialcomm)
{
	struct scpi_tcp *tcp = priv;

	(void)drvc;
	(void)resource;
	(void)serialcomm;

	if (!params || !params[1] || !params[2]) {
		sr_err("Invalid TCP parameters.");
		return SR_ERR;
	}

	tcp->tcp_dev = sr_tcp_dev_inst_new(params[1], params[2]);
	if (!tcp->tcp_dev)
		return SR_ERR;

	return SR_OK;
}

static int scpi_tcp_open(struct sr_scpi_dev_inst *scpi)
{
	struct scpi_tcp *tcp = scpi->priv;

	return sr_tcp_connect(tcp->tcp_dev);
}

static int scpi_tcp_connection_id(struct sr_scpi_dev_inst *scpi,
		char **connection_id)
{
	struct scpi_tcp *tcp = scpi->priv;
	char conn_text[128];
	int ret;

	ret = sr_tcp_get_port_path(tcp->tcp_dev, scpi->prefix, '/',
		conn_text, sizeof(conn_text));
	if (ret != SR_OK)
		return ret;

	*connection_id = g_strdup(conn_text);
	return SR_OK;
}

static int scpi_tcp_source_add(struct sr_session *session, void *priv,
		int events, int timeout, sr_receive_data_callback_t cb, void *cb_data)
{
	struct scpi_tcp *tcp = priv;

	return sr_tcp_source_add(session, tcp->tcp_dev,
		events, timeout, cb, cb_data);
}

static int scpi_tcp_source_remove(struct sr_session *session, void *priv)
{
	struct scpi_tcp *tcp = priv;

	return sr_tcp_source_remove(session, tcp->tcp_dev);
}

static int scpi_tcp_send(void *priv, const char *command)
{
	struct scpi_tcp *tcp = priv;
	const uint8_t *wrptr;
	size_t wrlen;
	int ret;

	wrptr = (const uint8_t *)command;
	wrlen = strlen(command);
	ret = sr_tcp_write_bytes(tcp->tcp_dev, wrptr, wrlen);
	if (ret < 0) {
		sr_err("Send error: %s", g_strerror(errno));
		return SR_ERR;
	}

	sr_spew("Successfully sent SCPI command: '%s'.", command);

	return SR_OK;
}

static int scpi_tcp_read_begin(void *priv)
{
	struct scpi_tcp *tcp = priv;

	tcp->response_bytes_read = 0;
	tcp->length_bytes_read = 0;

	return SR_OK;
}

/* Raw TCP mode: read response data */
static int scpi_tcp_raw_read_data(void *priv, char *buf, int maxlen)
{
	struct scpi_tcp *tcp = priv;
	uint8_t *rdptr;
	size_t rdlen;
	int ret;

	rdptr = (uint8_t *)buf;
	rdlen = maxlen;
	ret = sr_tcp_read_bytes(tcp->tcp_dev, rdptr, rdlen, FALSE);
	if (ret < 0) {
		sr_err("Receive error: %s", g_strerror(errno));
		return SR_ERR;
	}

	/* Raw mode: assume short reads indicate end of response */
	tcp->length_bytes_read = LENGTH_BYTES;
	tcp->response_length = ret < rdlen ? ret : rdlen + 1;
	tcp->response_bytes_read = ret;

	return ret;
}

/* Raw TCP mode: write data */
static int scpi_tcp_raw_write_data(void *priv, char *buf, int len)
{
	struct scpi_tcp *tcp = priv;
	const uint8_t *wrptr;
	int ret;

	wrptr = (const uint8_t *)buf;
	ret = sr_tcp_write_bytes(tcp->tcp_dev, wrptr, len);
	if (ret < 0)
		return SR_ERR;

	return ret;
}

/* Rigol TCP mode: read with length prefix */
static int scpi_tcp_rigol_read_data(void *priv, char *buf, int maxlen)
{
	struct scpi_tcp *tcp = priv;
	uint8_t *rdptr;
	size_t rdlen;
	int ret;

	/* Read length bytes first */
	if (tcp->length_bytes_read < sizeof(tcp->length_buf)) {
		rdptr = &tcp->length_buf[tcp->length_bytes_read];
		rdlen = sizeof(tcp->length_buf) - tcp->length_bytes_read;
		ret = sr_tcp_read_bytes(tcp->tcp_dev, rdptr, rdlen, FALSE);
		if (ret < 0)
			return SR_ERR;
		tcp->length_bytes_read += ret;
		if (tcp->length_bytes_read < sizeof(tcp->length_buf))
			return 0;

		/* Parse length from buffer (little endian) */
		tcp->response_length = RL32(tcp->length_buf);
	}

	if (tcp->response_bytes_read >= tcp->response_length)
		return SR_ERR;

	rdptr = (uint8_t *)buf;
	rdlen = maxlen;
	ret = sr_tcp_read_bytes(tcp->tcp_dev, rdptr, rdlen, FALSE);
	if (ret < 0)
		return SR_ERR;
	tcp->response_bytes_read += ret;

	return ret;
}

static int scpi_tcp_read_complete(void *priv)
{
	struct scpi_tcp *tcp = priv;
	gboolean have_length, have_response;

	have_length = tcp->length_bytes_read == LENGTH_BYTES;
	have_response = tcp->response_bytes_read >= tcp->response_length;

	return have_length && have_response;
}

static int scpi_tcp_close(struct sr_scpi_dev_inst *scpi)
{
	struct scpi_tcp *tcp = scpi->priv;

	return sr_tcp_disconnect(tcp->tcp_dev);
}

static void scpi_tcp_free(void *priv)
{
	struct scpi_tcp *tcp = priv;

	sr_tcp_dev_inst_free(tcp->tcp_dev);
}

/* TCP Raw device declaration */
const struct sr_scpi_dev_inst scpi_tcp_raw_dev = {
	.name          = "RAW TCP",
	.prefix        = "tcp-raw",
	.transport     = SCPI_TRANSPORT_RAW_TCP,
	.priv_size     = sizeof(struct scpi_tcp),
	.dev_inst_new  = scpi_tcp_dev_inst_new,
	.open          = scpi_tcp_open,
	.connection_id = scpi_tcp_connection_id,
	.source_add    = scpi_tcp_source_add,
	.source_remove = scpi_tcp_source_remove,
	.send          = scpi_tcp_send,
	.read_begin    = scpi_tcp_read_begin,
	.read_data     = scpi_tcp_raw_read_data,
	.write_data    = scpi_tcp_raw_write_data,
	.read_complete = scpi_tcp_read_complete,
	.close         = scpi_tcp_close,
	.free          = scpi_tcp_free,
};

/* TCP Rigol device declaration */
const struct sr_scpi_dev_inst scpi_tcp_rigol_dev = {
	.name          = "RIGOL TCP",
	.prefix        = "tcp-rigol",
	.transport     = SCPI_TRANSPORT_RIGOL_TCP,
	.priv_size     = sizeof(struct scpi_tcp),
	.dev_inst_new  = scpi_tcp_dev_inst_new,
	.open          = scpi_tcp_open,
	.connection_id = scpi_tcp_connection_id,
	.source_add    = scpi_tcp_source_add,
	.source_remove = scpi_tcp_source_remove,
	.send          = scpi_tcp_send,
	.read_begin    = scpi_tcp_read_begin,
	.read_data     = scpi_tcp_rigol_read_data,
	.read_complete = scpi_tcp_read_complete,
	.close         = scpi_tcp_close,
	.free          = scpi_tcp_free,
};

/*===========================================================================
 * USBTMC Transport Layer Implementation (via libusb)
 *===========================================================================*/

#ifdef HAVE_LIBUSB_1_0

#define MAX_TRANSFER_LENGTH 2048
#define TRANSFER_TIMEOUT 1000

#define SUBCLASS_USBTMC 0x03
#define USBTMC_USB488   0x01

/* USBTMC control requests */
enum {
	INITIATE_ABORT_BULK_OUT     =   1,
	CHECK_ABORT_BULK_OUT_STATUS =   2,
	INITIATE_ABORT_BULK_IN      =   3,
	CHECK_ABORT_BULK_IN_STATUS  =   4,
	INITIATE_CLEAR              =   5,
	CHECK_CLEAR_STATUS          =   6,
	GET_CAPABILITIES            =   7,
	INDICATOR_PULSE             =  64,
	READ_STATUS_BYTE            = 128,
	REN_CONTROL                 = 160,
	GO_TO_LOCAL                 = 161,
	LOCAL_LOCKOUT               = 162,
};

/* USBTMC status codes */
#define USBTMC_STATUS_SUCCESS      0x01

/* USBTMC capabilities */
#define USBTMC_INT_CAP_LISTEN_ONLY 0x01
#define USBTMC_INT_CAP_TALK_ONLY   0x02
#define USBTMC_INT_CAP_INDICATOR   0x04

#define USBTMC_DEV_CAP_TERMCHAR    0x01

#define USB488_DEV_CAP_DT1         0x01
#define USB488_DEV_CAP_RL1         0x02
#define USB488_DEV_CAP_SR1         0x04
#define USB488_DEV_CAP_SCPI        0x08

/* Bulk messages constants */
#define USBTMC_BULK_HEADER_SIZE 12

/* Bulk MsgID values */
#define DEV_DEP_MSG_OUT        1
#define REQUEST_DEV_DEP_MSG_IN 2
#define DEV_DEP_MSG_IN         2

/* bmTransferAttributes */
#define EOM               0x01
#define TERM_CHAR_ENABLED 0x02

struct scpi_usbtmc_libusb {
	struct sr_context *ctx;
	struct sr_usb_dev_inst *usb;
	int detached_kernel_driver;
	uint8_t interface;
	uint8_t bulk_in_ep;
	uint8_t bulk_out_ep;
	uint8_t interrupt_ep;
	uint8_t usbtmc_int_cap;
	uint8_t usbtmc_dev_cap;
	uint8_t usb488_dev_cap;
	uint8_t bTag;
	uint8_t bulkin_attributes;
	uint8_t buffer[MAX_TRANSFER_LENGTH];
	int response_length;
	int response_bytes_read;
	int remaining_length;
};

static GSList *scpi_usbtmc_libusb_scan(struct drv_context *drvc)
{
	struct libusb_device **devlist;
	struct libusb_device_descriptor des;
	struct libusb_config_descriptor *confdes;
	const struct libusb_interface_descriptor *intfdes;
	GSList *resources = NULL;
	int confidx, intfidx, ret, i;
	char *res;

	if (!drvc || !drvc->sr_ctx || !drvc->sr_ctx->libusb_ctx)
		return NULL;

	ret = libusb_get_device_list(drvc->sr_ctx->libusb_ctx, &devlist);
	if (ret < 0) {
		sr_err("Failed to get USB device list: %s.",
		       libusb_error_name(ret));
		return NULL;
	}

	for (i = 0; devlist[i]; i++) {
		libusb_get_device_descriptor(devlist[i], &des);

		for (confidx = 0; confidx < des.bNumConfigurations; confidx++) {
			ret = libusb_get_config_descriptor(devlist[i], confidx, &confdes);
			if (ret < 0)
				continue;

			for (intfidx = 0; intfidx < confdes->bNumInterfaces; intfidx++) {
				intfdes = confdes->interface[intfidx].altsetting;
				if (intfdes->bInterfaceClass    != LIBUSB_CLASS_APPLICATION ||
				    intfdes->bInterfaceSubClass != SUBCLASS_USBTMC ||
				    intfdes->bInterfaceProtocol != USBTMC_USB488)
					continue;

				sr_dbg("Found USBTMC device (VID:PID = %04x:%04x).",
				       des.idVendor, des.idProduct);
				res = g_strdup_printf("usbtmc/%d.%d",
				                      libusb_get_bus_number(devlist[i]),
				                      libusb_get_device_address(devlist[i]));
				resources = g_slist_append(resources, res);
			}
			libusb_free_config_descriptor(confdes);
		}
	}
	libusb_free_device_list(devlist, 1);

	return resources;
}

static int scpi_usbtmc_libusb_dev_inst_new(void *priv, struct drv_context *drvc,
		const char *resource, char **params, const char *serialcomm)
{
	struct scpi_usbtmc_libusb *uscpi = priv;
	GSList *devices;

	(void)resource;
	(void)serialcomm;

	if (!params || !params[1]) {
		sr_err("Invalid USBTMC parameters.");
		return SR_ERR;
	}

	uscpi->ctx = drvc->sr_ctx;
	devices = sr_usb_find(uscpi->ctx->libusb_ctx, params[1]);
	if (g_slist_length(devices) != 1) {
		sr_err("Failed to find USB device '%s'.", params[1]);
		g_slist_free_full(devices, (GDestroyNotify)sr_usb_dev_inst_free);
		return SR_ERR;
	}
	uscpi->usb = devices->data;
	g_slist_free(devices);

	return SR_OK;
}

static void usbtmc_bulk_out_header_write(void *header, uint8_t MsgID,
                                         uint8_t bTag,
                                         uint32_t TransferSize,
                                         uint8_t bmTransferAttributes,
                                         char TermChar)
{
	W8(header +  0, MsgID);
	W8(header +  1, bTag);
	W8(header +  2, ~bTag);
	W8(header +  3, 0);
	WL32(header +  4, TransferSize);
	W8(header +  8, bmTransferAttributes);
	W8(header +  9, TermChar);
	WL16(header + 10, 0);
}

static int usbtmc_bulk_in_header_read(void *header, uint8_t MsgID,
                                      unsigned char bTag,
                                      int32_t *TransferSize,
                                      uint8_t *bmTransferAttributes)
{
	if (R8(header + 0) != MsgID ||
	    R8(header + 1) != bTag ||
	    R8(header + 2) != (unsigned char)~bTag)
		return SR_ERR;
	if (TransferSize)
		*TransferSize = RL32(header + 4);
	if (bmTransferAttributes)
		*bmTransferAttributes = R8(header + 8);

	return SR_OK;
}

static int scpi_usbtmc_bulkout(struct scpi_usbtmc_libusb *uscpi,
                               uint8_t msg_id, const void *data, int32_t size,
                               uint8_t transfer_attributes)
{
	struct sr_usb_dev_inst *usb = uscpi->usb;
	int padded_size, ret, transferred;

	if (data && (size + USBTMC_BULK_HEADER_SIZE + 3) > (int)sizeof(uscpi->buffer)) {
		sr_err("USBTMC bulk out transfer is too big.");
		return SR_ERR;
	}

	uscpi->bTag++;
	uscpi->bTag += !uscpi->bTag; /* bTag == 0 is invalid */

	usbtmc_bulk_out_header_write(uscpi->buffer, msg_id, uscpi->bTag,
	                             size, transfer_attributes, 0);
	if (data)
		memcpy(uscpi->buffer + USBTMC_BULK_HEADER_SIZE, data, size);
	else
		size = 0;
	size += USBTMC_BULK_HEADER_SIZE;
	padded_size = (size + 3) & ~0x3;
	memset(uscpi->buffer + size, 0, padded_size - size);

	ret = libusb_bulk_transfer(usb->devhdl, uscpi->bulk_out_ep,
	                           uscpi->buffer, padded_size, &transferred,
	                           TRANSFER_TIMEOUT);
	if (ret < 0) {
		sr_err("USBTMC bulk out error: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	if (transferred < padded_size) {
		sr_dbg("USBTMC partial transfer (%d/%d).", transferred, padded_size);
		return SR_ERR;
	}

	return transferred - USBTMC_BULK_HEADER_SIZE;
}

static int scpi_usbtmc_bulkin_start(struct scpi_usbtmc_libusb *uscpi,
                                    uint8_t msg_id, void *data, int32_t size,
                                    uint8_t *transfer_attributes)
{
	struct sr_usb_dev_inst *usb = uscpi->usb;
	int ret, transferred, message_size;

	ret = libusb_bulk_transfer(usb->devhdl, uscpi->bulk_in_ep, data,
				   size, &transferred, TRANSFER_TIMEOUT);
	if (ret < 0) {
		sr_err("USBTMC bulk in error: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	if (transferred < USBTMC_BULK_HEADER_SIZE)
		return SR_ERR;

	if (usbtmc_bulk_in_header_read(data, msg_id, uscpi->bTag, &message_size,
	                               transfer_attributes) != SR_OK) {
		sr_err("Invalid USBTMC bulk in header.");
		return SR_ERR;
	}

	message_size += USBTMC_BULK_HEADER_SIZE;
	uscpi->response_length = MIN(transferred, message_size);
	uscpi->response_bytes_read = USBTMC_BULK_HEADER_SIZE;
	uscpi->remaining_length = message_size - uscpi->response_length;

	return transferred - USBTMC_BULK_HEADER_SIZE;
}

static int scpi_usbtmc_bulkin_continue(struct scpi_usbtmc_libusb *uscpi,
                                       void *data, int size)
{
	struct sr_usb_dev_inst *usb = uscpi->usb;
	int ret, transferred;

	ret = libusb_bulk_transfer(usb->devhdl, uscpi->bulk_in_ep, data, size,
	                           &transferred, TRANSFER_TIMEOUT);
	if (ret < 0) {
		sr_err("USBTMC bulk in continue error: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	uscpi->response_length = MIN(transferred, uscpi->remaining_length);
	uscpi->response_bytes_read = 0;
	uscpi->remaining_length -= uscpi->response_length;

	return transferred;
}

static int scpi_usbtmc_libusb_send(void *priv, const char *command)
{
	struct scpi_usbtmc_libusb *uscpi = priv;

	if (scpi_usbtmc_bulkout(uscpi, DEV_DEP_MSG_OUT,
	                        command, strlen(command), EOM) <= 0)
		return SR_ERR;

	sr_spew("Successfully sent SCPI command: '%s'.", command);

	return SR_OK;
}

static int scpi_usbtmc_libusb_read_begin(void *priv)
{
	struct scpi_usbtmc_libusb *uscpi = priv;

	uscpi->remaining_length = 0;

	if (scpi_usbtmc_bulkout(uscpi, REQUEST_DEV_DEP_MSG_IN,
	    NULL, INT32_MAX, 0) < 0)
		return SR_ERR;
	if (scpi_usbtmc_bulkin_start(uscpi, DEV_DEP_MSG_IN,
	                             uscpi->buffer, sizeof(uscpi->buffer),
	                             &uscpi->bulkin_attributes) < 0)
		return SR_ERR;

	return SR_OK;
}

static int scpi_usbtmc_libusb_read_data(void *priv, char *buf, int maxlen)
{
	struct scpi_usbtmc_libusb *uscpi = priv;
	int read_length;

	if (uscpi->response_bytes_read >= uscpi->response_length) {
		if (uscpi->remaining_length > 0) {
			if (scpi_usbtmc_bulkin_continue(uscpi, uscpi->buffer,
			                                sizeof(uscpi->buffer)) <= 0)
				return SR_ERR;
		} else {
			if (uscpi->bulkin_attributes & EOM)
				return SR_ERR;
			if (scpi_usbtmc_libusb_read_begin(uscpi) < 0)
				return SR_ERR;
		}
	}

	read_length = MIN(uscpi->response_length - uscpi->response_bytes_read, maxlen);
	memcpy(buf, uscpi->buffer + uscpi->response_bytes_read, read_length);
	uscpi->response_bytes_read += read_length;

	return read_length;
}

static int scpi_usbtmc_libusb_read_complete(void *priv)
{
	struct scpi_usbtmc_libusb *uscpi = priv;

	return uscpi->response_bytes_read >= uscpi->response_length &&
	       uscpi->remaining_length <= 0 &&
	       uscpi->bulkin_attributes & EOM;
}

static int scpi_usbtmc_libusb_open(struct sr_scpi_dev_inst *scpi)
{
	struct scpi_usbtmc_libusb *uscpi = scpi->priv;
	struct sr_usb_dev_inst *usb = uscpi->usb;
	struct libusb_device *dev;
	struct libusb_device_descriptor des;
	struct libusb_config_descriptor *confdes;
	const struct libusb_interface_descriptor *intfdes;
	const struct libusb_endpoint_descriptor *ep;
	int confidx, intfidx, epidx, config = 0, current_config;
	uint8_t capabilities[24];
	int ret, found = 0;

	if (usb->devhdl)
		return SR_OK;

	if (sr_usb_open(uscpi->ctx->libusb_ctx, usb) != SR_OK)
		return SR_ERR;

	dev = libusb_get_device(usb->devhdl);
	libusb_get_device_descriptor(dev, &des);

	for (confidx = 0; confidx < des.bNumConfigurations; confidx++) {
		ret = libusb_get_config_descriptor(dev, confidx, &confdes);
		if (ret < 0)
			continue;

		for (intfidx = 0; intfidx < confdes->bNumInterfaces; intfidx++) {
			intfdes = confdes->interface[intfidx].altsetting;
			if (intfdes->bInterfaceClass    != LIBUSB_CLASS_APPLICATION ||
			    intfdes->bInterfaceSubClass != SUBCLASS_USBTMC ||
			    intfdes->bInterfaceProtocol != USBTMC_USB488)
				continue;

			uscpi->interface = intfdes->bInterfaceNumber;
			config = confdes->bConfigurationValue;
			sr_dbg("Interface %d configuration %d.", uscpi->interface, config);

			for (epidx = 0; epidx < intfdes->bNumEndpoints; epidx++) {
				ep = &intfdes->endpoint[epidx];
				if (ep->bmAttributes == LIBUSB_TRANSFER_TYPE_BULK &&
				    !(ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)) {
					uscpi->bulk_out_ep = ep->bEndpointAddress;
					sr_dbg("Bulk OUT EP %d", uscpi->bulk_out_ep);
				}
				if (ep->bmAttributes == LIBUSB_TRANSFER_TYPE_BULK &&
				    ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) {
					uscpi->bulk_in_ep = ep->bEndpointAddress;
					sr_dbg("Bulk IN EP %d", uscpi->bulk_in_ep & 0x7f);
				}
				if (ep->bmAttributes == LIBUSB_TRANSFER_TYPE_INTERRUPT &&
				    ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) {
					uscpi->interrupt_ep = ep->bEndpointAddress;
					sr_dbg("Interrupt EP %d", uscpi->interrupt_ep & 0x7f);
				}
			}
			found = 1;
		}
		libusb_free_config_descriptor(confdes);
		if (found)
			break;
	}

	if (!found) {
		sr_err("Failed to find USBTMC interface.");
		return SR_ERR;
	}

	if (libusb_kernel_driver_active(usb->devhdl, uscpi->interface) == 1) {
		ret = libusb_detach_kernel_driver(usb->devhdl, uscpi->interface);
		if (ret < 0) {
			sr_err("Failed to detach kernel driver: %s.", libusb_error_name(ret));
			return SR_ERR;
		}
		uscpi->detached_kernel_driver = 1;
	}

	libusb_get_configuration(usb->devhdl, &current_config);
	if (current_config != config) {
		ret = libusb_set_configuration(usb->devhdl, config);
		if (ret < 0) {
			sr_err("Failed to set configuration: %s.", libusb_error_name(ret));
			return SR_ERR;
		}
	}

	ret = libusb_claim_interface(usb->devhdl, uscpi->interface);
	if (ret < 0) {
		sr_err("Failed to claim interface: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	/* Get capabilities */
	ret = libusb_control_transfer(usb->devhdl, LIBUSB_ENDPOINT_IN |
		LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		GET_CAPABILITIES, 0, uscpi->interface, capabilities,
		sizeof(capabilities), TRANSFER_TIMEOUT);
	if (ret == sizeof(capabilities)) {
		uscpi->usbtmc_int_cap = capabilities[4];
		uscpi->usbtmc_dev_cap = capabilities[5];
		uscpi->usb488_dev_cap = capabilities[15];
	}
	sr_dbg("Device capabilities: %s%s%s",
	       uscpi->usb488_dev_cap & USB488_DEV_CAP_SCPI ? "SCPI" : "",
	       uscpi->usb488_dev_cap & USB488_DEV_CAP_RL1 ? ", RL1" : "",
	       uscpi->usb488_dev_cap & USB488_DEV_CAP_DT1 ? ", DT1" : "");

	return SR_OK;
}

static int scpi_usbtmc_libusb_connection_id(struct sr_scpi_dev_inst *scpi,
		char **connection_id)
{
	struct scpi_usbtmc_libusb *uscpi = scpi->priv;
	struct sr_usb_dev_inst *usb = uscpi->usb;

	*connection_id = g_strdup_printf("%s/%" PRIu8 ".%" PRIu8,
		scpi->prefix, usb->bus, usb->address);

	return SR_OK;
}

static int scpi_usbtmc_libusb_source_add(struct sr_session *session,
		void *priv, int events, int timeout, sr_receive_data_callback_t cb,
		void *cb_data)
{
	struct scpi_usbtmc_libusb *uscpi = priv;
	(void)events;
	return compat_usb_source_add(uscpi->ctx, timeout, cb, cb_data);
}

static int scpi_usbtmc_libusb_source_remove(struct sr_session *session,
		void *priv)
{
	(void)session;
	(void)priv;
	return SR_OK;
}

static int scpi_usbtmc_libusb_close(struct sr_scpi_dev_inst *scpi)
{
	struct scpi_usbtmc_libusb *uscpi = scpi->priv;
	struct sr_usb_dev_inst *usb = uscpi->usb;
	int ret;

	if (!usb->devhdl)
		return SR_ERR;

	ret = libusb_release_interface(usb->devhdl, uscpi->interface);
	if (ret < 0)
		sr_err("Failed to release interface: %s.", libusb_error_name(ret));

	if (uscpi->detached_kernel_driver) {
		ret = libusb_attach_kernel_driver(usb->devhdl, uscpi->interface);
		if (ret < 0)
			sr_err("Failed to re-attach kernel driver: %s.", libusb_error_name(ret));
		uscpi->detached_kernel_driver = 0;
	}
	sr_usb_close(usb);

	return SR_OK;
}

static void scpi_usbtmc_libusb_free(void *priv)
{
	struct scpi_usbtmc_libusb *uscpi = priv;
	sr_usb_dev_inst_free(uscpi->usb);
}

const struct sr_scpi_dev_inst scpi_usbtmc_libusb_dev = {
	.name          = "USBTMC",
	.prefix        = "usbtmc",
	.transport     = SCPI_TRANSPORT_USBTMC,
	.priv_size     = sizeof(struct scpi_usbtmc_libusb),
	.scan          = scpi_usbtmc_libusb_scan,
	.dev_inst_new  = scpi_usbtmc_libusb_dev_inst_new,
	.open          = scpi_usbtmc_libusb_open,
	.connection_id = scpi_usbtmc_libusb_connection_id,
	.source_add    = scpi_usbtmc_libusb_source_add,
	.source_remove = scpi_usbtmc_libusb_source_remove,
	.send          = scpi_usbtmc_libusb_send,
	.read_begin    = scpi_usbtmc_libusb_read_begin,
	.read_data     = scpi_usbtmc_libusb_read_data,
	.read_complete = scpi_usbtmc_libusb_read_complete,
	.close         = scpi_usbtmc_libusb_close,
	.free          = scpi_usbtmc_libusb_free,
};

#else /* HAVE_LIBUSB_1_0 */

/* Stub implementation when libusb is not available */
const struct sr_scpi_dev_inst scpi_usbtmc_libusb_dev = {
	.name          = "USBTMC",
	.prefix        = "usbtmc",
	.transport     = SCPI_TRANSPORT_USBTMC,
	.priv_size     = 0,
	.dev_inst_new  = NULL,
	.open          = NULL,
	.free          = NULL,
};

#endif /* HAVE_LIBUSB_1_0 */

/*===========================================================================
 * Serial SCPI Transport Layer Implementation
 *===========================================================================*/

#ifdef HAVE_SERIAL_COMM

struct scpi_serial {
	struct sr_serial_dev_inst *serial;
	gboolean got_newline;
};

static GSList *scpi_serial_scan(struct drv_context *drvc)
{
	(void)drvc;
	/* Serial scan requires user-specified connection */
	return NULL;
}

static int scpi_serial_dev_inst_new(void *priv, struct drv_context *drvc,
		const char *resource, char **params, const char *serialcomm)
{
	struct scpi_serial *sscpi = priv;

	(void)drvc;
	(void)params;

	sscpi->serial = sr_serial_dev_inst_new(resource, serialcomm);

	return SR_OK;
}

static int scpi_serial_open(struct sr_scpi_dev_inst *scpi)
{
	struct scpi_serial *sscpi = scpi->priv;
	struct sr_serial_dev_inst *serial = sscpi->serial;

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return SR_ERR;

	sscpi->got_newline = FALSE;

	return SR_OK;
}

static int scpi_serial_connection_id(struct sr_scpi_dev_inst *scpi,
		char **connection_id)
{
	struct scpi_serial *sscpi = scpi->priv;
	struct sr_serial_dev_inst *serial = sscpi->serial;

	*connection_id = g_strdup(serial->port);

	return SR_OK;
}

static int scpi_serial_source_add(struct sr_session *session, void *priv,
		int events, int timeout, sr_receive_data_callback_t cb, void *cb_data)
{
	struct scpi_serial *sscpi = priv;
	struct sr_serial_dev_inst *serial = sscpi->serial;

	return serial_source_add(serial, events, timeout, cb, cb_data);
}

static int scpi_serial_source_remove(struct sr_session *session, void *priv)
{
	struct scpi_serial *sscpi = priv;
	struct sr_serial_dev_inst *serial = sscpi->serial;

	return serial_source_remove(serial);
}

static int scpi_serial_send(void *priv, const char *command)
{
	struct scpi_serial *sscpi = priv;
	struct sr_serial_dev_inst *serial = sscpi->serial;
	int result;

	result = serial_write_blocking(serial, command, strlen(command), 0);
	if (result < 0) {
		sr_err("Error sending SCPI command '%s'.", command);
		return SR_ERR;
	}

	sr_spew("Successfully sent SCPI command: '%s'.", command);

	return SR_OK;
}

static int scpi_serial_read_begin(void *priv)
{
	struct scpi_serial *sscpi = priv;

	sscpi->got_newline = FALSE;

	return SR_OK;
}

static int scpi_serial_read_data(void *priv, char *buf, int maxlen)
{
	struct scpi_serial *sscpi = priv;
	int ret;

	ret = serial_read_nonblocking(sscpi->serial, buf, maxlen);
	if (ret < 0)
		return ret;

	/* Check for line termination */
	sscpi->got_newline = FALSE;
	if (ret >= 1 && buf[ret - 1] == '\n') {
		sscpi->got_newline = TRUE;
		sr_spew("Received NL terminator");
	} else if (ret >= 2 && buf[ret - 2] == '\n' && buf[ret - 1] == '\r') {
		ret--;
		sscpi->got_newline = TRUE;
		sr_spew("Received NL+CR terminator");
	}

	return ret;
}

static int scpi_serial_read_complete(void *priv)
{
	struct scpi_serial *sscpi = priv;

	return sscpi->got_newline;
}

static int scpi_serial_close(struct sr_scpi_dev_inst *scpi)
{
	struct scpi_serial *sscpi = scpi->priv;

	return serial_close(sscpi->serial);
}

static void scpi_serial_free(void *priv)
{
	struct scpi_serial *sscpi = priv;

	sr_serial_dev_inst_free(sscpi->serial);
}

const struct sr_scpi_dev_inst scpi_serial_dev = {
	.name          = "serial",
	.prefix        = "",
	.transport     = SCPI_TRANSPORT_SERIAL,
	.priv_size     = sizeof(struct scpi_serial),
	.scan          = scpi_serial_scan,
	.dev_inst_new  = scpi_serial_dev_inst_new,
	.open          = scpi_serial_open,
	.connection_id = scpi_serial_connection_id,
	.source_add    = scpi_serial_source_add,
	.source_remove = scpi_serial_source_remove,
	.send          = scpi_serial_send,
	.read_begin    = scpi_serial_read_begin,
	.read_data     = scpi_serial_read_data,
	.read_complete = scpi_serial_read_complete,
	.close         = scpi_serial_close,
	.free          = scpi_serial_free,
};

#else /* HAVE_SERIAL_COMM */

const struct sr_scpi_dev_inst scpi_serial_dev = {
	.name          = "serial",
	.prefix        = "",
	.transport     = SCPI_TRANSPORT_SERIAL,
	.priv_size     = 0,
	.dev_inst_new  = NULL,
	.open          = NULL,
	.free          = NULL,
};

#endif /* HAVE_SERIAL_COMM */

/*===========================================================================
 * SCPI Device List
 *===========================================================================*/

static const struct sr_scpi_dev_inst *scpi_devs[] = {
	&scpi_tcp_raw_dev,
	&scpi_tcp_rigol_dev,
#ifdef HAVE_LIBUSB_1_0
	&scpi_usbtmc_libusb_dev,
#endif
#ifdef HAVE_SERIAL_COMM
	&scpi_serial_dev, /* Must be last - matches any resource */
#endif
};

/*===========================================================================
 * TCP Transport Helper Functions
 *===========================================================================*/

SR_PRIV struct sr_tcp_dev_inst *sr_tcp_dev_inst_new(
		const char *host_addr, const char *tcp_port)
{
	struct sr_tcp_dev_inst *tcp;

	tcp = g_malloc0(sizeof(*tcp));
	if (!tcp)
		return NULL;

	tcp->host_addr = host_addr ? g_strdup(host_addr) : NULL;
	tcp->tcp_port = tcp_port ? g_strdup(tcp_port) : NULL;
	tcp->sock_fd = -1;

	return tcp;
}

SR_PRIV void sr_tcp_dev_inst_free(struct sr_tcp_dev_inst *tcp)
{
	if (!tcp)
		return;

	(void)sr_tcp_disconnect(tcp);
	g_free(tcp->host_addr);
	g_free(tcp->tcp_port);
	g_free(tcp);
}

SR_PRIV int sr_tcp_get_port_path(struct sr_tcp_dev_inst *tcp,
		const char *prefix, char separator, char *path, size_t path_len)
{
	char sep_text[2];

	if (!tcp || !tcp->host_addr || !tcp->tcp_port)
		return SR_ERR_ARG;

	if (!prefix)
		prefix = "";
	if (!*prefix && !separator)
		separator = ':';

	sep_text[0] = separator;
	sep_text[1] = '\0';

	snprintf(path, path_len, "%s%s%s%s%s",
		prefix, *prefix ? sep_text : "",
		tcp->host_addr, sep_text, tcp->tcp_port);

	return SR_OK;
}

SR_PRIV int sr_tcp_connect(struct sr_tcp_dev_inst *tcp)
{
	struct addrinfo hints;
	struct addrinfo *results, *r;
	int ret;
	int fd;

	if (!tcp || !tcp->host_addr || !tcp->tcp_port)
		return SR_ERR_ARG;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	ret = getaddrinfo(tcp->host_addr, tcp->tcp_port, &hints, &results);
	if (ret != 0) {
		sr_err("Address lookup failed: %s:%s.",
			tcp->host_addr, tcp->tcp_port);
		return SR_ERR;
	}

	fd = -1;
	for (r = results; r; r = r->ai_next) {
		fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (fd < 0)
			continue;
		ret = connect(fd, r->ai_addr, r->ai_addrlen);
		if (ret != 0) {
			close(fd);
			fd = -1;
			continue;
		}
		break;
	}
	freeaddrinfo(results);

	if (fd < 0) {
		sr_err("Failed to connect to %s:%s.",
			tcp->host_addr, tcp->tcp_port);
		return SR_ERR;
	}

	tcp->sock_fd = fd;
	return SR_OK;
}

SR_PRIV int sr_tcp_disconnect(struct sr_tcp_dev_inst *tcp)
{
	if (!tcp)
		return SR_ERR_ARG;

	if (tcp->sock_fd < 0)
		return SR_OK;

	shutdown(tcp->sock_fd, SHUT_RDWR);
	close(tcp->sock_fd);
	tcp->sock_fd = -1;

	return SR_OK;
}

SR_PRIV int sr_tcp_write_bytes(struct sr_tcp_dev_inst *tcp,
		const uint8_t *data, size_t dlen)
{
	ssize_t rc;

	if (!tcp || !data)
		return SR_ERR_ARG;
	if (tcp->sock_fd < 0)
		return SR_ERR_IO;

	rc = send(tcp->sock_fd, data, dlen, 0);
	if (rc < 0)
		return SR_ERR_IO;

	return (int)rc;
}

SR_PRIV int sr_tcp_read_bytes(struct sr_tcp_dev_inst *tcp,
		uint8_t *data, size_t dlen, gboolean nonblocking)
{
	ssize_t rc;

	if (!tcp || !data)
		return SR_ERR_ARG;
	if (tcp->sock_fd < 0)
		return SR_ERR_IO;

	rc = recv(tcp->sock_fd, data, dlen, 0);
	if (rc < 0)
		return SR_ERR_IO;

	return (int)rc;
}

SR_PRIV int sr_tcp_source_add(struct sr_session *session,
		struct sr_tcp_dev_inst *tcp, int events, int timeout,
		sr_receive_data_callback_t cb, void *cb_data)
{
	if (!tcp || tcp->sock_fd < 0)
		return SR_ERR_ARG;

	return sr_session_source_add(session, tcp->sock_fd,
		events, timeout, cb, cb_data);
}

SR_PRIV int sr_tcp_source_remove(struct sr_session *session,
		struct sr_tcp_dev_inst *tcp)
{
	if (!tcp || tcp->sock_fd < 0)
		return SR_ERR_ARG;

	return sr_session_source_remove(session, tcp->sock_fd);
}

/*===========================================================================
 * SCPI Core Functions Implementation
 *===========================================================================*/

static int scpi_send_variadic(struct sr_scpi_dev_inst *scpi,
			 const char *format, va_list args)
{
	va_list args_copy;
	char *buf;
	int len, ret;

	va_copy(args_copy, args);
	len = g_vsnprintf(NULL, 0, format, args_copy);
	va_end(args_copy);

	buf = g_malloc0(len + 2);
	g_vsprintf(buf, format, args);
	if (buf[len - 1] != '\n')
		buf[len] = '\n';

	ret = scpi->send(scpi->priv, buf);

	g_free(buf);

	return ret;
}

SR_PRIV struct sr_scpi_dev_inst *scpi_dev_inst_new(struct drv_context *drvc,
		const char *resource, const char *serialcomm)
{
	struct sr_scpi_dev_inst *scpi = NULL;
	const struct sr_scpi_dev_inst *scpi_dev;
	gchar **params;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(scpi_devs); i++) {
		scpi_dev = scpi_devs[i];
		if (!strncmp(resource, scpi_dev->prefix, strlen(scpi_dev->prefix))) {
			sr_dbg("Opening %s device %s.", scpi_dev->name, resource);
			scpi = g_malloc(sizeof(*scpi));
			*scpi = *scpi_dev;
			scpi->priv = g_malloc0(scpi->priv_size);
			scpi->read_timeout_us = 1000 * 1000;
			params = g_strsplit(resource, "/", 0);
			if (scpi->dev_inst_new(scpi->priv, drvc, resource,
			                       params, serialcomm) != SR_OK) {
				sr_scpi_free(scpi);
				scpi = NULL;
			}
			g_strfreev(params);
			break;
		}
	}

	return scpi;
}

SR_PRIV int sr_scpi_open(struct sr_scpi_dev_inst *scpi)
{
	g_mutex_init(&scpi->scpi_mutex);

	return scpi->open(scpi);
}

SR_PRIV int sr_scpi_connection_id(struct sr_scpi_dev_inst *scpi,
		char **connection_id)
{
	return scpi->connection_id(scpi, connection_id);
}

SR_PRIV int sr_scpi_source_add(struct sr_session *session,
		struct sr_scpi_dev_inst *scpi, int events, int timeout,
		sr_receive_data_callback_t cb, void *cb_data)
{
	return scpi->source_add(session, scpi->priv, events, timeout, cb, cb_data);
}

SR_PRIV int sr_scpi_source_remove(struct sr_session *session,
		struct sr_scpi_dev_inst *scpi)
{
	return scpi->source_remove(session, scpi->priv);
}

SR_PRIV int sr_scpi_send(struct sr_scpi_dev_inst *scpi,
			 const char *format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);
	g_mutex_lock(&scpi->scpi_mutex);
	ret = scpi_send_variadic(scpi, format, args);
	g_mutex_unlock(&scpi->scpi_mutex);
	va_end(args);

	return ret;
}

SR_PRIV int sr_scpi_send_variadic(struct sr_scpi_dev_inst *scpi,
			 const char *format, va_list args)
{
	int ret;

	g_mutex_lock(&scpi->scpi_mutex);
	ret = scpi_send_variadic(scpi, format, args);
	g_mutex_unlock(&scpi->scpi_mutex);

	return ret;
}

SR_PRIV int sr_scpi_read_begin(struct sr_scpi_dev_inst *scpi)
{
	return scpi->read_begin(scpi->priv);
}

SR_PRIV int sr_scpi_read_data(struct sr_scpi_dev_inst *scpi,
			char *buf, int maxlen)
{
	int ret;

	g_mutex_lock(&scpi->scpi_mutex);
	ret = scpi->read_data(scpi->priv, buf, maxlen);
	g_mutex_unlock(&scpi->scpi_mutex);

	return ret;
}

SR_PRIV int sr_scpi_write_data(struct sr_scpi_dev_inst *scpi,
			char *buf, int len)
{
	int ret;

	g_mutex_lock(&scpi->scpi_mutex);
	if (scpi->write_data)
		ret = scpi->write_data(scpi->priv, buf, len);
	else
		ret = SR_ERR_NA;
	g_mutex_unlock(&scpi->scpi_mutex);

	return ret;
}

SR_PRIV int sr_scpi_read_complete(struct sr_scpi_dev_inst *scpi)
{
	return scpi->read_complete(scpi->priv);
}

SR_PRIV int sr_scpi_close(struct sr_scpi_dev_inst *scpi)
{
	int ret;

	g_mutex_lock(&scpi->scpi_mutex);
	ret = scpi->close(scpi);
	g_mutex_unlock(&scpi->scpi_mutex);
	g_mutex_clear(&scpi->scpi_mutex);

	return ret;
}

SR_PRIV void sr_scpi_free(struct sr_scpi_dev_inst *scpi)
{
	if (!scpi)
		return;

	if (scpi->free)
		scpi->free(scpi->priv);
	g_free(scpi->priv);
	g_free(scpi->actual_channel_name);
	g_free(scpi);
}

static int scpi_read_response(struct sr_scpi_dev_inst *scpi,
				GString *response, gint64 abs_timeout_us)
{
	int len, space;

	space = response->allocated_len - response->len;
	len = scpi->read_data(scpi->priv, &response->str[response->len], space);

	if (len < 0) {
		sr_err("Incomplete SCPI response read.");
		return SR_ERR;
	}

	if (len > 0) {
		g_string_set_size(response, response->len + len);
		return len;
	}

	if (g_get_monotonic_time() > abs_timeout_us) {
		sr_err("Timeout waiting for SCPI response.");
		return SR_ERR_TIMEOUT;
	}

	return 0;
}

SR_PRIV int sr_scpi_read_response(struct sr_scpi_dev_inst *scpi,
				  GString *response, gint64 abs_timeout_us)
{
	int ret;

	g_mutex_lock(&scpi->scpi_mutex);
	ret = scpi_read_response(scpi, response, abs_timeout_us);
	g_mutex_unlock(&scpi->scpi_mutex);

	return ret;
}

static int scpi_get_data(struct sr_scpi_dev_inst *scpi,
				const char *command, GString **scpi_response)
{
	int ret;
	GString *response;
	int space;
	gint64 timeout;

	if (command) {
		if (scpi_send_variadic(scpi, command, NULL) != SR_OK)
			return SR_ERR;
	}

	if (sr_scpi_read_begin(scpi) != SR_OK)
		return SR_ERR;

	timeout = g_get_monotonic_time() + scpi->read_timeout_us;

	response = *scpi_response;

	while (!sr_scpi_read_complete(scpi)) {
		space = response->allocated_len - response->len;
		if (space < 128) {
			int oldlen = response->len;
			g_string_set_size(response, oldlen + 1024);
			g_string_set_size(response, oldlen);
		}

		ret = scpi_read_response(scpi, response, timeout);

		if (ret < 0)
			return ret;
		if (ret > 0)
			timeout = g_get_monotonic_time() + scpi->read_timeout_us;
	}

	return SR_OK;
}

SR_PRIV int sr_scpi_get_data(struct sr_scpi_dev_inst *scpi,
			     const char *command, GString **scpi_response)
{
	int ret;

	g_mutex_lock(&scpi->scpi_mutex);
	ret = scpi_get_data(scpi, command, scpi_response);
	g_mutex_unlock(&scpi->scpi_mutex);

	return ret;
}

SR_PRIV int sr_scpi_get_string(struct sr_scpi_dev_inst *scpi,
			       const char *command, char **scpi_response)
{
	GString *response;

	*scpi_response = NULL;

	response = g_string_sized_new(1024);
	if (sr_scpi_get_data(scpi, command, &response) != SR_OK) {
		if (response)
			g_string_free(response, TRUE);
		return SR_ERR;
	}

	/* Strip trailing newline and carriage return */
	if (response->len >= 1 && response->str[response->len - 1] == '\n')
		g_string_truncate(response, response->len - 1);
	if (response->len >= 1 && response->str[response->len - 1] == '\r')
		g_string_truncate(response, response->len - 1);

	sr_spew("Got response: '%.70s', length %zu.",
		response->str, response->len);

	*scpi_response = g_string_free(response, FALSE);

	return SR_OK;
}

SR_PRIV int sr_scpi_get_bool(struct sr_scpi_dev_inst *scpi,
			     const char *command, gboolean *scpi_response)
{
	int ret;
	char *response;

	response = NULL;

	ret = sr_scpi_get_string(scpi, command, &response);
	if (ret != SR_OK && !response)
		return ret;

	if (parse_strict_bool(response, scpi_response) == SR_OK)
		ret = SR_OK;
	else
		ret = SR_ERR_DATA;

	g_free(response);

	return ret;
}

SR_PRIV int sr_scpi_get_int(struct sr_scpi_dev_inst *scpi,
			    const char *command, int *scpi_response)
{
	char *response;
	long val;
	int ret;

	response = NULL;

	ret = sr_scpi_get_string(scpi, command, &response);
	if (ret != SR_OK && !response)
		return ret;

	val = strtol(response, NULL, 10);
	*scpi_response = (int)val;
	ret = SR_OK;

	g_free(response);

	return ret;
}

SR_PRIV int sr_scpi_get_float(struct sr_scpi_dev_inst *scpi,
			      const char *command, float *scpi_response)
{
	char *response;
	int ret;

	response = NULL;

	ret = sr_scpi_get_string(scpi, command, &response);
	if (ret != SR_OK && !response)
		return ret;

	*scpi_response = (float)g_ascii_strtod(response, NULL);
	ret = SR_OK;

	g_free(response);

	return ret;
}

SR_PRIV int sr_scpi_get_double(struct sr_scpi_dev_inst *scpi,
			       const char *command, double *scpi_response)
{
	char *response;
	int ret;

	response = NULL;

	ret = sr_scpi_get_string(scpi, command, &response);
	if (ret != SR_OK && !response)
		return ret;

	*scpi_response = g_ascii_strtod(response, NULL);
	ret = SR_OK;

	g_free(response);

	return ret;
}

SR_PRIV int sr_scpi_get_opc(struct sr_scpi_dev_inst *scpi)
{
	unsigned int i;
	gboolean opc;

	for (i = 0; i < SCPI_READ_RETRIES; i++) {
		opc = FALSE;
		sr_scpi_get_bool(scpi, SCPI_CMD_OPC, &opc);
		if (opc)
			return SR_OK;
		g_usleep(SCPI_READ_RETRY_TIMEOUT_US);
	}

	return SR_ERR;
}

SR_PRIV int sr_scpi_get_floatv(struct sr_scpi_dev_inst *scpi,
			       const char *command, GArray **scpi_response)
{
	char *response;
	gchar **tokens;
	GArray *response_array;
	float tmp;
	int ret;
	size_t i;

	*scpi_response = NULL;
	response = NULL;

	ret = sr_scpi_get_string(scpi, command, &response);
	if (ret != SR_OK && !response)
		return ret;

	tokens = g_strsplit(response, ",", 0);
	response_array = g_array_new(TRUE, FALSE, sizeof(float));

	for (i = 0; tokens[i]; i++) {
		tmp = (float)g_ascii_strtod(tokens[i], NULL);
		g_array_append_val(response_array, tmp);
	}

	g_strfreev(tokens);
	g_free(response);

	if (response_array->len == 0) {
		g_array_free(response_array, TRUE);
		return SR_ERR_DATA;
	}

	*scpi_response = response_array;

	return SR_OK;
}

SR_PRIV int sr_scpi_get_uint8v(struct sr_scpi_dev_inst *scpi,
			       const char *command, GArray **scpi_response)
{
	char *response;
	gchar **tokens;
	GArray *response_array;
	int tmp;
	int ret;
	size_t i;

	*scpi_response = NULL;
	response = NULL;

	ret = sr_scpi_get_string(scpi, command, &response);
	if (ret != SR_OK && !response)
		return ret;

	tokens = g_strsplit(response, ",", 0);
	response_array = g_array_new(TRUE, FALSE, sizeof(uint8_t));

	for (i = 0; tokens[i]; i++) {
		tmp = (int)strtol(tokens[i], NULL, 10);
		g_array_append_val(response_array, tmp);
	}

	g_strfreev(tokens);
	g_free(response);

	if (response_array->len == 0) {
		g_array_free(response_array, TRUE);
		return SR_ERR_DATA;
	}

	*scpi_response = response_array;

	return SR_OK;
}

SR_PRIV int sr_scpi_get_block(struct sr_scpi_dev_inst *scpi,
			       const char *command, GByteArray **scpi_response)
{
	/* Simplified block read - not fully implemented */
	(void)scpi;
	(void)command;
	*scpi_response = NULL;
	return SR_ERR_NA;
}

SR_PRIV int sr_scpi_get_hw_id(struct sr_scpi_dev_inst *scpi,
			      struct sr_scpi_hw_info **scpi_response)
{
	int ret;
	char *response;
	gchar **tokens;
	struct sr_scpi_hw_info *hw_info;

	*scpi_response = NULL;
	response = NULL;

	ret = sr_scpi_get_string(scpi, SCPI_CMD_IDN, &response);
	if (ret != SR_OK && !response)
		return ret;

	tokens = g_strsplit(response, ",", 0);
	if (g_strv_length(tokens) < 3) {
		sr_dbg("Invalid IDN response: '%s'", response);
		g_strfreev(tokens);
		g_free(response);
		return SR_ERR_DATA;
	}

	hw_info = g_malloc0(sizeof(*hw_info));
	hw_info->manufacturer = g_strstrip(g_strdup(tokens[0]));
	hw_info->model = g_strstrip(g_strdup(tokens[1]));
	if (g_strv_length(tokens) >= 4) {
		hw_info->serial_number = g_strstrip(g_strdup(tokens[2]));
		hw_info->firmware_version = g_strstrip(g_strdup(tokens[3]));
	} else {
		hw_info->serial_number = g_strdup("Unknown");
		hw_info->firmware_version = g_strstrip(g_strdup(tokens[2]));
	}

	g_strfreev(tokens);
	g_free(response);

	*scpi_response = hw_info;

	return SR_OK;
}

SR_PRIV void sr_scpi_hw_info_free(struct sr_scpi_hw_info *hw_info)
{
	if (!hw_info)
		return;

	g_free(hw_info->manufacturer);
	g_free(hw_info->model);
	g_free(hw_info->serial_number);
	g_free(hw_info->firmware_version);
	g_free(hw_info);
}

SR_PRIV const char *sr_scpi_unquote_string(char *s)
{
	size_t s_len;

	if (!s || !*s)
		return s;
	s_len = strlen(s);
	if (s_len < 2)
		return s;

	if (s[0] != '\'' && s[0] != '"')
		return s;
	if (s[0] != s[s_len - 1])
		return s;

	s[s_len - 1] = '\0';
	return s + 1;
}

SR_PRIV const char *sr_vendor_alias(const char *raw_vendor)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(scpi_vendors); i++) {
		if (!g_ascii_strcasecmp(raw_vendor, scpi_vendors[i][0]))
			return scpi_vendors[i][1];
	}

	return raw_vendor;
}

SR_PRIV const char *sr_scpi_cmd_get(const struct scpi_command *cmdtable,
		int command)
{
	unsigned int i;
	const char *cmd;

	if (!cmdtable)
		return NULL;

	cmd = NULL;
	for (i = 0; cmdtable[i].string; i++) {
		if (cmdtable[i].command == command) {
			cmd = cmdtable[i].string;
			break;
		}
	}

	return cmd;
}

SR_PRIV int sr_scpi_cmd(const struct sr_dev_inst *sdi,
		const struct scpi_command *cmdtable,
		int channel_command, const char *channel_name,
		int command, ...)
{
	struct sr_scpi_dev_inst *scpi;
	va_list args;
	int ret;
	const char *channel_cmd;
	const char *cmd;

	scpi = sdi->conn;

	cmd = sr_scpi_cmd_get(cmdtable, command);
	if (!cmd)
		return SR_OK;

	g_mutex_lock(&scpi->scpi_mutex);

	channel_cmd = sr_scpi_cmd_get(cmdtable, channel_command);
	if (channel_cmd && channel_name &&
			g_strcmp0(channel_name, scpi->actual_channel_name)) {
		g_free(scpi->actual_channel_name);
		scpi->actual_channel_name = g_strdup(channel_name);
		ret = scpi_send_variadic(scpi, channel_cmd, (va_list){...});
		if (ret != SR_OK)
			return ret;
	}

	va_start(args, command);
	ret = scpi_send_variadic(scpi, cmd, args);
	va_end(args);

	g_mutex_unlock(&scpi->scpi_mutex);

	return ret;
}

SR_PRIV int sr_scpi_cmd_resp(const struct sr_dev_inst *sdi,
		const struct scpi_command *cmdtable,
		int channel_command, const char *channel_name,
		GVariant **gvar, const GVariantType *gvtype, int command, ...)
{
	/* Simplified implementation */
	(void)sdi;
	(void)cmdtable;
	(void)channel_command;
	(void)channel_name;
	(void)gvar;
	(void)gvtype;
	(void)command;
	return SR_ERR_NA;
}

SR_PRIV GSList *sr_scpi_scan(struct drv_context *drvc, GSList *options,
		struct sr_dev_inst *(*probe_device)(struct sr_scpi_dev_inst *scpi))
{
	GSList *resources, *l, *devices;
	struct sr_dev_inst *sdi;
	const char *resource;
	const char *serialcomm;
	gchar **res;
	unsigned i;

	(void)options;

	devices = NULL;
	for (i = 0; i < ARRAY_SIZE(scpi_devs); i++) {
		if (!scpi_devs[i]->scan)
			continue;
		resources = scpi_devs[i]->scan(drvc);
		for (l = resources; l; l = l->next) {
			res = g_strsplit(l->data, ":", 2);
			if (!res[0]) {
				g_strfreev(res);
				continue;
			}
			resource = res[0];
			serialcomm = res[1];

			sdi = probe_device(NULL);
			if (sdi) {
				devices = g_slist_append(devices, sdi);
			}
			g_strfreev(res);
		}
		g_slist_free_full(resources, g_free);
	}

	return devices;
}