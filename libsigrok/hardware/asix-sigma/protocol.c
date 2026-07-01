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
#include <stdio.h>
#include "protocol.h"

/*
 * The ASIX SIGMA hardware supports fixed 200MHz and 100MHz sample rates
 * (by means of separate firmware images). As well as 50MHz divided by
 * an integer divider in the 1..256 range (by the "typical" firmware).
 * Which translates to a strict lower boundary of around 195kHz.
 *
 * This driver "suggests" a subset of the available rates by listing a
 * few discrete values, while setter routines accept any user specified
 * rate that is supported by the hardware.
 */
static const uint64_t samplerates[] = {
	/* 50MHz and integer divider. 1/2/5 steps (where possible). */
	SR_KHZ(200), SR_KHZ(500),
	SR_MHZ(1), SR_MHZ(2), SR_MHZ(5),
	SR_MHZ(10), SR_MHZ(25), SR_MHZ(50),
	/* 100MHz/200MHz, fixed rates in special firmware. */
	SR_MHZ(100), SR_MHZ(200),
};

SR_PRIV GVariant *sigma_get_samplerates_list(void)
{
	return std_gvar_samplerates(samplerates, ARRAY_SIZE(samplerates));
}

static const char *firmware_files[] = {
	[SIGMA_FW_50MHZ] = "asix-sigma-50.fw", /* 50MHz, 8bit divider. */
	[SIGMA_FW_100MHZ] = "asix-sigma-100.fw", /* 100MHz, fixed. */
	[SIGMA_FW_200MHZ] = "asix-sigma-200.fw", /* 200MHz, fixed. */
	[SIGMA_FW_SYNC] = "asix-sigma-50sync.fw", /* Sync from external pin. */
	[SIGMA_FW_FREQ] = "asix-sigma-phasor.fw", /* Frequency counter. */
};

#define SIGMA_FIRMWARE_SIZE_LIMIT (256 * 1024)

static int sigma_ftdi_open(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int vid, pid;
	const char *serno;
	int ret;

	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	if (devc->ftdi.is_open)
		return SR_OK;

	vid = devc->id.vid;
	pid = devc->id.pid;
	serno = sdi->serial_num;
	if (!vid || !pid || !serno || !*serno)
		return SR_ERR_ARG;

	ret = ftdi_init(&devc->ftdi.ctx);
	if (ret < 0) {
		sr_err("Cannot initialize FTDI context (%d): %s.",
			ret, ftdi_get_error_string(&devc->ftdi.ctx));
		return SR_ERR_IO;
	}
	ret = ftdi_usb_open_desc_index(&devc->ftdi.ctx,
		vid, pid, NULL, serno, 0);
	if (ret < 0) {
		sr_err("Cannot open device (%d): %s.",
			ret, ftdi_get_error_string(&devc->ftdi.ctx));
		return SR_ERR_IO;
	}
	devc->ftdi.is_open = TRUE;

	return SR_OK;
}

static int sigma_ftdi_close(struct dev_context *devc)
{
	int ret;

	ret = ftdi_usb_close(&devc->ftdi.ctx);
	devc->ftdi.is_open = FALSE;
	devc->ftdi.must_close = FALSE;
	ftdi_deinit(&devc->ftdi.ctx);

	return ret == 0 ? SR_OK : SR_ERR_IO;
}

SR_PRIV int sigma_check_open(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	if (devc->ftdi.is_open)
		return SR_OK;

	ret = sigma_ftdi_open(sdi);
	if (ret != SR_OK)
		return ret;
	devc->ftdi.must_close = TRUE;

	return ret;
}

SR_PRIV int sigma_check_close(struct dev_context *devc)
{
	int ret;

	if (!devc)
		return SR_ERR_ARG;

	if (devc->ftdi.must_close) {
		ret = sigma_ftdi_close(devc);
		if (ret != SR_OK)
			return ret;
		devc->ftdi.must_close = FALSE;
	}

	return SR_OK;
}

SR_PRIV int sigma_force_open(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	ret = sigma_ftdi_open(sdi);
	if (ret != SR_OK)
		return ret;
	devc->ftdi.must_close = FALSE;

	return SR_OK;
}

SR_PRIV int sigma_force_close(struct dev_context *devc)
{
	return sigma_ftdi_close(devc);
}

/*
 * BEWARE! Error propagation is important, as are kinds of return values.
 *
 * - Raw USB tranport communicates the number of sent or received bytes,
 *   or negative error codes in the external library's(!) range of codes.
 * - Internal routines at the "sigrok driver level" communicate success
 *   or failure in terms of SR_OK et al error codes.
 * - Main loop style receive callbacks communicate booleans which arrange
 *   for repeated calls to drive progress during acquisition.
 *
 * Careful consideration by maintainers is essential, because all of the
 * above kinds of values are assignment compatbile from the compiler's
 * point of view. Implementation errors will go unnoticed at build time.
 */

static int sigma_read_raw(struct dev_context *devc, void *buf, size_t size)
{
	int ret;

	ret = ftdi_read_data(&devc->ftdi.ctx, (unsigned char *)buf, size);
	if (ret < 0) {
		sr_err("USB data read failed: %s",
			ftdi_get_error_string(&devc->ftdi.ctx));
	}

	return ret;
}

static int sigma_write_raw(struct dev_context *devc, const void *buf, size_t size)
{
	int ret;

	ret = ftdi_write_data(&devc->ftdi.ctx, buf, size);
	if (ret < 0) {
		sr_err("USB data write failed: %s",
			ftdi_get_error_string(&devc->ftdi.ctx));
	} else if ((size_t)ret != size) {
		sr_err("USB data write length mismatch.");
	}

	return ret;
}

static int sigma_read_sr(struct dev_context *devc, void *buf, size_t size)
{
	int ret;

	ret = sigma_read_raw(devc, buf, size);
	if (ret < 0 || (size_t)ret != size)
		return SR_ERR_IO;

	return SR_OK;
}

static int sigma_write_sr(struct dev_context *devc, const void *buf, size_t size)
{
	int ret;

	ret = sigma_write_raw(devc, buf, size);
	if (ret < 0 || (size_t)ret != size)
		return SR_ERR_IO;

	return SR_OK;
}

/*
 * Implementor's note: The local write buffer's size shall suffice for
 * any know FPGA register transaction that is involved in the supported
 * feature set of this sigrok device driver. If the length check trips,
 * that's a programmer's error and needs adjustment in the complete call
 * stack of the respective code path.
 */
#define SIGMA_MAX_REG_DEPTH	32

/*
 * Implementor's note: The FPGA command set supports register access
 * with automatic address adjustment. This operation is documented to
 * wrap within a 16-address range, it cannot cross boundaries where the
 * register address' nibble overflows. An internal helper assumes that
 * callers remain within this auto-adjustment range, and thus multi
 * register access requests can never exceed that count.
 */
#define SIGMA_MAX_REG_COUNT	16

/*
 * Byte read/write helpers local to this TU.
 *
 * read_u8_inc, read_u16le_inc, write_u8_inc, write_u16le_inc are provided by
 * compat_config.h (included via compat.h) and are NOT redefined here.
 *
 * write_u16be_inc and read_u24le_inc are NOT provided by the compat layer
 * (compat_config.h only ships LE _inc variants plus write_u24le_inc), so this
 * driver keeps its own local copies. They are static inline to avoid clashing
 * with any future driver that may define the same names.
 */
static inline void write_u16be_inc(uint8_t **ptr, uint16_t val) {
	**ptr = (val >> 8) & 0xff;
	(*ptr)++;
	**ptr = val & 0xff;
	(*ptr)++;
}

static inline uint32_t read_u24le_inc(const uint8_t **ptr) {
	uint32_t val = **ptr;
	(*ptr)++;
	val |= ((uint32_t)(**ptr)) << 8;
	(*ptr)++;
	val |= ((uint32_t)(**ptr)) << 16;
	(*ptr)++;
	return val;
}

SR_PRIV int sigma_write_register(struct dev_context *devc,
	uint8_t reg, uint8_t *data, size_t len)
{
	uint8_t buf[2 + SIGMA_MAX_REG_DEPTH * 2], *wrptr;
	size_t idx;

	if (len > SIGMA_MAX_REG_DEPTH) {
		sr_err("Short write buffer for %zu bytes to reg %u.", len, reg);
		return SR_ERR_BUG;
	}

	wrptr = buf;
	write_u8_inc(&wrptr, REG_ADDR_LOW | LO4(reg));
	write_u8_inc(&wrptr, REG_ADDR_HIGH | HI4(reg));
	for (idx = 0; idx < len; idx++) {
		write_u8_inc(&wrptr, REG_DATA_LOW | LO4(data[idx]));
		write_u8_inc(&wrptr, REG_DATA_HIGH_WRITE | HI4(data[idx]));
	}

	return sigma_write_sr(devc, buf, wrptr - buf);
}

SR_PRIV int sigma_set_register(struct dev_context *devc,
	uint8_t reg, uint8_t value)
{
	return sigma_write_register(devc, reg, &value, sizeof(value));
}

static int sigma_read_register(struct dev_context *devc,
	uint8_t reg, uint8_t *data, size_t len)
{
	uint8_t buf[3], *wrptr;
	int ret;

	wrptr = buf;
	write_u8_inc(&wrptr, REG_ADDR_LOW | LO4(reg));
	write_u8_inc(&wrptr, REG_ADDR_HIGH | HI4(reg));
	write_u8_inc(&wrptr, REG_READ_ADDR);
	ret = sigma_write_sr(devc, buf, wrptr - buf);
	if (ret != SR_OK)
		return ret;

	return sigma_read_sr(devc, data, len);
}

static int sigma_get_register(struct dev_context *devc,
	uint8_t reg, uint8_t *data)
{
	return sigma_read_register(devc, reg, data, sizeof(*data));
}

static int sigma_get_registers(struct dev_context *devc,
	uint8_t reg, uint8_t *data, size_t count)
{
	uint8_t buf[2 + SIGMA_MAX_REG_COUNT], *wrptr;
	size_t idx;
	int ret;

	if (count > SIGMA_MAX_REG_COUNT) {
		sr_err("Short command buffer for %zu reg reads at %u.", count, reg);
		return SR_ERR_BUG;
	}

	wrptr = buf;
	write_u8_inc(&wrptr, REG_ADDR_LOW | LO4(reg));
	write_u8_inc(&wrptr, REG_ADDR_HIGH | HI4(reg));
	for (idx = 0; idx < count; idx++)
		write_u8_inc(&wrptr, REG_READ_ADDR | REG_ADDR_INC);
	ret = sigma_write_sr(devc, buf, wrptr - buf);
	if (ret != SR_OK)
		return ret;

	return sigma_read_sr(devc, data, count);
}

static int sigma_read_pos(struct dev_context *devc,
	uint32_t *stoppos, uint32_t *triggerpos, uint8_t *mode)
{
	uint8_t result[7];
	const uint8_t *rdptr;
	uint32_t v32;
	uint8_t v8;
	int ret;

	/*
	 * Read 7 registers starting at trigger position LSB.
	 * Which yields two 24bit counter values, and mode flags.
	 */
	ret = sigma_get_registers(devc, READ_TRIGGER_POS_LOW,
		result, sizeof(result));
	if (ret != SR_OK)
		return ret;

	rdptr = &result[0];
	v32 = read_u24le_inc(&rdptr);
	if (triggerpos)
		*triggerpos = v32;
	v32 = read_u24le_inc(&rdptr);
	if (stoppos)
		*stoppos = v32;
	v8 = read_u8_inc(&rdptr);
	if (mode)
		*mode = v8;

	/*
	 * These positions consist of "the memory row" in the MSB fields,
	 * and "an event index" within the row in the LSB fields. Part
	 * of the memory row's content is sample data, another part is
	 * timestamps.
	 *
	 * The retrieved register values point to after the captured
	 * position. So they need to get decremented, and adjusted to
	 * cater for the timestamps when the decrement carries over to
	 * a different memory row.
	 */
	if (stoppos && (--*stoppos & ROW_MASK) == ROW_MASK)
		*stoppos -= CLUSTERS_PER_ROW;
	if (triggerpos && (--*triggerpos & ROW_MASK) == ROW_MASK)
		*triggerpos -= CLUSTERS_PER_ROW;

	return SR_OK;
}

static int sigma_read_dram(struct dev_context *devc,
	size_t startchunk, size_t numchunks, uint8_t *data)
{
	uint8_t buf[128], *wrptr, regval;
	size_t chunk;
	int sel, ret;
	gboolean is_last;

	if (2 + 3 * numchunks > ARRAY_SIZE(buf)) {
		sr_err("Short write buffer for %zu DRAM row reads.", numchunks);
		return SR_ERR_BUG;
	}

	/* Communicate DRAM start address (memory row, aka samples line). */
	wrptr = buf;
	write_u16be_inc(&wrptr, startchunk);
	ret = sigma_write_register(devc, WRITE_MEMROW, buf, wrptr - buf);
	if (ret != SR_OK)
		return ret;

	/*
	 * Access DRAM content. Fetch from DRAM to FPGA's internal RAM,
	 * then transfer via USB. Interleave the FPGA's DRAM access and
	 * USB transfer, use alternating buffers (0/1) in the process.
	 */
	wrptr = buf;
	write_u8_inc(&wrptr, REG_DRAM_BLOCK);
	write_u8_inc(&wrptr, REG_DRAM_WAIT_ACK);
	for (chunk = 0; chunk < numchunks; chunk++) {
		sel = chunk % 2;
		is_last = chunk == numchunks - 1;
		if (!is_last) {
			regval = REG_DRAM_BLOCK | REG_DRAM_SEL_BOOL(!sel);
			write_u8_inc(&wrptr, regval);
		}
		regval = REG_DRAM_BLOCK_DATA | REG_DRAM_SEL_BOOL(sel);
		write_u8_inc(&wrptr, regval);
		if (!is_last)
			write_u8_inc(&wrptr, REG_DRAM_WAIT_ACK);
	}
	ret = sigma_write_sr(devc, buf, wrptr - buf);
	if (ret != SR_OK)
		return ret;

	return sigma_read_sr(devc, data, numchunks * ROW_LENGTH_BYTES);
}

/* Upload trigger look-up tables to Sigma. */
SR_PRIV int sigma_write_trigger_lut(struct dev_context *devc,
	struct triggerlut *lut)
{
	size_t lut_addr;
	uint16_t bit;
	uint8_t m3d, m2d, m1d, m0d;
	uint8_t buf[6], *wrptr;
	uint8_t trgsel2;
	uint16_t lutreg, selreg;
	int ret;

	/*
	 * Translate the LUT part of the trigger configuration from the
	 * application's perspective to the hardware register's bitfield
	 * layout. Send the LUT to the device. This configures the logic
	 * which combines pin levels or edges.
	 */
	for (lut_addr = 0; lut_addr < 16; lut_addr++) {
		bit = BIT(lut_addr);

		/* - M4 M3S M3Q */
		m3d = 0;
		if (lut->m4 & bit)
			m3d |= BIT(2);
		if (lut->m3s & bit)
			m3d |= BIT(1);
		if (lut->m3q & bit)
			m3d |= BIT(0);

		/* M2D3 M2D2 M2D1 M2D0 */
		m2d = 0;
		if (lut->m2d[3] & bit)
			m2d |= BIT(3);
		if (lut->m2d[2] & bit)
			m2d |= BIT(2);
		if (lut->m2d[1] & bit)
			m2d |= BIT(1);
		if (lut->m2d[0] & bit)
			m2d |= BIT(0);

		/* M1D3 M1D2 M1D1 M1D0 */
		m1d = 0;
		if (lut->m1d[3] & bit)
			m1d |= BIT(3);
		if (lut->m1d[2] & bit)
			m1d |= BIT(2);
		if (lut->m1d[1] & bit)
			m1d |= BIT(1);
		if (lut->m1d[0] & bit)
			m1d |= BIT(0);

		/* M0D3 M0D2 M0D1 M0D0 */
		m0d = 0;
		if (lut->m0d[3] & bit)
			m0d |= BIT(3);
		if (lut->m0d[2] & bit)
			m0d |= BIT(2);
		if (lut->m0d[1] & bit)
			m0d |= BIT(1);
		if (lut->m0d[0] & bit)
			m0d |= BIT(0);

		/*
		 * Send 16bits with M3D/M2D and M1D/M0D bit masks to the
		 * TriggerSelect register, then strobe the LUT write by
		 * passing A3-A0 to TriggerSelect2. Hold RESET during LUT
		 * programming.
		 */
		wrptr = buf;
		lutreg = 0;
		lutreg <<= 4; lutreg |= m3d;
		lutreg <<= 4; lutreg |= m2d;
		lutreg <<= 4; lutreg |= m1d;
		lutreg <<= 4; lutreg |= m0d;
		write_u16be_inc(&wrptr, lutreg);
		ret = sigma_write_register(devc, WRITE_TRIGGER_SELECT,
			buf, wrptr - buf);
		if (ret != SR_OK)
			return ret;
		trgsel2 = TRGSEL2_RESET | TRGSEL2_LUT_WRITE |
			(lut_addr & TRGSEL2_LUT_ADDR_MASK);
		ret = sigma_set_register(devc, WRITE_TRIGGER_SELECT2, trgsel2);
		if (ret != SR_OK)
			return ret;
	}

	/*
	 * Send the parameters. This covers counters and durations.
	 */
	wrptr = buf;
	selreg = 0;
	selreg |= (lut->params.selinc & TRGSEL_SELINC_MASK) << TRGSEL_SELINC_SHIFT;
	selreg |= (lut->params.selres & TRGSEL_SELRES_MASK) << TRGSEL_SELRES_SHIFT;
	selreg |= (lut->params.sela & TRGSEL_SELA_MASK) << TRGSEL_SELA_SHIFT;
	selreg |= (lut->params.selb & TRGSEL_SELB_MASK) << TRGSEL_SELB_SHIFT;
	selreg |= (lut->params.selc & TRGSEL_SELC_MASK) << TRGSEL_SELC_SHIFT;
	selreg |= (lut->params.selpresc & TRGSEL_SELPRESC_MASK) << TRGSEL_SELPRESC_SHIFT;
	write_u16be_inc(&wrptr, selreg);
	write_u16be_inc(&wrptr, lut->params.cmpb);
	write_u16be_inc(&wrptr, lut->params.cmpa);
	ret = sigma_write_register(devc, WRITE_TRIGGER_SELECT, buf, wrptr - buf);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

/*
 * See Xilinx UG332 for Spartan-3 FPGA configuration. The SIGMA device
 * uses FTDI bitbang mode for netlist download in slave serial mode.
 *
 * 750kbps rate (four times the speed of sigmalogan) works well for
 * netlist download. All pins except INIT_B are output pins during
 * configuration download.
 *
 * Some pins are inverted as a byproduct of level shifting circuitry.
 * That's why high CCLK level (from the cable's point of view) is idle
 * from the FPGA's perspective.
 */
#define BB_PIN_CCLK BIT(0) /* D0, CCLK */
#define BB_PIN_PROG BIT(1) /* D1, PROG */
#define BB_PIN_D2   BIT(2) /* D2, (part of) SUICIDE */
#define BB_PIN_D3   BIT(3) /* D3, (part of) SUICIDE */
#define BB_PIN_D4   BIT(4) /* D4, (part of) SUICIDE (unused?) */
#define BB_PIN_INIT BIT(5) /* D5, INIT, input pin */
#define BB_PIN_DIN  BIT(6) /* D6, DIN */
#define BB_PIN_D7   BIT(7) /* D7, (part of) SUICIDE */

#define BB_BITRATE (750 * 1000)
#define BB_PINMASK (0xff & ~BB_PIN_INIT)

/*
 * Initiate slave serial mode for configuration download. Which is done
 * by pulsing PROG_B and sensing INIT_B. Make sure CCLK is idle before
 * initiating the configuration download.
 */
static int sigma_fpga_init_bitbang_once(struct dev_context *devc)
{
	const uint8_t suicide[] = {
		BB_PIN_D7 | BB_PIN_D2,
		BB_PIN_D7 | BB_PIN_D2,
		BB_PIN_D7 |           BB_PIN_D3,
		BB_PIN_D7 | BB_PIN_D2,
		BB_PIN_D7 |           BB_PIN_D3,
		BB_PIN_D7 | BB_PIN_D2,
		BB_PIN_D7 |           BB_PIN_D3,
		BB_PIN_D7 | BB_PIN_D2,
	};
	const uint8_t init_array[] = {
		BB_PIN_CCLK,
		BB_PIN_CCLK | BB_PIN_PROG,
		BB_PIN_CCLK | BB_PIN_PROG,
		BB_PIN_CCLK,
		BB_PIN_CCLK,
		BB_PIN_CCLK,
		BB_PIN_CCLK,
		BB_PIN_CCLK,
		BB_PIN_CCLK,
		BB_PIN_CCLK,
	};
	size_t retries;
	int ret;
	uint8_t data;

	/* Section 2. part 1), do the FPGA suicide. */
	ret = SR_OK;
	ret |= sigma_write_sr(devc, suicide, sizeof(suicide));
	ret |= sigma_write_sr(devc, suicide, sizeof(suicide));
	ret |= sigma_write_sr(devc, suicide, sizeof(suicide));
	ret |= sigma_write_sr(devc, suicide, sizeof(suicide));
	if (ret != SR_OK)
		return SR_ERR_IO;
	g_usleep(10 * 1000);

	/* Section 2. part 2), pulse PROG. */
	ret = sigma_write_sr(devc, init_array, sizeof(init_array));
	if (ret != SR_OK)
		return ret;
	g_usleep(10 * 1000);
	PURGE_FTDI_BOTH(&devc->ftdi.ctx);

	/*
	 * Wait until the FPGA asserts INIT_B. Check in a maximum number
	 * of bursts with a given delay between them.
	 */
	retries = 10;
	while (retries--) {
		do {
			ret = sigma_read_raw(devc, &data, sizeof(data));
			if (ret < 0)
				return SR_ERR_IO;
			if (ret == sizeof(data) && (data & BB_PIN_INIT))
				return SR_OK;
		} while (ret == sizeof(data));
		if (retries)
			g_usleep(10 * 1000);
	}

	return SR_ERR_TIMEOUT;
}

/*
 * This is belt and braces. Re-run the bitbang initiation sequence a few
 * times should first attempts fail.
 */
static int sigma_fpga_init_bitbang(struct dev_context *devc)
{
	size_t retries;
	int ret;

	retries = 10;
	while (retries--) {
		ret = sigma_fpga_init_bitbang_once(devc);
		if (ret == SR_OK)
			return ret;
		if (ret != SR_ERR_TIMEOUT)
			return ret;
	}
	return ret;
}

/*
 * Configure the FPGA for logic-analyzer mode.
 */
static int sigma_fpga_init_la(struct dev_context *devc)
{
	uint8_t buf[20], *wrptr;
	uint8_t data_55, data_aa, mode;
	uint8_t result[3];
	const uint8_t *rdptr;
	int ret;

	wrptr = buf;

	/* Read ID register. */
	write_u8_inc(&wrptr, REG_ADDR_LOW | LO4(READ_ID));
	write_u8_inc(&wrptr, REG_ADDR_HIGH | HI4(READ_ID));
	write_u8_inc(&wrptr, REG_READ_ADDR);

	/* Write 0x55 to scratch register, read back. */
	data_55 = 0x55;
	write_u8_inc(&wrptr, REG_ADDR_LOW | LO4(WRITE_TEST));
	write_u8_inc(&wrptr, REG_ADDR_HIGH | HI4(WRITE_TEST));
	write_u8_inc(&wrptr, REG_DATA_LOW | LO4(data_55));
	write_u8_inc(&wrptr, REG_DATA_HIGH_WRITE | HI4(data_55));
	write_u8_inc(&wrptr, REG_READ_ADDR);

	/* Write 0xaa to scratch register, read back. */
	data_aa = 0xaa;
	write_u8_inc(&wrptr, REG_ADDR_LOW | LO4(WRITE_TEST));
	write_u8_inc(&wrptr, REG_ADDR_HIGH | HI4(WRITE_TEST));
	write_u8_inc(&wrptr, REG_DATA_LOW | LO4(data_aa));
	write_u8_inc(&wrptr, REG_DATA_HIGH_WRITE | HI4(data_aa));
	write_u8_inc(&wrptr, REG_READ_ADDR);

	/* Initiate SDRAM initialization in mode register. */
	mode = WMR_SDRAMINIT;
	write_u8_inc(&wrptr, REG_ADDR_LOW | LO4(WRITE_MODE));
	write_u8_inc(&wrptr, REG_ADDR_HIGH | HI4(WRITE_MODE));
	write_u8_inc(&wrptr, REG_DATA_LOW | LO4(mode));
	write_u8_inc(&wrptr, REG_DATA_HIGH_WRITE | HI4(mode));

	ret = sigma_write_sr(devc, buf, wrptr - buf);
	if (ret != SR_OK) {
		sr_err("Could not request LA start response.");
		return ret;
	}
	ret = sigma_read_sr(devc, result, ARRAY_SIZE(result));
	if (ret != SR_OK) {
		sr_err("Could not receive LA start response.");
		return SR_ERR_IO;
	}
	rdptr = result;
	if (read_u8_inc(&rdptr) != 0xa6) {
		sr_err("Unexpected ID response.");
		return SR_ERR_DATA;
	}
	if (read_u8_inc(&rdptr) != data_55) {
		sr_err("Unexpected scratch read-back (55).");
		return SR_ERR_DATA;
	}
	if (read_u8_inc(&rdptr) != data_aa) {
		sr_err("Unexpected scratch read-back (aa).");
		return SR_ERR_DATA;
	}

	return SR_OK;
}

/*
 * Local static replacement for standard sigrok's sr_resource_load(). PXView
 * does not provide the sr_resource API. Declared static to avoid multiple
 * definition link errors with other compat drivers in the same libsigrok
 * build unit (e.g. saleae-logic-pro). Reads an entire firmware file (located
 * under the DS_RES_PATH directory) into a g_malloc'd buffer.
 */
static void *sigma_sr_resource_load(struct sr_context *ctx, int type,
	const char *name, size_t *size, size_t max_size)
{
	char path[512];
	FILE *fp;
	long file_size;
	size_t n_read;
	void *buf;

	(void)ctx;
	(void)type;

	if (DS_RES_PATH[0] != '\0')
		snprintf(path, sizeof(path), "%s/%s", DS_RES_PATH, name);
	else
		snprintf(path, sizeof(path), "%s", name);

	fp = fopen(path, "rb");
	if (!fp) {
		sr_err("Failed to open resource '%s'.", path);
		return NULL;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		sr_err("Failed to seek resource '%s'.", path);
		fclose(fp);
		return NULL;
	}
	file_size = ftell(fp);
	if (file_size < 0) {
		sr_err("Failed to tell size of resource '%s'.", path);
		fclose(fp);
		return NULL;
	}
	rewind(fp);

	if ((size_t)file_size > max_size) {
		sr_err("Size %ld of '%s' exceeds limit %zu.", file_size, name, max_size);
		fclose(fp);
		return NULL;
	}

	buf = g_try_malloc((size_t)file_size);
	if (!buf) {
		sr_err("Failed to allocate buffer for '%s'.", name);
		fclose(fp);
		return NULL;
	}

	n_read = fread(buf, 1, (size_t)file_size, fp);
	fclose(fp);

	if (n_read != (size_t)file_size) {
		sr_err("Failed to read '%s': premature end of file.", name);
		g_free(buf);
		return NULL;
	}

	*size = (size_t)file_size;
	return buf;
}

/*
 * Read the firmware from a file and transform it into a series of bitbang
 * pulses used to program the FPGA. Note that the *bb_cmd must be free()'d
 * by the caller of this function.
 */
static int sigma_fw_2_bitbang(struct sr_context *ctx, const char *name,
	uint8_t **bb_cmd, size_t *bb_cmd_size)
{
	uint8_t *firmware;
	size_t file_size;
	uint8_t *p;
	size_t l;
	uint32_t imm;
	size_t bb_size;
	uint8_t *bb_stream, *bbs, byte, mask, v;

	/* Retrieve the on-disk firmware file content. */
	firmware = sigma_sr_resource_load(ctx, SR_RESOURCE_FIRMWARE, name,
		&file_size, SIGMA_FIRMWARE_SIZE_LIMIT);
	if (!firmware)
		return SR_ERR_IO;

	/* Unscramble the file content (XOR with "random" sequence). */
	p = firmware;
	l = file_size;
	imm = 0x3f6df2ab;
	while (l--) {
		imm = (imm + 0xa853753) % 177 + (imm * 0x8034052);
		*p++ ^= imm & 0xff;
	}

	/*
	 * Generate a sequence of bitbang samples. With two samples per
	 * FPGA configuration bit, providing the level for the DIN signal
	 * as well as two edges for CCLK. See Xilinx UG332 for details
	 * ("slave serial" mode).
	 *
	 * Note that CCLK is inverted in hardware. That's why the
	 * respective bit is first set and then cleared in the bitbang
	 * sample sets. So that the DIN level will be stable when the
	 * data gets sampled at the rising CCLK edge, and the signals'
	 * setup time constraint will be met.
	 *
	 * The caller will put the FPGA into download mode, will send
	 * the bitbang samples, and release the allocated memory.
	 */
	bb_size = file_size * 8 * 2;
	bb_stream = g_try_malloc(bb_size);
	if (!bb_stream) {
		sr_err("Memory allocation failed during firmware upload.");
		g_free(firmware);
		return SR_ERR_MALLOC;
	}
	bbs = bb_stream;
	p = firmware;
	l = file_size;
	while (l--) {
		byte = *p++;
		mask = 0x80;
		while (mask) {
			v = (byte & mask) ? BB_PIN_DIN : 0;
			mask >>= 1;
			*bbs++ = v | BB_PIN_CCLK;
			*bbs++ = v;
		}
	}
	g_free(firmware);

	/* The transformation completed successfully, return the result. */
	*bb_cmd = bb_stream;
	*bb_cmd_size = bb_size;

	return SR_OK;
}

static int upload_firmware(struct sr_context *ctx, struct dev_context *devc,
	enum sigma_firmware_idx firmware_idx)
{
	int ret;
	uint8_t *buf;
	uint8_t pins;
	size_t buf_size;
	const char *firmware;

	/* Check for valid firmware file selection. */
	if (firmware_idx >= ARRAY_SIZE(firmware_files))
		return SR_ERR_ARG;
	firmware = firmware_files[firmware_idx];
	if (!firmware || !*firmware)
		return SR_ERR_ARG;

	/* Avoid downloading the same firmware multiple times. */
	if (devc->firmware_idx == firmware_idx) {
		sr_info("Not uploading firmware file '%s' again.", firmware);
		return SR_OK;
	}

	devc->state = SIGMA_CONFIG;

	/* Set the cable to bitbang mode. */
	ret = ftdi_set_bitmode(&devc->ftdi.ctx, BB_PINMASK, BITMODE_BITBANG);
	if (ret < 0) {
		sr_err("Could not setup cable mode for upload: %s",
			ftdi_get_error_string(&devc->ftdi.ctx));
		return SR_ERR;
	}
	ret = ftdi_set_baudrate(&devc->ftdi.ctx, BB_BITRATE);
	if (ret < 0) {
		sr_err("Could not setup bitrate for upload: %s",
			ftdi_get_error_string(&devc->ftdi.ctx));
		return SR_ERR;
	}

	/* Initiate FPGA configuration mode. */
	ret = sigma_fpga_init_bitbang(devc);
	if (ret) {
		sr_err("Could not initiate firmware upload to hardware");
		return ret;
	}

	/* Prepare wire format of the firmware image. */
	ret = sigma_fw_2_bitbang(ctx, firmware, &buf, &buf_size);
	if (ret != SR_OK) {
		sr_err("Could not prepare file %s for upload.", firmware);
		return ret;
	}

	/* Write the FPGA netlist to the cable. */
	sr_info("Uploading firmware file '%s'.", firmware);
	ret = sigma_write_sr(devc, buf, buf_size);
	g_free(buf);
	if (ret != SR_OK) {
		sr_err("Could not upload firmware file '%s'.", firmware);
		return ret;
	}

	/* Leave bitbang mode and discard pending input data. */
	ret = ftdi_set_bitmode(&devc->ftdi.ctx, 0, BITMODE_RESET);
	if (ret < 0) {
		sr_err("Could not setup cable mode after upload: %s",
			ftdi_get_error_string(&devc->ftdi.ctx));
		return SR_ERR;
	}
	PURGE_FTDI_BOTH(&devc->ftdi.ctx);
	while (sigma_read_raw(devc, &pins, sizeof(pins)) > 0)
		;

	/* Initialize the FPGA for logic-analyzer mode. */
	ret = sigma_fpga_init_la(devc);
	if (ret != SR_OK) {
		sr_err("Hardware response after firmware upload failed.");
		return ret;
	}

	/* Keep track of successful firmware download completion. */
	devc->state = SIGMA_IDLE;
	devc->firmware_idx = firmware_idx;
	sr_info("Firmware uploaded.");

	return SR_OK;
}

/*
 * Setup acquisition timeout based on limits.
 */
SR_PRIV int sigma_set_acquire_timeout(struct dev_context *devc)
{
	int ret;
	GVariant *data;
	uint64_t user_count, user_msecs;
	uint64_t worst_cluster_time_ms;
	uint64_t count_msecs, acquire_msecs;

	sr_sw_limits_init(&devc->limit.acquire);
	devc->late_trigger_timeout = FALSE;

	/* Get sample count limit, convert to msecs. */
	ret = sr_sw_limits_config_get(&devc->limit.config,
		SR_CONF_LIMIT_SAMPLES, &data);
	if (ret != SR_OK)
		return ret;
	user_count = g_variant_get_uint64(data);
	g_variant_unref(data);
	count_msecs = 0;
	if (devc->use_triggers) {
		user_count *= 100 - devc->capture_ratio;
		user_count /= 100;
	}
	if (user_count)
		count_msecs = 1000 * user_count / devc->clock.samplerate + 1;

	/* Get time limit, which is in msecs. */
	ret = sr_sw_limits_config_get(&devc->limit.config,
		SR_CONF_LIMIT_MSEC, &data);
	if (ret != SR_OK)
		return ret;
	user_msecs = g_variant_get_uint64(data);
	g_variant_unref(data);
	if (devc->use_triggers) {
		user_msecs *= 100 - devc->capture_ratio;
		user_msecs /= 100;
	}

	/* Get the lesser of them, with both being optional. */
	acquire_msecs = ~UINT64_C(0);
	if (user_count && count_msecs < acquire_msecs)
		acquire_msecs = count_msecs;
	if (user_msecs && user_msecs < acquire_msecs)
		acquire_msecs = user_msecs;
	if (acquire_msecs == ~UINT64_C(0))
		return SR_OK;

	/* Add some slack, and use that timeout for acquisition. */
	worst_cluster_time_ms = 1000 * 65536 / devc->clock.samplerate;
	acquire_msecs += 2 * worst_cluster_time_ms;
	data = g_variant_new_uint64(acquire_msecs);
	ret = sr_sw_limits_config_set(&devc->limit.acquire,
		SR_CONF_LIMIT_MSEC, data);
	g_variant_unref(data);
	if (ret != SR_OK)
		return ret;

	/* Deferred or immediate (trigger-less) timeout period start. */
	if (devc->use_triggers)
		devc->late_trigger_timeout = TRUE;
	else
		sr_sw_limits_acquisition_start(&devc->limit.acquire);

	return SR_OK;
}

/*
 * Check whether a caller specified samplerate matches the device's
 * hardware constraints.
 */
SR_PRIV int sigma_normalize_samplerate(uint64_t want_rate, uint64_t *have_rate)
{
	uint64_t div, rate;

	/* Accept exact matches for 100/200MHz. */
	if (want_rate == SR_MHZ(200) || want_rate == SR_MHZ(100)) {
		if (have_rate)
			*have_rate = want_rate;
		return SR_OK;
	}

	/* Accept 200kHz to 50MHz range, and map to near value. */
	if (want_rate >= SR_KHZ(200) && want_rate <= SR_MHZ(50)) {
		div = SR_MHZ(50) / want_rate;
		rate = SR_MHZ(50) / div;
		if (have_rate)
			*have_rate = rate;
		return SR_OK;
	}

	return SR_ERR_ARG;
}

/* Gets called at probe time. Can seed software settings from hardware state. */
SR_PRIV int sigma_fetch_hw_config(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	/* Seed configuration values from defaults. */
	devc->firmware_idx = SIGMA_FW_NONE;
	devc->clock.samplerate = samplerates[0];

	return SR_ERR_NA;
}

/* Gets called after successful (volatile) hardware configuration. */
SR_PRIV int sigma_store_hw_config(const struct sr_dev_inst *sdi)
{
	(void)sdi;
	return SR_ERR_NA;
}

SR_PRIV int sigma_set_samplerate(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct compat_drv_context *drvc;
	uint64_t samplerate;
	int ret;
	size_t num_channels;

	devc = sdi->priv;
	drvc = sdi->driver->priv;

	/* Accept any caller specified rate which the hardware supports. */
	ret = sigma_normalize_samplerate(devc->clock.samplerate, &samplerate);
	if (ret != SR_OK)
		return ret;

	/*
	 * Depending on the samplerates of 200/100/50- MHz, specific
	 * firmware is required and higher rates might limit the set
	 * of available channels.
	 */
	num_channels = devc->interp.num_channels;
	if (samplerate <= SR_MHZ(50)) {
		ret = upload_firmware(drvc->sr_ctx, devc, SIGMA_FW_50MHZ);
		num_channels = 16;
	} else if (samplerate == SR_MHZ(100)) {
		ret = upload_firmware(drvc->sr_ctx, devc, SIGMA_FW_100MHZ);
		num_channels = 8;
	} else if (samplerate == SR_MHZ(200)) {
		ret = upload_firmware(drvc->sr_ctx, devc, SIGMA_FW_200MHZ);
		num_channels = 4;
	}

	if (ret == SR_OK) {
		devc->interp.num_channels = num_channels;
		devc->interp.samples_per_event = 16 / devc->interp.num_channels;
	}

	if (ret == SR_OK)
		(void)sigma_store_hw_config(sdi);

	return ret;
}

/*
 * Arrange for a session feed submit buffer.
 */

#define CHUNK_SIZE	(4 * 1024 * 1024)

struct submit_buffer {
	size_t unit_size;
	size_t max_samples, curr_samples;
	uint8_t *sample_data;
	uint8_t *write_pointer;
	struct sr_dev_inst *sdi;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;
};

static int alloc_submit_buffer(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct submit_buffer *buffer;
	size_t size;

	devc = sdi->priv;

	buffer = g_malloc0(sizeof(*buffer));
	devc->buffer = buffer;

	buffer->unit_size = sizeof(uint16_t);
	size = CHUNK_SIZE;
	size /= buffer->unit_size;
	buffer->max_samples = size;
	size *= buffer->unit_size;
	buffer->sample_data = g_try_malloc0(size);
	if (!buffer->sample_data)
		return SR_ERR_MALLOC;
	buffer->write_pointer = buffer->sample_data;
	sr_sw_limits_init(&devc->limit.submit);

	buffer->sdi = sdi;
	memset(&buffer->logic, 0, sizeof(buffer->logic));
	buffer->logic.unitsize = buffer->unit_size;
	buffer->logic.data = buffer->sample_data;
	memset(&buffer->packet, 0, sizeof(buffer->packet));
	buffer->packet.type = SR_DF_LOGIC;
	buffer->packet.payload = &buffer->logic;

	return SR_OK;
}

static int setup_submit_limit(struct dev_context *devc)
{
	struct sr_sw_limits *limits;
	int ret;
	GVariant *data;
	uint64_t total;

	limits = &devc->limit.submit;

	ret = sr_sw_limits_config_get(&devc->limit.config,
		SR_CONF_LIMIT_SAMPLES, &data);
	if (ret != SR_OK)
		return ret;
	total = g_variant_get_uint64(data);
	g_variant_unref(data);

	sr_sw_limits_init(limits);
	if (total) {
		data = g_variant_new_uint64(total);
		ret = sr_sw_limits_config_set(limits,
			SR_CONF_LIMIT_SAMPLES, data);
		g_variant_unref(data);
		if (ret != SR_OK)
			return ret;
	}

	sr_sw_limits_acquisition_start(limits);

	return SR_OK;
}

static void free_submit_buffer(struct dev_context *devc)
{
	struct submit_buffer *buffer;

	if (!devc)
		return;

	buffer = devc->buffer;
	if (!buffer)
		return;
	devc->buffer = NULL;

	g_free(buffer->sample_data);
	g_free(buffer);
}

static int flush_submit_buffer(struct dev_context *devc)
{
	struct submit_buffer *buffer;
	int ret;

	buffer = devc->buffer;

	if (!buffer->curr_samples)
		return SR_OK;

	buffer->logic.length = buffer->curr_samples * buffer->unit_size;
	ret = sr_session_send(buffer->sdi, &buffer->packet);
	if (ret != SR_OK)
		return ret;

	buffer->curr_samples = 0;
	buffer->write_pointer = buffer->sample_data;

	return SR_OK;
}

static int addto_submit_buffer(struct dev_context *devc,
	uint16_t sample, size_t count)
{
	struct submit_buffer *buffer;
	struct sr_sw_limits *limits;
	int ret;

	buffer = devc->buffer;
	limits = &devc->limit.submit;
	if (!devc->use_triggers && sr_sw_limits_check(limits))
		count = 0;

	while (count--) {
		write_u16le_inc(&buffer->write_pointer, sample);
		buffer->curr_samples++;
		if (buffer->curr_samples == buffer->max_samples) {
			ret = flush_submit_buffer(devc);
			if (ret != SR_OK)
				return ret;
		}
		sr_sw_limits_update_samples_read(limits, 1);
		if (!devc->use_triggers && sr_sw_limits_check(limits))
			break;
	}

	return SR_OK;
}

static void sigma_location_break_down(struct sigma_location *loc)
{
	loc->line = loc->raw / ROW_LENGTH_U16;
	loc->line += ROW_COUNT;
	loc->line %= ROW_COUNT;
	loc->cluster = loc->raw % ROW_LENGTH_U16;
	loc->event = loc->cluster % EVENTS_PER_CLUSTER;
	loc->cluster = loc->cluster / EVENTS_PER_CLUSTER;
}

static gboolean sigma_location_is_eq(struct sigma_location *loc1,
	struct sigma_location *loc2, gboolean with_event)
{
	if (!loc1 || !loc2)
		return FALSE;

	if (loc1->line != loc2->line)
		return FALSE;
	if (loc1->cluster != loc2->cluster)
		return FALSE;

	if (with_event && loc1->event != loc2->event)
		return FALSE;

	return TRUE;
}

static void sigma_location_decrement(struct sigma_location *loc,
	gboolean with_event)
{
	if (!loc)
		return;

	if (with_event) {
		if (loc->event--)
			return;
		loc->event = EVENTS_PER_CLUSTER - 1;
	}

	if (loc->cluster--)
		return;
	loc->cluster = CLUSTERS_PER_ROW - 1;

	if (loc->line--)
		return;
	loc->line = ROW_COUNT - 1;
}

static void sigma_location_increment(struct sigma_location *loc)
{
	if (!loc)
		return;

	if (++loc->event < EVENTS_PER_CLUSTER)
		return;
	loc->event = 0;
	if (++loc->cluster < CLUSTERS_PER_ROW)
		return;
	loc->cluster = 0;
	if (++loc->line < ROW_COUNT)
		return;
	loc->line = 0;
}

static void rewind_trig_arm_pos(struct dev_context *devc, size_t count)
{
	struct sigma_sample_interp *interp;

	if (!devc)
		return;
	interp = &devc->interp;

	if (!devc->use_triggers) {
		interp->trig_arm.raw = ~0;
		sigma_location_break_down(&interp->trig_arm);
		return;
	}

	interp->trig_arm = interp->trig;
	while (count--) {
		if (sigma_location_is_eq(&interp->trig_arm, &interp->start, TRUE))
			break;
		sigma_location_decrement(&interp->trig_arm, TRUE);
	}
}

static int alloc_sample_buffer(struct dev_context *devc,
	size_t stop_pos, size_t trig_pos, uint8_t mode)
{
	struct sigma_sample_interp *interp;
	gboolean wrapped;
	size_t alloc_size;

	interp = &devc->interp;

	wrapped = mode & RMR_ROUND;
	interp->start.raw = 0;
	interp->stop.raw = stop_pos;
	if (wrapped) {
		interp->start.raw = stop_pos;
		interp->start.raw >>= ROW_SHIFT;
		interp->start.raw++;
		interp->start.raw <<= ROW_SHIFT;
		interp->stop.raw = stop_pos;
		interp->stop.raw >>= ROW_SHIFT;
		interp->stop.raw--;
		interp->stop.raw <<= ROW_SHIFT;
	}
	interp->trig.raw = trig_pos;
	interp->iter.raw = 0;

	sigma_location_break_down(&interp->start);
	sigma_location_break_down(&interp->stop);
	sigma_location_break_down(&interp->trig);
	sigma_location_break_down(&interp->iter);

	rewind_trig_arm_pos(devc, 4 * EVENTS_PER_CLUSTER);
	memset(&interp->trig_chk, 0, sizeof(interp->trig_chk));

	memset(&interp->fetch, 0, sizeof(interp->fetch));
	interp->fetch.lines_total = interp->stop.line + 1;
	interp->fetch.lines_total -= interp->start.line;
	interp->fetch.lines_total += ROW_COUNT;
	interp->fetch.lines_total %= ROW_COUNT;
	interp->fetch.lines_done = 0;

	interp->fetch.lines_per_read = 32;
	alloc_size = sizeof(devc->interp.fetch.rcvd_lines[0]);
	alloc_size *= devc->interp.fetch.lines_per_read;
	devc->interp.fetch.rcvd_lines = g_try_malloc0(alloc_size);
	if (!devc->interp.fetch.rcvd_lines)
		return SR_ERR_MALLOC;

	return SR_OK;
}

static uint16_t sigma_deinterlace_data_4x4(uint16_t indata, int idx);
static uint16_t sigma_deinterlace_data_2x8(uint16_t indata, int idx);

static int fetch_sample_buffer(struct dev_context *devc)
{
	struct sigma_sample_interp *interp;
	size_t count;
	int ret;
	const uint8_t *rdptr;
	uint16_t ts, data;

	interp = &devc->interp;

	if (!interp->fetch.lines_done) {
		interp->iter = interp->start;
	}

	count = interp->fetch.lines_total - interp->fetch.lines_done;
	if (count > interp->fetch.lines_per_read)
		count = interp->fetch.lines_per_read;
	ret = sigma_read_dram(devc, interp->iter.line, count,
		(uint8_t *)interp->fetch.rcvd_lines);
	if (ret != SR_OK)
		return ret;
	interp->fetch.lines_rcvd = count;
	interp->fetch.curr_line = &interp->fetch.rcvd_lines[0];

	if (!interp->fetch.lines_done) {
		rdptr = (void *)interp->fetch.curr_line;
		ts = read_u16le_inc(&rdptr);
		data = read_u16le_inc(&rdptr);
		if (interp->samples_per_event == 4) {
			data = sigma_deinterlace_data_4x4(data, 0);
		} else if (interp->samples_per_event == 2) {
			data = sigma_deinterlace_data_2x8(data, 0);
		}
		interp->last.ts = ts;
		interp->last.sample = data;
	}

	return SR_OK;
}

static void free_sample_buffer(struct dev_context *devc)
{
	g_free(devc->interp.fetch.rcvd_lines);
	devc->interp.fetch.rcvd_lines = NULL;
	devc->interp.fetch.lines_per_read = 0;
}

/*
 * Parse application provided trigger conditions to the driver's internal
 * presentation.
 *
 * NOTE: sr_session_trigger_get stubbed in compat layer.
 */
SR_PRIV int sigma_convert_trigger(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;
	memset(&devc->trigger, 0, sizeof(devc->trigger));
	devc->use_triggers = FALSE;

	/* Trigger support stubbed - triggers not available in compat layer */
	return SR_OK;
}

static gboolean sample_matches_trigger(struct dev_context *devc, uint16_t sample)
{
	struct sigma_sample_interp *interp;
	uint16_t last_sample;
	struct sigma_trigger *t;
	gboolean simple_match, rising_match, falling_match;
	gboolean matched;

	if (!devc)
		return FALSE;
	if (!devc->use_triggers)
		return FALSE;
	interp = &devc->interp;
	if (!interp->trig_chk.armed)
		return FALSE;

	last_sample = interp->last.sample;
	t = &devc->trigger;
	simple_match = (sample & t->simplemask) == t->simplevalue;
	rising_match = ((last_sample & t->risingmask) == 0) &&
			((sample & t->risingmask) == t->risingmask);
	falling_match = ((last_sample & t->fallingmask) == t->fallingmask) &&
			((sample & t->fallingmask) == 0);
	matched = simple_match && rising_match && falling_match;

	return matched;
}

static int send_trigger_marker(struct dev_context *devc)
{
	int ret;

	ret = flush_submit_buffer(devc);
	if (ret != SR_OK)
		return ret;
	/* Trigger marker stubbed - std_session_send_df_trigger not available */
	return SR_OK;
}

static int check_and_submit_sample(struct dev_context *devc,
	uint16_t sample, size_t count)
{
	gboolean triggered;
	int ret;

	triggered = sample_matches_trigger(devc, sample);
	if (triggered) {
		send_trigger_marker(devc);
		devc->interp.trig_chk.matched = TRUE;
	}

	ret = addto_submit_buffer(devc, sample, count);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

static void sigma_location_check(struct dev_context *devc)
{
	struct sigma_sample_interp *interp;

	if (!devc)
		return;
	interp = &devc->interp;

	if (interp->trig_chk.armed) {
		interp->trig_chk.evt_remain--;
		if (!interp->trig_chk.evt_remain || interp->trig_chk.matched)
			interp->trig_chk.armed = FALSE;
	}
	if (!interp->trig_chk.armed && !interp->trig_chk.matched) {
		if (sigma_location_is_eq(&interp->iter, &interp->trig_arm, TRUE)) {
			interp->trig_chk.armed = TRUE;
			interp->trig_chk.matched = FALSE;
			interp->trig_chk.evt_remain = 8 * EVENTS_PER_CLUSTER;
		}
	}

	if (interp->trig_chk.armed) {
		if (sigma_location_is_eq(&interp->iter, &interp->trig, TRUE)) {
			(void)send_trigger_marker(devc);
			interp->trig_chk.matched = TRUE;
		}
	}
}

static uint16_t sigma_dram_cluster_ts(struct sigma_dram_cluster *cluster)
{
	return read_u16le((const uint8_t *)&cluster->timestamp);
}

static uint16_t sigma_dram_cluster_data(struct sigma_dram_cluster *cl, int idx)
{
	return read_u16le((const uint8_t *)&cl->samples[idx]);
}

static uint16_t sigma_deinterlace_data_2x8(uint16_t indata, int idx)
{
	uint16_t outdata;

	indata >>= idx;
	outdata = 0;
	outdata |= (indata >> (0 * 2 - 0)) & (1 << 0);
	outdata |= (indata >> (1 * 2 - 1)) & (1 << 1);
	outdata |= (indata >> (2 * 2 - 2)) & (1 << 2);
	outdata |= (indata >> (3 * 2 - 3)) & (1 << 3);
	outdata |= (indata >> (4 * 2 - 4)) & (1 << 4);
	outdata |= (indata >> (5 * 2 - 5)) & (1 << 5);
	outdata |= (indata >> (6 * 2 - 6)) & (1 << 6);
	outdata |= (indata >> (7 * 2 - 7)) & (1 << 7);
	return outdata;
}

static uint16_t sigma_deinterlace_data_4x4(uint16_t indata, int idx)
{
	uint16_t outdata;

	indata >>= idx;
	outdata = 0;
	outdata |= (indata >> (0 * 4 - 0)) & (1 << 0);
	outdata |= (indata >> (1 * 4 - 1)) & (1 << 1);
	outdata |= (indata >> (2 * 4 - 2)) & (1 << 2);
	outdata |= (indata >> (3 * 4 - 3)) & (1 << 3);
	return outdata;
}

static void sigma_decode_dram_cluster(struct dev_context *devc,
	struct sigma_dram_cluster *dram_cluster,
	size_t events_in_cluster)
{
	uint16_t tsdiff, ts, sample, item16;
	size_t count;
	size_t evt;

	ts = sigma_dram_cluster_ts(dram_cluster);
	tsdiff = ts - devc->interp.last.ts;
	if (tsdiff > 0) {
		sample = devc->interp.last.sample;
		count = tsdiff * devc->interp.samples_per_event;
		(void)check_and_submit_sample(devc, sample, count);
	}
	devc->interp.last.ts = ts + EVENTS_PER_CLUSTER;

	sample = 0;
	for (evt = 0; evt < events_in_cluster; evt++) {
		item16 = sigma_dram_cluster_data(dram_cluster, evt);
		if (devc->interp.samples_per_event == 4) {
			sample = sigma_deinterlace_data_4x4(item16, 0);
			check_and_submit_sample(devc, sample, 1);
			devc->interp.last.sample = sample;
			sample = sigma_deinterlace_data_4x4(item16, 1);
			check_and_submit_sample(devc, sample, 1);
			devc->interp.last.sample = sample;
			sample = sigma_deinterlace_data_4x4(item16, 2);
			check_and_submit_sample(devc, sample, 1);
			devc->interp.last.sample = sample;
			sample = sigma_deinterlace_data_4x4(item16, 3);
			check_and_submit_sample(devc, sample, 1);
			devc->interp.last.sample = sample;
		} else if (devc->interp.samples_per_event == 2) {
			sample = sigma_deinterlace_data_2x8(item16, 0);
			check_and_submit_sample(devc, sample, 1);
			devc->interp.last.sample = sample;
			sample = sigma_deinterlace_data_2x8(item16, 1);
			check_and_submit_sample(devc, sample, 1);
			devc->interp.last.sample = sample;
		} else {
			sample = item16;
			check_and_submit_sample(devc, sample, 1);
			devc->interp.last.sample = sample;
		}
		sigma_location_increment(&devc->interp.iter);
		sigma_location_check(devc);
	}
}

static int decode_chunk_ts(struct dev_context *devc,
	struct sigma_dram_line *dram_line,
	size_t events_in_line)
{
	struct sigma_dram_cluster *dram_cluster;
	size_t clusters_in_line;
	size_t events_in_cluster;
	size_t cluster;

	clusters_in_line = events_in_line;
	clusters_in_line += EVENTS_PER_CLUSTER - 1;
	clusters_in_line /= EVENTS_PER_CLUSTER;

	for (cluster = 0; cluster < clusters_in_line; cluster++) {
		dram_cluster = &dram_line->cluster[cluster];

		if ((cluster == clusters_in_line - 1) &&
		    (events_in_line % EVENTS_PER_CLUSTER)) {
			events_in_cluster = events_in_line % EVENTS_PER_CLUSTER;
		} else {
			events_in_cluster = EVENTS_PER_CLUSTER;
		}

		sigma_decode_dram_cluster(devc, dram_cluster,
			events_in_cluster);
	}

	return SR_OK;
}

static int download_capture(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sigma_sample_interp *interp;
	uint32_t stoppos, triggerpos;
	uint8_t modestatus;
	int ret;
	size_t chunks_per_receive_call;

	devc = sdi->priv;
	interp = &devc->interp;

	ret = sigma_get_register(devc, READ_MODE, &modestatus);
	if (ret != SR_OK) {
		sr_err("Could not determine current device state.");
		return FALSE;
	}
	if (!(modestatus & RMR_POSTTRIGGERED)) {
		sr_info("Downloading sample data.");
		devc->state = SIGMA_DOWNLOAD;

		modestatus = WMR_FORCESTOP | WMR_SDRAMWRITEEN;
		ret = sigma_set_register(devc, WRITE_MODE, modestatus);
		if (ret != SR_OK)
			return FALSE;
		do {
			ret = sigma_get_register(devc, READ_MODE, &modestatus);
			if (ret != SR_OK) {
				sr_err("Could not poll for post-trigger state.");
				return FALSE;
			}
		} while (!(modestatus & RMR_POSTTRIGGERED));
	}

	if (!interp->fetch.lines_per_read) {
		ret = sigma_set_register(devc, WRITE_MODE, WMR_SDRAMREADEN);
		if (ret != SR_OK)
			return FALSE;

		ret = sigma_read_pos(devc, &stoppos, &triggerpos, &modestatus);
		if (ret != SR_OK) {
			sr_err("Could not query capture positions/state.");
			return FALSE;
		}
		if (!devc->use_triggers)
			triggerpos = ~0;
		if (!(modestatus & RMR_TRIGGERED))
			triggerpos = ~0;

		ret = alloc_sample_buffer(devc, stoppos, triggerpos, modestatus);
		if (ret != SR_OK)
			return FALSE;

		ret = alloc_submit_buffer(sdi);
		if (ret != SR_OK)
			return FALSE;
		ret = setup_submit_limit(devc);
		if (ret != SR_OK)
			return FALSE;
	}

	chunks_per_receive_call = 50;
	while (interp->fetch.lines_done < interp->fetch.lines_total) {
		size_t dl_events_in_line;

		ret = fetch_sample_buffer(devc);
		if (ret != SR_OK)
			return FALSE;

		while (interp->fetch.lines_rcvd--) {
			dl_events_in_line = EVENTS_PER_ROW;
			if (interp->iter.line == interp->stop.line) {
				dl_events_in_line = interp->stop.raw & ROW_MASK;
			}
			decode_chunk_ts(devc, interp->fetch.curr_line,
				dl_events_in_line);
			interp->fetch.curr_line++;
			interp->fetch.lines_done++;
		}

		if (!--chunks_per_receive_call) {
			ret = flush_submit_buffer(devc);
			if (ret != SR_OK)
				return FALSE;
			break;
		}
	}

	if (interp->fetch.lines_done >= interp->fetch.lines_total) {
		ret = flush_submit_buffer(devc);
		if (ret != SR_OK)
			return FALSE;
		free_submit_buffer(devc);
		free_sample_buffer(devc);

		ret = std_session_send_df_end(sdi, LOG_PREFIX);
		if (ret != SR_OK)
			return FALSE;

		devc->state = SIGMA_IDLE;
		sr_dev_acquisition_stop(sdi);
	}

	return TRUE;
}

static int sigma_capture_mode(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;
	uint32_t stoppos, triggerpos;
	uint8_t mode;
	gboolean full, wrapped, triggered, complete;

	devc = sdi->priv;

	ret = sigma_read_pos(devc, &stoppos, &triggerpos, &mode);
	if (ret != SR_OK)
		return FALSE;
	stoppos >>= ROW_SHIFT;
	full = stoppos >= ROW_COUNT - 2;
	wrapped = mode & RMR_ROUND;
	triggered = mode & RMR_TRIGGERED;
	complete = mode & RMR_POSTTRIGGERED;

	if (complete)
		return download_capture(sdi);

	if (sr_sw_limits_check(&devc->limit.acquire))
		return download_capture(sdi);
	if (devc->late_trigger_timeout && triggered) {
		sr_sw_limits_acquisition_start(&devc->limit.acquire);
		devc->late_trigger_timeout = FALSE;
	}

	(void)full;
	if (!devc->use_triggers && wrapped)
		return download_capture(sdi);

	return TRUE;
}

SR_PRIV int sigma_receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	(void)fd;
	(void)revents;

	devc = sdi->priv;

	if (devc->state == SIGMA_IDLE)
		return TRUE;

	if (devc->state == SIGMA_STOPPING)
		return download_capture(sdi);
	if (devc->state == SIGMA_DOWNLOAD)
		return download_capture(sdi);
	if (devc->state == SIGMA_CAPTURE)
		return sigma_capture_mode(sdi);

	return TRUE;
}

/* Build a LUT entry used by the trigger functions. */
static void build_lut_entry(uint16_t *lut_entry,
	uint16_t spec_value, uint16_t spec_mask)
{
	size_t quad, bitidx, ch;
	uint16_t quadmask, bitmask;
	gboolean spec_value_low, bit_idx_low;

	for (quad = 0; quad < 4; quad++) {
		lut_entry[quad] = ~0;
		for (bitidx = 0; bitidx < 16; bitidx++) {
			for (ch = 0; ch < 4; ch++) {
				quadmask = BIT(ch);
				bitmask = quadmask << (quad * 4);
				if (!(spec_mask & bitmask))
					continue;
				spec_value_low = !(spec_value & bitmask);
				bit_idx_low = !(bitidx & quadmask);
				if (spec_value_low == bit_idx_low)
					continue;
				lut_entry[quad] &= ~BIT(bitidx);
			}
		}
	}
}

/* Add a logical function to LUT mask. */
static void add_trigger_function(enum triggerop oper, enum triggerfunc func,
	size_t index, gboolean neg, uint16_t *mask)
{
	int x[2][2], a, b, aset, bset, rset;
	size_t bitidx;

	memset(x, 0, sizeof(x));
	switch (oper) {
	case OP_LEVEL:
		x[0][1] = 1;
		x[1][1] = 1;
		break;
	case OP_NOT:
		x[0][0] = 1;
		x[1][0] = 1;
		break;
	case OP_RISE:
		x[0][1] = 1;
		break;
	case OP_FALL:
		x[1][0] = 1;
		break;
	case OP_RISEFALL:
		x[0][1] = 1;
		x[1][0] = 1;
		break;
	case OP_NOTRISE:
		x[1][1] = 1;
		x[0][0] = 1;
		x[1][0] = 1;
		break;
	case OP_NOTFALL:
		x[1][1] = 1;
		x[0][0] = 1;
		x[0][1] = 1;
		break;
	case OP_NOTRISEFALL:
		x[1][1] = 1;
		x[0][0] = 1;
		break;
	}

	if (neg) {
		size_t i, j;
		int tmp;

		for (i = 0; i < 2; i++) {
			for (j = 0; j < 2; j++) {
				tmp = x[i][j];
				x[i][j] = x[1 - i][1 - j];
				x[1 - i][1 - j] = tmp;
			}
		}
	}

	for (bitidx = 0; bitidx < 16; bitidx++) {
		a = (bitidx & BIT(2 * index + 0)) ? 1 : 0;
		b = (bitidx & BIT(2 * index + 1)) ? 1 : 0;

		aset = (*mask & BIT(bitidx)) ? 1 : 0;
		bset = x[b][a];

		if (func == FUNC_AND || func == FUNC_NAND)
			rset = aset & bset;
		else if (func == FUNC_OR || func == FUNC_NOR)
			rset = aset | bset;
		else if (func == FUNC_XOR || func == FUNC_NXOR)
			rset = aset ^ bset;
		else
			rset = 0;

		if (func == FUNC_NAND || func == FUNC_NOR || func == FUNC_NXOR)
			rset = 1 - rset;

		if (rset)
			*mask |= BIT(bitidx);
		else
			*mask &= ~BIT(bitidx);
	}
}

SR_PRIV int sigma_build_basic_trigger(struct dev_context *devc,
	struct triggerlut *lut)
{
	uint16_t masks[2];
	size_t bitidx, condidx;
	uint16_t value, mask;

	memset(lut, 0, sizeof(*lut));
	if (!devc->use_triggers)
		return SR_OK;

	lut->m4 = 0xa000;
	lut->m3q = 0xffff;

	value = devc->trigger.simplevalue;
	mask = devc->trigger.simplemask;
	build_lut_entry(lut->m2d, value, mask);

	memset(&masks, 0, sizeof(masks));
	condidx = 0;
	for (bitidx = 0; bitidx < 16; bitidx++) {
		mask = BIT(bitidx);
		value = devc->trigger.risingmask | devc->trigger.fallingmask;
		if (!(value & mask))
			continue;
		if (condidx == 0)
			build_lut_entry(lut->m0d, mask, mask);
		if (condidx == 1)
			build_lut_entry(lut->m1d, mask, mask);
		masks[condidx++] = mask;
		if (condidx == ARRAY_SIZE(masks))
			break;
	}

	if (masks[0] || masks[1]) {
		lut->m3q = 0;
		if (masks[0] & devc->trigger.risingmask)
			add_trigger_function(OP_RISE, FUNC_OR, 0, 0, &lut->m3q);
		if (masks[0] & devc->trigger.fallingmask)
			add_trigger_function(OP_FALL, FUNC_OR, 0, 0, &lut->m3q);
		if (masks[1] & devc->trigger.risingmask)
			add_trigger_function(OP_RISE, FUNC_OR, 1, 0, &lut->m3q);
		if (masks[1] & devc->trigger.fallingmask)
			add_trigger_function(OP_FALL, FUNC_OR, 1, 0, &lut->m3q);
	}

	lut->params.selres = TRGSEL_SELCODE_NEVER;
	lut->params.selinc = TRGSEL_SELCODE_LEVEL;
	lut->params.sela = 0;
	lut->params.cmpa = 0;

	return SR_OK;
}