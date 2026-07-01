/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010-2012 Håvard Espeland <gus@ping.uio.no>,
 * Copyright (C) 2010 Martin Stensgård <mastensg@ping.uio.no>
 * Copyright (C) 2010 Carl Henrik Lunde <chlunde@ping.uio.no>
 * Copyright (C) 2020 Gerhard Sittig <gerhard.sittig@gmx.net>
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
 * ASIX SIGMA/SIGMA2 logic analyzer driver
 * Adapted for PXView compat layer
 */

#include "hardware/compat/compat.h"
#include "protocol.h"

/*
 * Channels are labelled 1-16, see this vendor's image of the cable:
 * http://tools.asix.net/img/sigma_sigmacab_pins_720.jpg (TI/TO are
 * additional trigger in/out signals).
 */
static const char *channel_names[] = {
	"1", "2", "3", "4", "5", "6", "7", "8",
	"9", "10", "11", "12", "13", "14", "15", "16",
};

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_PROBE_NAMES,
};

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_CONN | SR_CONF_GET,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_EXTERNAL_CLOCK | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_EXTERNAL_CLOCK_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_CLOCK_EDGE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_MATCH | SR_CONF_LIST,
	SR_CONF_CAPTURE_RATIO | SR_CONF_GET | SR_CONF_SET,
};

static const char *ext_clock_edges[] = {
	"rising",
	"falling",
	"either",
};

static const int32_t trigger_matches[] = {
	SR_TRIGGER_ZERO,
	SR_TRIGGER_ONE,
	SR_TRIGGER_RISING,
	SR_TRIGGER_FALLING,
};

static void clear_helper(struct dev_context *devc)
{
	(void)sigma_force_close(devc);
}

static int dev_clear(const struct sr_dev_driver *di)
{
	struct compat_drv_context *drvc;
	GSList *l;
	struct sr_dev_inst *sdi;

	if (!di || !di->priv)
		return SR_ERR_ARG;

	drvc = di->priv;
	for (l = drvc->instances; l; l = l->next) {
		sdi = l->data;
		if (sdi->priv)
			clear_helper(sdi->priv);
	}

	return std_dev_clear_compat(di);
}

static gboolean bus_addr_in_devices(int bus, int addr, GSList *devs)
{
	struct sr_usb_dev_inst *usb;

	for (/* EMPTY */; devs; devs = devs->next) {
		usb = devs->data;
		if (usb->bus == bus && usb->address == addr)
			return TRUE;
	}

	return FALSE;
}

static gboolean known_vid_pid(uint16_t vid, uint16_t pid)
{
	if (vid != USB_VENDOR_ASIX)
		return FALSE;
	if (pid != USB_PRODUCT_SIGMA && pid != USB_PRODUCT_OMEGA)
		return FALSE;
	return TRUE;
}

/* Stub for sr_usb_find - returns empty list in compat layer */
static GSList *sr_usb_find_stub(struct sr_context *ctx, const char *conn)
{
	(void)ctx;
	(void)conn;
	return NULL;
}

/* Stub for sr_atoul_base */
static int sr_atoul_base_stub(const char *str, unsigned long *val, char **end, int base)
{
	char *endptr;
	unsigned long result;
	
	result = strtoul(str, &endptr, base);
	if (endptr == str)
		return SR_ERR_ARG;
	if (val)
		*val = result;
	if (end)
		*end = endptr;
	return SR_OK;
}

/* Stub for sr_parse_probe_names - returns default names */
static char **sr_parse_probe_names_stub(const char *str,
	const char *defaults[], int num_defaults, int max, size_t *count)
{
	size_t i;
	char **names;
	
	(void)str;
	(void)max;
	
	names = g_malloc0(sizeof(char *) * (num_defaults + 1));
	for (i = 0; i < num_defaults; i++)
		names[i] = g_strdup(defaults[i]);
	if (count)
		*count = num_defaults;
	return names;
}

/* Stub for sr_samplerate_string */
static char *sr_samplerate_string_stub(uint64_t samplerate)
{
	char *str;
	if (samplerate >= SR_MHZ(1))
		str = g_strdup_printf("%" PRIu64 " MHz", samplerate / 1000000);
	else if (samplerate >= SR_KHZ(1))
		str = g_strdup_printf("%" PRIu64 " kHz", samplerate / 1000);
	else
		str = g_strdup_printf("%" PRIu64 " Hz", samplerate);
	return str;
}

/* Local helper for write_u8_inc (defined in protocol.c but not in header) */
static inline void local_write_u8_inc(uint8_t **ptr, uint8_t val) {
	**ptr = val;
	(*ptr)++;
}

/* Local helper for std_str_idx (compat layer stub) */
static int local_std_str_idx(GVariant *data, const char *const strs[], size_t count)
{
	const char *str;
	size_t i;
	
	str = g_variant_get_string(data, NULL);
	for (i = 0; i < count; i++) {
		if (strcmp(str, strs[i]) == 0)
			return (int)i;
	}
	return -1;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct compat_drv_context *drvc;
	libusb_context *usbctx;
	const char *conn;
	const char *probe_names;
	GSList *l, *conn_devices;
	struct sr_config *src;
	GSList *devices;
	libusb_device **devlist, *devitem;
	int bus, addr;
	struct libusb_device_descriptor des;
	struct libusb_device_handle *hdl;
	int ret;
	char conn_id[20];
	char serno_txt[16];
	char *end;
	unsigned long serno_num, serno_pre;
	enum asix_device_type dev_type;
	const char *dev_text;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	size_t devidx, chidx;
	size_t count;
	uint16_t vid, pid;

	drvc = di->priv;
	usbctx = drvc->sr_ctx->libusb_ctx;

	/* Find all devices which match an (optional) conn= spec. */
	conn = NULL;
	probe_names = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_PROBE_NAMES:
			probe_names = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	conn_devices = NULL;
	if (conn)
		conn_devices = sr_usb_find_stub(drvc->sr_ctx, conn);
	if (conn && !conn_devices)
		return NULL;

	/* Find all ASIX logic analyzers (which match the connection spec). */
	devices = NULL;
	libusb_get_device_list(usbctx, &devlist);
	for (devidx = 0; devlist[devidx]; devidx++) {
		devitem = devlist[devidx];

		/* Check for connection match if a user spec was given. */
		bus = libusb_get_bus_number(devitem);
		addr = libusb_get_device_address(devitem);
		if (conn && !bus_addr_in_devices(bus, addr, conn_devices))
			continue;
		snprintf(conn_id, sizeof(conn_id), "%d.%d", bus, addr);

		/*
		 * Check for known VID:PID pairs. Get the serial number,
		 * to then derive the device type from it.
		 */
		libusb_get_device_descriptor(devitem, &des);
		vid = des.idVendor;
		pid = des.idProduct;
		if (!known_vid_pid(vid, pid))
			continue;
		if (!des.iSerialNumber) {
			sr_warn("Cannot get serial number (index 0).");
			continue;
		}
		ret = libusb_open(devitem, &hdl);
		if (ret < 0) {
			sr_warn("Cannot open USB device %04x.%04x: %s.",
				vid, pid, libusb_error_name(ret));
			continue;
		}
		ret = libusb_get_string_descriptor_ascii(hdl,
			des.iSerialNumber,
			(unsigned char *)serno_txt, sizeof(serno_txt));
		if (ret < 0) {
			sr_warn("Cannot get serial number (%s).",
				libusb_error_name(ret));
			libusb_close(hdl);
			continue;
		}
		libusb_close(hdl);

		/*
		 * All ASIX logic analyzers have a serial number, which
		 * reads as a hex number, and tells the device type.
		 */
		ret = sr_atoul_base_stub(serno_txt, &serno_num, &end, 16);
		if (ret != SR_OK || !end || *end) {
			sr_warn("Cannot interpret serial number %s.", serno_txt);
			continue;
		}
		dev_type = ASIX_TYPE_NONE;
		dev_text = NULL;
		serno_pre = serno_num >> 16;
		switch (serno_pre) {
		case 0xa601:
			dev_type = ASIX_TYPE_SIGMA;
			dev_text = "SIGMA";
			sr_info("Found SIGMA, serno %s.", serno_txt);
			break;
		case 0xa602:
			dev_type = ASIX_TYPE_SIGMA;
			dev_text = "SIGMA2";
			sr_info("Found SIGMA2, serno %s.", serno_txt);
			break;
		case 0xa603:
			dev_type = ASIX_TYPE_OMEGA;
			dev_text = "OMEGA";
			sr_info("Found OMEGA, serno %s.", serno_txt);
			if (!ASIX_WITH_OMEGA) {
				sr_warn("OMEGA support is not implemented yet.");
				continue;
			}
			break;
		default:
			sr_warn("Unknown serno %s, skipping.", serno_txt);
			continue;
		}

		/* Create a device instance, add it to the result set. */

		sdi = g_malloc0(sizeof(*sdi));
		devices = g_slist_append(devices, sdi);
		sdi->status = SR_ST_INITIALIZING;
		sdi->vendor = g_strdup("ASIX");
		sdi->model = g_strdup(dev_text);
		sdi->serial_num = g_strdup(serno_txt);
		sdi->connection_id = g_strdup(conn_id);
		sdi->inst_type = SR_INST_USB;
		sdi->conn = sr_usb_dev_inst_new(bus, addr, NULL);
		
		devc = g_malloc0(sizeof(*devc));
		sdi->priv = devc;
		devc->channel_names = sr_parse_probe_names_stub(probe_names,
			channel_names, ARRAY_SIZE(channel_names),
			ARRAY_SIZE(channel_names), &count);
		for (chidx = 0; chidx < count; chidx++)
			sr_channel_new(sdi, chidx, SR_CHANNEL_LOGIC,
				TRUE, devc->channel_names[chidx]);
		devc->id.vid = vid;
		devc->id.pid = pid;
		devc->id.serno = serno_num;
		devc->id.prefix = serno_pre;
		devc->id.type = dev_type;
		sr_sw_limits_init(&devc->limit.config);
		devc->capture_ratio = 50;
		devc->use_triggers = FALSE;
		devc->interp.num_channels = 16; /* Default 16 channels */
		devc->interp.samples_per_event = 1; /* Default 1 sample per event */
		devc->clock.samplerate = SR_MHZ(1); /* Default samplerate */

		/* Get current hardware configuration (or use defaults). */
		(void)sigma_fetch_hw_config(sdi);
	}
	libusb_free_device_list(devlist, 1);
	g_slist_free_full(conn_devices, (GDestroyNotify)sr_usb_dev_inst_free);

	return std_scan_complete_compat(di, devices);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	if (devc->id.type == ASIX_TYPE_OMEGA && !ASIX_WITH_OMEGA) {
		sr_err("OMEGA support is not implemented yet.");
		return SR_ERR_NA;
	}

	return sigma_force_open(sdi);
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	return sigma_force_close(devc);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	const char *clock_text;

	(void)cg;

	if (!sdi)
		return SR_ERR;
	devc = sdi->priv;

	switch (key) {
	case SR_CONF_CONN:
		*data = g_variant_new_string(sdi->connection_id);
		break;
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->clock.samplerate);
		break;
	case SR_CONF_EXTERNAL_CLOCK:
		*data = g_variant_new_boolean(devc->clock.use_ext_clock);
		break;
	case SR_CONF_EXTERNAL_CLOCK_SOURCE:
		clock_text = devc->channel_names[devc->clock.clock_pin];
		*data = g_variant_new_string(clock_text);
		break;
	case SR_CONF_CLOCK_EDGE:
		clock_text = ext_clock_edges[devc->clock.clock_edge];
		*data = g_variant_new_string(clock_text);
		break;
	case SR_CONF_LIMIT_MSEC:
	case SR_CONF_LIMIT_SAMPLES:
		return sr_sw_limits_config_get(&devc->limit.config, key, data);
	case SR_CONF_CAPTURE_RATIO:
		*data = g_variant_new_uint64(devc->capture_ratio);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	int ret;
	uint64_t want_rate, have_rate;
	const char **names;
	size_t count;
	int idx;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		want_rate = g_variant_get_uint64(data);
		ret = sigma_normalize_samplerate(want_rate, &have_rate);
		if (ret != SR_OK)
			return ret;
		if (have_rate != want_rate) {
			char *text_want, *text_have;
			text_want = sr_samplerate_string_stub(want_rate);
			text_have = sr_samplerate_string_stub(have_rate);
			sr_info("Adjusted samplerate %s to %s.",
				text_want, text_have);
			g_free(text_want);
			g_free(text_have);
		}
		devc->clock.samplerate = have_rate;
		break;
	case SR_CONF_EXTERNAL_CLOCK:
		devc->clock.use_ext_clock = g_variant_get_boolean(data);
		break;
	case SR_CONF_EXTERNAL_CLOCK_SOURCE:
		names = (const char **)devc->channel_names;
		count = g_strv_length(devc->channel_names);
		idx = local_std_str_idx(data, names, count);
		if (idx < 0)
			return SR_ERR_ARG;
		devc->clock.clock_pin = idx;
		break;
	case SR_CONF_CLOCK_EDGE:
		idx = local_std_str_idx(data, ext_clock_edges, ARRAY_SIZE(ext_clock_edges));
		if (idx < 0)
			return SR_ERR_ARG;
		devc->clock.clock_edge = idx;
		break;
	case SR_CONF_LIMIT_MSEC:
	case SR_CONF_LIMIT_SAMPLES:
		return sr_sw_limits_config_set(&devc->limit.config, key, data);
	case SR_CONF_CAPTURE_RATIO:
		devc->capture_ratio = g_variant_get_uint64(data);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	const char **names;
	size_t count;

	devc = sdi ? sdi->priv : NULL;
	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		if (cg)
			return SR_ERR_NA;
		return std_config_list(key, data, sdi, cg, scanopts, ARRAY_SIZE(scanopts), drvopts, ARRAY_SIZE(drvopts), devopts, ARRAY_SIZE(devopts));
	case SR_CONF_SAMPLERATE:
		*data = sigma_get_samplerates_list();
		break;
	case SR_CONF_EXTERNAL_CLOCK_SOURCE:
		if (!devc)
			return SR_ERR_ARG;
		names = (const char **)devc->channel_names;
		count = g_strv_length(devc->channel_names);
		*data = g_variant_new_strv(names, count);
		break;
	case SR_CONF_CLOCK_EDGE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(ext_clock_edges));
		break;
	case SR_CONF_TRIGGER_MATCH:
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	uint16_t pindis_mask;
	uint8_t async, div;
	int ret;
	size_t triggerpin;
	uint8_t trigsel2;
	struct triggerinout triggerinout_conf;
	struct triggerlut lut;
	uint8_t regval, cmd_bytes[4], *wrptr;

	devc = sdi->priv;

	/* Convert caller's trigger spec to driver's internal format. */
	ret = sigma_convert_trigger(sdi);
	if (ret != SR_OK) {
		sr_err("Could not configure triggers.");
		return ret;
	}

	/*
	 * Setup the device's samplerate from the value which up to now
	 * just got checked and stored.
	 */
	if (devc->clock.use_ext_clock) {
		if (devc->clock.samplerate != SR_MHZ(50))
			sr_info("External clock, forcing 50MHz samplerate.");
		devc->clock.samplerate = SR_MHZ(50);
	}
	ret = sigma_set_samplerate(sdi);
	if (ret != SR_OK)
		return ret;
	ret = sigma_set_acquire_timeout(devc);
	if (ret != SR_OK)
		return ret;

	/* Enter trigger programming mode. */
	trigsel2 = TRGSEL2_RESET;
	ret = sigma_set_register(devc, WRITE_TRIGGER_SELECT2, trigsel2);
	if (ret != SR_OK)
		return ret;

	trigsel2 = 0;
	if (devc->clock.samplerate >= SR_MHZ(100)) {
		/* 100 and 200 MHz mode. */
		ret = sigma_set_register(devc, WRITE_TRIGGER_SELECT2, 0x81);
		if (ret != SR_OK)
			return ret;

		/* Find which pin to trigger on from mask. */
		for (triggerpin = 0; triggerpin < 8; triggerpin++) {
			if (devc->trigger.risingmask & BIT(triggerpin))
				break;
			if (devc->trigger.fallingmask & BIT(triggerpin))
				break;
		}

		/* Set trigger pin and light LED on trigger. */
		trigsel2 = triggerpin & TRGSEL2_PINS_MASK;
		trigsel2 |= TRGSEL2_LEDSEL1;

		if (devc->trigger.fallingmask)
			trigsel2 |= TRGSEL2_PINPOL_RISE;

	} else if (devc->clock.samplerate <= SR_MHZ(50)) {
		/* 50MHz firmware modes. */

		ret = sigma_build_basic_trigger(devc, &lut);
		if (ret != SR_OK)
			return ret;

		ret = sigma_write_trigger_lut(devc, &lut);
		if (ret != SR_OK)
			return ret;

		trigsel2 = TRGSEL2_LEDSEL1 | TRGSEL2_LEDSEL0;
	}

	/* Setup trigger in and out pins to default values. */
	memset(&triggerinout_conf, 0, sizeof(triggerinout_conf));
	triggerinout_conf.trgout_bytrigger = TRUE;
	triggerinout_conf.trgout_enable = TRUE;
	wrptr = cmd_bytes;
	regval = 0;
	if (triggerinout_conf.trgout_bytrigger)
		regval |= TRGOPT_TRGOOUTEN;
	local_write_u8_inc(&wrptr, regval);
	regval &= ~TRGOPT_CLEAR_MASK;
	if (triggerinout_conf.trgout_enable)
		regval |= TRGOPT_TRGOEN;
	local_write_u8_inc(&wrptr, regval);
	ret = sigma_write_register(devc, WRITE_TRIGGER_OPTION,
		cmd_bytes, wrptr - cmd_bytes);
	if (ret != SR_OK)
		return ret;

	/* Leave trigger programming mode. */
	ret = sigma_set_register(devc, WRITE_TRIGGER_SELECT2, trigsel2);
	if (ret != SR_OK)
		return ret;

	/*
	 * Samplerate dependent clock and channels configuration.
	 */
	pindis_mask = ~BITS_MASK(devc->interp.num_channels);
	if (devc->clock.samplerate > SR_MHZ(50)) {
		ret = sigma_set_register(devc, WRITE_CLOCK_SELECT,
			pindis_mask & 0xff);
	} else {
		wrptr = cmd_bytes;
		async = 0;
		div = SR_MHZ(50) / devc->clock.samplerate - 1;
		if (devc->clock.use_ext_clock) {
			async = CLKSEL_CLKSEL8;
			div = devc->clock.clock_pin + 1;
			switch (devc->clock.clock_edge) {
			case SIGMA_CLOCK_EDGE_RISING:
				div |= CLKSEL_RISING;
				break;
			case SIGMA_CLOCK_EDGE_FALLING:
				div |= CLKSEL_FALLING;
				break;
			case SIGMA_CLOCK_EDGE_EITHER:
				div |= CLKSEL_RISING;
				div |= CLKSEL_FALLING;
				break;
			}
		}
		*wrptr++ = async;
		*wrptr++ = div;
		*wrptr++ = (pindis_mask >> 8) & 0xff;
		*wrptr++ = pindis_mask & 0xff;
		ret = sigma_write_register(devc, WRITE_CLOCK_SELECT,
			cmd_bytes, wrptr - cmd_bytes);
	}
	if (ret != SR_OK)
		return ret;

	/* Setup maximum post trigger time. */
	ret = sigma_set_register(devc, WRITE_POST_TRIGGER,
		(devc->capture_ratio * 255) / 100);
	if (ret != SR_OK)
		return ret;

	/* Start acquisition. */
	regval = WMR_TRGRES | WMR_SDRAMWRITEEN;
	if (devc->use_triggers)
		regval |= WMR_TRGEN;
	ret = sigma_set_register(devc, WRITE_MODE, regval);
	if (ret != SR_OK)
		return ret;

	ret = std_session_send_df_header(sdi, LOG_PREFIX);
	if (ret != SR_OK)
		return ret;

	/* Add capture source. */
	ret = sr_session_source_add(sdi->session, -1, 0, 10,
		sigma_receive_data, (void *)sdi);
	if (ret != SR_OK)
		return ret;

	devc->state = SIGMA_CAPTURE;

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	/*
	 * When acquisition is currently running, keep the receive
	 * routine registered and have it stop the acquisition upon the
	 * next invocation. Else unregister the receive routine here
	 * already.
	 */
	if (devc->state == SIGMA_CAPTURE) {
		devc->state = SIGMA_STOPPING;
	} else {
		devc->state = SIGMA_IDLE;
		(void)sr_session_source_remove(sdi->session, -1);
	}

	return SR_OK;
}

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *asix_sigma_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver asix_sigma_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int asix_sigma_compat_init(struct sr_context *sr_ctx)
{
	asix_sigma_drv_ptr = &asix_sigma_driver_info;
	return std_init(asix_sigma_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int asix_sigma_compat_cleanup(void)
{
	/* Call dev_clear to free per-device resources before cleanup */
	dev_clear(asix_sigma_drv_ptr);
	return std_cleanup(asix_sigma_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *asix_sigma_compat_scan(GSList *options)
{
	return scan(asix_sigma_drv_ptr, options);
}

/* Wrapper: PXView dev_clear(void) -> standard dev_clear(driver) */
static int asix_sigma_compat_dev_clear(void)
{
	return dev_clear(asix_sigma_drv_ptr);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int asix_sigma_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int asix_sigma_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int asix_sigma_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int asix_sigma_compat_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int asix_sigma_compat_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver asix_sigma_driver_info = {
	.name = "asix-sigma",
	.longname = "ASIX SIGMA/SIGMA2",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = asix_sigma_compat_init,
	.cleanup = asix_sigma_compat_cleanup,
	.scan = asix_sigma_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = asix_sigma_compat_config_get,
	.config_set = asix_sigma_compat_config_set,
	.config_list = asix_sigma_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = asix_sigma_compat_acquisition_start,
	.dev_acquisition_stop = asix_sigma_compat_acquisition_stop,
	.priv = NULL,
};