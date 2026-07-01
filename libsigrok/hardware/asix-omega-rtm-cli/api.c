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
 * This sigrok driver implementation uses the vendor's CLI application
 * for the ASIX OMEGA to operate the device in real time mode. The
 * external process handles the device detection, USB communication
 * (FTDI FIFO), FPGA netlist download, and device control. The process'
 * stdout provides a continuous RLE compressed stream of 16bit samples
 * taken at 200MHz.
 *
 * Known limitations: The samplerate is fixed. Hardware triggers are not
 * available in this mode. The start of the acquisition takes a few
 * seconds, but the device's native protocol is unknown and its firmware
 * is unavailable, so that a native sigrok driver is in some distant
 * future. Users need to initiate the acquisition in sigrok early so
 * that the device is capturing when the event of interest happens.
 *
 * The vendor application's executable either must be named omegartmcli
 * and must be found in PATH, or the OMEGARTMCLI environment variable
 * must contain its location. A scan option could be used when a
 * suitable SR_CONF key gets identified which communicates executable
 * locations.
 *
 * When multiple devices are connected, then a conn=sn=... specification
 * can select one of the devices. The serial number should contain six
 * or eight hex digits (this follows the vendor's approach for the CLI
 * application).
 */

/* Rule 1: replace <config.h> with the PXView compat header. */
#include "hardware/compat/compat.h"

#include <stdlib.h>
#include <string.h>

#include "protocol.h"

static const char *channel_names[] = {
	"1", "2", "3", "4", "5", "6", "7", "8",
	"9", "10", "11", "12", "13", "14", "15", "16",
};

static const uint64_t samplerates[] = {
	SR_MHZ(200),
};

static const uint32_t scanopts[] = {
	SR_CONF_CONN, /* Accepts serial number specs. */
};

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_CONN | SR_CONF_GET,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_LIST,
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	const char *conn, *probe_names, *serno, *exe;
	GSList *devices, *l;
	struct sr_config *src;
	size_t argc, chmax, chidx;
	gchar **argv, *output, *vers_text, *eol;
	GSpawnFlags flags;
	GError *error;
	gboolean ok;
	char serno_buff[10];
	struct sr_dev_inst *sdi;
	struct dev_context *devc;

	/*
	 * Extract optional serial number from conn= spec.
	 *
	 * PXView's libsigrok does not provide sr_serial_extract_options().
	 * Manually walk the options GSList and pick up SR_CONF_CONN (which
	 * for this driver is a "sn=SERNO" spec, not a serial port path) and
	 * the optional SR_CONF_PROBE_NAMES comma-separated user spec.
	 */
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
	(void)probe_names; /* See channel name allocation below. */
	if (!conn || !*conn)
		conn = NULL;
	serno = NULL;
	if (conn) {
		if (!g_str_has_prefix(conn, "sn=")) {
			sr_err("conn= must specify a serial number.");
			return NULL;
		}
		serno = conn + strlen("sn=");
		if (!*serno)
			serno = NULL;
	}
	if (serno)
		sr_dbg("User specified serial number: %s", serno);
	if (serno && strlen(serno) == 4) {
		sr_dbg("Adding 03 prefix to user specified serial number");
		snprintf(serno_buff, sizeof(serno_buff), "03%s", serno);
		serno = serno_buff;
	}
	if (serno && strlen(serno) != 6 && strlen(serno) != 8) {
		sr_err("Serial number must be 03xxxx or A603xxxx");
		serno = NULL;
	}

	devices = NULL;

	/*
	 * Check availability of the external executable. Notice that
	 * failure is non-fatal, the scan can take place even when users
	 * don't request and don't expect to use Asix Omega devices.
	 */
	exe = getenv("OMEGARTMCLI");
	if (!exe || !*exe)
		exe = "omegartmcli";
	sr_dbg("Vendor application executable: %s", exe);
	argv = g_malloc0(5 * sizeof(argv[0]));
	argc = 0;
	argv[argc++] = g_strdup(exe);
	argv[argc++] = g_strdup("-version");
	argv[argc++] = NULL;
	flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL;
	output = NULL;
	error = NULL;
	ok = g_spawn_sync(NULL, argv, NULL, flags, NULL, NULL,
		&output, NULL, NULL, &error);
	if (error && error->code != G_SPAWN_ERROR_NOENT)
		sr_err("Cannot execute RTM CLI process: %s", error->message);
	if (error) {
		ok = FALSE;
		g_error_free(error);
	}
	if (!output || !*output)
		ok = FALSE;
	if (!ok) {
		sr_dbg("External RTM CLI execution failed.");
		g_free(output);
		g_strfreev(argv);
		return NULL;
	}

	/*
	 * Get the executable's version from second stdout line. This
	 * only executes when the executable is found, failure to get
	 * the version information is considered fatal.
	 */
	vers_text = strstr(output, "Version ");
	if (!vers_text)
		ok = FALSE;
	if (ok) {
		vers_text += strlen("Version ");
		eol = strchr(vers_text, '\n');
		if (eol)
			*eol = '\0';
		eol = strchr(vers_text, '\r');
		if (eol)
			*eol = '\0';
		if (!vers_text || !*vers_text)
			ok = FALSE;
	}
	if (!ok) {
		sr_err("Cannot get RTM CLI executable's version.");
		g_free(output);
		g_strfreev(argv);
		return NULL;
	}
	sr_info("RTM CLI executable version: %s", vers_text);

	/*
	 * Create a device instance, add it to the result set. Create a
	 * device context. Change the -version command into the command
	 * for acquisition for later use in the driver's lifetime.
	 */
	sdi = g_malloc0(sizeof(*sdi));
	devices = g_slist_append(devices, sdi);
	sdi->status = SR_ST_INITIALIZING;
	sdi->vendor = g_strdup("ASIX");
	sdi->model = g_strdup("OMEGA RTM CLI");
	sdi->version = g_strdup(vers_text);
	if (serno)
		sdi->serial_num = g_strdup(serno);
	if (conn)
		sdi->connection_id = g_strdup(conn);
	devc = g_malloc0(sizeof(*devc));
	sdi->priv = devc;

	/*
	 * Build the channel names array.
	 *
	 * PXView's compat layer provides sr_parse_probe_names() as a stub
	 * with an incompatible signature (returns void, takes a struct
	 * sr_channel ** instead of returning a freshly allocated char **).
	 * Follow the kingst-la2016 pattern and build the array manually
	 * from the default channel_names table. The optional user-supplied
	 * probe_names comma-separated spec is not honoured here (matches
	 * what kingst-la2016 does in its compat conversion).
	 */
	chmax = ARRAY_SIZE(channel_names);
	devc->channel_names = g_malloc0(sizeof(char *) * (chmax + 1));
	for (chidx = 0; chidx < chmax; chidx++) {
		devc->channel_names[chidx] = g_strdup(channel_names[chidx]);
		sr_channel_new(sdi, chidx, SR_CHANNEL_LOGIC,
			TRUE, devc->channel_names[chidx]);
	}

	sr_sw_limits_init(&devc->limits);
	argc = 1;
	g_free(argv[argc]);
	argv[argc++] = g_strdup("-bin");
	if (serno) {
		argv[argc++] = g_strdup("-serial");
		argv[argc++] = g_strdup(serno);
	}
	argv[argc++] = NULL;
	devc->child.argv = argv;
	devc->child.flags = flags | G_SPAWN_CLOEXEC_PIPES;
	devc->child.fd_stdin_write = -1;
	devc->child.fd_stdout_read = -1;

	/* Rule 4: std_scan_complete -> std_scan_complete_compat. */
	return std_scan_complete_compat(di, devices);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;

	switch (key) {
	case SR_CONF_CONN:
		if (!sdi->connection_id)
			return SR_ERR_NA;
		*data = g_variant_new_string(sdi->connection_id);
		break;
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(samplerates[0]);
		break;
	case SR_CONF_LIMIT_MSEC:
	case SR_CONF_LIMIT_SAMPLES:
		return sr_sw_limits_config_get(&devc->limits, key, data);
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_MSEC:
	case SR_CONF_LIMIT_SAMPLES:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		if (cg)
			return SR_ERR_NA;
		return STD_CONFIG_LIST(key, data, sdi, cg,
			scanopts, drvopts, devopts);
	case SR_CONF_SAMPLERATE:
		*data = std_gvar_samplerates(ARRAY_AND_SIZE(samplerates));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;
	int fd, events;
	uint64_t remain_count;

	devc = sdi->priv;

	/* Start the external acquisition process. */
	ret = omega_rtm_cli_open(sdi);
	if (ret != SR_OK)
		return ret;
	fd = devc->child.fd_stdout_read;
	events = G_IO_IN | G_IO_ERR;

	/*
	 * Start supervising acquisition limits. Arrange for a stricter
	 * "samples count" check than supported by the common approach.
	 */
	sr_sw_limits_acquisition_start(&devc->limits);
	ret = sr_sw_limits_get_remain(&devc->limits,
		&remain_count, NULL, NULL, NULL);
	if (ret != SR_OK)
		return ret;
	if (remain_count) {
		devc->samples.remain_count = remain_count;
		devc->samples.check_count = TRUE;
	}

	/* Rule 2: std_session_send_df_header(sdi) -> 2-arg form. */
	ret = std_session_send_df_header(sdi, LOG_PREFIX);
	if (ret != SR_OK)
		return ret;

	/*
	 * Start processing the external process' output.
	 *
	 * Rule 14: PXView's sr_session_source_add() is 5-arg (no session
	 * first parameter) and the callback is sr_receive_data_callback_t
	 * which takes (int fd, int revents, const struct sr_dev_inst *sdi).
	 * Pass sdi directly (no (void *) cast) -- the callback signature
	 * accepts it verbatim.
	 */
	ret = sr_session_source_add(fd, events, 10,
		omega_rtm_cli_receive_data, sdi);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;
	int fd;

	devc = sdi->priv;

	/*
	 * Implementor's note: Do run all stop activities even if
	 * some of them may fail. Emit diagnostics messages as errors
	 * are seen, but don't return early.
	 */

	/*
	 * Stop processing the external process' output.
	 *
	 * Rule 14: PXView's sr_session_source_remove() is 1-arg (no
	 * session first parameter).
	 */
	fd = devc->child.fd_stdout_read;
	if (fd >= 0) {
		ret = sr_session_source_remove(fd);
		if (ret != SR_OK) {
			sr_err("Cannot stop reading acquisition data");
		}
	}

	/* Rule 2: std_session_send_df_end(sdi) -> 2-arg form. */
	ret = std_session_send_df_end(sdi, LOG_PREFIX);
	(void)ret;

	ret = omega_rtm_cli_close(sdi);
	if (ret != SR_OK) {
		sr_err("Could not terminate acquisition process");
	}
	(void)ret;

	return SR_OK;
}

/*
 * The upstream driver used std_dummy_dev_open/std_dummy_dev_close (no-ops)
 * for the dev_open/dev_close callbacks. PXView does not provide these
 * helpers, so provide local no-op stand-ins. The device's child process
 * is started/stopped in dev_acquisition_start/stop; open/close have
 * nothing to do here.
 */
static int asix_omega_rtm_cli_dev_open(struct sr_dev_inst *sdi)
{
	(void)sdi;
	return SR_OK;
}

static int asix_omega_rtm_cli_dev_close(struct sr_dev_inst *sdi)
{
	(void)sdi;
	return SR_OK;
}

/* ===========================================================================
 * PXView compat wrapper layer.
 *
 * PXView's sr_dev_driver callbacks have different signatures from standard
 * sigrok's (int vs uint32_t key, extra ch parameter, cb_data in acquisition
 * start/stop, no dev_list/dev_clear fields). These thin wrappers adapt the
 * standard sigrok callbacks above to PXView's expected signatures.
 *
 * Rule 13: the driver_info struct is named asix_omega_rtm_cli_driver_info
 * (non-static, with an extern forward declaration) and a static driver
 * pointer `asix_omega_rtm_cli_drv_ptr` is kept so the wrappers can pass
 * the driver to std_init/std_cleanup/scan.
 * ========================================================================== */

/* Rule 13: static driver pointer for wrapper use. */
static struct sr_dev_driver *asix_omega_rtm_cli_drv_ptr;

/* Rule 13: forward declaration - defined at end of file. */
extern SR_PRIV struct sr_dev_driver asix_omega_rtm_cli_driver_info;

/* Rule 9: Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx). */
static int asix_omega_rtm_cli_compat_init(struct sr_context *sr_ctx)
{
	asix_omega_rtm_cli_drv_ptr = &asix_omega_rtm_cli_driver_info;
	return std_init(asix_omega_rtm_cli_drv_ptr, sr_ctx);
}

/* Rule 9: Wrapper: PXView cleanup(void) -> standard cleanup(driver). */
static int asix_omega_rtm_cli_compat_cleanup(void)
{
	return std_cleanup(asix_omega_rtm_cli_drv_ptr);
}

/* Rule 9: Wrapper: PXView scan(options) -> standard scan(driver, options). */
static GSList *asix_omega_rtm_cli_compat_scan(GSList *options)
{
	return scan(asix_omega_rtm_cli_drv_ptr, options);
}

/*
 * Rule 9: Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard
 * (key,data,sdi,cg). The original driver has no per-channel config_get
 * logic, so ch is dropped.
 */
static int asix_omega_rtm_cli_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/*
 * Rule 9 + Rule 10: Wrapper: PXView config_set(id,data,sdi,ch,cg) ->
 * standard (key,data,sdi,cg). The original driver has no per-channel
 * config_set logic (no .config_channel_set callback), so the ch != NULL
 * branch is empty and ch is dropped.
 */
static int asix_omega_rtm_cli_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Rule 9: Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard
 * (key,data,sdi,cg). */
static int asix_omega_rtm_cli_compat_config_list(int info_id,
	GVariant **data, const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/*
 * Rule 9: Wrapper: PXView acquisition_start(sdi, cb_data) ->
 * standard(sdi). cb_data is dropped.
 */
static int asix_omega_rtm_cli_compat_acquisition_start(
	struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/*
 * Rule 9: Wrapper: PXView acquisition_stop(const sdi, cb_data) ->
 * standard(sdi). cb_data is dropped; sdi is made non-const for the
 * internal call.
 */
static int asix_omega_rtm_cli_compat_acquisition_stop(
	const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/*
 * Rule 8 + Rule 13: PXView-compatible driver info struct.
 *
 * - .context = NULL -> .priv = NULL
 * - removed .dev_list/.dev_clear/.config_channel_set (PXView handles
 *   these via the compat layer's std_init/std_cleanup wrappers)
 * - added .dev_mode_list = NULL, .dev_destroy = NULL, .dev_status_get = NULL
 *   (use the compat layer's default no-op implementations)
 * - added .driver_type = DRIVER_TYPE_HARDWARE
 * - .dev_open/.dev_close point to the local no-op stand-ins above
 *   (the upstream used std_dummy_dev_open/std_dummy_dev_close which PXView
 *   does not provide)
 *
 * Rule 7: SR_REGISTER_DEV_DRIVER() is removed -- PXView registers the
 * driver via hwdriver.c's driver table (maintained by the main agent).
 */
struct sr_dev_driver asix_omega_rtm_cli_driver_info = {
	.name = "asix-omega-rtm-cli",
	.longname = "ASIX OMEGA RTM CLI",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = asix_omega_rtm_cli_compat_init,
	.cleanup = asix_omega_rtm_cli_compat_cleanup,
	.scan = asix_omega_rtm_cli_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = asix_omega_rtm_cli_compat_config_get,
	.config_set = asix_omega_rtm_cli_compat_config_set,
	.config_list = asix_omega_rtm_cli_compat_config_list,
	.dev_open = asix_omega_rtm_cli_dev_open,
	.dev_close = asix_omega_rtm_cli_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = asix_omega_rtm_cli_compat_acquisition_start,
	.dev_acquisition_stop = asix_omega_rtm_cli_compat_acquisition_stop,
	.priv = NULL,
};
