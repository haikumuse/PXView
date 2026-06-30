/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2022 Gerhard Sittig <gerhard.sittig@gmx.net>
 * Copyright (C) 2020 Florian Schmidt <schmidt_florian@gmx.de>
 * Copyright (C) 2013 Marcus Comstedt <marcus@mc.pp.se>
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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
#include "protocol.h"

/* USB PID dependent MCU firmware. Model dependent FPGA bitstream. */
#define MCU_FWFILE_FMT	"kingst-la-%04x.fw"
#define FPGA_FWFILE_FMT	"kingst-%s-fpga.bitstream"

/*
 * List of known devices and their features. See @ref kingst_model
 * for the fields' type and meaning. Table is sorted by EEPROM magic.
 * More specific items need to go first (additional byte[2/6]). Not
 * all devices are covered by this driver implementation, but telling
 * users what was detected is considered useful.
 *
 * TODO Verify the identification of models that were not tested before.
 */
static const struct kingst_model models[] = {
	{ 0x02, 0x01, "LA2016", "la2016a1", SR_MHZ(200), 16, 1, 0, },
	{ 0x02, 0x00, "LA2016", "la2016",   SR_MHZ(200), 16, 1, 0, },
	{ 0x03, 0x01, "LA1016", "la1016a1", SR_MHZ(100), 16, 1, 0, },
	{ 0x03, 0x00, "LA1016", "la1016",   SR_MHZ(100), 16, 1, 0, },
	{ 0x04, 0x00, "LA1010", "la1010a0", SR_MHZ(100), 16, 0, SR_MHZ(800), },
	{ 0x05, 0x00, "LA5016", "la5016a1", SR_MHZ(500), 16, 2, SR_MHZ(800), },
	{ 0x06, 0x00, "LA5032", "la5032a0", SR_MHZ(500), 32, 4, SR_MHZ(800), },
	{ 0x07, 0x00, "LA1010", "la1010a1", SR_MHZ(100), 16, 0, SR_MHZ(800), },
	{ 0x08, 0x00, "LA2016", "la2016a1", SR_MHZ(200), 16, 1, 0, },
	{ 0x09, 0x00, "LA1016", "la1016a1", SR_MHZ(100), 16, 1, 0, },
	{ 0x0a, 0x00, "LA1010", "la1010a2", SR_MHZ(100), 16, 0, SR_MHZ(800), },
	{ 0x0b, 0x10, "LA2016", "la2016a2", SR_MHZ(200), 16, 1, 0, },
	{ 0x0c, 0x10, "LA5016", "la5016a2", SR_MHZ(500), 16, 2, SR_MHZ(800), },
	{ 0x0c, 0x00, "LA5016", "la5016a2", SR_MHZ(500), 16, 2, SR_MHZ(800), },
	{ 0x41, 0x00, "LA5016", "la5016a1", SR_MHZ(500), 16, 2, SR_MHZ(800), },
};

/* USB vendor class control requests, executed by the Cypress FX2 MCU. */
#define CMD_FPGA_ENABLE	0x10
#define CMD_FPGA_SPI	0x20	/* R/W access to FPGA registers via SPI. */
#define CMD_BULK_START	0x30	/* Start sample data download via USB EP6 IN. */
#define CMD_BULK_RESET	0x38	/* Flush FIFO of FX2 USB EP6 IN. */
#define CMD_FPGA_INIT	0x50	/* Used before and after FPGA bitstream upload. */
#define CMD_KAUTH	0x60	/* Communicate to auth IC (U10). Not used. */
#define CMD_EEPROM	0xa2	/* R/W access to EEPROM content. */

/*
 * FPGA register addresses (base addresses when registers span multiple
 * bytes, in that case data is kept in little endian format). Passed to
 * CMD_FPGA_SPI requests. The FX2 MCU transparently handles the detail
 * of SPI transfers encoding the read (1) or write (0) direction in the
 * MSB of the address field. There are some 60 byte-wide FPGA registers.
 *
 * Unfortunately the FPGA registers change their meaning between the
 * read and write directions of access, or exclusively provide one of
 * these directions and not the other. This is an arbitrary vendor's
 * choice, there is nothing which the sigrok driver could do about it.
 * Values written to registers typically cannot get read back, neither
 * verified after writing a configuration, nor queried upon startup for
 * automatic detection of the current configuration. Neither appear to
 * be there echo registers for presence and communication checks, nor
 * version identifying registers, as far as we know.
 */
#define REG_RUN		0x00	/* Read capture status, write start capture. */
#define REG_PWM_EN	0x02	/* User PWM channels on/off. */
#define REG_CAPT_MODE	0x03	/* Write 0x00 capture to SDRAM, 0x01 streaming. */
#define REG_PIN_STATE	0x04	/* Read current pin state (real time display). */
#define REG_BULK	0x08	/* Write start addr, byte count to download samples. */
#define REG_SAMPLING	0x10	/* Write capture config, read capture SDRAM location. */
#define REG_TRIGGER	0x20	/* Write level and edge trigger config. */
#define REG_UNKNOWN_30	0x30
#define REG_THRESHOLD	0x68	/* Write PWM config to setup input threshold DAC. */
#define REG_PWM1	0x70	/* Write config for user PWM1. */
#define REG_PWM2	0x78	/* Write config for user PWM2. */

/* Bit patterns to write to REG_CAPT_MODE. */
#define CAPTMODE_TO_RAM	0x00
#define CAPTMODE_STREAM	0x01

/* Bit patterns to write to REG_RUN, setup run mode. */
#define RUNMODE_HALT	0x00
#define RUNMODE_RUN	0x03

/* Bit patterns when reading from REG_RUN, get run state. */
#define RUNSTATE_IDLE_BIT	(1UL << 0)
#define RUNSTATE_DRAM_BIT	(1UL << 1)
#define RUNSTATE_TRGD_BIT	(1UL << 2)
#define RUNSTATE_POST_BIT	(1UL << 3)

static int ctrl_in(const struct sr_dev_inst *sdi,
	uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
	void *data, uint16_t wLength)
{
	struct sr_usb_dev_inst *usb;
	int ret;

	usb = sdi->conn;

	ret = libusb_control_transfer(usb->devhdl,
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
		bRequest, wValue, wIndex, data, wLength,
		DEFAULT_TIMEOUT_MS);
	if (ret != wLength) {
		sr_dbg("USB ctrl in: %d bytes, req %d val %#x idx %d: %s.",
			wLength, bRequest, wValue, wIndex,
			libusb_error_name(ret));
		sr_err("Cannot read %d bytes from USB: %s.",
			wLength, libusb_error_name(ret));
		return SR_ERR_IO;
	}

	return SR_OK;
}

static int ctrl_out(const struct sr_dev_inst *sdi,
	uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
	void *data, uint16_t wLength)
{
	struct sr_usb_dev_inst *usb;
	int ret;

	usb = sdi->conn;

	ret = libusb_control_transfer(usb->devhdl,
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
		bRequest, wValue, wIndex, data, wLength,
		DEFAULT_TIMEOUT_MS);
	if (ret != wLength) {
		sr_dbg("USB ctrl out: %d bytes, req %d val %#x idx %d: %s.",
			wLength, bRequest, wValue, wIndex,
			libusb_error_name(ret));
		sr_err("Cannot write %d bytes to USB: %s.",
			wLength, libusb_error_name(ret));
		return SR_ERR_IO;
	}

	return SR_OK;
}

/* HACK Experiment to spot FPGA registers of interest. */
static void la2016_dump_fpga_registers(const struct sr_dev_inst *sdi,
	const char *caption, size_t reg_lower, size_t reg_upper)
{
	static const size_t dump_chunk_len = 16;

	size_t rdlen;
	uint8_t rdbuf[0x80 - 0x00];	/* Span all FPGA registers. */
	const uint8_t *rdptr;
	int ret;
	size_t dump_addr, indent, dump_len;
	GString *txt;

	if (sr_log_loglevel_get() < SR_LOG_SPEW)
		return;

	if (!reg_lower && !reg_upper) {
		reg_lower = 0;
		reg_upper = sizeof(rdbuf);
	}
	if (reg_upper - reg_lower > sizeof(rdbuf))
		reg_upper = sizeof(rdbuf) - reg_lower;

	rdlen = reg_upper - reg_lower;
	ret = ctrl_in(sdi, CMD_FPGA_SPI, reg_lower, 0, rdbuf, rdlen);
	if (ret != SR_OK) {
		sr_err("Cannot get registers space.");
		return;
	}
	rdptr = rdbuf;

	sr_spew("FPGA registers dump: %s", caption ? : "for fun");
	dump_addr = reg_lower;
	while (rdlen) {
		dump_len = rdlen;
		indent = dump_addr % dump_chunk_len;
		if (dump_len > dump_chunk_len)
			dump_len = dump_chunk_len;
		if (dump_len + indent > dump_chunk_len)
			dump_len = dump_chunk_len - indent;
		txt = sr_hexdump_new(rdptr, dump_len);
		sr_spew("  %04zx  %*s%s",
			dump_addr, (int)(3 * indent), "", txt->str);
		sr_hexdump_free(txt);
		dump_addr += dump_len;
		rdptr += dump_len;
		rdlen -= dump_len;
	}
}

/*
 * Check the necessity for FPGA bitstream upload, because another upload
 * would take some 600ms which is undesirable after program startup. Try
 * to access some FPGA registers and check the values' plausibility. The
 * check should fail on the safe side, request another upload when in
 * doubt. A positive response (the request to continue operation with the
 * currently active bitstream) should be conservative. Accessing multiple
 * registers is considered cheap compared to the cost of bitstream upload.
 *
 * It helps though that both the vendor software and the sigrok driver
 * use the same bundle of MCU firmware and FPGA bitstream for any of the
 * supported models. We don't expect to successfully communicate to the
 * device yet disagree on its protocol. Ideally we would access version
 * identifying registers for improved robustness, but are not aware of
 * any. A bitstream reload can always be forced by a power cycle.
 */
static int check_fpga_bitstream(const struct sr_dev_inst *sdi)
{
	uint8_t init_rsp;
	uint8_t buff[REG_PWM_EN - REG_RUN]; /* Larger of REG_RUN, REG_PWM_EN. */
	int ret;
	uint16_t run_state;
	uint8_t pwm_en;
	size_t read_len;
	const uint8_t *rdptr;

	sr_dbg("Checking operation of the FPGA bitstream.");
	la2016_dump_fpga_registers(sdi, "bitstream check", 0, 0);

	init_rsp = ~0;
	ret = ctrl_in(sdi, CMD_FPGA_INIT, 0x00, 0, &init_rsp, sizeof(init_rsp));
	if (ret != SR_OK || init_rsp != 0) {
		sr_dbg("FPGA init query failed, or unexpected response.");
		return SR_ERR_IO;
	}

	read_len = sizeof(run_state);
	ret = ctrl_in(sdi, CMD_FPGA_SPI, REG_RUN, 0, buff, read_len);
	if (ret != SR_OK) {
		sr_dbg("FPGA register access failed (run state).");
		return SR_ERR_IO;
	}
	rdptr = buff;
	run_state = read_u16le_inc(&rdptr);
	sr_spew("FPGA register: run state 0x%04x.", run_state);
	if (run_state && (run_state & 0x3) != 0x1) {
		sr_dbg("Unexpected FPGA register content (run state).");
		return SR_ERR_DATA;
	}
	if (run_state && (run_state & ~0xf) != 0x85e0) {
		sr_dbg("Unexpected FPGA register content (run state).");
		return SR_ERR_DATA;
	}

	read_len = sizeof(pwm_en);
	ret = ctrl_in(sdi, CMD_FPGA_SPI, REG_PWM_EN, 0, buff, read_len);
	if (ret != SR_OK) {
		sr_dbg("FPGA register access failed (PWM enable).");
		return SR_ERR_IO;
	}
	rdptr = buff;
	pwm_en = read_u8_inc(&rdptr);
	sr_spew("FPGA register: PWM enable 0x%02x.", pwm_en);
	if ((pwm_en & 0x3) != 0x0) {
		sr_dbg("Unexpected FPGA register content (PWM enable).");
		return SR_ERR_DATA;
	}

	sr_info("Could re-use current FPGA bitstream. No upload required.");
	return SR_OK;
}

static int upload_fpga_bitstream(const struct sr_dev_inst *sdi,
	const char *bitstream_fname)
{
	struct sr_dev_driver *di;
	struct compat_drv_context *drvc;
	struct sr_usb_dev_inst *usb;
	struct sr_resource bitstream;
	uint32_t bitstream_size;
	uint8_t buffer[sizeof(uint32_t)];
	uint8_t *wrptr;
	uint8_t block[4096];
	int len, act_len;
	unsigned int pos;
	int ret;
	unsigned int zero_pad_to;

	di = sdi->driver;
	drvc = di->priv;
	usb = sdi->conn;

	sr_info("Uploading FPGA bitstream '%s'.", bitstream_fname);

	ret = sr_resource_open(drvc->sr_ctx, &bitstream,
		SR_RESOURCE_FIRMWARE, bitstream_fname);
	if (ret != SR_OK) {
		sr_err("Cannot find FPGA bitstream %s.", bitstream_fname);
		return ret;
	}

	bitstream_size = (uint32_t)bitstream.size;
	wrptr = buffer;
	write_u32le_inc(&wrptr, bitstream_size);
	ret = ctrl_out(sdi, CMD_FPGA_INIT, 0x00, 0, buffer, wrptr - buffer);
	if (ret != SR_OK) {
		sr_err("Cannot initiate FPGA bitstream upload.");
		sr_resource_close(drvc->sr_ctx, &bitstream);
		return ret;
	}
	zero_pad_to = bitstream_size;
	zero_pad_to += LA2016_EP2_PADDING - 1;
	zero_pad_to /= LA2016_EP2_PADDING;
	zero_pad_to *= LA2016_EP2_PADDING;

	pos = 0;
	while (1) {
		if (pos < bitstream.size) {
			len = (int)sr_resource_read(drvc->sr_ctx, &bitstream,
				block, sizeof(block));
			if (len < 0) {
				sr_err("Cannot read FPGA bitstream.");
				sr_resource_close(drvc->sr_ctx, &bitstream);
				return SR_ERR_IO;
			}
		} else {
			/*  Zero-pad until 'zero_pad_to'. */
			len = zero_pad_to - pos;
			if ((unsigned)len > sizeof(block))
				len = sizeof(block);
			memset(&block, 0, len);
		}
		if (len == 0)
			break;

		ret = libusb_bulk_transfer(usb->devhdl, USB_EP_FPGA_BITSTREAM,
			&block[0], len, &act_len, DEFAULT_TIMEOUT_MS);
		if (ret != 0) {
			sr_dbg("Cannot write FPGA bitstream, block %#x len %d: %s.",
				pos, (int)len, libusb_error_name(ret));
			ret = SR_ERR_IO;
			break;
		}
		if (act_len != len) {
			sr_dbg("Short write for FPGA bitstream, block %#x len %d: got %d.",
				pos, (int)len, act_len);
			ret = SR_ERR_IO;
			break;
		}
		pos += len;
	}
	sr_resource_close(drvc->sr_ctx, &bitstream);
	if (ret != SR_OK)
		return ret;
	sr_info("FPGA bitstream upload (%" PRIu64 " bytes) done.",
		bitstream.size);

	return SR_OK;
}

static int enable_fpga_bitstream(const struct sr_dev_inst *sdi)
{
	int ret;
	uint8_t resp;

	ret