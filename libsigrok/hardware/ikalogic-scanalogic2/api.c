/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Marc Schink <sigrok-dev@marcschink.de>
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

#include <config.h>
#include "protocol.h"

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	SR_CONF_LIMIT_SAMPLES | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_MATCH | SR_CONF_LIST,
	SR_CONF_CAPTURE_RATIO | SR_CONF_GET | SR_CONF_SET,
};

static const int32_t trigger_matches[] = {
	SR_TRIGGER_RISING,
	SR_TRIGGER_FALLING,
	SR_TRIGGER_EDGE,
};

SR_PRIV const uint64_t sl2_samplerates[NUM_SAMPLERATES] = {
	SR_KHZ(1.25),
	SR_KHZ(10),
	SR_KHZ(50),
	SR_KHZ(100),
	SR_KHZ(250),
	SR_KHZ(500),
	SR_MHZ(1),
	SR_MHZ(2.5),
	SR_MHZ(5),
	SR_MHZ(10),
	SR_MHZ(20),
};

static const char *channel_names[] = {
	"0", "1", "2", "3",
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *usb_devices, *devices, *l;
	struct drv_context *drvc;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_usb_dev_inst *usb;
	struct device_info dev_info;
	unsigned int i;
	int ret;

	(void)options;

	devices = NULL;
	drvc = di->priv;

	usb_devices = sr_usb_find(drvc->sr_ctx->libusb_ctx, USB_VID_PID);

	if (!usb_devices)
		return NULL;

	for (l = usb_devices; l; l = l->next) {
		usb = l->data;

		if ((ret = sl2_get_device_info(di, *usb, &dev_info)) < 0) {
			sr_warn("Failed to get device information: %d.", ret);
			sr_usb_dev_inst_free(usb);
			continue;
		}

		devc = g_malloc0(sizeof(struct dev_context));

		if (!(devc->xfer_in = libusb_alloc_transfer(0))) {
			sr_err("Transfer malloc failed.");
			sr_usb_dev_inst_free(usb);
			g_free(devc);
			continue;
		}

		if (!(devc->xfer_out = libusb_alloc_transfer(0))) {
			sr_err("Transfer malloc failed.");
			sr_usb_dev_inst_free(usb);
			libusb_free_transfer(devc->xfer_in);
			g_free(devc);
			continue;
		}

		sdi = g_malloc0(sizeof(struct sr_dev_inst));
		sdi->status = SR_ST_INACTIVE;
		sdi->vendor = g_strdup("IKALOGIC");
		sdi->model = g_strdup("Scanalogic-2");
		sdi->version = g_strdup_printf("%u.%u", dev_info.fw_ver_major, dev_info.fw_ver_minor);
		sdi->serial_num = g_strdup_printf("%d", dev_info.serial);
		sdi->priv = devc;
		sdi->inst_type = SR_INST_USB;
		sdi->conn = usb;

		for (i = 0; i < ARRAY_SIZE(channel_names); i++)
			devc->channels[i] = sr_channel_new(sdi, i,
				SR_CHANNEL_LOGIC, TRUE, channel_names[i]);

		devc->state = STATE_IDLE;
		devc->next_state = STATE_IDLE;

		/* Set default samplerate. */
		sl2_set_samplerate(sdi, DEFAULT_SAMPLERATE);

		/* Set default capture ratio. */
		devc->capture_ratio = 0;

		/* Set default after trigger delay. */
		devc->after_trigger_delay = 0;

		memset(devc->xfer_buf_in, 0, LIBUSB_CONTROL_SETUP_SIZE +
			PACKET_LENGTH);
		memset(devc->xfer_buf_out, 0, LIBUSB_CONTROL_SETUP_SIZE +
			PACKET_LENGTH);

		libusb_fill_control_setup(devc->xfer_buf_in,
			USB_REQUEST_TYPE_IN, USB_HID_GET_REPORT,
			USB_HID_REPORT_TYPE_FEATURE, USB_INTERFACE,
			PACKET_LENGTH);
		libusb_fill_control_setup(devc->xfer_buf_out,
			USB_REQUEST_TYPE_OUT, USB_HID_SET_REPORT,
			USB_HID_REPORT_TYPE_FEATURE, USB_INTERFACE,
			PACKET_LENGTH);

		devc->xfer_data_in = devc->xfer_buf_in +
			LIBUSB_CONTROL_SETUP_SIZE;
		devc->xfer_data_out = devc->xfer_buf_out +
			LIBUSB_CONTROL_SETUP_SIZE;

		devices = g_slist_append(devices, sdi);
	}

	g_slist_free(usb_devices);

	return std_scan_complete_compat(di, devices);
}

static void clear_helper(struct dev_context *devc)
{
	libusb_free_transfer(devc->xfer_in);
	libusb_free_transfer(devc->xfer_out);
}

/*
 * std_dev_clear_with_callback() is not available in the PXView compat layer.
 * Inline the clear_helper logic: iterate instances, call clear_helper on each
 * devc, then use std_dev_clear_compat() to free everything.
 */
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

static int dev_open(struct sr_dev_inst *sdi)
{
	struct sr_dev_driver *di = sdi->driver;
	struct drv_context *drvc = di->priv;
	struct dev_context *devc;
	struct sr_usb_dev_inst *usb;
	uint8_t buffer[PACKET_LENGTH];
	int ret;

	usb = sdi->conn;
	devc = sdi->priv;

	if (sr_usb_open(drvc->sr_ctx->libusb_ctx, usb) != SR_OK)
		return SR_ERR;

	if (libusb_kernel_driver_active(usb->devhdl, USB_INTERFACE) == 1) {
		ret = libusb_detach_kernel_driver(usb->devhdl, USB_INTERFACE);
		if (ret < 0) {
			sr_err("Failed to detach kernel driver: %s.",
				libusb_error_name(ret));
			return SR_ERR;
		}
	}

	if ((ret = libusb_claim_interface(usb->devhdl, USB_INTERFACE)) < 0) {
		sr_err("Failed to claim interface: %s.",
			libusb_error_name(ret));
		return SR_ERR;
	}

	libusb_fill_control_transfer(devc->xfer_in, usb->devhdl,
		devc->xfer_buf_in, sl2_receive_transfer_in,
		sdi, USB_TIMEOUT_MS);

	libusb_fill_control_transfer(devc->xfer_out, usb->devhdl,
		devc->xfer_buf_out, sl2_receive_transfer_out,
		sdi, USB_TIMEOUT_MS);

	memset(buffer, 0, sizeof(buffer));

	buffer[0] = CMD_RESET;
	if ((ret = sl2_transfer_out(usb->devhdl, buffer)) != PACKET_LENGTH) {
		sr_err("Device reset failed: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	/*
	 * Set the device to idle state. If the device is not in idle state it
	 * possibly will reset itself after a few seconds without being used
	 * and thereby close the connection.
	 */
	buffer[0] = CMD_IDLE;
	if ((ret = sl2_transfer_out(usb->devhdl, buffer)) != PACKET_LENGTH) {
		sr_err("Failed to set device in idle state: %s.",
			libusb_error_name(ret));
		return SR_ERR;
	}

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct sr_usb_dev_inst *usb;

	usb = sdi->conn;

	if (!usb->devhdl)
		return SR_ERR_BUG;

	libusb_release_interface(usb->devhdl, USB_INTERFACE);
	libusb_close(usb->devhdl);

	usb->devhdl = NULL;

	return SR_OK;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->samplerate);
		break;
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
	uint64_t samplerate, limit_samples;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		limit_samples = g_variant_get_uint64(data);
		return sl2_set_limit_samples(sdi, limit_samples);
	case SR_CONF_SAMPLERATE:
		samplerate = g_variant_get_uint64(data);
		return sl2_set_samplerate(sdi, samplerate);
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
	switch (key) {
	case SR_CONF_DEVICE_OPTIONS:
		/*
		 * This driver does not provide scan options, so pass a NULL
		 * array with a zero count. std_config_list() treats NULL
		 * arrays as empty.
		 */
		return std_config_list(key, data, sdi, cg,
			NULL, 0,
			drvopts, ARRAY_SIZE(drvopts),
			devopts, ARRAY_SIZE(devopts));
	case SR_CONF_SAMPLERATE:
		*data = std_gvar_samplerates(ARRAY_AND_SIZE(sl2_samplerates));
		break;
	case SR_CONF_TRIGGER_MATCH:
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
		break;
	case SR_CONF_LIMIT_SAMPLES:
		*data = std_gvar_tuple_u64(0, MAX_SAMPLES);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct sr_dev_driver *di = sdi->driver;
	struct drv_context *drvc;
	struct dev_context *devc;
	uint16_t trigger_bytes, tmp;
	unsigned int i, j;
	int ret;

	devc = sdi->priv;
	drvc = di->priv;

	devc->wait_data_ready_locked = TRUE;
	devc->stopping_in_progress = FALSE;
	devc->transfer_error = FALSE;
	devc->samples_processed = 0;
	devc->channel = 0;
	devc->sample_packet = 0;

	/*
	 * The trigger must be configured first because the calculation of the
	 * pre and post trigger samples depends on a configured trigger.
	 */
	sl2_convert_trigger(sdi);
	sl2_calculate_trigger_samples(sdi);

	trigger_bytes = devc->pre_trigger_bytes + devc->post_trigger_bytes;

	/* Calculate the number of expected sample packets. */
	devc->num_sample_packets = trigger_bytes / PACKET_NUM_SAMPLE_BYTES;

	/* Round up the number of expected sample packets. */
	if (trigger_bytes % PACKET_NUM_SAMPLE_BYTES != 0)
		devc->num_sample_packets++;

	devc->num_enabled_channels = 0;

	/*
	 * Count the number of enabled channels and number them for a sequential
	 * access.
	 */
	for (i = 0, j = 0; i < NUM_CHANNELS; i++) {
		if (devc->channels[i]->enabled) {
			devc->num_enabled_channels++;
			devc->channel_map[j] = i;
			j++;
		}
	}

	sr_dbg("Number of enabled channels: %i.", devc->num_enabled_channels);

	/* Set up the transfer buffer for the acquisition. */
	devc->xfer_data_out[0] = CMD_SAMPLE;
	devc->xfer_data_out[1] = 0x00;

	tmp = GUINT16_TO_LE(devc->pre_trigger_bytes);
	memcpy(devc->xfer_data_out + 2, &tmp, sizeof(tmp));

	tmp = GUINT16_TO_LE(devc->post_trigger_bytes);
	memcpy(devc->xfer_data_out + 4, &tmp, sizeof(tmp));

	devc->xfer_data_out[6] = devc->samplerate_id;
	devc->xfer_data_out[7] = devc->trigger_type;
	devc->xfer_data_out[8] = devc->trigger_channel;
	devc->xfer_data_out[9] = 0x00;

	tmp = GUINT16_TO_LE(devc->after_trigger_delay);
	memcpy(devc->xfer_data_out + 10, &tmp, sizeof(tmp));

	if ((ret = libusb_submit_transfer(devc->xfer_out)) != 0) {
		sr_err("Submit transfer failed: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	usb_source_add(drvc->sr_ctx, 100,
			ikalogic_scanalogic2_receive_data, sdi);

	std_session_send_df_header(sdi, LOG_PREFIX);

	devc->next_state = STATE_SAMPLE;

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	sdi->status = SR_ST_STOPPING;

	return SR_OK;
}

/*--- PXView compat wrappers -----------------------------------------------*/

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *ikalogic_scanalogic2_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver ikalogic_scanalogic2_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int ikalogic_scanalogic2_compat_init(struct sr_context *sr_ctx)
{
	ikalogic_scanalogic2_drv_ptr = &ikalogic_scanalogic2_driver_info;
	return std_init(ikalogic_scanalogic2_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int ikalogic_scanalogic2_compat_cleanup(void)
{
	/* Call dev_clear to free per-device resources before cleanup. */
	std_dev_clear_compat(ikalogic_scanalogic2_drv_ptr);
	return std_cleanup(ikalogic_scanalogic2_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options).
 * The internal scan() already finalizes with std_scan_complete_compat(), so
 * the wrapper only needs to delegate (calling it again would double-register
 * the discovered devices). */
static GSList *ikalogic_scanalogic2_compat_scan(GSList *options)
{
	return scan(ikalogic_scanalogic2_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int ikalogic_scanalogic2_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int ikalogic_scanalogic2_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int ikalogic_scanalogic2_compat_config_list(int info_id,
	GVariant **data, const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int ikalogic_scanalogic2_compat_acquisition_start(
	struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int ikalogic_scanalogic2_compat_acquisition_stop(
	const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver ikalogic_scanalogic2_driver_info = {
	.name = "ikalogic-scanalogic2",
	.longname = "IKALOGIC Scanalogic-2",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = ikalogic_scanalogic2_compat_init,
	.cleanup = ikalogic_scanalogic2_compat_cleanup,
	.scan = ikalogic_scanalogic2_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = ikalogic_scanalogic2_compat_config_get,
	.config_set = ikalogic_scanalogic2_compat_config_set,
	.config_list = ikalogic_scanalogic2_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = ikalogic_scanalogic2_compat_acquisition_start,
	.dev_acquisition_stop = ikalogic_scanalogic2_compat_acquisition_stop,
	.priv = NULL,
};
