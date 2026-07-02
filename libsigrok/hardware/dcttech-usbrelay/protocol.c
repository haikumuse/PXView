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

#include "hardware/compat/compat.h"

#include <libusb.h>
#include <string.h>

#include "protocol.h"

/*
 * HID feature-report helpers built on libusb control transfers.
 *
 * The original driver used hidapi's hid_get_feature_report() and
 * hid_send_feature_report(). Both take a buffer whose first byte is the
 * HID report ID followed by the report payload. hidapi transparently
 * handles the report-ID byte on the application side.
 *
 * With libusb we send GET_REPORT / SET_REPORT class control transfers to
 * the HID interface. The wValue field carries the report type (0x03 =
 * feature) in the high byte and the report ID in the low byte. The data
 * phase carries the report payload WITHOUT a leading report-ID byte when
 * the report ID is 0 (which is the case for this device: REPORT_NUMBER
 * is 0). This matches the Windows HID stack behaviour that hidapi builds
 * on, where HidD_GetFeature/HidD_SetFeature strip the report ID for
 * report ID 0.
 *
 * To preserve the original buffer layout ([reportID, data...]) used by
 * the caller, these helpers read/write only the payload portion
 * (buf[1..len-1]) and report the full logical length (len) on success.
 * Callers continue to compare the return value against sizeof(report)
 * exactly as the original hidapi-based code did.
 *
 * Reference: uni-t-dmm/protocol.c hid_chip_init() uses the same control
 * transfer encoding (wValue 0x300, interface 0, class request type).
 */

static int hid_get_feature_report_impl(struct libusb_device_handle *handle,
	uint8_t *buf, size_t len)
{
	int ret;

	if (len < 2)
		return SR_ERR_ARG;

	ret = libusb_control_transfer(handle,
		LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE |
		LIBUSB_ENDPOINT_IN,
		0x01, /* HID bRequest: GET_REPORT */
		(uint16_t)(0x0300 | (buf[0] & 0xff)), /* feature | report ID */
		DCTTECH_USB_INTERFACE, /* wIndex: interface */
		buf + 1, (int)(len - 1), /* payload only */
		1000);
	if (ret < 0) {
		sr_err("HID GET_REPORT failed: %s.", libusb_error_name(ret));
		return ret;
	}
	/* Re-account for the report-ID byte that the caller expects. */
	return ret + 1;
}

static int hid_send_feature_report_impl(struct libusb_device_handle *handle,
	const uint8_t *buf, size_t len)
{
	int ret;

	if (len < 2)
		return SR_ERR_ARG;

	ret = libusb_control_transfer(handle,
		LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE |
		LIBUSB_ENDPOINT_OUT,
		0x09, /* HID bRequest: SET_REPORT */
		(uint16_t)(0x0300 | (buf[0] & 0xff)), /* feature | report ID */
		DCTTECH_USB_INTERFACE, /* wIndex: interface */
		(uint8_t *)(buf + 1), (int)(len - 1), /* payload only */
		1000);
	if (ret < 0) {
		sr_err("HID SET_REPORT failed: %s.", libusb_error_name(ret));
		return ret;
	}
	return ret + 1;
}

/* SR_PRIV wrappers so api.c's scan-time probe can read reports from a
 * transient handle. See protocol.h for the buffer convention. */
SR_PRIV int dcttech_usbrelay_hid_get_feature_report(
	struct libusb_device_handle *handle, uint8_t *buf, size_t len)
{
	return hid_get_feature_report_impl(handle, buf, len);
}

SR_PRIV int dcttech_usbrelay_hid_send_feature_report(
	struct libusb_device_handle *handle, const uint8_t *buf, size_t len)
{
	return hid_send_feature_report_impl(handle, buf, len);
}

SR_PRIV int dcttech_usbrelay_update_state(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	uint8_t report[1 + REPORT_BYTECOUNT];
	int ret;
	GString *txt;

	devc = sdi->priv;

	if (!devc->hid_dev) {
		sr_err("Cannot read state, device not opened.");
		return SR_ERR_BUG;
	}

	/* Get another HID report. */
	memset(report, 0, sizeof(report));
	report[0] = REPORT_NUMBER;
	ret = hid_get_feature_report_impl(devc->hid_dev, report, sizeof(report));
	if (ret != (int)sizeof(report))
		return SR_ERR_IO;
	if (sr_log_loglevel_get() >= SR_LOG_SPEW) {
		txt = sr_hexdump_new(report, sizeof(report));
		sr_spew("Got report bytes: %s.", txt->str);
		sr_hexdump_free(txt);
	}

	/* Update relay state cache from HID report content. */
	devc->relay_state = report[1 + STATE_INDEX];
	devc->relay_state &= devc->relay_mask;

	return SR_OK;
}

SR_PRIV int dcttech_usbrelay_switch_cg(const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg, gboolean on)
{
	struct dev_context *devc;
	struct channel_group_context *cgc;
	gboolean is_all;
	size_t relay_idx;
	uint8_t report[1 + REPORT_BYTECOUNT];
	int ret;
	GString *txt;

	devc = sdi->priv;

	if (!devc->hid_dev) {
		sr_err("Cannot switch relay, device not opened.");
		return SR_ERR_BUG;
	}

	/* Determine if all or a single relay should be turned off or on. */
	is_all = !cg ? TRUE : FALSE;
	if (is_all) {
		relay_idx = 0;
	} else {
		cgc = cg->priv;
		relay_idx = cgc->number;
	}

	/*
	 * Construct and send the HID report. Notice the weird(?) bit
	 * pattern. Bit 1 is low when all relays are affected at once,
	 * and high to control an individual relay? Bit 0 communicates
	 * whether the relay(s) should be on or off? And all other bits
	 * are always set? It's assumed that the explicit assignment of
	 * full byte values simplifies future maintenance.
	 */
	memset(report, 0, sizeof(report));
	report[0] = REPORT_NUMBER;
	if (is_all) {
		if (on) {
			report[1] = 0xfe;
		} else {
			report[1] = 0xfc;
		}
	} else {
		if (on) {
			report[1] = 0xff;
			report[2] = relay_idx;
		} else {
			report[1] = 0xfd;
			report[2] = relay_idx;
		}
	}
	if (sr_log_loglevel_get() >= SR_LOG_SPEW) {
		txt = sr_hexdump_new(report, sizeof(report));
		sr_spew("Sending report bytes: %s", txt->str);
		sr_hexdump_free(txt);
	}
	ret = hid_send_feature_report_impl(devc->hid_dev, report, sizeof(report));
	if (ret != (int)sizeof(report))
		return SR_ERR_IO;

	/* Update relay state cache (non-fatal). */
	(void)dcttech_usbrelay_update_state(sdi);

	return SR_OK;
}

/* Answers the query from cached relay state. Beware of 1-based indexing. */
SR_PRIV int dcttech_usbrelay_query_cg(const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg, gboolean *on)
{
	struct dev_context *devc;
	struct channel_group_context *cgc;
	size_t relay_idx;
	uint32_t relay_mask;

	devc = sdi->priv;
	if (!cg)
		return SR_ERR_ARG;
	cgc = cg->priv;
	relay_idx = cgc->number;
	if (relay_idx < 1 || relay_idx > devc->relay_count)
		return SR_ERR_ARG;
	relay_mask = 1U << (relay_idx - 1);

	*on = devc->relay_state & relay_mask;

	return SR_OK;
}
