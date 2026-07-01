/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Matthias Heidbrink <m-sigrok@heidbrink.biz>
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
#include <errno.h>
#include "protocol.h"

/*
 * Local wrapper around PXView's 3-arg serial_timeout(). Standard sigrok's
 * serial_timeout() takes (serial, bytes) and derives the baudrate from the
 * serial port's internal state. PXView requires the baudrate explicitly, so
 * parse it from the serialcomm string (format like "9600/8n1"). Falls back
 * to a conservative default when parsing fails. Same pattern as the
 * atten-pps3xxx and korad-kaxxxxp compat drivers.
 */
SR_PRIV int conrad_serial_timeout(struct sr_serial_dev_inst *serial, int bytes)
{
	uint64_t baudrate = 0;
	const char *sc;

	if (serial && serial->serialcomm && serial->serialcomm[0]) {
		sc = serial->serialcomm;
		errno = 0;
		baudrate = (uint64_t)g_ascii_strtoull(sc, NULL, 10);
		if (errno != 0 || baudrate == 0)
			baudrate = 0;
	}

	return serial_timeout(serial, baudrate, bytes);
}

/**
 * Send command with parameter.
 *
 * @param[in] cmd Command
 * @param[in] param Parameter (0..999, depending on command).
 *
 * @retval SR_OK Success.
 * @retval SR_ERR_ARG Invalid argument.
 * @retval SR_ERR Error.
 */
SR_PRIV int send_msg1(const struct sr_dev_inst *sdi, char cmd, int param)
{
	struct sr_serial_dev_inst *serial;
	char buf[5];

	if (!sdi || !(serial = sdi->conn))
		return SR_ERR_ARG;

	snprintf(buf, sizeof(buf), "%c%03d", cmd, param);
	buf[4] = '\r';

	sr_spew("send_msg1(): %c%c%c%c\\r", buf[0], buf[1], buf[2], buf[3]);

	if (serial_write_blocking(serial, buf, sizeof(buf),
			conrad_serial_timeout(serial, sizeof(buf))) < (int)sizeof(buf)) {
		sr_err("Write error for cmd=%c", cmd);
		return SR_ERR;
	}

	/*
	 * Wait 50ms to ensure that the device does not swallow any of the
	 * following commands.
	 */
	g_usleep(50 * 1000);

	return SR_OK;
}
