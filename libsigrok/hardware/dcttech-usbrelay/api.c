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

/*
 * DCTtech USBRelay driver, PXView compat port.
 *
 * Migration notes (from the upstream hidapi-based driver):
 *
 *  - hidapi is replaced by direct libusb control transfers. See
 *    protocol.c for the hid_get_feature_report / hid_send_feature_report
 *    helpers and the rationale for the buffer layout.
 *  - Device enumeration uses libusb_get_device_list() filtered by
 *    VENDOR_ID/PRODUCT_ID instead of hid_enumerate(). Vendor/product
 *    strings are retrieved with libusb_get_string_descriptor_ascii().
 *  - The conn= spec accepts "sn=<serial>", "<vid>.<pid>" (hex) or
 *    "<bus>.<addr>" (decimal). hidapi-style OS paths are not supported
 *    because libusb identifies devices by bus+address.
 *  - The device is re-opened in dev_open() by matching the bus/address
 *    stored in dev_context during scan(), because the VID/PID pair is
 *    shared across several V-USB projects and is not a unique handle.
 *  - sr_atoul_base() and sr_serial_extract_options() are not provided
 *    by PXView; a local strtoul-based stub and a manual option walk
 *    replace them (same approach as hp-59306a, juntek-jds6600).
 */

#include "hardware/compat/compat.h"

#include <ctype.h>
#include <errno.h>
#include <libusb.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"

/*
 * Local stub for sr_atoul_base(). PXView's libsigrok does not export it.
 * Matches the standard sigrok semantics: SR_OK on success (at least one
 * digit consumed), SR_ERR on parse failure. Mirrors the stub in
 * juntek-jds6600/protocol.c.
 */
static int sr_atoul_base(const char *str, unsigned long *val, char **end, int base)
{
	char *endptr;
	unsigned long result;

	if (!str)
		return SR_ERR_ARG;

	errno = 0;
	result = strtoul(str, &endptr, base);
	if (endptr == str)
		return SR_ERR;
	if (errno != 0)
		return SR_ERR;
	if (val)
		*val = result;
	if (end)
		*end = endptr;
	return SR_OK;
}

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_MULTIPLEXER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONN | SR_CONF_GET,
	SR_CONF_ENABLED | SR_CONF_SET, /* Enable/disable all relays at once. */
};

static const uint32_t devopts_cg[] = {
	SR_CONF_ENABLED | SR_CONF_GET | SR_CONF_SET,
};

/* Forward declaration - defined at end of file. */
extern struct sr_dev_driver dcttech_usbrelay_driver_info;

/*
 * Open a libusb device, detach the kernel driver if one is active, and
 * claim the HID interface so that control transfers can be issued.
 * Returns the handle or NULL. On success, *detached indicates whether
 * a kernel driver was detached (so the caller can re-attach it later).
 *
 * On Windows libusb_kernel_driver_active() is a no-op that returns 0,
 * so detach/attach never fires; the flag is still tracked for the
 * benefit of any non-Windows build.
 */
static struct libusb_device_handle *open_and_claim(struct libusb_device *dev,
	gboolean *detached)
{
	struct libusb_device_handle *hdl;
	int ret;

	*detached = FALSE;
	hdl = NULL;
	ret = libusb_open(dev, &hdl);
	if (ret != 0) {
		sr_err("Cannot open USB device: %s.", libusb_error_name(ret));
		return NULL;
	}

	if (libusb_kernel_driver_active(hdl, DCTTECH_USB_INTERFACE) == 1) {
		ret = libusb_detach_kernel_driver(hdl, DCTTECH_USB_INTERFACE);
		if (ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND
				&& ret != LIBUSB_ERROR_NOT_SUPPORTED) {
			sr_err("Cannot detach kernel driver: %s.",
				libusb_error_name(ret));
			libusb_close(hdl);
			return NULL;
		}
		*detached = TRUE;
	}

	ret = libusb_claim_interface(hdl, DCTTECH_USB_INTERFACE);
	if (ret < 0) {
		sr_err("Cannot claim interface %d: %s.",
			DCTTECH_USB_INTERFACE, libusb_error_name(ret));
		if (*detached)
			libusb_attach_kernel_driver(hdl, DCTTECH_USB_INTERFACE);
		libusb_close(hdl);
		return NULL;
	}

	return hdl;
}

/* Release the HID interface, re-attach the kernel driver if it was
 * detached earlier, and close the handle. */
static void release_and_close(struct libusb_device_handle *hdl,
	gboolean detached)
{
	int ret;

	if (!hdl)
		return;

	ret = libusb_release_interface(hdl, DCTTECH_USB_INTERFACE);
	if (ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND
			&& ret != LIBUSB_ERROR_NO_DEVICE)
		sr_err("Cannot release interface %d: %s.",
			DCTTECH_USB_INTERFACE, libusb_error_name(ret));

	if (detached) {
		ret = libusb_attach_kernel_driver(hdl, DCTTECH_USB_INTERFACE);
		if (ret < 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED)
			sr_dbg("Cannot re-attach kernel driver: %s.",
				libusb_error_name(ret));
	}

	libusb_close(hdl);
}

/*
 * Probe a single candidate device. The handle is already opened and the
 * HID interface claimed by the caller. Reads the feature report to get
 * the board serial number and the current relay state, then constructs
 * the sr_dev_inst with one channel group per relay.
 *
 * Returns NULL if the device does not match (wrong product string,
 * non-printable serial, serial mismatch) or on communication failure.
 * The caller always closes the handle regardless of the outcome.
 */
static struct sr_dev_inst *probe_device_common(
	struct libusb_device_handle *hdl, struct libusb_device *dev,
	const char *want_serno, const char *vendor, const char *product)
{
	char nonws[16], *s, *endp;
	unsigned long relay_count;
	int ret;
	char serno[SERNO_LENGTH + 1];
	uint8_t curr_state;
	uint8_t report[1 + REPORT_BYTECOUNT];
	GString *txt;
	size_t snr_pos;
	char c;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct channel_group_context *cgc;
	size_t idx, nr;
	struct sr_channel_group *cg;
	char cg_name[24];
	uint8_t bus, addr;

	/*
	 * Get relay count from product string. Weak condition,
	 * accept any trailing number regardless of preceeding text.
	 */
	snprintf(nonws, sizeof(nonws), "%s", product);
	s = nonws;
	s += strlen(s);
	while (s > nonws && isdigit((int)s[-1]))
		s--;
	ret = sr_atoul_base(s, &relay_count, &endp, 10);
	if (ret != SR_OK || !endp || *endp)
		return NULL;
	if (!relay_count)
		return NULL;
	sr_info("Relay count %lu from product string %s.", relay_count, nonws);

	/* Get an HID report. */
	memset(&report, 0, sizeof(report));
	report[0] = REPORT_NUMBER;
	ret = dcttech_usbrelay_hid_get_feature_report(hdl, report, sizeof(report));
	if (sr_log_loglevel_get() >= SR_LOG_SPEW) {
		txt = sr_hexdump_new(report, sizeof(report));
		sr_spew("Got report bytes: %s, rc %d.", txt->str, ret);
		sr_hexdump_free(txt);
	}
	if (ret < 0) {
		sr_err("Cannot read HID report: %s.", libusb_error_name(ret));
		return NULL;
	}
	if (ret != (int)sizeof(report)) {
		sr_err("Unexpected HID report length %d.", ret);
		return NULL;
	}

	/*
	 * Serial number must be all printable characters. Relay state
	 * is for information only, gets re-retrieved before configure
	 * API calls (get/set).
	 */
	memset(serno, 0, sizeof(serno));
	for (snr_pos = 0; snr_pos < SERNO_LENGTH; snr_pos++) {
		c = report[1 + snr_pos];
		serno[snr_pos] = c;
		if (c < 0x20 || c > 0x7e) {
			sr_warn("Skipping, non-printable serial.");
			return NULL;
		}
	}
	curr_state = report[1 + STATE_INDEX];
	sr_info("HID report data: serial number %s, relay state 0x%02x.",
		serno, curr_state);

	/* Optionally filter by serial number. */
	if (want_serno && *want_serno && strcmp(serno, want_serno) != 0) {
		sr_dbg("Serial number does not match user spec. Skipping.");
		return NULL;
	}

	/* Identify for sdi->connection_id / devc->hid_path. */
	bus = libusb_get_bus_number(dev);
	addr = libusb_get_device_address(dev);

	/* Create a device instance. */
	sdi = g_malloc0(sizeof(*sdi));
	sdi->vendor = g_strdup(vendor);
	sdi->model = g_strdup(product);
	sdi->serial_num = g_strdup(serno);
	sdi->connection_id = g_strdup_printf("%u.%u", bus, addr);
	sdi->driver = &dcttech_usbrelay_driver_info;
	sdi->inst_type = SR_INST_USB;

	/* Create channels (groups). */
	devc = g_malloc0(sizeof(*devc));
	sdi->priv = devc;
	devc->hid_path = g_strdup_printf("%u.%u", bus, addr);
	devc->usb_vid = VENDOR_ID;
	devc->usb_pid = PRODUCT_ID;
	devc->usb_bus = bus;
	devc->usb_addr = addr;
	devc->relay_count = relay_count;
	devc->relay_mask = (1U << relay_count) - 1;
	devc->relay_state = curr_state & devc->relay_mask;
	for (idx = 0; idx < devc->relay_count; idx++) {
		nr = idx + 1;
		snprintf(cg_name, sizeof(cg_name), "R%zu", nr);
		cgc = g_malloc0(sizeof(*cgc));
		cgc->number = nr;
		cg = sr_channel_group_new(sdi, cg_name, cgc);
		(void)cg;
	}

	return sdi;
}

/*
 * Parse a conn= spec into the matching criteria used by scan().
 *
 *   "sn=<serial>"  -> want_serno filled, bus/addr search disabled
 *   "<vid>.<pid>"  -> vid/pid filled (hex, 4 digits each); since the
 *                     driver's fixed VID/PID already match this, the
 *                     search is not narrowed further
 *   "<bus>.<addr>" -> bus/addr filled (decimal); the enumeration only
 *                     probes the device at that bus/address
 *
 * Returns SR_OK if the spec was parsed (or was NULL), SR_ERR_ARG if it
 * was syntactically invalid. On SR_ERR_ARG the caller falls back to a
 * full enumeration.
 */
static int parse_conn_spec(const char *conn,
	char *want_serno, size_t want_serno_sz,
	uint8_t *want_bus, uint8_t *want_addr,
	gboolean *have_bus_addr)
{
	unsigned long vid, pid, bus, addr;
	char *endp;

	*want_bus = 0;
	*want_addr = 0;
	*have_bus_addr = FALSE;
	memset(want_serno, 0, want_serno_sz);

	if (!conn || !*conn)
		return SR_OK;

	if (g_str_has_prefix(conn, "sn=")) {
		snprintf(want_serno, want_serno_sz, "%s", conn + 3);
		return SR_OK;
	}

	/* Try "<vid>.<pid>" (hex, 4 digits each). */
	vid = strtoul(conn, &endp, 16);
	if (endp == conn + 4 && *endp == '.' && vid) {
		const char *p = endp + 1;
		pid = strtoul(p, &endp, 16);
		if (endp == p + 4 && *endp == '\0' && pid) {
			/*
			 * VID.PID spec. The driver only ever matches
			 * VENDOR_ID/PRODUCT_ID, so this is accepted as-is
			 * without narrowing the bus/addr search.
			 */
			return SR_OK;
		}
	}

	/* Try "<bus>.<addr>" (decimal). */
	bus = strtoul(conn, &endp, 10);
	if (endp > conn && *endp == '.' && bus <= 255) {
		const char *p = endp + 1;
		addr = strtoul(p, &endp, 10);
		if (endp > p && *endp == '\0' && addr <= 255) {
			*want_bus = (uint8_t)bus;
			*want_addr = (uint8_t)addr;
			*have_bus_addr = TRUE;
			return SR_OK;
		}
	}

	return SR_ERR_ARG;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct drv_context *drvc;
	GSList *l, *devices;
	struct sr_config *cfg;
	const char *conn;
	char want_serno[SERNO_LENGTH + 1];
	uint8_t want_bus, want_addr;
	gboolean have_bus_addr;
	struct libusb_context *usb_ctx;
	struct libusb_device **devlist;
	struct libusb_device_descriptor desc;
	ssize_t ndev, i;
	struct libusb_device_handle *hdl;
	gboolean detached;
	char vendor[64], product[64];
	int ret;
	struct sr_dev_inst *sdi;

	drvc = di->priv;
	devices = NULL;

	/*
	 * Get optional conn= spec. PXView does not provide
	 * sr_serial_extract_options(), so manually walk the options list
	 * (same pattern as hp-59306a, devantech-eth008 after compat).
	 */
	conn = NULL;
	for (l = options; l; l = l->next) {
		cfg = l->data;
		if (cfg->key == SR_CONF_CONN) {
			conn = g_variant_get_string(cfg->data, NULL);
			break;
		}
	}

	ret = parse_conn_spec(conn, want_serno, sizeof(want_serno),
		&want_bus, &want_addr, &have_bus_addr);
	if (ret != SR_OK) {
		sr_warn("Unrecognized conn= spec %s, enumerating all matches.",
			conn);
	}

	/*
	 * Enumerate USB devices. The firmware is V-USB based; the USB
	 * VID:PID pair is shared across several projects, so the vendor
	 * and product _strings_ must be inspected to actually identify
	 * the device. The board serial number lives in the HID report
	 * (the USB serial descriptor is not reliable).
	 */
	usb_ctx = drvc->sr_ctx->libusb_ctx;
	if (!usb_ctx) {
		sr_err("No libusb context available.");
		return NULL;
	}

	ndev = libusb_get_device_list(usb_ctx, &devlist);
	if (ndev < 0) {
		sr_err("Failed to get USB device list: %s.",
			libusb_error_name((int)ndev));
		return NULL;
	}

	for (i = 0; devlist[i]; i++) {
		libusb_get_device_descriptor(devlist[i], &desc);
		if (desc.idVendor != VENDOR_ID || desc.idProduct != PRODUCT_ID)
			continue;

		/* Narrow by bus.addr if the user asked for a specific one. */
		if (have_bus_addr) {
			if (libusb_get_bus_number(devlist[i]) != want_bus)
				continue;
			if (libusb_get_device_address(devlist[i]) != want_addr)
				continue;
		}

		/* Open and claim so we can issue control transfers. */
		hdl = open_and_claim(devlist[i], &detached);
		if (!hdl)
			continue;

		/* Get vendor/product strings (ASCII). */
		vendor[0] = product[0] = '\0';
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(hdl,
				desc.iManufacturer, (unsigned char *)vendor,
				sizeof(vendor));
			if (ret < 0)
				vendor[0] = '\0';
		}
		if (desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(hdl,
				desc.iProduct, (unsigned char *)product,
				sizeof(product));
			if (ret < 0)
				product[0] = '\0';
		}

		/*
		 * Filter by vendor/product strings. The HID report is
		 * only fetched after this cheap check passes.
		 */
		if (!vendor[0] || !product[0]) {
			release_and_close(hdl, detached);
			continue;
		}
		if (strcmp(vendor, VENDOR_STRING) != 0) {
			release_and_close(hdl, detached);
			continue;
		}
		if (!g_str_has_prefix(product, PRODUCT_STRING_PREFIX)) {
			release_and_close(hdl, detached);
			continue;
		}

		sr_dbg("Checking %04hx:%04hx at %u.%u, vendor %s, product %s.",
			desc.idVendor, desc.idProduct,
			libusb_get_bus_number(devlist[i]),
			libusb_get_device_address(devlist[i]),
			vendor, product);

		sdi = probe_device_common(hdl, devlist[i], want_serno,
			vendor, product);
		release_and_close(hdl, detached);
		if (sdi)
			devices = g_slist_append(devices, sdi);
	}

	libusb_free_device_list(devlist, 1);

	return std_scan_complete_compat(di, devices);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_dev_driver *di;
	struct drv_context *drvc;
	struct libusb_context *usb_ctx;
	struct libusb_device **devlist;
	struct libusb_device_descriptor desc;
	ssize_t ndev, i;
	struct libusb_device_handle *hdl;
	gboolean detached;

	devc = sdi->priv;
	di = sdi->driver;
	drvc = di->priv;

	/* Close a stale handle if one is somehow present. */
	if (devc->hid_dev) {
		release_and_close(devc->hid_dev, devc->detached_kernel_driver);
		devc->hid_dev = NULL;
		devc->detached_kernel_driver = FALSE;
	}

	usb_ctx = drvc->sr_ctx->libusb_ctx;
	if (!usb_ctx)
		return SR_ERR_BUG;

	/*
	 * Re-open by matching the bus/address captured during scan().
	 * VID/PID alone is insufficient because it is shared across
	 * several V-USB projects.
	 */
	ndev = libusb_get_device_list(usb_ctx, &devlist);
	if (ndev < 0) {
		sr_err("Failed to get USB device list: %s.",
			libusb_error_name((int)ndev));
		return SR_ERR_IO;
	}

	hdl = NULL;
	for (i = 0; devlist[i]; i++) {
		libusb_get_device_descriptor(devlist[i], &desc);
		if (desc.idVendor != devc->usb_vid ||
				desc.idProduct != devc->usb_pid)
			continue;
		if (libusb_get_bus_number(devlist[i]) != devc->usb_bus)
			continue;
		if (libusb_get_device_address(devlist[i]) != devc->usb_addr)
			continue;
		hdl = open_and_claim(devlist[i], &detached);
		break;
	}
	libusb_free_device_list(devlist, 1);

	if (!hdl) {
		sr_err("Device %u.%u not found or cannot be opened.",
			devc->usb_bus, devc->usb_addr);
		return SR_ERR_IO;
	}

	devc->hid_dev = hdl;
	devc->detached_kernel_driver = detached;

	/* Refresh the relay state cache. */
	(void)dcttech_usbrelay_update_state(sdi);

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	if (devc->hid_dev) {
		release_and_close(devc->hid_dev, devc->detached_kernel_driver);
		devc->hid_dev = NULL;
		devc->detached_kernel_driver = FALSE;
	}

	return SR_OK;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	gboolean on;
	int ret;

	if (!cg) {
		switch (key) {
		case SR_CONF_CONN:
			if (!sdi->connection_id)
				return SR_ERR_NA;
			*data = g_variant_new_string(sdi->connection_id);
			return SR_OK;
		default:
			return SR_ERR_NA;
		}
	}

	switch (key) {
	case SR_CONF_ENABLED:
		ret = dcttech_usbrelay_query_cg(sdi, cg, &on);
		if (ret != SR_OK)
			return ret;
		*data = g_variant_new_boolean(on);
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	gboolean on;

	if (!cg) {
		switch (key) {
		case SR_CONF_ENABLED:
			/* Enable/disable all channels at the same time. */
			on = g_variant_get_boolean(data);
			return dcttech_usbrelay_switch_cg(sdi, cg, on);
		default:
			return SR_ERR_NA;
		}
	} else {
		switch (key) {
		case SR_CONF_ENABLED:
			on = g_variant_get_boolean(data);
			return dcttech_usbrelay_switch_cg(sdi, cg, on);
		default:
			return SR_ERR_NA;
		}
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{

	if (!cg) {
		switch (key) {
		case SR_CONF_SCAN_OPTIONS:
		case SR_CONF_DEVICE_OPTIONS:
			return STD_CONFIG_LIST(key, data, sdi, cg,
				scanopts, drvopts, devopts);
		default:
			return SR_ERR_NA;
		}
	}

	switch (key) {
	case SR_CONF_DEVICE_OPTIONS:
		*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg));
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

/* ===========================================================================
 * PXView compat wrapper layer
 *
 * PXView's sr_dev_driver callbacks have different signatures from standard
 * sigrok's (int vs uint32_t key, extra ch parameter, cb_data in acquisition
 * start/stop, no dev_list/dev_clear fields). These thin wrappers adapt the
 * standard sigrok callbacks above to PXView's expected signatures.
 *
 * This is a relay multiplexer driver. The original sigrok source used
 * std_dummy_dev_acquisition_start / std_dummy_dev_acquisition_stop (no-ops
 * returning SR_OK) for the acquisition callbacks. Neither exists in the
 * PXView compat layer, so the wrappers below are no-ops that simply return
 * SR_OK, matching the original semantics. There is no data acquisition
 * stream -- relay state is queried/set via SR_CONF_ENABLED config calls.
 * ==========================================================================*/

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *dcttech_usbrelay_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int dcttech_usbrelay_compat_init(struct sr_context *sr_ctx)
{
	dcttech_usbrelay_drv_ptr = &dcttech_usbrelay_driver_info;
	return std_init(dcttech_usbrelay_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int dcttech_usbrelay_compat_cleanup(void)
{
	/* Clear device instances before tearing down the driver context. */
	std_dev_clear(dcttech_usbrelay_drv_ptr);
	return std_cleanup(dcttech_usbrelay_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *dcttech_usbrelay_compat_scan(GSList *options)
{
	return scan(dcttech_usbrelay_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int dcttech_usbrelay_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int dcttech_usbrelay_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int dcttech_usbrelay_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/*
 * Wrapper: PXView acquisition_start(sdi, cb_data).
 * Original used std_dummy_dev_acquisition_start (no-op). This driver is a
 * relay multiplexer and does not perform data acquisition, so this is a no-op.
 */
static int dcttech_usbrelay_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/*
 * Wrapper: PXView acquisition_stop(const sdi, cb_data).
 * Original used std_dummy_dev_acquisition_stop. This driver is a relay
 * multiplexer and does not perform data acquisition, so this is a no-op.
 */
static int dcttech_usbrelay_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/* PXView-compatible driver info struct */
struct sr_dev_driver dcttech_usbrelay_driver_info = {
	.name = "dcttech-usbrelay",
	.longname = "dcttech usbrelay",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = dcttech_usbrelay_compat_init,
	.cleanup = dcttech_usbrelay_compat_cleanup,
	.scan = dcttech_usbrelay_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = dcttech_usbrelay_compat_config_get,
	.config_set = dcttech_usbrelay_compat_config_set,
	.config_list = dcttech_usbrelay_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = dcttech_usbrelay_compat_acquisition_start,
	.dev_acquisition_stop = dcttech_usbrelay_compat_acquisition_stop,
	.priv = NULL,
};
