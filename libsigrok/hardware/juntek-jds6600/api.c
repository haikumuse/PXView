/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2023 Gerhard Sittig <gerhard.sittig@gmx.net>
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
 * PXView port of the libsigrok juntek-jds6600 driver API layer.
 *
 * Migration notes (see AGENTS.md / task spec):
 * - Rule 1: includes replaced by "hardware/compat/compat.h".
 * - Rule 6: sr_dev_inst_new uses the manual g_malloc0 + field assignment
 *   pattern (conrad-digi-35-cpu style), not the compat_sr_dev_inst_new
 *   wrapper, because the original sigrok source already used the manual
 *   pattern and the device needs sdi->conn / sdi->connection_id set up
 *   before identification.
 * - Rule 7: SR_REGISTER_DEV_DRIVER removed; the driver_info struct is
 *   defined directly (extern linkage so hwdriver.c can reference it).
 * - Rule 8: 8 compat wrappers (.init, .cleanup, .scan, .config_get,
 *   .config_set, .config_list, .dev_acquisition_start, .dev_acquisition_stop)
 *   adapt PXView signatures; .dev_list/.dev_clear are not fields in PXView's
 *   sr_dev_driver struct (cleanup invokes jds6600_dev_clear internally).
 * - Rule 9: config_channel_set merged into config_set (ch param dropped).
 * - Rule 11: sr_config_set_compat is provided by compat.h when needed.
 * - Rule 12: driver_info named juntek_jds6600_driver_info.
 * - Rule 14 / point 1: .dev_acquisition_start/_stop point to the local
 *   std_dummy_dev_acquisition_start/_stop no-ops from protocol.c.
 * - Driver-specific point 3: scan manually walks the options GSList for
 *   SR_CONF_CONN / SR_CONF_SERIALCOMM (PXView lacks sr_serial_extract_options).
 * - Driver-specific point 5: no analog datafeed, no sr_analog_init.
 * - std_gvar_min_max_step_array(arr) -> std_gvar_min_max_step(arr[0],arr[1],arr[2])
 *   (PXView compat provides the 3-arg form, not the array form).
 * - std_gvar_array_str(strs,count) -> std_gvar_tuple_array(strs,count)
 *   (PXView compat provides the latter, both produce GVariant "as").
 * - std_str_idx(data,strs,count) setter helper: PXView's compat.h declares a
 *   different std_str_idx signature (getter form). Local jds6600_str_idx
 *   helper provides the "find index of string in array" semantics the
 *   SR_CONF_PATTERN_MODE setter needs.
 */

#include "hardware/compat/compat.h"
#include <string.h>
#include "protocol.h"

#define DFLT_SERIALCOMM	"115200/8n1"

#define VENDOR_TEXT	"Juntek"
#define MODEL_TEXT	"JDS6600"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_SIGNAL_GENERATOR,
};

static const uint32_t devopts[] = {
	SR_CONF_CONN | SR_CONF_GET,
	SR_CONF_ENABLED | SR_CONF_SET,
	SR_CONF_PHASE | SR_CONF_GET | SR_CONF_SET,
};

static const uint32_t devopts_cg[] = {
	SR_CONF_ENABLED | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_PATTERN_MODE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_OUTPUT_FREQUENCY | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_AMPLITUDE | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_OFFSET | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_DUTY_CYCLE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

/*
 * Local "find index of string GVariant in array" helper (setter form).
 *
 * Standard sigrok's std_str_idx(data, strs, count) returns the index of the
 * string carried by GVariant @data within the @strs array, or -1 when not
 * found. PXView's compat.h declares a different std_str_idx signature
 * (getter form: std_str_idx(sdi, key, data, strs, count)), so a local helper
 * is required for the SR_CONF_PATTERN_MODE setter path.
 */
static int jds6600_str_idx(GVariant *data, const char *const strs[], size_t count)
{
	const char *s;
	size_t i;

	if (!data || !strs)
		return -1;
	s = g_variant_get_string(data, NULL);
	if (!s)
		return -1;
	for (i = 0; i < count; i++) {
		if (!strs[i])
			continue;
		if (g_strcmp0(strs[i], s) == 0)
			return (int)i;
	}
	return -1;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *devices;
	const char *conn, *serialcomm;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_serial_dev_inst *ser;
	int ret;
	size_t ch_idx, idx, ch_nr;
	char cg_name[8];
	struct sr_channel_group *cg;
	struct sr_channel *ch;
	GSList *l;
	struct sr_config *src;

	devices = NULL;

	/*
	 * PXView's libsigrok does not provide sr_serial_extract_options(),
	 * so manually walk the options GSList looking for SR_CONF_CONN and
	 * SR_CONF_SERIALCOMM (same pattern as conrad-digi-35-cpu, atorch,
	 * hp-59306a). Driver-specific point 3.
	 */
	conn = NULL;
	serialcomm = DFLT_SERIALCOMM;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return devices;

	ser = sr_serial_dev_inst_new(conn, serialcomm);
	if (!ser)
		return devices;
	ret = serial_open(ser, SERIAL_RDWR);
	if (ret != SR_OK) {
		sr_serial_dev_inst_free(ser);
		return devices;
	}

	/*
	 * Rule 6: sr_dev_inst_new pattern follows conrad-digi-35-cpu
	 * (manual g_malloc0 + field assignment). The device needs sdi->conn
	 * and sdi->connection_id set up before jds6600_identify() can talk
	 * to the firmware.
	 */
	sdi = g_malloc0(sizeof(*sdi));
	sdi->status = SR_ST_INACTIVE;
	sdi->inst_type = SR_INST_USB;
	sdi->conn = ser;
	sdi->connection_id = g_strdup(conn);
	devc = g_malloc0(sizeof(*devc));
	sdi->priv = devc;

	ret = jds6600_identify(sdi);
	if (ret != SR_OK)
		goto fail;
	ret = jds6600_setup_devc(sdi);
	if (ret != SR_OK)
		goto fail;
	(void)serial_close(ser);

	sdi->vendor = g_strdup(VENDOR_TEXT);
	sdi->model = g_strdup(MODEL_TEXT);
	if (devc->device.serial_number)
		sdi->serial_num = g_strdup(devc->device.serial_number);

	ch_idx = 0;
	for (idx = 0; idx < MAX_GEN_CHANNELS; idx++) {
		ch_nr = idx + 1;
		snprintf(cg_name, sizeof(cg_name), "CH%zu", ch_nr);
		cg = sr_channel_group_new(sdi, cg_name, NULL);
		(void)cg;
		ch = sr_channel_new(sdi, ch_idx,
			SR_CHANNEL_ANALOG, FALSE, cg_name);
		cg->channels = g_slist_append(cg->channels, ch);
		ch_idx++;
	}

	devices = g_slist_append(devices, sdi);
	/*
	 * Rule 4: scan_complete_compat (PXView provides std_scan_complete_compat
	 * in compat_driver.h, not std_scan_complete).
	 */
	return std_scan_complete_compat(di, devices);

fail:
	(void)serial_close(ser);
	sr_serial_dev_inst_free(ser);
	if (devc) {
		g_free(devc->device.serial_number);
		g_free(devc->waveforms.fw_codes);
		g_free(devc->waveforms.names);
	}
	g_free(devc);
	sr_dev_inst_free(sdi);

	return devices;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	int ret;
	size_t cg_idx;
	struct devc_wave *waves;
	struct devc_chan *chan;
	double dvalue;
	const char *s;

	devc = sdi ? sdi->priv : NULL;
	if (!cg) {
		switch (key) {
		case SR_CONF_CONN:
			if (!sdi->connection_id)
				return SR_ERR_NA;
			*data = g_variant_new_string(sdi->connection_id);
			return SR_OK;
		case SR_CONF_PHASE:
			if (!devc)
				return SR_ERR_NA;
			ret = jds6600_get_phase_chans(sdi);
			if (ret != SR_OK)
				return SR_ERR_NA;
			dvalue = devc->channels_phase;
			*data = g_variant_new_double(dvalue);
			return SR_OK;
		default:
			return SR_ERR_NA;
		}
	}

	if (!devc)
		return SR_ERR_NA;
	ret = g_slist_index(sdi->channel_groups, cg);
	if (ret < 0)
		return SR_ERR_NA;
	cg_idx = (size_t)ret;
	if (cg_idx >= ARRAY_SIZE(devc->channel_config))
		return SR_ERR_NA;
	chan = &devc->channel_config[cg_idx];

	switch (key) {
	case SR_CONF_ENABLED:
		ret = jds6600_get_chans_enable(sdi);
		if (ret != SR_OK)
			return SR_ERR_NA;
		*data = g_variant_new_boolean(chan->enabled);
		return SR_OK;
	case SR_CONF_PATTERN_MODE:
		ret = jds6600_get_waveform(sdi, cg_idx);
		if (ret != SR_OK)
			return SR_ERR_NA;
		waves = &devc->waveforms;
		s = waves->names[chan->waveform_index];
		*data = g_variant_new_string(s);
		return SR_OK;
	case SR_CONF_OUTPUT_FREQUENCY:
		ret = jds6600_get_frequency(sdi, cg_idx);
		if (ret != SR_OK)
			return SR_ERR_NA;
		dvalue = chan->output_frequency;
		*data = g_variant_new_double(dvalue);
		return SR_OK;
	case SR_CONF_AMPLITUDE:
		ret = jds6600_get_amplitude(sdi, cg_idx);
		if (ret != SR_OK)
			return SR_ERR_NA;
		dvalue = chan->amplitude;
		*data = g_variant_new_double(dvalue);
		return SR_OK;
	case SR_CONF_OFFSET:
		ret = jds6600_get_offset(sdi, cg_idx);
		if (ret != SR_OK)
			return SR_ERR_NA;
		dvalue = chan->offset;
		*data = g_variant_new_double(dvalue);
		return SR_OK;
	case SR_CONF_DUTY_CYCLE:
		ret = jds6600_get_dutycycle(sdi, cg_idx);
		if (ret != SR_OK)
			return SR_ERR_NA;
		dvalue = chan->dutycycle;
		*data = g_variant_new_double(dvalue);
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	struct devc_wave *waves;
	struct devc_chan *chan;
	size_t cg_idx;
	double dvalue;
	gboolean on;
	int ret, idx;

	devc = sdi ? sdi->priv : NULL;

	if (!cg) {
		switch (key) {
		case SR_CONF_ENABLED:
			/* Enable/disable all channels at the same time. */
			on = g_variant_get_boolean(data);
			if (!devc)
				return SR_ERR_ARG;
			cg_idx = devc->device.channel_count_gen;
			while (cg_idx) {
				chan = &devc->channel_config[--cg_idx];
				chan->enabled = on;
			}
			ret = jds6600_set_chans_enable(sdi);
			if (ret != SR_OK)
				return SR_ERR_NA;
			return SR_OK;
		case SR_CONF_PHASE:
			if (!devc)
				return SR_ERR_ARG;
			dvalue = g_variant_get_double(data);
			devc->channels_phase = dvalue;
			ret = jds6600_set_phase_chans(sdi);
			if (ret != SR_OK)
				return SR_ERR_NA;
			return SR_OK;
		default:
			return SR_ERR_NA;
		}
	}

	ret = g_slist_index(sdi->channel_groups, cg);
	if (ret < 0)
		return SR_ERR_NA;
	cg_idx = (size_t)ret;
	if (cg_idx >= ARRAY_SIZE(devc->channel_config))
		return SR_ERR_NA;
	chan = &devc->channel_config[cg_idx];

	switch (key) {
	case SR_CONF_ENABLED:
		on = g_variant_get_boolean(data);
		chan->enabled = on;
		ret = jds6600_set_chans_enable(sdi);
		if (ret != SR_OK)
			return SR_ERR_NA;
		return SR_OK;
	case SR_CONF_PATTERN_MODE:
		waves = &devc->waveforms;
		idx = jds6600_str_idx(data, waves->names, waves->names_count);
		if (idx < 0)
			return SR_ERR_NA;
		if ((size_t)idx >= waves->names_count)
			return SR_ERR_NA;
		chan->waveform_index = idx;
		chan->waveform_code = waves->fw_codes[chan->waveform_index];
		ret = jds6600_set_waveform(sdi, cg_idx);
		if (ret != SR_OK)
			return SR_ERR_NA;
		return SR_OK;
	case SR_CONF_OUTPUT_FREQUENCY:
		dvalue = g_variant_get_double(data);
		chan->output_frequency = dvalue;
		ret = jds6600_set_frequency(sdi, cg_idx);
		if (ret != SR_OK)
			return SR_ERR_NA;
		return SR_OK;
	case SR_CONF_AMPLITUDE:
		dvalue = g_variant_get_double(data);
		chan->amplitude = dvalue;
		ret = jds6600_set_amplitude(sdi, cg_idx);
		if (ret != SR_OK)
			return SR_ERR_NA;
		return SR_OK;
	case SR_CONF_OFFSET:
		dvalue = g_variant_get_double(data);
		chan->offset = dvalue;
		ret = jds6600_set_offset(sdi, cg_idx);
		if (ret != SR_OK)
			return SR_ERR_NA;
		return SR_OK;
	case SR_CONF_DUTY_CYCLE:
		dvalue = g_variant_get_double(data);
		chan->dutycycle = dvalue;
		ret = jds6600_set_dutycycle(sdi, cg_idx);
		if (ret != SR_OK)
			return SR_ERR_NA;
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	struct devc_wave *waves;
	double fspec[3];

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

	if (!sdi)
		return SR_ERR_NA;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_NA;
	switch (key) {
	case SR_CONF_DEVICE_OPTIONS:
		*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg));
		return SR_OK;
	case SR_CONF_PATTERN_MODE:
		waves = &devc->waveforms;
		/*
		 * PXView compat provides std_gvar_tuple_array() (produces
		 * GVariant "as") instead of standard sigrok's
		 * std_gvar_array_str(). Both have identical semantics.
		 */
		*data = std_gvar_array_str(waves->names, waves->names_count);
		return SR_OK;
	case SR_CONF_OUTPUT_FREQUENCY:
		/* Announce range as tuple of min, max, step. */
		fspec[0] = 0.01;
		fspec[1] = devc->device.max_output_frequency;
		fspec[2] = 0.01;
		/*
		 * PXView compat provides std_gvar_min_max_step(min,max,step)
		 * instead of standard sigrok's std_gvar_min_max_step_array(arr).
		 */
		*data = std_gvar_min_max_step(fspec[0], fspec[1], fspec[2]);
		return SR_OK;
	case SR_CONF_DUTY_CYCLE:
		/* Announce range as tuple of min, max, step. */
		fspec[0] = 0.0;
		fspec[1] = 1.0;
		fspec[2] = 0.001;
		*data = std_gvar_min_max_step(fspec[0], fspec[1], fspec[2]);
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

/* ===========================================================================
 * PXView compat wrapper layer (Rules 7, 8, 9, 12, 14).
 *
 * PXView's sr_dev_driver callbacks have different signatures from standard
 * sigrok's: init/cleanup/scan lack the driver parameter, config_get/set/list
 * use int keys and carry an extra sr_channel parameter, and
 * dev_acquisition_start/stop carry an extra cb_data parameter. These thin
 * wrappers adapt PXView signatures to the standard sigrok-style functions
 * defined above.
 *
 * The JDS6600 is a signal generator that does not stream sample data, so the
 * acquisition_start/_stop wrappers delegate to the local no-ops from
 * protocol.c (std_dummy_dev_acquisition_start/_stop).
 *
 * cleanup invokes jds6600_dev_clear first to release each devc's waveform
 * names / codes / quick_req / serial_number buffers before the driver
 * context is torn down (atorch pattern).
 * ==========================================================================*/

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *juntek_jds6600_drv_ptr;

/* Forward declaration - defined at end of file (Rule 7). */
extern struct sr_dev_driver juntek_jds6600_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int juntek_jds6600_compat_init(struct sr_context *sr_ctx)
{
	juntek_jds6600_drv_ptr = &juntek_jds6600_driver_info;
	return std_init(juntek_jds6600_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver).
 * Invoke jds6600_dev_clear first to release each devc's private resources
 * before std_cleanup tears down the driver context. */
static int juntek_jds6600_compat_cleanup(void)
{
	jds6600_dev_clear(juntek_jds6600_drv_ptr);
	return std_cleanup(juntek_jds6600_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *juntek_jds6600_compat_scan(GSList *options)
{
	return scan(juntek_jds6600_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg).
 * Rule 9: ch parameter dropped (no per-channel config_get logic). */
static int juntek_jds6600_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg).
 * Rule 9: config_channel_set merged into config_set; ch parameter dropped. */
static int juntek_jds6600_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int juntek_jds6600_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/*
 * Wrapper: PXView acquisition_start(sdi, cb_data).
 * Rule 14 / point 1: delegates to the local std_dummy_dev_acquisition_start
 * no-op (signal generator, no datafeed).
 */
static int juntek_jds6600_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return std_dummy_dev_acquisition_start(sdi);
}

/*
 * Wrapper: PXView acquisition_stop(const sdi, cb_data).
 * Rule 14 / point 1: delegates to the local std_dummy_dev_acquisition_stop
 * no-op (signal generator, no datafeed).
 */
static int juntek_jds6600_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return std_dummy_dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/*
 * Rule 12: driver_info named juntek_jds6600_driver_info.
 * Rule 8: PXView-compatible fields filled in.
 */
struct sr_dev_driver juntek_jds6600_driver_info = {
	.name = "juntek-jds6600",
	.longname = "JUNTEK JDS6600",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = juntek_jds6600_compat_init,
	.cleanup = juntek_jds6600_compat_cleanup,
	.scan = juntek_jds6600_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = juntek_jds6600_compat_config_get,
	.config_set = juntek_jds6600_compat_config_set,
	.config_list = juntek_jds6600_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = juntek_jds6600_compat_acquisition_start,
	.dev_acquisition_stop = juntek_jds6600_compat_acquisition_stop,
	.priv = NULL,
};
