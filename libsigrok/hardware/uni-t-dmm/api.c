/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012-2013 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "hardware/compat/compat.h"
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_MULTIMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_SET | SR_CONF_GET,
	SR_CONF_LIMIT_MSEC | SR_CONF_SET | SR_CONF_GET,
};

/*
 * Note 1: The actual baudrate of the Cyrustek ES519xx chip used in this DMM
 * is 19230. However, the WCH CH9325 chip (UART to USB/HID) used in (some
 * versions of) the UNI-T UT-D04 cable doesn't support 19230 baud. It only
 * supports 19200, and setting an unsupported baudrate will result in the
 * default of 2400 being used (which will not work with this DMM, of course).
 */

/*
 * Supported DMM model table.
 *
 * Upstream sigrok registers one driver per model (each model is its own
 * sr_dev_driver with the parser baked in via the embedded dmm_info). PXView
 * exposes a single uni_t_dmm driver, so the model descriptors are kept here
 * as plain data. scan() selects the active model (currently the first entry,
 * the es519xx-based UT60G/72-7750 class) and stores it in dev_context.
 *
 * The ID column from the upstream DMM() macro is preserved as a comment for
 * traceability.
 */
#define DMM(CHIPSET, VENDOR, MODEL, BAUDRATE, PACKETSIZE, \
		VALID, PARSE, DETAILS) \
	{ \
		VENDOR, MODEL, BAUDRATE, PACKETSIZE, \
		VALID, PARSE, DETAILS, sizeof(struct CHIPSET##_info) \
	}

static const struct dmm_info uni_t_dmm_models[] = {
	/* {{{ es519xx */
	/* tenma-72-7750 */
	DMM(es519xx, "Tenma", "72-7750", 19200,
		/* The baudrate is actually 19230, see "Note 1" above. */
		ES519XX_11B_PACKET_SIZE,
		sr_es519xx_19200_11b_packet_valid, sr_es519xx_19200_11b_parse,
		NULL),
	/* uni-t-ut60g */
	DMM(es519xx, "UNI-T", "UT60G", 19200,
		/* The baudrate is actually 19230, see "Note 1" above. */
		ES519XX_11B_PACKET_SIZE,
		sr_es519xx_19200_11b_packet_valid, sr_es519xx_19200_11b_parse,
		NULL),
	/* uni-t-ut61e */
	DMM(es519xx, "UNI-T", "UT61E", 19200,
		/* The baudrate is actually 19230, see "Note 1" above. */
		ES519XX_14B_PACKET_SIZE,
		sr_es519xx_19200_14b_packet_valid, sr_es519xx_19200_14b_parse,
		NULL),
	/* }}} */
	/* {{{ fs9721 */
	/* tecpel-dmm-8061 */
	DMM(fs9721, "Tecpel", "DMM-8061", 2400,
		FS9721_PACKET_SIZE,
		sr_fs9721_packet_valid, sr_fs9721_parse,
		sr_fs9721_00_temp_c),
	/* tenma-72-7745 */
	DMM(fs9721, "Tenma", "72-7745", 2400,
		FS9721_PACKET_SIZE,
		sr_fs9721_packet_valid, sr_fs9721_parse,
		sr_fs9721_00_temp_c),
	/* uni-t-ut60a */
	DMM(fs9721, "UNI-T", "UT60A", 2400,
		FS9721_PACKET_SIZE,
		sr_fs9721_packet_valid, sr_fs9721_parse,
		NULL),
	/* uni-t-ut60e */
	DMM(fs9721, "UNI-T", "UT60E", 2400,
		FS9721_PACKET_SIZE,
		sr_fs9721_packet_valid, sr_fs9721_parse,
		sr_fs9721_00_temp_c),
	/* voltcraft-vc820 */
	DMM(fs9721, "Voltcraft", "VC-820", 2400,
		FS9721_PACKET_SIZE,
		sr_fs9721_packet_valid, sr_fs9721_parse,
		NULL),
	/* voltcraft-vc840 */
	DMM(fs9721, "Voltcraft", "VC-840", 2400,
		FS9721_PACKET_SIZE,
		sr_fs9721_packet_valid, sr_fs9721_parse,
		sr_fs9721_00_temp_c),
	/* }}} */
	/* {{{ fs9922 */
	/* uni-t-ut61b */
	DMM(fs9922, "UNI-T", "UT61B", 2400,
		FS9922_PACKET_SIZE,
		sr_fs9922_packet_valid, sr_fs9922_parse,
		NULL),
	/* uni-t-ut61c */
	DMM(fs9922, "UNI-T", "UT61C", 2400,
		FS9922_PACKET_SIZE,
		sr_fs9922_packet_valid, sr_fs9922_parse,
		NULL),
	/* uni-t-ut61d */
	DMM(fs9922, "UNI-T", "UT61D", 2400,
		FS9922_PACKET_SIZE,
		sr_fs9922_packet_valid, sr_fs9922_parse,
		NULL),
	/* voltcraft-vc830 */
	DMM(fs9922, "Voltcraft", "VC-830", 2400,
		/*
		 * Note: The VC830 doesn't set the 'volt' and 'diode' bits of
		 * the FS9922 protocol. Instead, it only sets the user-defined
		 * bit "z1" to indicate "diode mode" and "voltage".
		 */
		FS9922_PACKET_SIZE,
		sr_fs9922_packet_valid, sr_fs9922_parse,
		&sr_fs9922_z1_diode),
	/* }}} */
	/*
	 * ut372 entry disabled: the ut372 parser (sr_ut372_packet_valid /
	 * sr_ut372_parse) was never migrated into PXView (no dmm/ut372.c).
	 * Re-enable when the ut372 parser is ported.
	 */
	/* {{{ ut71x */
	/* tenma-72-7730 */
	DMM(ut71x, "Tenma", "72-7730", 2400,
		UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* tenma-72-7732 */
	DMM(ut71x, "Tenma", "72-7732", 2400,
		UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* tenma-72-9380a */
	DMM(ut71x, "Tenma", "72-9380A", 2400,
		UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* uni-t-ut71a */
	DMM(ut71x, "UNI-T", "UT71A", 2400, UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* uni-t-ut71b */
	DMM(ut71x, "UNI-T", "UT71B", 2400, UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* uni-t-ut71c */
	DMM(ut71x, "UNI-T", "UT71C", 2400, UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* uni-t-ut71d */
	DMM(ut71x, "UNI-T", "UT71D", 2400, UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* uni-t-ut71e */
	DMM(ut71x, "UNI-T", "UT71E", 2400, UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* uni-t-ut804 */
	DMM(ut71x, "UNI-T", "UT804", 2400, UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* voltcraft-vc920 */
	DMM(ut71x, "Voltcraft", "VC-920", 2400, UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* voltcraft-vc940 */
	DMM(ut71x, "Voltcraft", "VC-940", 2400, UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* voltcraft-vc960 */
	DMM(ut71x, "Voltcraft", "VC-960", 2400, UT71X_PACKET_SIZE,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL),
	/* }}} */
	/* {{{ vc870 */
	/* voltcraft-vc870 */
	DMM(vc870, "Voltcraft", "VC-870", 9600, VC870_PACKET_SIZE,
		sr_vc870_packet_valid, sr_vc870_parse, NULL),
	/* }}} */
};

#undef DMM

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *usb_devices, *devices, *l;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct drv_context *drvc;
	const struct dmm_info *dmm;
	struct sr_usb_dev_inst *usb;
	struct sr_config *src;
	const char *conn;

	drvc = di->priv;
	/* PXView exposes a single driver; use the first model as default. */
	dmm = &uni_t_dmm_models[0];

	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return NULL;

	devices = NULL;
	if (!(usb_devices = sr_usb_find(drvc->sr_ctx->libusb_ctx, conn))) {
		g_slist_free_full(usb_devices, g_free);
		return NULL;
	}

	for (l = usb_devices; l; l = l->next) {
		usb = l->data;
		devc = g_malloc0(sizeof(struct dev_context));
		devc->first_run = TRUE;
		devc->dmm = dmm;
		sr_sw_limits_init(&devc->limits);
		sdi = g_malloc0(sizeof(struct sr_dev_inst));
		sdi->status = SR_ST_INACTIVE;
		sdi->vendor = g_strdup(dmm->vendor);
		sdi->model = g_strdup(dmm->device);
		sdi->priv = devc;
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "P1");
		sdi->inst_type = SR_INST_USB;
		sdi->conn = usb;
		devices = g_slist_append(devices, sdi);
	}

	return std_scan_complete_compat(di, devices);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct sr_dev_driver *di;
	struct drv_context *drvc;
	struct sr_usb_dev_inst *usb;

	di = sdi->driver;
	drvc = di->priv;
	usb = sdi->conn;

	return sr_usb_open(drvc->sr_ctx->libusb_ctx, usb);
}

/*
 * The upstream driver used std_dummy_dev_close (a no-op) with a TODO marker.
 * PXView does not provide std_dummy_dev_close, so a local no-op stands in for
 * it. The HID interface is claimed in hid_chip_init() during acquisition; a
 * real release/close path can be filled in here later.
 */
static int dev_close(struct sr_dev_inst *sdi)
{
	(void)sdi;
	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	devc = sdi->priv;

	return sr_sw_limits_config_set(&devc->limits, key, data);
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return std_config_list(key, data, sdi, cg,
		scanopts, ARRAY_SIZE(scanopts),
		drvopts, ARRAY_SIZE(drvopts),
		devopts, ARRAY_SIZE(devopts));
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	sr_sw_limits_acquisition_start(&devc->limits);

	std_session_send_df_header(sdi, LOG_PREFIX);

	/*
	 * PXView's sr_session_source_add() takes (poll_object, events,
	 * timeout, cb, sdi) and tracks the session via the sdi back-reference.
	 */
	sr_session_source_add(-1, 0, 10,
			uni_t_dmm_receive_data, sdi);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	std_session_send_df_end(sdi, NULL);
	sr_session_source_remove(-1);

	return SR_OK;
}

/* ===========================================================================
 * PXView compat wrapper layer
 * ===========================================================================
 */

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *uni_t_dmm_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver uni_t_dmm_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int uni_t_dmm_compat_init(struct sr_context *sr_ctx)
{
	uni_t_dmm_drv_ptr = &uni_t_dmm_driver_info;
	return std_init(uni_t_dmm_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int uni_t_dmm_compat_cleanup(void)
{
	return std_cleanup(uni_t_dmm_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *uni_t_dmm_compat_scan(GSList *options)
{
	return scan(uni_t_dmm_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int uni_t_dmm_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	(void)sdi;
	(void)cg;
	(void)data;
	(void)id;
	/* No config_get implementation in original driver */
	return SR_ERR_NA;
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int uni_t_dmm_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int uni_t_dmm_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int uni_t_dmm_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int uni_t_dmm_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver uni_t_dmm_driver_info = {
	.name = "uni-t-dmm",
	.longname = "uni-t-dmm (UNI-T/DreamSourceLab USB-HID DMMs)",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = uni_t_dmm_compat_init,
	.cleanup = uni_t_dmm_compat_cleanup,
	.scan = uni_t_dmm_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = uni_t_dmm_compat_config_get,
	.config_set = uni_t_dmm_compat_config_set,
	.config_list = uni_t_dmm_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = uni_t_dmm_compat_acquisition_start,
	.dev_acquisition_stop = uni_t_dmm_compat_acquisition_stop,
	.priv = NULL,
};
