/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
 * Copyright (C) 2012 Uwe Hermann <uwe@hermann-uwe.de>
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
#include <glib.h>
#include <string.h>
#include "protocol.h"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_MULTIMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
};

/*
 * ===========================================================================
 * Variant index enum.
 *
 * Each variant gets a sequential index that is used to look up its descriptor
 * in serial_dmm_devs[]. The SERIAL_DMM_DRV() macro generates per-variant
 * wrapper functions that pass the index to the shared scan/config/acquisition
 * helpers.
 * ===========================================================================
 */
enum {
	/* ES519XX-based variants */
#ifdef HAVE_DMM_PARSER_ES519XX
	SERIAL_DMM_ISO_TECH_IDM103N,
	SERIAL_DMM_TENMA_72_7750_SER,
	SERIAL_DMM_UNI_T_UT60G_SER,
	SERIAL_DMM_UNI_T_UT61E_SER,
#endif
	/* FS9721-based variants */
#ifdef HAVE_DMM_PARSER_FS9721
	SERIAL_DMM_DIGITEK_DT4000ZC,
	SERIAL_DMM_MASTECH_MS8250B,
	SERIAL_DMM_PCE_PCE_DM32,
	SERIAL_DMM_PEAKTECH_3330,
	SERIAL_DMM_TECPEL_DMM_8061_SER,
	SERIAL_DMM_TEKPOWER_TP4000ZC,
	SERIAL_DMM_TENMA_72_7745_SER,
	SERIAL_DMM_UNI_T_UT60A_SER,
	SERIAL_DMM_UNI_T_UT60E_SER,
	SERIAL_DMM_VA_VA18B,
	SERIAL_DMM_VA_VA40B,
	SERIAL_DMM_VOLTCRAFT_VC820_SER,
	SERIAL_DMM_VOLTCRAFT_VC840_SER,
#endif
	/* FS9922-based variants */
#ifdef HAVE_DMM_PARSER_FS9922
	SERIAL_DMM_GWINSTEK_GDM_397,
	SERIAL_DMM_PEAKTECH_2025,
	SERIAL_DMM_SPARKFUN_70C,
	SERIAL_DMM_UNI_T_UT61B_SER,
	SERIAL_DMM_UNI_T_UT61C_SER,
	SERIAL_DMM_UNI_T_UT61D_SER,
	SERIAL_DMM_VOLTCRAFT_VC830_SER,
#endif
	/* UT71X-based variants */
#ifdef HAVE_DMM_PARSER_UT71X
	SERIAL_DMM_TENMA_72_7730_SER,
	SERIAL_DMM_TENMA_72_7732_SER,
	SERIAL_DMM_TENMA_72_9380A_SER,
	SERIAL_DMM_UNI_T_UT71A_SER,
	SERIAL_DMM_UNI_T_UT71B_SER,
	SERIAL_DMM_UNI_T_UT71C_SER,
	SERIAL_DMM_UNI_T_UT71D_SER,
	SERIAL_DMM_UNI_T_UT71E_SER,
	SERIAL_DMM_UNI_T_UT804_SER,
	SERIAL_DMM_VOLTCRAFT_VC920_SER,
	SERIAL_DMM_VOLTCRAFT_VC940_SER,
	SERIAL_DMM_VOLTCRAFT_VC960_SER,
#endif
	/* VC870-based variants */
#ifdef HAVE_DMM_PARSER_VC870
	SERIAL_DMM_VOLTCRAFT_VC870_SER,
#endif
};

/*
 * ===========================================================================
 * Variant descriptor table.
 *
 * Each entry describes one serial-DMM model: vendor/model strings, default
 * serial parameters, packet size, and the parser hooks (packet_valid /
 * packet_parse / dmm_details). Variants whose DMM parser has not been migrated
 * are excluded via #ifdef guards.
 *
 * NOTE: victor-dmm (FS9922-based, conn="hid/victor") is excluded because it
 * is a USB-HID device, not a pure serial device, and PXView's serial compat
 * layer does not handle HID conn paths.
 * ===========================================================================
 */
SR_PRIV const struct dmm_info serial_dmm_devs[] = {
#ifdef HAVE_DMM_PARSER_ES519XX
	{ /* SERIAL_DMM_ISO_TECH_IDM103N */
		"ISO-TECH", "IDM103N", NULL, "2400/7o1/rts=0/dtr=1",
		ES519XX_11B_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_es519xx_2400_11b_packet_valid, sr_es519xx_2400_11b_parse, NULL,
		sizeof(struct es519xx_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_TENMA_72_7750_SER */
		"Tenma", "72-7750 (UT-D02 cable)", NULL, "19200/7o1/rts=0/dtr=1",
		ES519XX_11B_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_es519xx_19200_11b_packet_valid, sr_es519xx_19200_11b_parse, NULL,
		sizeof(struct es519xx_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT60G_SER */
		"UNI-T", "UT60G (UT-D02 cable)", NULL, "19200/7o1/rts=0/dtr=1",
		ES519XX_11B_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_es519xx_19200_11b_packet_valid, sr_es519xx_19200_11b_parse, NULL,
		sizeof(struct es519xx_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT61E_SER */
		"UNI-T", "UT61E (UT-D02 cable)", NULL, "19200/7o1/rts=0/dtr=1",
		ES519XX_14B_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_es519xx_19200_14b_packet_valid, sr_es519xx_19200_14b_parse, NULL,
		sizeof(struct es519xx_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
#endif /* HAVE_DMM_PARSER_ES519XX */

#ifdef HAVE_DMM_PARSER_FS9721
	{ /* SERIAL_DMM_DIGITEK_DT4000ZC */
		"Digitek", "DT4000ZC", NULL, "2400/8n1/dtr=1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, sr_fs9721_10_temp_c,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_MASTECH_MS8250B */
		"MASTECH", "MS8250B", NULL, "2400/8n1/rts=0/dtr=1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, NULL,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_PCE_PCE_DM32 */
		"PCE", "PCE-DM32", NULL, "2400/8n1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, sr_fs9721_01_10_temp_f_c,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_PEAKTECH_3330 */
		"PeakTech", "3330", NULL, "2400/8n1/dtr=1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, sr_fs9721_01_10_temp_f_c,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_TECPEL_DMM_8061_SER */
		"Tecpel", "DMM-8061 (UT-D02 cable)", NULL, "2400/8n1/rts=0/dtr=1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, sr_fs9721_00_temp_c,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_TEKPOWER_TP4000ZC */
		"TekPower", "TP4000ZC", NULL, "2400/8n1/dtr=1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, sr_fs9721_10_temp_c,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_TENMA_72_7745_SER */
		"Tenma", "72-7745 (UT-D02 cable)", NULL, "2400/8n1/rts=0/dtr=1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, sr_fs9721_00_temp_c,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT60A_SER */
		"UNI-T", "UT60A (UT-D02 cable)", NULL, "2400/8n1/rts=0/dtr=1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, NULL,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT60E_SER */
		"UNI-T", "UT60E (UT-D02 cable)", NULL, "2400/8n1/rts=0/dtr=1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, sr_fs9721_00_temp_c,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_VA_VA18B */
		"V&A", "VA18B", NULL, "2400/8n1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, sr_fs9721_01_temp_c,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_VA_VA40B */
		"V&A", "VA40B", NULL, "2400/8n1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, sr_fs9721_max_c_min,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_VOLTCRAFT_VC820_SER */
		"Voltcraft", "VC-820 (UT-D02 cable)", NULL, "2400/8n1/rts=0/dtr=1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, NULL,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_VOLTCRAFT_VC840_SER */
		"Voltcraft", "VC-840 (UT-D02 cable)", NULL, "2400/8n1/rts=0/dtr=1",
		FS9721_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse, sr_fs9721_00_temp_c,
		sizeof(struct fs9721_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
#endif /* HAVE_DMM_PARSER_FS9721 */

#ifdef HAVE_DMM_PARSER_FS9922
	{ /* SERIAL_DMM_GWINSTEK_GDM_397 */
		"GW Instek", "GDM-397", NULL, "2400/8n1/rts=0/dtr=1",
		FS9922_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9922_packet_valid, sr_fs9922_parse, NULL,
		sizeof(struct fs9922_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_PEAKTECH_2025 */
		"PeakTech", "2025", NULL, "2400/8n1/rts=0/dtr=1",
		FS9922_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9922_packet_valid, sr_fs9922_parse, NULL,
		sizeof(struct fs9922_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_SPARKFUN_70C */
		"SparkFun", "70C", NULL, "2400/8n1/rts=0/dtr=1",
		FS9922_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9922_packet_valid, sr_fs9922_parse, NULL,
		sizeof(struct fs9922_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT61B_SER */
		"UNI-T", "UT61B (UT-D02 cable)", NULL, "2400/8n1/rts=0/dtr=1",
		FS9922_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9922_packet_valid, sr_fs9922_parse, NULL,
		sizeof(struct fs9922_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT61C_SER */
		"UNI-T", "UT61C (UT-D02 cable)", NULL, "2400/8n1/rts=0/dtr=1",
		FS9922_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9922_packet_valid, sr_fs9922_parse, NULL,
		sizeof(struct fs9922_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT61D_SER */
		"UNI-T", "UT61D (UT-D02 cable)", NULL, "2400/8n1/rts=0/dtr=1",
		FS9922_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9922_packet_valid, sr_fs9922_parse, NULL,
		sizeof(struct fs9922_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_VOLTCRAFT_VC830_SER */
		/*
		 * Note: The VC830 doesn't set the 'volt' and 'diode' bits of
		 * the FS9922 protocol. Instead, it only sets the user-defined
		 * bit "z1" to indicate "diode mode" and "voltage".
		 */
		"Voltcraft", "VC-830 (UT-D02 cable)", NULL, "2400/8n1/rts=0/dtr=1",
		FS9922_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_fs9922_packet_valid, sr_fs9922_parse, &sr_fs9922_z1_diode,
		sizeof(struct fs9922_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
#endif /* HAVE_DMM_PARSER_FS9922 */

#ifdef HAVE_DMM_PARSER_UT71X
	{ /* SERIAL_DMM_TENMA_72_7730_SER */
		"Tenma", "72-7730 (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_TENMA_72_7732_SER */
		"Tenma", "72-7732 (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_TENMA_72_9380A_SER */
		"Tenma", "72-9380A (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT71A_SER */
		"UNI-T", "UT71A (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT71B_SER */
		"UNI-T", "UT71B (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT71C_SER */
		"UNI-T", "UT71C (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT71D_SER */
		"UNI-T", "UT71D (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT71E_SER */
		"UNI-T", "UT71E (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_UNI_T_UT804_SER */
		"UNI-T", "UT804", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_VOLTCRAFT_VC920_SER */
		"Voltcraft", "VC-920 (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_VOLTCRAFT_VC940_SER */
		"Voltcraft", "VC-940 (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
	{ /* SERIAL_DMM_VOLTCRAFT_VC960_SER */
		"Voltcraft", "VC-960 (UT-D02 cable)", NULL, "2400/7o1/rts=0/dtr=1",
		UT71X_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_ut71x_packet_valid, sr_ut71x_parse, NULL,
		sizeof(struct ut71x_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
#endif /* HAVE_DMM_PARSER_UT71X */

#ifdef HAVE_DMM_PARSER_VC870
	{ /* SERIAL_DMM_VOLTCRAFT_VC870_SER */
		"Voltcraft", "VC-870 (UT-D02 cable)", NULL, "9600/8n1/rts=0/dtr=1",
		VC870_PACKET_SIZE, 0, 0, NULL,
		1, NULL,
		sr_vc870_packet_valid, sr_vc870_parse, NULL,
		sizeof(struct vc870_info),
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
#endif /* HAVE_DMM_PARSER_VC870 */
};

/*
 * ===========================================================================
 * Shared driver functions.
 *
 * These are called by the per-variant wrapper functions generated by the
 * SERIAL_DMM_DRV macro. The variant index (idx) selects the descriptor from
 * serial_dmm_devs[].
 * ===========================================================================
 */

/*
 * Probe a serial port for a valid DMM packet.
 *
 * Adapts the upstream scan() to PXView's serial_stream_detect signature
 * (7-arg: serial, buf, buflen, packet_size, is_valid, timeout_ms, baudrate).
 * PXView's version does not support packet_valid_len or packet_len output,
 * but all migrated variants use fixed-size packets so this is sufficient.
 */
static GSList *serial_dmm_scan(struct sr_dev_driver *di, GSList *options, int idx)
{
	const struct dmm_info *dmm;
	struct sr_config *src;
	GSList *l, *devices;
	const char *conn, *serialcomm;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	int ret;
	uint8_t buf[128];
	size_t len;

	dmm = &serial_dmm_devs[idx];
	conn = dmm->conn;
	serialcomm = dmm->serialcomm;

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
		return NULL;

	serial = sr_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	sr_info("Probing serial port %s.", conn);

	if (dmm->after_open) {
		ret = dmm->after_open(serial);
		if (ret != SR_OK) {
			sr_err("Activity after port open failed: %d.", ret);
			serial_close(serial);
			return NULL;
		}
	}

	devices = NULL;

	/* Request a packet if the DMM requires this. */
	if (dmm->packet_request) {
		if ((ret = dmm->packet_request(serial)) < 0) {
			sr_err("Failed to request packet: %d.", ret);
			serial_close(serial);
			return NULL;
		}
	}

	/*
	 * There's no way to get an ID from the multimeter. It just sends data
	 * periodically (or upon request), so the best we can do is check if
	 * the packets match the expected format.
	 *
	 * PXView's serial_stream_detect takes (serial, buf, &len, packet_size,
	 * packet_valid, timeout_ms, baudrate). The baudrate parameter is unused
	 * by PXView's implementation but required by the signature.
	 */
	len = sizeof(buf);
	ret = serial_stream_detect(serial, buf, &len, dmm->packet_size,
			dmm->packet_valid, 3000, 0);
	if (ret != SR_OK)
		goto scan_cleanup;

	sr_info("Found device on port %s.", conn);

	/* Initialize optional DMM state if the variant needs it. */
	if (dmm->dmm_state_init)
		dmm->dmm_state = dmm->dmm_state_init();

	/* Set up the device instance. */
	sdi = g_malloc0(sizeof(*sdi));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup(dmm->vendor);
	sdi->model = g_strdup(dmm->device);
	devc = g_malloc0(sizeof(*devc));
	sr_sw_limits_init(&devc->limits);
	devc->dmm = dmm;
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	sdi->priv = devc;

	/* Create channels. All migrated variants use a single channel. */
	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "P1");

	devices = g_slist_append(devices, sdi);

scan_cleanup:
	serial_close(serial);

	return std_scan_complete_compat(di, devices);
}

static int serial_dmm_config_get(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	const struct dmm_info *dmm;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_get(&devc->limits, key, data);
	default:
		dmm = devc->dmm;
		if (!dmm || !dmm->config_get)
			return SR_ERR_NA;
		return dmm->config_get(dmm->dmm_state, key, data, sdi, cg);
	}
}

static int serial_dmm_config_set(uint32_t key, GVariant *data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	const struct dmm_info *dmm;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	(void)cg;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	default:
		dmm = devc->dmm;
		if (!dmm || !dmm->config_set)
			return SR_ERR_NA;
		return dmm->config_set(dmm->dmm_state, key, data, sdi, cg);
	}
}

static int serial_dmm_config_list(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	const struct dmm_info *dmm;
	int ret;

	/* Use common logic for standard keys. */
	if (!sdi)
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);

	/*
	 * Check for device-specific config_list handler. ERR_NA from
	 * that handler is non-fatal, just falls back to common logic.
	 */
	dmm = ((struct dev_context *)sdi->priv)->dmm;
	if (dmm && dmm->config_list) {
		ret = dmm->config_list(dmm->dmm_state, key, data, sdi, cg);
		if (ret != SR_ERR_NA)
			return ret;
	}

	return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
}

static int serial_dmm_dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	const struct dmm_info *dmm;
	struct sr_serial_dev_inst *serial;

	devc = sdi->priv;

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, LOG_PREFIX);

	dmm = devc->dmm;
	/*
	 * The acquire_start hook (used by brymen-bm52x etc.) can re-route
	 * the receive callback. Not used by any migrated variant, but the
	 * hook is checked for completeness.
	 */
	if (dmm && dmm->acquire_start) {
		sr_receive_data_callback_t cb_func = serial_dmm_receive_data;
		void *cb_data = (void *)sdi;
		int ret = dmm->acquire_start(dmm->dmm_state, sdi, &cb_func, &cb_data);
		if (ret < 0)
			return ret;
	}

	serial = sdi->conn;
	serial_source_add(serial, G_IO_IN, 50, serial_dmm_receive_data, sdi);

	return SR_OK;
}

/*
 * Local dev_acquisition_stop implementation.
 * PXView does not provide std_serial_dev_acquisition_stop, so we implement
 * the stop sequence locally: remove the serial source, close the serial
 * port, and send the DF_END packet.
 */
static int serial_dmm_dev_acquisition_stop(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;

	serial = sdi->conn;

	serial_source_remove(serial);
	serial_close(serial);

	std_session_send_df_end(sdi, LOG_PREFIX);

	return SR_OK;
}

/*
 * ===========================================================================
 * SERIAL_DMM_DRV macro.
 *
 * Generates PXView-compatible wrapper functions and the driver_info struct
 * for one serial-DMM variant. Pattern follows mic-985xx's MIC_DRV macro.
 *
 * ID        - C identifier prefix for the variant (e.g. iso_tech_idm103n)
 * IDX       - enum value indexing into serial_dmm_devs[]
 * NAME      - short driver name string (e.g. "iso-tech-idm103n")
 * LONGNAME  - long driver name string (e.g. "ISO-TECH IDM103N")
 * ===========================================================================
 */
#define SERIAL_DMM_DRV(ID, IDX, NAME, LONGNAME) \
\
static int ID##_compat_init(struct sr_context *sr_ctx) \
{ \
	return std_init(&ID##_driver_info, sr_ctx); \
} \
\
static int ID##_compat_cleanup(void) \
{ \
	return std_cleanup(&ID##_driver_info); \
} \
\
static GSList *ID##_compat_scan(GSList *options) \
{ \
	return serial_dmm_scan(&ID##_driver_info, options, IDX); \
} \
\
static int ID##_compat_config_get(int id, GVariant **data, \
		const struct sr_dev_inst *sdi, const struct sr_channel *ch, \
		const struct sr_channel_group *cg) \
{ \
	(void)ch; \
	return serial_dmm_config_get((uint32_t)id, data, sdi, cg); \
} \
\
static int ID##_compat_config_set(int id, GVariant *data, \
		struct sr_dev_inst *sdi, struct sr_channel *ch, \
		struct sr_channel_group *cg) \
{ \
	(void)ch; \
	return serial_dmm_config_set((uint32_t)id, data, sdi, cg); \
} \
\
static int ID##_compat_config_list(int info_id, GVariant **data, \
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg) \
{ \
	return serial_dmm_config_list((uint32_t)info_id, data, sdi, cg); \
} \
\
static int ID##_compat_acquisition_start(struct sr_dev_inst *sdi, \
		void *cb_data) \
{ \
	(void)cb_data; \
	return serial_dmm_dev_acquisition_start(sdi); \
} \
\
static int ID##_compat_acquisition_stop(const struct sr_dev_inst *sdi, \
		void *cb_data) \
{ \
	(void)cb_data; \
	return serial_dmm_dev_acquisition_stop(sdi); \
} \
\
struct sr_dev_driver ID##_driver_info = { \
	.name = NAME, \
	.longname = LONGNAME, \
	.api_version = 1, \
	.driver_type = DRIVER_TYPE_HARDWARE, \
	.init = ID##_compat_init, \
	.cleanup = ID##_compat_cleanup, \
	.scan = ID##_compat_scan, \
	.dev_mode_list = compat_dev_mode_list_default, \
	.config_get = ID##_compat_config_get, \
	.config_set = ID##_compat_config_set, \
	.config_list = ID##_compat_config_list, \
	.dev_open = std_serial_dev_open, \
	.dev_close = std_serial_dev_close, \
	.dev_destroy = compat_dev_destroy_default, \
	.dev_status_get = compat_dev_status_get_default, \
	.dev_acquisition_start = ID##_compat_acquisition_start, \
	.dev_acquisition_stop = ID##_compat_acquisition_stop, \
	.priv = NULL, \
};

/*
 * ===========================================================================
 * Per-variant driver_info generation.
 *
 * Each line generates a complete set of wrapper functions + driver_info struct
 * for one variant. Guarded by the corresponding DMM parser macro so that
 * variants without a migrated parser are excluded.
 * ===========================================================================
 */

#ifdef HAVE_DMM_PARSER_ES519XX
SERIAL_DMM_DRV(iso_tech_idm103n, SERIAL_DMM_ISO_TECH_IDM103N,
		"iso-tech-idm103n", "ISO-TECH IDM103N")
SERIAL_DMM_DRV(tenma_72_7750_ser, SERIAL_DMM_TENMA_72_7750_SER,
		"tenma-72-7750-ser", "Tenma 72-7750")
SERIAL_DMM_DRV(uni_t_ut60g_ser, SERIAL_DMM_UNI_T_UT60G_SER,
		"uni-t-ut60g-ser", "UNI-T UT60G")
SERIAL_DMM_DRV(uni_t_ut61e_ser, SERIAL_DMM_UNI_T_UT61E_SER,
		"uni-t-ut61e-ser", "UNI-T UT61E")
#endif /* HAVE_DMM_PARSER_ES519XX */

#ifdef HAVE_DMM_PARSER_FS9721
SERIAL_DMM_DRV(digitek_dt4000zc, SERIAL_DMM_DIGITEK_DT4000ZC,
		"digitek-dt4000zc", "Digitek DT4000ZC")
SERIAL_DMM_DRV(mastech_ms8250b, SERIAL_DMM_MASTECH_MS8250B,
		"mastech-ms8250b", "MASTECH MS8250B")
SERIAL_DMM_DRV(pce_pce_dm32, SERIAL_DMM_PCE_PCE_DM32,
		"pce-pce-dm32", "PCE PCE-DM32")
SERIAL_DMM_DRV(peaktech_3330, SERIAL_DMM_PEAKTECH_3330,
		"peaktech-3330", "PeakTech 3330")
SERIAL_DMM_DRV(tecpel_dmm_8061_ser, SERIAL_DMM_TECPEL_DMM_8061_SER,
		"tecpel-dmm-8061-ser", "Tecpel DMM-8061")
SERIAL_DMM_DRV(tekpower_tp4000zc, SERIAL_DMM_TEKPOWER_TP4000ZC,
		"tekpower-tp4000ZC", "TekPower TP4000ZC")
SERIAL_DMM_DRV(tenma_72_7745_ser, SERIAL_DMM_TENMA_72_7745_SER,
		"tenma-72-7745-ser", "Tenma 72-7745")
SERIAL_DMM_DRV(uni_t_ut60a_ser, SERIAL_DMM_UNI_T_UT60A_SER,
		"uni-t-ut60a-ser", "UNI-T UT60A")
SERIAL_DMM_DRV(uni_t_ut60e_ser, SERIAL_DMM_UNI_T_UT60E_SER,
		"uni-t-ut60e-ser", "UNI-T UT60E")
SERIAL_DMM_DRV(va_va18b, SERIAL_DMM_VA_VA18B,
		"va-va18b", "V&A VA18B")
SERIAL_DMM_DRV(va_va40b, SERIAL_DMM_VA_VA40B,
		"va-va40b", "V&A VA40B")
SERIAL_DMM_DRV(voltcraft_vc820_ser, SERIAL_DMM_VOLTCRAFT_VC820_SER,
		"voltcraft-vc820-ser", "Voltcraft VC-820")
SERIAL_DMM_DRV(voltcraft_vc840_ser, SERIAL_DMM_VOLTCRAFT_VC840_SER,
		"voltcraft-vc840-ser", "Voltcraft VC-840")
#endif /* HAVE_DMM_PARSER_FS9721 */

#ifdef HAVE_DMM_PARSER_FS9922
SERIAL_DMM_DRV(gwinstek_gdm_397, SERIAL_DMM_GWINSTEK_GDM_397,
		"gwinstek-gdm-397", "GW Instek GDM-397")
SERIAL_DMM_DRV(peaktech_2025, SERIAL_DMM_PEAKTECH_2025,
		"peaktech-2025", "PeakTech 2025")
SERIAL_DMM_DRV(sparkfun_70c, SERIAL_DMM_SPARKFUN_70C,
		"sparkfun-70c", "SparkFun 70C")
SERIAL_DMM_DRV(uni_t_ut61b_ser, SERIAL_DMM_UNI_T_UT61B_SER,
		"uni-t-ut61b-ser", "UNI-T UT61B")
SERIAL_DMM_DRV(uni_t_ut61c_ser, SERIAL_DMM_UNI_T_UT61C_SER,
		"uni-t-ut61c-ser", "UNI-T UT61C")
SERIAL_DMM_DRV(uni_t_ut61d_ser, SERIAL_DMM_UNI_T_UT61D_SER,
		"uni-t-ut61d-ser", "UNI-T UT61D")
SERIAL_DMM_DRV(voltcraft_vc830_ser, SERIAL_DMM_VOLTCRAFT_VC830_SER,
		"voltcraft-vc830-ser", "Voltcraft VC-830")
#endif /* HAVE_DMM_PARSER_FS9922 */

#ifdef HAVE_DMM_PARSER_UT71X
SERIAL_DMM_DRV(tenma_72_7730_ser, SERIAL_DMM_TENMA_72_7730_SER,
		"tenma-72-7730-ser", "Tenma 72-7730")
SERIAL_DMM_DRV(tenma_72_7732_ser, SERIAL_DMM_TENMA_72_7732_SER,
		"tenma-72-7732-ser", "Tenma 72-7732")
SERIAL_DMM_DRV(tenma_72_9380a_ser, SERIAL_DMM_TENMA_72_9380A_SER,
		"tenma-72-9380a-ser", "Tenma 72-9380A")
SERIAL_DMM_DRV(uni_t_ut71a_ser, SERIAL_DMM_UNI_T_UT71A_SER,
		"uni-t-ut71a-ser", "UNI-T UT71A")
SERIAL_DMM_DRV(uni_t_ut71b_ser, SERIAL_DMM_UNI_T_UT71B_SER,
		"uni-t-ut71b-ser", "UNI-T UT71B")
SERIAL_DMM_DRV(uni_t_ut71c_ser, SERIAL_DMM_UNI_T_UT71C_SER,
		"uni-t-ut71c-ser", "UNI-T UT71C")
SERIAL_DMM_DRV(uni_t_ut71d_ser, SERIAL_DMM_UNI_T_UT71D_SER,
		"uni-t-ut71d-ser", "UNI-T UT71D")
SERIAL_DMM_DRV(uni_t_ut71e_ser, SERIAL_DMM_UNI_T_UT71E_SER,
		"uni-t-ut71e-ser", "UNI-T UT71E")
SERIAL_DMM_DRV(uni_t_ut804_ser, SERIAL_DMM_UNI_T_UT804_SER,
		"uni-t-ut804-ser", "UNI-T UT804")
SERIAL_DMM_DRV(voltcraft_vc920_ser, SERIAL_DMM_VOLTCRAFT_VC920_SER,
		"voltcraft-vc920-ser", "Voltcraft VC-920")
SERIAL_DMM_DRV(voltcraft_vc940_ser, SERIAL_DMM_VOLTCRAFT_VC940_SER,
		"voltcraft-vc940-ser", "Voltcraft VC-940")
SERIAL_DMM_DRV(voltcraft_vc960_ser, SERIAL_DMM_VOLTCRAFT_VC960_SER,
		"voltcraft-vc960-ser", "Voltcraft VC-960")
#endif /* HAVE_DMM_PARSER_UT71X */

#ifdef HAVE_DMM_PARSER_VC870
SERIAL_DMM_DRV(voltcraft_vc870_ser, SERIAL_DMM_VOLTCRAFT_VC870_SER,
		"voltcraft-vc870-ser", "Voltcraft VC-870")
#endif /* HAVE_DMM_PARSER_VC870 */
