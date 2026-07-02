/*
 * This file is part of the libsigrok project.
 *
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

#ifndef LIBSIGROK_HARDWARE_DCTTECH_USBRELAY_PROTOCOL_H
#define LIBSIGROK_HARDWARE_DCTTECH_USBRELAY_PROTOCOL_H

/*
 * PXView compat layer. This driver was originally written against the
 * hidapi library (hid_open/hid_get_feature_report/hid_send_feature_report
 * /hid_enumerate). PXView does not ship hidapi, so the HID feature-report
 * path is re-implemented on top of libusb control transfers (matching the
 * approach used by the uni-t-dmm driver's hid_chip_init()). The public
 * protocol.h interface is unchanged; only the dev_context storage type for
 * the device handle changed from hid_device* to libusb_device_handle*.
 */
#include <glib.h>
#include <stdint.h>
#include "hardware/compat/compat.h"

/*
 * libusb.h is pulled in transitively via compat.h -> libsigrok-internal.h.
 * Forward-declare the handle type so that protocol.h can be included from
 * translation units that do not need the full libusb header.
 */
struct libusb_device_handle;

#undef LOG_PREFIX
#define LOG_PREFIX "dcttech-usbrelay"

/* USB identification. */
#define VENDOR_ID 0x16c0
#define PRODUCT_ID 0x05df
#define VENDOR_STRING "www.dcttech.com"
#define PRODUCT_STRING_PREFIX "USBRelay"

/* HID report layout. */
#define REPORT_NUMBER 0
#define REPORT_BYTECOUNT 8
#define SERNO_LENGTH 5
#define STATE_INDEX 7

/*
 * The DCTtech USBRelay is a V-USB based HID device. The HID interface
 * number is 0. Control transfers target the interface recipient and use
 * the class request type for HID get/set report.
 */
#define DCTTECH_USB_INTERFACE 0

struct dev_context {
	/*
	 * Connection spec stored as "bus.addr" for re-opening the device
	 * in dev_open(). Retained for sdi->connection_id compatibility.
	 */
	char *hid_path;
	uint16_t usb_vid, usb_pid;
	/*
	 * USB bus and address, captured during scan(). Used to re-open the
	 * exact device in dev_open() by walking the libusb device list and
	 * matching bus+address (VID/PID are shared across several V-USB
	 * projects, so they are not a sufficient identifier).
	 */
	uint8_t usb_bus, usb_addr;
	/*
	 * Replaces the original hid_device *hid_dev. Claimed in dev_open(),
	 * released in dev_close().
	 */
	struct libusb_device_handle *hid_dev;
	gboolean detached_kernel_driver;
	size_t relay_count;
	uint32_t relay_mask;
	uint32_t relay_state;
};

struct channel_group_context {
	size_t number;
};

SR_PRIV int dcttech_usbrelay_update_state(const struct sr_dev_inst *sdi);
SR_PRIV int dcttech_usbrelay_switch_cg(const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg, gboolean on);
SR_PRIV int dcttech_usbrelay_query_cg(const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg, gboolean *on);

/*
 * HID feature-report helpers (libusb control-transfer based, replacing the
 * upstream hidapi calls of the same name). Exposed as SR_PRIV because
 * api.c's scan-time probe needs to read a report from a transient handle
 * before dev_open() has populated devc->hid_dev. The buffer convention
 * matches hidapi: buf[0] is the report ID, buf[1..len-1] is the payload.
 * Returns the full logical length (len) on success, or a libusb error
 * code (< 0) on failure.
 */
SR_PRIV int dcttech_usbrelay_hid_get_feature_report(
	struct libusb_device_handle *handle, uint8_t *buf, size_t len);
SR_PRIV int dcttech_usbrelay_hid_send_feature_report(
	struct libusb_device_handle *handle, const uint8_t *buf, size_t len);

#endif
