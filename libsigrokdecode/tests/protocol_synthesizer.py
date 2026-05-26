#!/usr/bin/env python3
"""
protocol_synthesizer.py - A library for generating valid protocol bitstreams.

This library provides classes to build protocol-compliant logic signals
for testing decoder implementations.
"""

import math

class BitstreamBuilder:
    def __init__(self, num_channels, sample_count, samplerate=1000000):
        self.num_channels = num_channels
        self.sample_count = sample_count
        self.samplerate = samplerate
        self.channels = [[0] * sample_count for _ in range(num_channels)]
        self.pos = 0

    def set_pos(self, pos):
        self.pos = pos

    def get_pos(self):
        return self.pos

    def set_level(self, ch, level, duration_samples=1):
        for i in range(duration_samples):
            if self.pos + i < self.sample_count:
                self.channels[ch][self.pos + i] = 1 if level else 0
        self.pos += duration_samples

    def set_idle(self, ch, level):
        for i in range(self.pos, self.sample_count):
            self.channels[ch][i] = 1 if level else 0

    def get_bitpacked(self):
        """Convert list of 0/1 to bit-packed bytes."""
        result = bytearray()
        bytes_per_channel = math.ceil(self.sample_count / 8)
        for ch in range(self.num_channels):
            packed = bytearray(bytes_per_channel)
            for i, val in enumerate(self.channels[ch]):
                if val:
                    packed[i // 8] |= (1 << (i % 8))
            result.extend(packed)
        return bytes(result)

class UARTGenerator:
    def __init__(self, builder, channel, baud=9600, data_bits=8, parity='none', stop_bits=1):
        self.builder = builder
        self.channel = channel
        self.bit_width = int(builder.samplerate / baud)
        self.data_bits = data_bits
        self.parity = parity
        self.stop_bits = stop_bits
        self.builder.set_idle(channel, 1)

    def write_byte(self, val):
        # Start bit
        self.builder.set_level(self.channel, 0, self.bit_width)
        # Data bits
        p = 0
        for i in range(self.data_bits):
            bit = (val >> i) & 1
            self.builder.set_level(self.channel, bit, self.bit_width)
            p ^= bit
        # Parity bit
        if self.parity != 'none':
            p_bit = p if self.parity == 'even' else (1 - p)
            self.builder.set_level(self.channel, p_bit, self.bit_width)
        # Stop bits
        self.builder.set_level(self.channel, 1, int(self.bit_width * self.stop_bits))

class CANGenerator:
    def __init__(self, builder, channel, bitrate=500000):
        self.builder = builder
        self.channel = channel
        self.bit_width = int(builder.samplerate / bitrate)
        self.builder.set_idle(channel, 1) # Recessive idle

    def _crc15_can(self, bits):
        crc = 0
        for bit in bits:
            if (bit ^ (crc >> 14)) & 1:
                crc = (crc << 1) ^ 0x4599
            else:
                crc <<= 1
        return crc & 0x7fff

    def send_frame(self, ident, data, ide=False, rtr=False):
        raw_bits = [0] # SOF
        # ID
        if not ide:
            for i in range(10, -1, -1): raw_bits.append((ident >> i) & 1)
            raw_bits.extend([rtr, 0, 0]) # RTR, IDE, r0
        else:
            # Extended ID not implemented in this snippet for brevity, but same logic
            pass
        
        # DLC
        dlc = len(data)
        for i in range(3, -1, -1): raw_bits.append((dlc >> i) & 1)
        # Data
        for b in data:
            for i in range(7, -1, -1): raw_bits.append((b >> i) & 1)
        
        # CRC
        crc = self._crc15_can(raw_bits[1:])
        for i in range(14, -1, -1): raw_bits.append((crc >> i) & 1)
        
        # Stuffing (SOF to CRC)
        stuffed = [raw_bits[0]]
        consecutive = 1
        last = raw_bits[0]
        for b in raw_bits[1:]:
            if b == last:
                consecutive += 1
            else:
                consecutive = 1
                last = b
            stuffed.append(b)
            if consecutive == 5:
                stuffed.append(1 - last)
                last = 1 - last
                consecutive = 1
        
        # Trailer (Fixed form)
        stuffed.extend([1, 0, 1]) # CRC Delim (R), ACK (D), ACK Delim (R)
        stuffed.extend([1]*7) # EOF
        stuffed.extend([1]*3) # IFS

        # Write stuffed bits to channel (NRZ: recessive=1, dominant=0)
        for bit in stuffed:
            self.builder.set_level(self.channel, bit, self.bit_width)
        
class USBGenerator:
    def __init__(self, builder, dp_ch, dm_ch, speed='full'):
        self.builder = builder
        self.dp = dp_ch
        self.dm = dm_ch
        # Full speed: 12Mbps, Low speed: 1.5Mbps
        self.bit_width = int(builder.samplerate / (12e6 if speed == 'full' else 1.5e6))
        self.last_state = 'J' # Full speed idle is J (DP high, DM low)
        self.builder.set_idle(dp_ch, 1)
        self.builder.set_idle(dm_ch, 0)

    def _send_bit(self, bit):
        # USB uses NRZI: 0 = toggle, 1 = no change
        if bit == 0:
            self.last_state = 'K' if self.last_state == 'J' else 'J'
        
        dp = 1 if self.last_state == 'J' else 0
        dm = 1 - dp
        self.builder.set_level(self.dp, dp, self.bit_width)
        self.builder.pos -= self.bit_width
        self.builder.set_level(self.dm, dm, self.bit_width)

    def send_packet(self, data):
        # Sync: 00000001 (NRZI toggles on 0s)
        for b in [0,0,0,0,0,0,0,1]: self._send_bit(b)
        
        # Data with bit-stuffing (after 6 ones, insert 0)
        ones_count = 0
        for byte in data:
            for i in range(8):
                bit = (byte >> i) & 1
                if bit == 1:
                    ones_count += 1
                    self._send_bit(1)
                    if ones_count == 6:
                        self._send_bit(0)
                        ones_count = 0
                else:
                    self._send_bit(0)
                    ones_count = 0
        
        # EOP: SE0 for 2 bit widths, then J
        self.builder.set_level(self.dp, 0, self.bit_width * 2)
        self.builder.pos -= self.bit_width * 2
        self.builder.set_level(self.dm, 0, self.bit_width * 2)
        self.builder.set_level(self.dp, 1, self.bit_width)
        self.builder.pos -= self.bit_width
        self.builder.set_level(self.dm, 0, self.bit_width)

class ManchesterGenerator:
    """Used for Ethernet (10Mbps) or IR protocols."""
    def __init__(self, builder, channel, bitrate=10e6, invert=False):
        self.builder = builder
        self.channel = channel
        self.half_width = int(builder.samplerate / (bitrate * 2))
        self.invert = invert

    def write_bit(self, bit):
        # Standard Manchester: 1 = Low-to-High, 0 = High-to-Low
        # (or IEEE 802.3: 0 = Low-to-High, 1 = High-to-Low - we follow Standard)
        if bit ^ self.invert:
            self.builder.set_level(self.channel, 0, self.half_width)
            self.builder.set_level(self.channel, 1, self.half_width)
        else:
            self.builder.set_level(self.channel, 1, self.half_width)
            self.builder.set_level(self.channel, 0, self.half_width)

class I2CGenerator:
    def __init__(self, builder, scl_ch, sda_ch, speed=100000):
        self.builder = builder
        self.scl = scl_ch
        self.sda = sda_ch
        self.half_period = int(builder.samplerate / (speed * 2))
        # Idle state
        self.builder.set_idle(scl_ch, 1)
        self.builder.set_idle(sda_ch, 1)

    def start(self):
        # SCL high, SDA falls
        self.builder.set_level(self.scl, 1, self.half_period)
        self.builder.pos -= self.half_period # Overlap
        self.builder.set_level(self.sda, 1, self.half_period // 2)
        self.builder.set_level(self.sda, 0, self.half_period // 2)

    def stop(self):
        # SCL high, SDA rises
        self.builder.set_level(self.scl, 0, self.half_period)
        self.builder.set_level(self.sda, 0, self.half_period)
        self.builder.set_level(self.scl, 1, self.half_period)
        self.builder.pos -= self.half_period
        self.builder.set_level(self.sda, 0, self.half_period // 2)
        self.builder.set_level(self.sda, 1, self.half_period // 2)

    def write_byte(self, val):
        for i in range(7, -1, -1):
            bit = (val >> i) & 1
            self.builder.set_level(self.scl, 0, self.half_period)
            self.builder.pos -= self.half_period
            self.builder.set_level(self.sda, bit, self.half_period)
            self.builder.set_level(self.scl, 1, self.half_period)
        # ACK slot
        self.builder.set_level(self.scl, 0, self.half_period)
        self.builder.pos -= self.half_period
        self.builder.set_level(self.sda, 0, self.half_period) # Assume ACK
        self.builder.set_level(self.scl, 1, self.half_period)

class SPIGenerator:
    def __init__(self, builder, clk, mosi, miso, cs, speed=1000000, cpol=0, cpha=0):
        self.builder = builder
        self.clk = clk
        self.mosi = mosi
        self.miso = miso
        self.cs = cs
        self.half_period = int(builder.samplerate / (speed * 2))
        self.cpol = cpol
        self.cpha = cpha
        self.builder.set_idle(cs, 1)
        self.builder.set_idle(clk, cpol)

    def select(self):
        self.builder.set_level(self.cs, 0, self.half_period)

    def deselect(self):
        self.builder.set_level(self.cs, 1, self.half_period)

    def write_byte(self, val):
        for i in range(7, -1, -1):
            bit = (val >> i) & 1
            if self.cpol == 0:
                # Clock idles low
                self.builder.set_level(self.mosi, bit, 0)
                self.builder.set_level(self.clk, 1, self.half_period)
                self.builder.set_level(self.clk, 0, self.half_period)
            else:
                # Clock idles high
                self.builder.set_level(self.mosi, bit, 0)
                self.builder.set_level(self.clk, 0, self.half_period)
                self.builder.set_level(self.clk, 1, self.half_period)

class PS2Generator:
    def __init__(self, builder, clk_ch, data_ch, freq=12500):
        self.builder = builder
        self.clk = clk_ch
        self.data = data_ch
        self.half_period = int(builder.samplerate / (freq * 2))
        self.builder.set_idle(clk_ch, 1)
        self.builder.set_idle(data_ch, 1)

    def write_byte(self, val):
        # 11 bits: 1 start (0), 8 data (LSB first), 1 parity (odd), 1 stop (1)
        p = 1
        bits = [0]
        for i in range(8):
            bit = (val >> i) & 1
            bits.append(bit)
            p ^= bit
        bits.append(p)
        bits.append(1)

        period = self.half_period * 2
        for b in bits:
            # Data must be stable for the full bit period (both CLK high and CLK low)
            self.builder.set_level(self.data, b, period)
            self.builder.pos -= period  # Don't advance, just overlay
            # CLK high for first half, low for second half
            self.builder.set_level(self.clk, 1, self.half_period)
            self.builder.set_level(self.clk, 0, self.half_period)

        # Idle: CLK high for a while
        self.builder.set_level(self.clk, 1, self.half_period * 4)
        self.builder.set_level(self.data, 1, self.half_period * 4)

class JTAGGenerator:
    """4 channels: TDI(ch0), TDO(ch1), TCK(ch2), TMS(ch3)
    Channel order matches C decoder: TDI=0, TDO=1, TCK=2, TMS=3"""
    def __init__(self, builder, tdi, tdo, tck, tms, speed=100000):
        self.builder = builder
        self.tdi = tdi
        self.tdo = tdo
        self.tck = tck
        self.tms = tms
        self.half_period = max(1, int(builder.samplerate / (speed * 2)))
        self.builder.set_idle(tck, 0)
        self.builder.set_idle(tms, 0)
        self.builder.set_idle(tdi, 0)
        self.builder.set_idle(tdo, 0)

    def _clock_cycle(self, tms, tdi):
        """One TCK cycle: set TDI/TMS stable, TCK high, TCK low."""
        period = self.half_period * 2
        # Set TCK low for the full period first, then overlay TMS/TDI
        self.builder.set_level(self.tck, 0, period)
        self.builder.pos -= period
        # Set TMS and TDI for the full period
        self.builder.set_level(self.tms, tms, period)
        self.builder.pos -= period
        self.builder.set_level(self.tdi, tdi, period)
        self.builder.pos -= period
        # Now write TCK high for first half
        self.builder.set_level(self.tck, 1, self.half_period)
        self.builder.set_level(self.tck, 0, self.half_period)

    def reset_tap(self):
        """Go to Test-Logic-Reset: TMS=1 for 5 TCK cycles."""
        for _ in range(5):
            self._clock_cycle(1, 0)

    def go_to_run_test_idle(self):
        """From TLR to Run-Test-Idle: TMS=0."""
        self._clock_cycle(0, 0)

    def shift_dr(self, data, num_bits=8):
        """
        Go from Run-Test-Idle to Shift-DR, shift in data (LSB first),
        then exit back to Run-Test-Idle.
        """
        # Run-Test-Idle -> Select-DR: TMS=1
        self._clock_cycle(1, 0)
        # Select-DR -> Capture-DR: TMS=0
        self._clock_cycle(0, 0)
        # Capture-DR -> Shift-DR: TMS=0
        self._clock_cycle(0, 0)

        # Shift in data bits (LSB first), keep TMS=0 for all but last bit
        for i in range(num_bits - 1):
            bit = (data >> i) & 1
            self._clock_cycle(0, bit)

        # Last bit: TMS=1 to exit Shift-DR -> Exit1-DR
        last_bit = (data >> (num_bits - 1)) & 1
        self._clock_cycle(1, last_bit)

        # Exit1-DR -> Update-DR: TMS=1
        self._clock_cycle(1, 0)
        # Update-DR -> Run-Test-Idle: TMS=0
        self._clock_cycle(0, 0)

class MDIOGenerator:
    """2 channels: MDC(ch0), MDIO(ch1)"""
    def __init__(self, builder, mdc_ch, mdio_ch, speed=100000):
        self.builder = builder
        self.mdc = mdc_ch
        self.mdio = mdio_ch
        self.half_period = max(1, int(builder.samplerate / (speed * 2)))
        self.builder.set_idle(mdc_ch, 0)
        self.builder.set_idle(mdio_ch, 1)

    def _send_bit(self, bit):
        """Set MDIO stable, then MDC high, MDC low."""
        period = self.half_period * 2
        # Set MDC low for full period, then overlay MDIO
        self.builder.set_level(self.mdc, 0, period)
        self.builder.pos -= period
        self.builder.set_level(self.mdio, bit, period)
        self.builder.pos -= period
        # MDC high for first half, low for second half
        self.builder.set_level(self.mdc, 1, self.half_period)
        self.builder.set_level(self.mdc, 0, self.half_period)

    def read_clause22(self, phy_addr=0x01, reg_addr=0x00, data=0x1141):
        """Generate Clause 22 read frame."""
        # PREAMBLE: 32 bits of 1
        for _ in range(32):
            self._send_bit(1)

        # START: 01
        self._send_bit(0)
        self._send_bit(1)

        # OPCODE: 10 (read)
        self._send_bit(1)
        self._send_bit(0)

        # PHY ADDR: 5 bits MSB first
        for i in range(4, -1, -1):
            self._send_bit((phy_addr >> i) & 1)

        # REG ADDR: 5 bits MSB first
        for i in range(4, -1, -1):
            self._send_bit((reg_addr >> i) & 1)

        # TURNAROUND: Z (high, MDIO high-Z), then 0
        self._send_bit(1)  # High-Z (slave releases, pull-up)
        self._send_bit(0)  # Slave drives 0

        # DATA: 16 bits MSB first (slave drives)
        for i in range(15, -1, -1):
            self._send_bit((data >> i) & 1)

class MicrowireGenerator:
    """4 channels: SK(ch0), SI(ch1), SO(ch2), CS(ch3)"""
    def __init__(self, builder, sk_ch, si_ch, so_ch, cs_ch, speed=100000):
        self.builder = builder
        self.sk = sk_ch
        self.si = si_ch
        self.so = so_ch
        self.cs = cs_ch
        self.half_period = max(1, int(builder.samplerate / (speed * 2)))
        self.builder.set_idle(sk_ch, 0)
        self.builder.set_idle(si_ch, 0)
        self.builder.set_idle(so_ch, 0)
        self.builder.set_idle(cs_ch, 1)

    def _send_bit(self, si_bit, so_bit=0):
        """Set SI/SO stable, then SK high, SK low."""
        period = self.half_period * 2
        # Set SK low for full period, then overlay SI and SO
        self.builder.set_level(self.sk, 0, period)
        self.builder.pos -= period
        self.builder.set_level(self.si, si_bit, period)
        self.builder.pos -= period
        self.builder.set_level(self.so, so_bit, period)
        self.builder.pos -= period
        # SK high for first half, low for second half
        self.builder.set_level(self.sk, 1, self.half_period)
        self.builder.set_level(self.sk, 0, self.half_period)

    def read(self, opcode=0b10, addr=0x00, data=0xABCD):
        """
        Generate a read command:
        - CS goes low to start
        - Send opcode (2 bits), address (8 bits), then 16 dummy clocks for data out
        - CS goes high to end
        """
        # CS low to start
        self.builder.set_level(self.cs, 0, self.half_period)

        # Opcode: 2 bits MSB first
        for i in range(1, -1, -1):
            self._send_bit((opcode >> i) & 1)

        # Address: 8 bits MSB first
        for i in range(7, -1, -1):
            self._send_bit((addr >> i) & 1)

        # Data out: 16 dummy clocks, SO has data from slave
        for i in range(15, -1, -1):
            self._send_bit(0, (data >> i) & 1)

        # CS high to end
        self.builder.set_level(self.cs, 1, self.half_period)

class HDLCGenerator:
    """1 channel (RX)"""
    def __init__(self, builder, channel, bitrate=9600):
        self.builder = builder
        self.channel = channel
        self.bit_width = int(builder.samplerate / bitrate)
        self.builder.set_idle(channel, 1)  # Idle high

    def _crc16_ccitt(self, data):
        """CRC-16-CCITT (polynomial 0x8408, init 0xFFFF)."""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 1:
                    crc = (crc >> 1) ^ 0x8408
                else:
                    crc >>= 1
        return crc ^ 0xFFFF

    def _send_bit_nrz(self, bit):
        """NRZ encoding: directly set channel level for bit_width samples."""
        self.builder.set_level(self.channel, bit, self.bit_width)

    def send_frame(self, address, control, data):
        """
        Generate HDLC frame with bit-stuffing:
        Flag | Address | Control | Data | FCS | Flag
        """
        # Collect all raw bits (excluding flags) for CRC
        frame_bytes = [address, control] + list(data)
        fcs = self._crc16_ccitt(frame_bytes)
        # FCS is LSB first (little-endian)
        frame_bytes.append(fcs & 0xFF)
        frame_bytes.append((fcs >> 8) & 0xFF)

        # Opening flag: 0x7E = 01111110
        flag_bits = []
        for i in range(7, -1, -1):
            flag_bits.append((0x7E >> i) & 1)
        for b in flag_bits:
            self._send_bit_nrz(b)

        # Frame content with bit-stuffing
        consecutive_ones = 0
        for byte in frame_bytes:
            for i in range(8):
                bit = (byte >> i) & 1  # LSB first
                self._send_bit_nrz(bit)
                if bit == 1:
                    consecutive_ones += 1
                    if consecutive_ones == 5:
                        # Stuff a 0
                        self._send_bit_nrz(0)
                        consecutive_ones = 0
                else:
                    consecutive_ones = 0

        # Closing flag: 0x7E = 01111110
        for b in flag_bits:
            self._send_bit_nrz(b)

class I2SGenerator:
    """3 channels: SCLK(ch0), LRCK(ch1), SD(ch2)"""
    def __init__(self, builder, sclk_ch, lrck_ch, sd_ch, sample_rate=8000, bits_per_sample=16):
        self.builder = builder
        self.sclk = sclk_ch
        self.lrck = lrck_ch
        self.sd = sd_ch
        self.bits_per_sample = bits_per_sample
        self.half_period = max(1, int(builder.samplerate / (sample_rate * bits_per_sample * 2)))
        self.builder.set_idle(sclk_ch, 0)
        self.builder.set_idle(lrck_ch, 0)
        self.builder.set_idle(sd_ch, 0)

    def _send_channel_data(self, data, num_bits, lrck_level):
        """
        Send data MSB first with LRCK held at lrck_level.
        Data changes on falling SCLK edge, sampled on rising edge.
        """
        for i in range(num_bits - 1, -1, -1):
            bit = (data >> i) & 1
            period = self.half_period * 2
            # Set SCLK low for full period, then overlay SD and LRCK
            self.builder.set_level(self.sclk, 0, period)
            self.builder.pos -= period
            self.builder.set_level(self.sd, bit, period)
            self.builder.pos -= period
            self.builder.set_level(self.lrck, lrck_level, period)
            self.builder.pos -= period
            # Rising edge then falling edge
            self.builder.set_level(self.sclk, 1, self.half_period)
            self.builder.set_level(self.sclk, 0, self.half_period)

    def send_frame(self, left_data=0x1234, right_data=0x5678):
        """Send one complete frame: left channel + right channel."""
        # Left channel: LRCK low
        self._send_channel_data(left_data, self.bits_per_sample, 0)
        # Right channel: LRCK high
        self._send_channel_data(right_data, self.bits_per_sample, 1)

class ISO7816Generator:
    """2 channels: CLK(ch0), IO(ch1)"""
    def __init__(self, builder, clk_ch, io_ch, etu=372, samplerate=1000000):
        self.builder = builder
        self.clk = clk_ch
        self.io = io_ch
        self.bit_width = etu  # 1 etu in samples
        self.clk_half_period = max(1, int(samplerate / 4000000))  # CLK ~2MHz
        self.builder.set_idle(clk_ch, 0)
        self.builder.set_idle(io_ch, 1)

    def _run_clk(self, num_half_periods):
        """Toggle CLK for the given number of half periods."""
        for _ in range(num_half_periods):
            self.builder.set_level(self.clk, 1, self.clk_half_period)
            self.builder.set_level(self.clk, 0, self.clk_half_period)

    def _send_byte(self, val):
        """
        Send a byte: 1 start bit (low), 8 data bits (LSB first),
        1 parity bit, 2 etu guard time.
        """
        # Compute parity (even)
        p = 0
        for i in range(8):
            p ^= (val >> i) & 1

        bits = [0]  # Start bit
        for i in range(8):
            bits.append((val >> i) & 1)  # LSB first
        bits.append(p)  # Parity bit

        for bit in bits:
            self.builder.set_level(self.io, bit, self.bit_width)
            # Run CLK during each etu
            clk_half = self.bit_width // (self.clk_half_period * 2)
            self._run_clk(max(clk_half, 1))

        # Guard time: 2 etu of idle (high) on IO
        self.builder.set_level(self.io, 1, self.bit_width * 2)
        clk_half = (self.bit_width * 2) // (self.clk_half_period * 2)
        self._run_clk(max(clk_half, 1))

    def send_atr(self):
        """Generate ATR (Answer To Reset)."""
        # IO idle high
        self.builder.set_level(self.io, 1, self.bit_width * 4)
        # TS: 0x3B (direct convention)
        self._send_byte(0x3B)
        # T0: 0x80 (TD1 present, no historical bytes)
        self._send_byte(0x80)
        # TD1: 0x31 (T=1 protocol)
        self._send_byte(0x31)

class SWDGenerator:
    """2 channels: SWCLK(ch0), SWDIO(ch1)"""
    def __init__(self, builder, swclk_ch, swdio_ch, speed=100000):
        self.builder = builder
        self.swclk = swclk_ch
        self.swdio = swdio_ch
        self.half_period = max(1, int(builder.samplerate / (speed * 2)))
        self.builder.set_idle(swclk_ch, 0)
        self.builder.set_idle(swdio_ch, 0)

    def _clock_bit(self, swdio_val):
        """Set SWDIO, then SWCLK high, SWCLK low."""
        period = self.half_period * 2
        # Set SWCLK low for full period, then overlay SWDIO
        self.builder.set_level(self.swclk, 0, period)
        self.builder.pos -= period
        self.builder.set_level(self.swdio, swdio_val, period)
        self.builder.pos -= period
        # SWCLK high then low
        self.builder.set_level(self.swclk, 1, self.half_period)
        self.builder.set_level(self.swclk, 0, self.half_period)

    def _turnaround(self):
        """1 clock with SWDIO high (floating/pull-up)."""
        self._clock_bit(1)

    def _read_bits(self, num_bits):
        """Read num_bits from SWDIO (simulate with known data)."""
        for _ in range(num_bits):
            self._clock_bit(1)

    def line_reset(self):
        """Line reset: 50+ clocks with SWDIO=1."""
        for _ in range(50):
            self._clock_bit(1)

    def read_dp(self, addr2=0, addr3=0, data=0x0BC11477):
        """
        Read DP register.
        addr2, addr3: A2, A3 bits of DP register address.
        data: 32-bit read value to simulate.
        """
        # Request phase: 8 bits
        # Start: 1
        start = 1
        apndp = 0  # DP
        rnw = 1    # Read
        # Parity of bits [1:4] = APnDP ^ RnW ^ A2 ^ A3
        parity = (apndp ^ rnw ^ addr2 ^ addr3) & 1
        stop = 0
        park = 1

        request_bits = [start, apndp, rnw, addr2, addr3, parity, stop, park]
        for bit in request_bits:
            self._clock_bit(bit)

        # Turnaround: 1 clock
        self._turnaround()

        # ACK: 3 clocks (001 = OK/FAULT response from target)
        # We simulate ACK=001 (OK)
        ack_bits = [1, 0, 0]  # Written LSB first on wire: bit0=1, bit1=0, bit2=0 => ACK=001
        for bit in ack_bits:
            self._clock_bit(bit)

        # Data: 32 bits LSB first
        for i in range(32):
            self._clock_bit((data >> i) & 1)

        # Data parity: even parity of 32 data bits
        data_parity = 0
        for i in range(32):
            data_parity ^= (data >> i) & 1
        self._clock_bit(data_parity)

        # Turnaround: 1 clock
        self._turnaround()

class OneWireGenerator:
    """1 channel"""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.samplerate = samplerate
        self.builder.set_idle(channel, 1)  # Idle high (pull-up)

    def _reset(self):
        """
        Generate reset pulse + presence detect.
        Master pulls low 480us, releases 480us.
        Slave pulls low starting ~60us after release for ~60us.
        """
        # Master pulls low for 480us
        self.builder.set_level(self.channel, 0, 480)
        # Release (high via pull-up)
        self.builder.set_level(self.channel, 1, 15)
        # Slave presence: pulls low for ~60us starting at ~15us after release
        self.builder.set_level(self.channel, 0, 60)
        # Release back to high
        self.builder.set_level(self.channel, 1, 480 - 15 - 60)

    def _write_bit(self, bit):
        """
        Write a bit:
        Write 1: pull low 1-15us, release for rest of time slot (60-120us total)
        Write 0: pull low 60-120us, release for 1-15us
        """
        if bit:
            # Write 1: short low pulse
            self.builder.set_level(self.channel, 0, 10)
            self.builder.set_level(self.channel, 1, 60 - 10)
        else:
            # Write 0: long low pulse
            self.builder.set_level(self.channel, 0, 60)
            self.builder.set_level(self.channel, 1, 10)

    def _read_bit(self, bit):
        """
        Read a bit (master initiates same as write 1, but sample at 15us).
        We simulate the slave driving the line.
        """
        # Master pulls low briefly
        self.builder.set_level(self.channel, 0, 1)
        # Release, slave drives the bit
        self.builder.set_level(self.channel, bit, 15)
        # Rest of time slot
        self.builder.set_level(self.channel, 1, 60 - 1 - 15)

    def _write_byte(self, val):
        """Write byte LSB first."""
        for i in range(8):
            self._write_bit((val >> i) & 1)

    def _read_byte(self, val):
        """Read byte LSB first (simulate slave response)."""
        result = 0
        for i in range(8):
            bit = (val >> i) & 1
            self._read_bit(bit)
        return result

    def read_rom(self, rom_data=None):
        """
        Generate reset + presence + Read ROM command (0x33) + read 8 bytes.
        """
        if rom_data is None:
            rom_data = [0x28, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00]

        # Reset + presence
        self._reset()

        # Command: 0x33 (Read ROM), LSB first
        self._write_byte(0x33)

        # Read 8 bytes of ROM data
        for b in rom_data:
            self._read_byte(b)

class NRZIGenerator:
    """NRZI encoding: bit 0 = toggle level, bit 1 = no change."""
    def __init__(self, builder, channel, bitrate=1000000):
        self.builder = builder
        self.channel = channel
        self.bit_width = int(builder.samplerate / bitrate)
        self.current_level = 0
        self.builder.set_idle(channel, 0)

    def send_bits(self, bits_list):
        for bit in bits_list:
            if bit == 0:
                self.current_level = 1 - self.current_level
            self.builder.set_level(self.channel, self.current_level, self.bit_width)

    def send_bytes(self, data):
        for byte in data:
            for i in range(7, -1, -1):
                bit = (byte >> i) & 1
                if bit == 0:
                    self.current_level = 1 - self.current_level
                self.builder.set_level(self.channel, self.current_level, self.bit_width)

class IRNECGenerator:
    """NEC IR protocol generator (baseband)."""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.sr = samplerate
        self.builder.set_idle(channel, 0)

    def _us(self, us):
        return int(us * self.sr / 1e6)

    def _send_bit(self, bit):
        # 562.5us burst (high) then space (low)
        self.builder.set_level(self.channel, 1, self._us(562.5))
        if bit:
            self.builder.set_level(self.channel, 0, self._us(1687.5))
        else:
            self.builder.set_level(self.channel, 0, self._us(562.5))

    def send_nec(self, address=0x04, command=0x08):
        # Leader: 9ms burst + 4.5ms space
        self.builder.set_level(self.channel, 1, self._us(9000))
        self.builder.set_level(self.channel, 0, self._us(4500))
        # 32 bits: address + ~address + command + ~command
        for byte_val in [address, (~address) & 0xFF, command, (~command) & 0xFF]:
            for i in range(7, -1, -1):
                self._send_bit((byte_val >> i) & 1)

class IRRC5Generator:
    """RC5 IR protocol generator (baseband Manchester)."""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.half_period = int(889 * samplerate / 1e6)  # 889us half-bit
        self.builder.set_idle(channel, 0)

    def _write_manchester(self, bit):
        # RC5 Manchester: 1 = low-then-high, 0 = high-then-low
        if bit:
            self.builder.set_level(self.channel, 0, self.half_period)
            self.builder.set_level(self.channel, 1, self.half_period)
        else:
            self.builder.set_level(self.channel, 1, self.half_period)
            self.builder.set_level(self.channel, 0, self.half_period)

    def send_rc5(self, address=0x00, command=0x01, toggle=0):
        # 2 start bits (1,1), toggle bit, 5 address bits, 6 command bits
        bits = [1, 1, toggle]
        for i in range(4, -1, -1):
            bits.append((address >> i) & 1)
        for i in range(5, -1, -1):
            bits.append((command >> i) & 1)
        for bit in bits:
            self._write_manchester(bit)

class IRRC6Generator:
    """RC6 IR protocol generator (baseband Manchester)."""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.sr = samplerate
        self.half_period = int(222 * samplerate / 1e6)  # 222us half-bit (444us bit period)
        self.builder.set_idle(channel, 0)

    def _us(self, us):
        return int(us * self.sr / 1e6)

    def _write_manchester(self, bit):
        # RC6 Manchester: 1 = high-then-low, 0 = low-then-high
        if bit:
            self.builder.set_level(self.channel, 1, self.half_period)
            self.builder.set_level(self.channel, 0, self.half_period)
        else:
            self.builder.set_level(self.channel, 0, self.half_period)
            self.builder.set_level(self.channel, 1, self.half_period)

    def _write_toggle(self, bit):
        # Double-width Manchester for toggle bit (889us each half)
        double_half = self._us(889)
        if bit:
            self.builder.set_level(self.channel, 1, double_half)
            self.builder.set_level(self.channel, 0, double_half)
        else:
            self.builder.set_level(self.channel, 0, double_half)
            self.builder.set_level(self.channel, 1, double_half)

    def send_rc6(self, address=0x00, command=0x01, toggle=0):
        # Leader: 2.667ms low + 889us high
        self.builder.set_level(self.channel, 0, self._us(2667))
        self.builder.set_level(self.channel, 1, self._us(889))
        # Start bit: 1
        self._write_manchester(1)
        # 3 mode bits: 000
        for bit in [0, 0, 0]:
            self._write_manchester(bit)
        # Toggle bit (double-width)
        self._write_toggle(toggle)
        # 8 address bits
        for i in range(7, -1, -1):
            self._write_manchester((address >> i) & 1)
        # 8 command bits
        for i in range(7, -1, -1):
            self._write_manchester((command >> i) & 1)

class IRSIRCGenerator:
    """SIRC IR protocol generator (baseband)."""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.sr = samplerate
        self.builder.set_idle(channel, 0)

    def _us(self, us):
        return int(us * self.sr / 1e6)

    def _send_bit(self, bit):
        if bit:
            self.builder.set_level(self.channel, 1, self._us(1200))
        else:
            self.builder.set_level(self.channel, 1, self._us(600))
        self.builder.set_level(self.channel, 0, self._us(600))

    def send_sirc(self, command=0x15, address=0x01):
        # Start: 2400us burst + 600us space
        self.builder.set_level(self.channel, 1, self._us(2400))
        self.builder.set_level(self.channel, 0, self._us(600))
        # 12 bits: 7 command + 5 address, LSB first
        bits = []
        for i in range(7):
            bits.append((command >> i) & 1)
        for i in range(5):
            bits.append((address >> i) & 1)
        for bit in bits:
            self._send_bit(bit)

class PWMGenerator:
    """PWM waveform generator."""
    def __init__(self, builder, channel, freq=1000, duty=0.5):
        self.builder = builder
        self.channel = channel
        self.freq = freq
        self.duty = duty
        self.period = int(builder.samplerate / freq)
        self.builder.set_idle(channel, 0)

    def send_cycles(self, n):
        high_samples = int(self.period * self.duty)
        low_samples = self.period - high_samples
        for _ in range(n):
            self.builder.set_level(self.channel, 1, high_samples)
            self.builder.set_level(self.channel, 0, low_samples)

class DCCGenerator:
    """DCC model railway protocol generator."""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.sr = samplerate
        self.builder.set_idle(channel, 1)

    def _us(self, us):
        return int(us * self.sr / 1e6)

    def _send_bit(self, bit):
        if bit:
            # '1': 58us high + 58us low
            self.builder.set_level(self.channel, 1, self._us(58))
            self.builder.set_level(self.channel, 0, self._us(58))
        else:
            # '0': 100us high + 100us low
            self.builder.set_level(self.channel, 1, self._us(100))
            self.builder.set_level(self.channel, 0, self._us(100))

    def send_packet(self, address=0x03, instruction=0x7F):
        # Preamble: 14+ bits of '1'
        for _ in range(14):
            self._send_bit(1)
        # Start bit
        self._send_bit(0)
        # Address byte MSB first
        for i in range(7, -1, -1):
            self._send_bit((address >> i) & 1)
        # Instruction byte MSB first
        for i in range(7, -1, -1):
            self._send_bit((instruction >> i) & 1)
        # Error detection: XOR of address and instruction
        xor_byte = address ^ instruction
        for i in range(7, -1, -1):
            self._send_bit((xor_byte >> i) & 1)
        # End bit
        self._send_bit(0)

class DMX512Generator:
    """DMX512 protocol generator."""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.sr = samplerate
        self.bit_samples = int(4 * samplerate / 1e6)  # 4us per bit at 250kbaud
        self.builder.set_idle(channel, 1)

    def _send_byte(self, val):
        # Start bit (0)
        self.builder.set_level(self.channel, 0, self.bit_samples)
        # 8 data bits LSB first
        for i in range(8):
            self.builder.set_level(self.channel, (val >> i) & 1, self.bit_samples)
        # 2 stop bits (1,1)
        self.builder.set_level(self.channel, 1, self.bit_samples * 2)

    def send_frame(self, slots=None):
        if slots is None:
            slots = [0x00, 0xFF, 0x80, 0x40, 0x20]
        # Break: 88us low (22 bit times)
        break_samples = self.bit_samples * 22
        self.builder.set_level(self.channel, 0, break_samples)
        # Mark After Break: 8us high (2 bit times)
        mab_samples = self.bit_samples * 2
        self.builder.set_level(self.channel, 1, mab_samples)
        # Start code: 0x00
        self._send_byte(0x00)
        # Data slots
        for slot in slots:
            self._send_byte(slot)

class DALIGenerator:
    """DALI protocol generator (Manchester encoded)."""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.bit_samples = int(416.7 * samplerate / 1e6)  # ~417 samples per bit
        self.half_bit = self.bit_samples // 2
        self.builder.set_idle(channel, 0)

    def _write_manchester(self, bit):
        # DALI Manchester: 1 = high-to-low at mid-bit, 0 = low-to-high at mid-bit
        if bit:
            self.builder.set_level(self.channel, 1, self.half_bit)
            self.builder.set_level(self.channel, 0, self.half_bit)
        else:
            self.builder.set_level(self.channel, 0, self.half_bit)
            self.builder.set_level(self.channel, 1, self.half_bit)

    def send_forward(self, address=0xFF, command=0x20):
        # Start: high for 2 bit periods (no transition)
        self.builder.set_level(self.channel, 1, self.bit_samples * 2)
        # 16 bits Manchester: address byte + command byte
        for i in range(7, -1, -1):
            self._write_manchester((address >> i) & 1)
        for i in range(7, -1, -1):
            self._write_manchester((command >> i) & 1)

class CECGenerator:
    """HDMI CEC protocol generator."""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.sr = samplerate
        self.builder.set_idle(channel, 0)

    def _us(self, us):
        return int(us * self.sr / 1e6)

    def _send_start(self):
        # Start bit: high for 3.7ms, low for 0.8ms
        self.builder.set_level(self.channel, 1, self._us(3700))
        self.builder.set_level(self.channel, 0, self._us(800))

    def _send_bit(self, bit):
        if bit:
            # 1 = high 0.6ms + low 0.6ms
            self.builder.set_level(self.channel, 1, self._us(600))
            self.builder.set_level(self.channel, 0, self._us(600))
        else:
            # 0 = high 1.5ms + low 0.6ms
            self.builder.set_level(self.channel, 1, self._us(1500))
            self.builder.set_level(self.channel, 0, self._us(600))

    def send_frame(self, initiator=0x0, follower=0x4):
        # Start bit
        self._send_start()
        # Header: 4-bit initiator + 4-bit follower
        header = (initiator << 4) | follower
        for i in range(7, -1, -1):
            self._send_bit((header >> i) & 1)
        # EOM = 1
        self._send_bit(1)
        # ACK = 1
        self._send_bit(1)

class SPDIFGenerator:
    """S/PDIF biphase mark code generator."""
    def __init__(self, builder, channel, bit_samples=100):
        self.builder = builder
        self.channel = channel
        self.bit_samples = bit_samples
        self.builder.set_idle(channel, 0)

    def _send_bmc_bit(self, bit, current_level):
        # Always transition at start of bit period
        level = 1 - current_level
        if bit:
            # '1': additional transition at mid-period
            half = self.bit_samples // 2
            self.builder.set_level(self.channel, level, half)
            level = 1 - level
            self.builder.set_level(self.channel, level, self.bit_samples - half)
        else:
            # '0': no mid-period transition
            self.builder.set_level(self.channel, level, self.bit_samples)
        return level

    def _send_preamble_b(self, current_level):
        half = self.bit_samples // 2
        # Preamble B: 3 high, 3 low, 2 high (in half-bit units)
        # Starting from opposite of current_level
        start_level = 1 - current_level
        if start_level == 1:
            self.builder.set_level(self.channel, 1, half * 3)
            self.builder.set_level(self.channel, 0, half * 3)
            self.builder.set_level(self.channel, 1, half * 2)
            return 1
        else:
            self.builder.set_level(self.channel, 0, half * 3)
            self.builder.set_level(self.channel, 1, half * 3)
            self.builder.set_level(self.channel, 0, half * 2)
            return 0

    def send_frame(self, subframe_data=0x00000000):
        current_level = 0
        # Preamble B (block start)
        current_level = self._send_preamble_b(current_level)
        # Subframe data: 32 bits BMC encoded
        # Bits: 4 auxiliary + 20 audio + V U C P
        for i in range(31, -1, -1):
            bit = (subframe_data >> i) & 1
            current_level = self._send_bmc_bit(bit, current_level)

class WiegandGenerator:
    """Wiegand 26-bit protocol generator (2 channels: DATA0, DATA1)."""
    def __init__(self, builder, ch0, ch1, samplerate=1000000):
        self.builder = builder
        self.data0 = ch0  # Bit 0 channel
        self.data1 = ch1  # Bit 1 channel
        self.sr = samplerate
        self.pulse_width = int(50 * samplerate / 1e6)   # 50us pulse
        self.inter_gap = int(1000 * samplerate / 1e6)    # 1ms gap
        # Both idle high
        self.builder.set_idle(ch0, 1)
        self.builder.set_idle(ch1, 1)

    def _send_bit(self, bit):
        if bit == 0:
            # Pulse DATA0 low for pulse_width, DATA1 stays high
            self.builder.set_level(self.data0, 0, self.pulse_width)
            self.builder.pos -= self.pulse_width
            self.builder.set_level(self.data1, 1, self.pulse_width)
            # Both high for inter-bit gap
            self.builder.set_level(self.data0, 1, self.inter_gap)
            self.builder.pos -= self.inter_gap
            self.builder.set_level(self.data1, 1, self.inter_gap)
        else:
            # Pulse DATA1 low for pulse_width, DATA0 stays high
            self.builder.set_level(self.data1, 0, self.pulse_width)
            self.builder.pos -= self.pulse_width
            self.builder.set_level(self.data0, 1, self.pulse_width)
            # Both high for inter-bit gap
            self.builder.set_level(self.data0, 1, self.inter_gap)
            self.builder.pos -= self.inter_gap
            self.builder.set_level(self.data1, 1, self.inter_gap)

    def send_wiegand26(self, facility=0x01, card=0x0001):
        # Bits 1-8: facility code MSB first
        fac_bits = []
        for i in range(7, -1, -1):
            fac_bits.append((facility >> i) & 1)
        # Bits 9-24: card number MSB first
        card_bits = []
        for i in range(15, -1, -1):
            card_bits.append((card >> i) & 1)
        # Even parity over bits 1-12 (facility + first 4 card bits)
        parity_bits = fac_bits + card_bits
        even_count = sum(parity_bits[:12])
        ep = 1 if even_count % 2 == 1 else 0  # Make total even
        # Odd Parity over bits 13-24 (last 12 card bits)
        odd_count = sum(parity_bits[12:24])
        op = 1 if odd_count % 2 == 0 else 0  # Make total odd
        # Full 26 bits: even_parity + 8 facility + 16 card + odd_parity
        all_bits = [ep] + fac_bits + card_bits + [op]
        for bit in all_bits:
            self._send_bit(bit)

class OpenthermGenerator:
    """Opentherm protocol generator (Manchester encoded)."""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.half_bit = int(500 * samplerate / 1e6)  # 500us half-bit
        self.builder.set_idle(channel, 0)

    def _write_manchester(self, bit):
        # Opentherm Manchester: 1 = low-to-high at mid-bit, 0 = high-to-low at mid-bit
        if bit:
            self.builder.set_level(self.channel, 0, self.half_bit)
            self.builder.set_level(self.channel, 1, self.half_bit)
        else:
            self.builder.set_level(self.channel, 1, self.half_bit)
            self.builder.set_level(self.channel, 0, self.half_bit)

    def send_read_request(self, msg_type=0x00, data_id=0x00, data_value=0x0000):
        # 32-bit frame: 1 start bit + 8 msg type + 8 data ID + 16 data value
        bits = [1]  # Start bit
        for i in range(7, -1, -1):
            bits.append((msg_type >> i) & 1)
        for i in range(7, -1, -1):
            bits.append((data_id >> i) & 1)
        for i in range(15, -1, -1):
            bits.append((data_value >> i) & 1)
        for bit in bits:
            self._write_manchester(bit)

class SENTGenerator:
    """SENT (Single Edge Nibble Transmission) generator."""
    def __init__(self, builder, channel, tick_freq=15000, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.tick_samples = int(samplerate / tick_freq)  # ~67 samples per tick
        self.builder.set_idle(channel, 0)

    def _send_nibble(self, value):
        # Low for 12 ticks, high for (value*12 + 12) ticks
        low_ticks = 12
        high_ticks = value * 12 + 12
        self.builder.set_level(self.channel, 0, low_ticks * self.tick_samples)
        self.builder.set_level(self.channel, 1, high_ticks * self.tick_samples)

    def _sent_crc4(self, nibbles):
        crc = 5
        for nibble in nibbles:
            for i in range(3, -1, -1):
                bit = (nibble >> i) & 1
                if (crc >> 3) ^ bit:
                    crc = ((crc << 1) ^ 0x7) & 0xF
                else:
                    crc = (crc << 1) & 0xF
        return crc

    def send_message(self, status=0, data_nibbles=None):
        if data_nibbles is None:
            data_nibbles = [1, 2, 3, 4, 5, 6]
        # Calibration pulse: 56 ticks low + 12 ticks high
        self.builder.set_level(self.channel, 0, 56 * self.tick_samples)
        self.builder.set_level(self.channel, 1, 12 * self.tick_samples)
        # Status nibble
        self._send_nibble(status)
        # Data nibbles
        for nib in data_nibbles:
            self._send_nibble(nib)
        # CRC nibble
        crc = self._sent_crc4([status] + data_nibbles)
        self._send_nibble(crc)

class MIPIRFFEGenerator:
    """MIPI RFFE protocol generator (2 channels: SCLK, SDATA)."""
    def __init__(self, builder, scl_ch, sda_ch, freq=1000000, samplerate=1000000):
        self.builder = builder
        self.scl = scl_ch
        self.sda = sda_ch
        self.bit_width = max(2, int(samplerate / freq))
        self.builder.set_idle(scl_ch, 1)
        self.builder.set_idle(sda_ch, 1)

    def _start_condition(self):
        # SDATA falls while SCLK high
        half = self.bit_width // 2
        self.builder.set_level(self.scl, 1, half)
        self.builder.pos -= half
        self.builder.set_level(self.sda, 1, half // 2)
        self.builder.set_level(self.sda, 0, half - half // 2)

    def _stop_condition(self):
        # SDATA rises while SCLK high
        half = self.bit_width // 2
        self.builder.set_level(self.scl, 1, half)
        self.builder.pos -= half
        self.builder.set_level(self.sda, 0, half // 2)
        self.builder.set_level(self.sda, 1, half - half // 2)

    def _write_bit(self, bit):
        # SDATA stable for full bit width, SCLK pulses low then high
        half = self.bit_width // 2
        self.builder.set_level(self.sda, bit, self.bit_width)
        self.builder.pos -= self.bit_width
        self.builder.set_level(self.scl, 0, half)
        self.builder.set_level(self.scl, 1, half)

    def send_ext_write(self, command=0x11, address=0x0, data=0x55):
        # Start condition
        self._start_condition()
        # Command byte MSB first
        for i in range(7, -1, -1):
            self._write_bit((command >> i) & 1)
        # Address byte MSB first
        for i in range(7, -1, -1):
            self._write_bit((address >> i) & 1)
        # Data byte MSB first
        for i in range(7, -1, -1):
            self._write_bit((data >> i) & 1)
        # Parity bit (even parity over command + address + data = 24 bits)
        all_bits = []
        for byte_val in [command, address, data]:
            for i in range(7, -1, -1):
                all_bits.append((byte_val >> i) & 1)
        parity = sum(all_bits) % 2
        self._write_bit(parity)
        # Stop condition
        self._stop_condition()


class Z80Generator:
    """Z80 CPU bus protocol generator.
    Channels: MREQ(ch0), IORQ(ch1), RD(ch2), WR(ch3), M1(ch4), RFSH(ch5), A0(ch6), D0-D3(ch7-10)
    All control signals active low."""
    def __init__(self, builder, bit_width=100):
        self.builder = builder
        self.bw = bit_width
        for ch in range(7):
            builder.set_idle(ch, 1)
        for ch in range(7, 11):
            builder.set_idle(ch, 0)

    def _set_addr_data(self, addr_lo, data_lo4):
        self.builder.set_level(6, addr_lo & 1, 0)
        for i in range(4):
            self.builder.set_level(7 + i, (data_lo4 >> i) & 1, 0)

    def m1_cycle(self, addr, opcode):
        """Generate an M1 (opcode fetch) cycle."""
        bw = self.bw
        self.builder.set_level(4, 0, 0)
        self._set_addr_data(addr, opcode & 0xF)
        self.builder.set_level(4, 0, bw)
        self.builder.set_level(0, 0, 0)
        self.builder.set_level(2, 0, 0)
        self._set_addr_data(addr, opcode & 0xF)
        self.builder.set_level(0, 0, bw)
        self.builder.pos -= bw
        self.builder.set_level(2, 0, bw)
        self._set_addr_data(addr, opcode & 0xF)
        self.builder.set_level(4, 1, bw)
        self.builder.set_level(2, 1, 0)
        self.builder.set_level(0, 1, 0)
        self.builder.set_level(5, 0, 0)
        self.builder.set_level(0, 0, 0)
        self.builder.set_level(5, 0, bw)
        self.builder.set_level(0, 0, bw)
        self.builder.pos -= bw
        self.builder.set_level(5, 1, bw)
        self.builder.set_level(0, 1, 0)

    def mem_read_cycle(self, addr, data):
        """Generate a memory read cycle."""
        bw = self.bw
        self._set_addr_data(addr, 0)
        self.builder.set_level(6, addr & 1, bw)
        self.builder.set_level(0, 0, 0)
        self.builder.set_level(2, 0, 0)
        self._set_addr_data(addr, data & 0xF)
        self.builder.set_level(0, 0, bw)
        self.builder.pos -= bw
        self.builder.set_level(2, 0, bw)
        self._set_addr_data(addr, data & 0xF)
        self.builder.set_level(0, 1, bw)
        self.builder.set_level(2, 1, 0)

class MCS48Generator:
    """MCS-48 (8048 family) bus protocol generator.
    Channels: ALE(ch0), RD(ch1), WR(ch2), PSEN(ch3), EA(ch4), D0-D7(ch5-12), A0(ch13)
    Control signals active low except ALE."""
    def __init__(self, builder, bit_width=100):
        self.builder = builder
        self.bw = bit_width
        builder.set_idle(0, 0)
        builder.set_idle(1, 1)
        builder.set_idle(2, 1)
        builder.set_idle(3, 1)
        builder.set_idle(4, 1)
        for ch in range(5, 14):
            builder.set_idle(ch, 0)

    def _set_data(self, val):
        for i in range(8):
            self.builder.set_level(5 + i, (val >> i) & 1, 0)

    def opcode_fetch(self, addr, opcode):
        """Generate an opcode fetch cycle."""
        bw = self.bw
        self.builder.set_level(13, addr & 1, 0)
        self._set_data(addr & 0xFF)
        self.builder.set_level(0, 1, bw)
        self.builder.set_level(0, 0, bw)
        self._set_data(opcode)
        self.builder.set_level(3, 0, 0)
        self.builder.set_level(3, 0, bw)
        self.builder.set_level(3, 1, bw)

class GPIBGenerator:
    """GPIB (IEEE-488) protocol generator.
    Channels: DIO1-DIO8(ch0-7), DAV(ch8), NRFD(ch9), NDAC(ch10), ATN(ch11), SRQ(ch12), IFC(ch13), REN(ch14), EOI(ch15)
    All handshake signals active low."""
    def __init__(self, builder, bit_width=100):
        self.builder = builder
        self.bw = bit_width
        builder.set_idle(8, 1)
        builder.set_idle(9, 1)
        builder.set_idle(10, 1)
        builder.set_idle(11, 1)
        builder.set_idle(12, 1)
        builder.set_idle(13, 1)
        builder.set_idle(14, 0)
        builder.set_idle(15, 1)

    def _set_dio(self, val):
        for i in range(8):
            self.builder.set_level(i, (val >> i) & 1, 0)

    def send_command(self, cmd_byte):
        """Send a command byte with ATN asserted."""
        bw = self.bw
        self.builder.set_level(11, 0, bw)
        self._set_dio(cmd_byte)
        self.builder.set_level(8, 1, bw)
        self.builder.set_level(8, 0, bw)
        self.builder.set_level(9, 1, 0)
        self.builder.set_level(10, 0, 0)
        self.builder.set_level(9, 1, bw)
        self.builder.pos -= bw
        self.builder.set_level(10, 0, bw)
        self.builder.set_level(8, 1, bw)
        self.builder.set_level(10, 1, 0)

    def command_sequence(self):
        """Send UNL + LAG 0 command sequence."""
        bw = self.bw
        self.builder.set_level(11, 0, 0)
        self.send_command(0x3F)
        self.send_command(0x20)
        self.builder.set_level(11, 1, bw)

class LPCGenerator:
    """LPC (Low Pin Count) bus protocol generator.
    Channels: LFRAME(ch0), LCLK(ch1), LAD0-LAD3(ch2-5)
    LCLK toggles continuously, LAD changes on rising edge."""
    def __init__(self, builder, bit_width=50):
        self.builder = builder
        self.bw = bit_width
        builder.set_idle(0, 1)
        builder.set_idle(1, 0)

    def _clock_pulse(self):
        self.builder.set_level(1, 1, self.bw)
        self.builder.set_level(1, 0, self.bw)

    def _set_lad(self, val4):
        for i in range(4):
            self.builder.set_level(2 + i, (val4 >> i) & 1, 0)

    def io_write(self, addr, data):
        """Generate an I/O write cycle."""
        self.builder.set_level(0, 0, 0)
        self._set_lad(0x0)
        self._clock_pulse()
        self.builder.set_level(0, 1, 0)
        self._set_lad(0x2)
        self._clock_pulse()
        self._set_lad(addr & 0xF)
        self._clock_pulse()
        self._set_lad((addr >> 4) & 0xF)
        self._clock_pulse()
        self._set_lad(data & 0xF)
        self._clock_pulse()
        self._set_lad(0xF)
        self._clock_pulse()
        self._clock_pulse()
        self._set_lad(0x0)
        self._clock_pulse()

class AM230xGenerator:
    """AM2302/DHT22 temperature sensor protocol generator.
    1 channel: DATA(ch0). At 1MHz samplerate."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        builder.set_idle(channel, 1)

    def _us(self, us):
        return int(us * self.sr / 1000000)

    def send_reading(self, humidity, temp):
        """Send a 40-bit AM230x reading."""
        ch = self.ch
        self.builder.set_level(ch, 0, self._us(1000))
        self.builder.set_level(ch, 1, self._us(30))
        self.builder.set_level(ch, 0, self._us(80))
        self.builder.set_level(ch, 1, self._us(80))
        hum_int = int(humidity * 10)
        temp_int = int(temp * 10)
        checksum = ((hum_int >> 8) + (hum_int & 0xFF) + (temp_int >> 8) + (temp_int & 0xFF)) & 0xFF
        bits = []
        for i in range(15, -1, -1):
            bits.append((hum_int >> i) & 1)
        for i in range(15, -1, -1):
            bits.append((temp_int >> i) & 1)
        for i in range(7, -1, -1):
            bits.append((checksum >> i) & 1)
        for b in bits:
            self.builder.set_level(ch, 0, self._us(50))
            if b:
                self.builder.set_level(ch, 1, self._us(70))
            else:
                self.builder.set_level(ch, 1, self._us(26))
        self.builder.set_level(ch, 1, self._us(50))

class DCF77Generator:
    """DCF77 time signal generator.
    1 channel: DATA(ch0). At 1MHz samplerate."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        builder.set_idle(channel, 1)

    def _ms(self, ms):
        return int(ms * self.sr / 1000)

    def send_bit(self, bit):
        """Send one second of DCF77 data."""
        if bit:
            self.builder.set_level(self.ch, 0, self._ms(200))
            self.builder.set_level(self.ch, 1, self._ms(800))
        else:
            self.builder.set_level(self.ch, 0, self._ms(100))
            self.builder.set_level(self.ch, 1, self._ms(900))

    def send_time(self, bits_list):
        """Send a sequence of DCF77 bits."""
        for b in bits_list:
            self.send_bit(b)

class CaliperGenerator:
    """Digital caliper protocol generator.
    2 channels: CLK(ch0), DATA(ch1)."""
    def __init__(self, builder, clk_ch=0, data_ch=1, samplerate=1000000):
        self.builder = builder
        self.clk = clk_ch
        self.data = data_ch
        self.bit_width = int(samplerate / 8000)
        builder.set_idle(clk_ch, 0)
        builder.set_idle(data_ch, 0)

    def send_value(self, value, num_bits=24):
        """Send a value LSB first, clocked by CLK."""
        for i in range(num_bits):
            bit = (value >> i) & 1
            self.builder.set_level(self.data, bit, 0)
            self.builder.set_level(self.clk, 1, self.bit_width)
            self.builder.set_level(self.clk, 0, self.bit_width)

class C2Generator:
    """C2 interface (Intel) protocol generator.
    2 channels: SDIO(ch0), SCLK(ch1)."""
    def __init__(self, builder, sdio_ch=0, sclk_ch=1, samplerate=1000000):
        self.builder = builder
        self.sdio = sdio_ch
        self.sclk = sclk_ch
        self.half_period = int(samplerate / 200000)
        builder.set_idle(sdio_ch, 1)
        builder.set_idle(sclk_ch, 1)

    def send_reset(self):
        """Send a C2 reset command."""
        self.builder.set_level(self.sclk, 1, self.half_period)
        self.builder.set_level(self.sdio, 0, 0)
        self.builder.set_level(self.sdio, 0, self.half_period)
        for i in range(8):
            bit = (0xFF >> i) & 1
            self.builder.set_level(self.sdio, bit, 0)
            self.builder.set_level(self.sclk, 0, self.half_period)
            self.builder.set_level(self.sclk, 1, self.half_period)

class AVRPDIGenerator:
    """AVR PDI (Program and Debug Interface) generator.
    2 channels: PDI_DATA(ch0), PDI_CLK(ch1)."""
    def __init__(self, builder, data_ch=0, clk_ch=1, samplerate=1000000):
        self.builder = builder
        self.data = data_ch
        self.clk = clk_ch
        self.half_period = int(samplerate / 200000)
        builder.set_idle(data_ch, 0)
        builder.set_idle(clk_ch, 0)

    def send_byte(self, val):
        """Send a PDI byte with start and stop bits, LSB first."""
        self.builder.set_level(self.data, 0, 0)
        self.builder.set_level(self.clk, 1, self.half_period)
        self.builder.set_level(self.clk, 0, self.half_period)
        for i in range(8):
            bit = (val >> i) & 1
            self.builder.set_level(self.data, bit, 0)
            self.builder.set_level(self.clk, 1, self.half_period)
            self.builder.set_level(self.clk, 0, self.half_period)
        self.builder.set_level(self.data, 1, 0)
        self.builder.set_level(self.clk, 1, self.half_period)
        self.builder.set_level(self.clk, 0, self.half_period)

    def send_break(self):
        """Send BREAK instruction (0x0B)."""
        self.send_byte(0x0B)

class DeltaSigmaGenerator:
    """Delta-sigma modulator generator.
    2 channels: CLK(ch0), DATA(ch1)."""
    def __init__(self, builder, clk_ch=0, data_ch=1, samplerate=1000000):
        self.builder = builder
        self.clk = clk_ch
        self.data = data_ch
        self.half_period = int(samplerate / 200000)
        builder.set_idle(clk_ch, 0)
        builder.set_idle(data_ch, 0)

    def send_pattern(self, pattern):
        """Send a bit pattern. CLK toggles, DATA changes on falling edge."""
        for bit in pattern:
            self.builder.set_level(self.data, bit, 0)
            self.builder.set_level(self.clk, 1, self.half_period)
            self.builder.set_level(self.clk, 0, self.half_period)

class EM4100Generator:
    """EM4100 RFID protocol generator.
    1 channel: DATA(ch0). Manchester encoded, 64 bits."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        self.bit_period = int(samplerate * 64 / 1000000)
        self.half_bit = self.bit_period // 2
        builder.set_idle(channel, 0)

    def _manchester_bit(self, bit):
        if bit:
            self.builder.set_level(self.ch, 0, self.half_bit)
            self.builder.set_level(self.ch, 1, self.half_bit)
        else:
            self.builder.set_level(self.ch, 1, self.half_bit)
            self.builder.set_level(self.ch, 0, self.half_bit)

    def send_card(self, card_id):
        """Send an EM4100 card data (simplified)."""
        for _ in range(9):
            self._manchester_bit(1)
        data_bits = []
        for i in range(39, -1, -1):
            data_bits.append((card_id >> i) & 1)
        for row in range(10):
            row_bits = data_bits[row*4:(row+1)*4]
            for b in row_bits:
                self._manchester_bit(b)
            self._manchester_bit(sum(row_bits) % 2)
        for col in range(4):
            col_p = sum(data_bits[col + row*4] for row in range(10)) % 2
            self._manchester_bit(col_p)
        self._manchester_bit(0)

class OneSingleWireGenerator:
    """Single wire UART-like protocol generator.
    2 channels: DATA(ch0), VCC(ch1)."""
    def __init__(self, builder, data_ch=0, vcc_ch=1, baud=9600, samplerate=1000000):
        self.builder = builder
        self.data = data_ch
        self.vcc = vcc_ch
        self.bit_width = int(samplerate / baud)
        builder.set_idle(data_ch, 1)
        builder.set_idle(vcc_ch, 1)

    def write_byte(self, val):
        """Send a byte on single wire (8N1 UART)."""
        self.builder.set_level(self.vcc, 1, 0)
        self.builder.set_level(self.data, 0, self.bit_width)
        for i in range(8):
            bit = (val >> i) & 1
            self.builder.set_level(self.data, bit, self.bit_width)
        self.builder.set_level(self.data, 1, self.bit_width)

class OOKGenerator:
    """On-Off Keying generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, bitrate=1000, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.bit_width = int(samplerate / bitrate)
        builder.set_idle(channel, 0)

    def send_bits(self, bits):
        """Send a sequence of OOK bits."""
        for b in bits:
            self.builder.set_level(self.ch, b, self.bit_width)

class SonyMDGenerator:
    """Sony Minidisc protocol generator (Manchester encoded).
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, bitrate=230400, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.half_width = int(samplerate / (bitrate * 2))
        builder.set_idle(channel, 0)

    def send_byte_manchester(self, val):
        """Send a byte with Manchester encoding."""
        for i in range(8):
            bit = (val >> i) & 1
            if bit:
                self.builder.set_level(self.ch, 0, self.half_width)
                self.builder.set_level(self.ch, 1, self.half_width)
            else:
                self.builder.set_level(self.ch, 1, self.half_width)
                self.builder.set_level(self.ch, 0, self.half_width)

class TDMAudioGenerator:
    """TDM (Time Division Multiplexed) audio generator.
    3 channels: CLK(ch0), SYNC(ch1), DATA(ch2)."""
    def __init__(self, builder, clk_ch=0, sync_ch=1, data_ch=2, samplerate=1000000):
        self.builder = builder
        self.clk = clk_ch
        self.sync = sync_ch
        self.data = data_ch
        self.half_period = int(samplerate / 200000)
        builder.set_idle(clk_ch, 0)
        builder.set_idle(sync_ch, 0)
        builder.set_idle(data_ch, 0)

    def send_frame(self, num_channels=8, bits_per_channel=32):
        """Send one TDM frame with multiple channels."""
        hp = self.half_period
        self.builder.set_level(self.sync, 1, 0)
        for ch_idx in range(num_channels):
            for bit_idx in range(bits_per_channel):
                data_bit = 1 if (ch_idx == 0 and bit_idx == 31) else 0
                self.builder.set_level(self.data, data_bit, 0)
                self.builder.set_level(self.clk, 1, hp)
                self.builder.set_level(self.clk, 0, hp)
                if ch_idx == 0 and bit_idx == 0:
                    self.builder.set_level(self.sync, 0, 0)

class TLC5620Generator:
    """TLC5620 DAC protocol generator.
    2 channels: CLK(ch0), DATA(ch1)."""
    def __init__(self, builder, clk_ch=0, data_ch=1, samplerate=1000000):
        self.builder = builder
        self.clk = clk_ch
        self.data = data_ch
        self.half_period = int(samplerate / 200000)
        builder.set_idle(clk_ch, 0)
        builder.set_idle(data_ch, 0)

    def write(self, channel, rng, data_val):
        """Send a write command: 2-bit channel + 1-bit range + 8-bit data."""
        bits = []
        bits.append((channel >> 1) & 1)
        bits.append(channel & 1)
        bits.append(rng)
        for i in range(7, -1, -1):
            bits.append((data_val >> i) & 1)
        for b in bits:
            self.builder.set_level(self.data, b, 0)
            self.builder.set_level(self.clk, 1, self.half_period)
            self.builder.set_level(self.clk, 0, self.half_period)

class USBPowerDeliveryGenerator:
    """USB PD BMC (Biphase Mark Coding) generator.
    1 channel: CC(ch0).
    Protocol: preamble (alternating 0/1) + 4b5b SOP + header + data + CRC32 + EOP.
    Large idle gap (>3*UI) separates packets.

    BMC encoding:
    - Bit '0': one transition at start of bit period, signal stays for 2*UI (full period)
    - Bit '1': transition at start and at mid-bit, each half is UI long
    - Edge interval for '0' = 2*UI, for '1' = UI
    """
    def __init__(self, builder, channel=0, samplerate=10000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        self.ui = int(samplerate / 600000)  # UI = 1/600kHz
        builder.set_idle(channel, 0)

    def _bmc_bit(self, bit):
        """BMC encode: always transition at start, bit=1 has mid-bit transition.
        Bit '0': transition at start, hold for 2*UI (no mid-bit transition)
        Bit '1': transition at start, transition at mid-bit (each half = UI)"""
        current = self.builder.channels[self.ch][max(0, self.builder.pos - 1)] if self.builder.pos > 0 else 0
        new_level = 1 - current
        if bit:
            # '1': transition at start, transition at mid-bit
            self.builder.set_level(self.ch, new_level, self.ui)
            self.builder.set_level(self.ch, 1 - new_level, self.ui)
        else:
            # '0': transition at start, hold for 2*UI
            self.builder.set_level(self.ch, new_level, self.ui * 2)

    def _send_4b5b_symbol(self, data4):
        """Encode a 4-bit nibble using 4b5b and send it via BMC."""
        # 4b5b encode table: maps 4-bit value to 5-bit code
        enc4b5b = {
            0x0: 0b11110, 0x1: 0b01001, 0x2: 0b10100, 0x3: 0b10101,
            0x4: 0b01010, 0x5: 0b01011, 0x6: 0b01110, 0x7: 0b01111,
            0x8: 0b10010, 0x9: 0b10011, 0xA: 0b10110, 0xB: 0b10111,
            0xC: 0b11010, 0xD: 0b11011, 0xE: 0b11100, 0xF: 0b11101,
        }
        code5 = enc4b5b.get(data4 & 0xF, 0b11110)
        for i in range(4, -1, -1):
            self._bmc_bit((code5 >> i) & 1)

    def _send_special_symbol(self, sym):
        """Send a special 4b5b symbol via BMC."""
        # Special symbols
        SYNC1 = 0b11000  # K-code 0x11
        SYNC2 = 0b11001  # K-code 0x12
        SYNC3 = 0b11010  # K-code 0x13
        RST1  = 0b00111  # K-code 0x14
        RST2  = 0b00101  # K-code 0x15
        EOP   = 0b01101  # K-code 0x16
        symbols = {
            'SYNC-1': SYNC1, 'SYNC-2': SYNC2, 'SYNC-3': SYNC3,
            'RST-1': RST1, 'RST-2': RST2, 'EOP': EOP,
        }
        code5 = symbols[sym]
        for i in range(4, -1, -1):
            self._bmc_bit((code5 >> i) & 1)

    def _send_16bit(self, val):
        """Send a 16-bit value using 4b5b encoding (4 nibbles)."""
        for i in range(3, -1, -1):
            self._send_4b5b_symbol((val >> (i * 4)) & 0xF)

    def _send_32bit(self, val):
        """Send a 32-bit value using 4b5b encoding (8 nibbles)."""
        for i in range(7, -1, -1):
            self._send_4b5b_symbol((val >> (i * 4)) & 0xF)

    def _compute_crc32(self, data_bytes):
        """Compute CRC32 for USB PD."""
        crc = 0xFFFFFFFF
        for byte in data_bytes:
            crc ^= byte
            for _ in range(8):
                if crc & 1:
                    crc = (crc >> 1) ^ 0xEDB88320
                else:
                    crc >>= 1
        return crc ^ 0xFFFFFFFF

    def send_packet(self, header=0x1161, data_words=None):
        """Send a complete USB PD packet.
        header: 16-bit header (default: SRC, rev2, GoodCRC)
        data_words: list of 32-bit data words (None for control message)"""
        if data_words is None:
            data_words = []

        # Large idle gap before packet (>3*UI)
        self.builder.set_level(self.ch, 0, self.ui * 4)

        # Preamble: 64 bits alternating 0,1
        for i in range(64):
            self._bmc_bit(i % 2)

        # SOP sequence for "SOP": SYNC-1, SYNC-1, SYNC-1, SYNC-2
        self._send_special_symbol('SYNC-1')
        self._send_special_symbol('SYNC-1')
        self._send_special_symbol('SYNC-1')
        self._send_special_symbol('SYNC-2')

        # Header (16 bits, 4b5b encoded)
        self._send_16bit(header)

        # Data words (32 bits each, 4b5b encoded)
        for word in data_words:
            self._send_32bit(word)

        # CRC32 (over header + data, as bytes)
        crc_data = bytearray()
        crc_data.append(header & 0xFF)
        crc_data.append((header >> 8) & 0xFF)
        for word in data_words:
            crc_data.append(word & 0xFF)
            crc_data.append((word >> 8) & 0xFF)
            crc_data.append((word >> 16) & 0xFF)
            crc_data.append((word >> 24) & 0xFF)
        crc = self._compute_crc32(crc_data)
        self._send_32bit(crc)

        # EOP
        self._send_special_symbol('EOP')

        # Large idle gap after packet
        self.builder.set_level(self.ch, 0, self.ui * 4)

class MillerGenerator:
    """Miller encoding generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, bitrate=1000, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.bit_width = int(samplerate / bitrate)
        self.half_bit = self.bit_width // 2
        builder.set_idle(channel, 0)

    def send_bits(self, bits):
        """Miller encoding: 1=transition at mid-bit, 0=no transition."""
        current = 0
        for i, b in enumerate(bits):
            if b:
                self.builder.set_level(self.ch, current, self.half_bit)
                current = 1 - current
                self.builder.set_level(self.ch, current, self.half_bit)
            else:
                self.builder.set_level(self.ch, current, self.bit_width)
                if i > 0 and bits[i-1] == 1:
                    current = 1 - current

class IEBUSGenerator:
    """IEBus (Inter-Equipment Bus) Manchester encoded generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, bitrate=250, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.half_width = int(samplerate / (bitrate * 2))
        builder.set_idle(channel, 0)

    def _manchester_bit(self, bit):
        if bit:
            self.builder.set_level(self.ch, 0, self.half_width)
            self.builder.set_level(self.ch, 1, self.half_width)
        else:
            self.builder.set_level(self.ch, 1, self.half_width)
            self.builder.set_level(self.ch, 0, self.half_width)

    def send_frame(self, sync_bits, cmd, data):
        """Send a frame: sync + command byte + data byte."""
        for b in sync_bits:
            self._manchester_bit(b)
        for i in range(7, -1, -1):
            self._manchester_bit((cmd >> i) & 1)
        for i in range(7, -1, -1):
            self._manchester_bit((data >> i) & 1)

class IEEE488Generator:
    """Simplified IEEE-488 (GPIB) single-line generator.
    1 channel: DAV(ch0)."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.bit_width = int(samplerate / 1000)
        builder.set_idle(channel, 1)

    def send_byte_handshake(self, val):
        """Simulate DAV handshake for one byte."""
        self.builder.set_level(self.ch, 0, self.bit_width)
        self.builder.set_level(self.ch, 1, self.bit_width * 2)

class FSiGenerator:
    """FSI (Flexible Set Interface) generator.
    2 channels: CLK(ch0), DATA(ch1)."""
    def __init__(self, builder, clk_ch=0, data_ch=1, samplerate=1000000):
        self.builder = builder
        self.clk = clk_ch
        self.data = data_ch
        self.half_period = int(samplerate / 200000)
        builder.set_idle(clk_ch, 0)
        builder.set_idle(data_ch, 0)

    def send_frame(self, cmd, data_val):
        """Send a frame: start + command + data + CRC."""
        self.builder.set_level(self.data, 1, 0)
        self.builder.set_level(self.clk, 1, self.half_period)
        self.builder.set_level(self.clk, 0, self.half_period)
        for i in range(7, -1, -1):
            bit = (cmd >> i) & 1
            self.builder.set_level(self.data, bit, 0)
            self.builder.set_level(self.clk, 1, self.half_period)
            self.builder.set_level(self.clk, 0, self.half_period)
        for i in range(7, -1, -1):
            bit = (data_val >> i) & 1
            self.builder.set_level(self.data, bit, 0)
            self.builder.set_level(self.clk, 1, self.half_period)
            self.builder.set_level(self.clk, 0, self.half_period)
        crc = cmd ^ data_val
        for i in range(7, -1, -1):
            bit = (crc >> i) & 1
            self.builder.set_level(self.data, bit, 0)
            self.builder.set_level(self.clk, 1, self.half_period)
            self.builder.set_level(self.clk, 0, self.half_period)

class EthANGenerator:
    """Ethernet Auto-Negotiation Fast Link Pulse generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        builder.set_idle(channel, 0)

    def _us(self, us):
        return int(us * self.sr / 1000000)

    def send_flp_burst(self, data16):
        """Send one FLP burst."""
        for i in range(17):
            self.builder.set_level(self.ch, 1, self._us(0.1) if self.sr >= 10000000 else 1)
            if i < 16:
                bit = (data16 >> (15 - i)) & 1
                low_time = self._us(6.25) if bit else self._us(12.5)
                self.builder.set_level(self.ch, 0, low_time)
            else:
                self.builder.set_level(self.ch, 0, self._us(100))

    def send_negotiation(self, ability=0x0040):
        """Send 17 FLP bursts with technology ability."""
        for _ in range(17):
            self.send_flp_burst(ability)
            self.builder.set_level(self.ch, 0, self._us(16500))

class QiGenerator:
    """Qi wireless charging ASK generator.
    1 channel: DATA(ch0). Differential bi-phase encoding."""
    def __init__(self, builder, channel=0, bitrate=2000, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.bit_width = int(samplerate / bitrate)
        self.half_bit = self.bit_width // 2
        builder.set_idle(channel, 1)

    def _diff_biphase_bit(self, bit, last_level):
        new_level = 1 - last_level
        self.builder.set_level(self.ch, new_level, self.half_bit)
        if bit:
            mid_level = 1 - new_level
            self.builder.set_level(self.ch, mid_level, self.half_bit)
            return mid_level
        else:
            self.builder.set_level(self.ch, new_level, self.half_bit)
            return new_level

    def send_packet(self, header, data_bytes):
        """Send a Qi packet: preamble [1,1,1,1,0] + header byte (11-bit UART) + data bytes (11-bit UART each) + checksum byte (11-bit UART)."""
        last = 1
        # Preamble: [1,1,1,1,0] triggers DATA state in decoder
        for b in [1, 1, 1, 1, 0]:
            last = self._diff_biphase_bit(b, last)
        # Header byte: 11-bit UART (start=0 + 8 data LSB first + parity + stop=1)
        last = self._send_qi_byte(header, last)
        # Data bytes
        for byte in data_bytes:
            last = self._send_qi_byte(byte, last)
        # Checksum: XOR of header + all data bytes
        chk = header
        for byte in data_bytes:
            chk ^= byte
        last = self._send_qi_byte(chk, last)

    def _send_qi_byte(self, val, last):
        """Send one 11-bit Qi byte: start(0) + 8 data(LSB first) + odd parity + stop(1)."""
        p = 1
        bits = [0]  # start bit
        for i in range(8):
            bit = (val >> i) & 1
            bits.append(bit)
            p ^= bit
        bits.append(p)  # odd parity
        bits.append(1)  # stop bit
        for b in bits:
            last = self._diff_biphase_bit(b, last)
        return last

class PXX1Generator:
    """PXX1 RC protocol generator.
    1 channel: DATA(ch0). 8.4us per bit."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.bit_width = int(samplerate * 8.4 / 1000000)
        builder.set_idle(channel, 0)

    def send_bind_frame(self, data16):
        """Send a bind frame with 16 bits."""
        self.builder.set_level(self.ch, 1, self.bit_width * 4)
        self.builder.set_level(self.ch, 0, self.bit_width)
        for i in range(15, -1, -1):
            bit = (data16 >> i) & 1
            if bit:
                self.builder.set_level(self.ch, 1, self.bit_width * 2)
                self.builder.set_level(self.ch, 0, self.bit_width)
            else:
                self.builder.set_level(self.ch, 1, self.bit_width)
                self.builder.set_level(self.ch, 0, self.bit_width * 2)

class PJDLGenerator:
    """PJDL (PJON Data Link) generator. 8N1 UART-like.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, baud=9600, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.bit_width = int(samplerate / baud)
        builder.set_idle(channel, 1)

    def write_byte(self, val):
        """Send a byte 8N1."""
        self.builder.set_level(self.ch, 0, self.bit_width)
        for i in range(8):
            bit = (val >> i) & 1
            self.builder.set_level(self.ch, bit, self.bit_width)
        self.builder.set_level(self.ch, 1, self.bit_width)

class RCEncodeGenerator:
    """RC encode (pulse width) generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        builder.set_idle(channel, 0)

    def _us(self, us):
        return int(us * self.sr / 1000000)

    def send_pattern(self, bits):
        """Send RC pattern: long pulse = 1, short pulse = 0."""
        for b in bits:
            self.builder.set_level(self.ch, 1, self._us(500))
            if b:
                self.builder.set_level(self.ch, 0, self._us(1500))
            else:
                self.builder.set_level(self.ch, 0, self._us(500))
        self.builder.set_level(self.ch, 0, self._us(5000))

class RGBLEDWS281xGenerator:
    """WS2812B LED protocol generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        builder.set_idle(channel, 0)

    def _us(self, us):
        return int(us * self.sr / 1000000)

    def _send_bit(self, bit):
        if bit:
            self.builder.set_level(self.ch, 1, self._us(0.7))
            self.builder.set_level(self.ch, 0, self._us(0.6))
        else:
            self.builder.set_level(self.ch, 1, self._us(0.35))
            self.builder.set_level(self.ch, 0, self._us(0.8))

    def send_rgb(self, green, red, blue):
        """Send GRB data for one LED (24 bits)."""
        for i in range(7, -1, -1):
            self._send_bit((green >> i) & 1)
        for i in range(7, -1, -1):
            self._send_bit((red >> i) & 1)
        for i in range(7, -1, -1):
            self._send_bit((blue >> i) & 1)

    def send_reset(self):
        """Send reset signal (low >50us)."""
        self.builder.set_level(self.ch, 0, self._us(55))

class RinnaiControlPanelGenerator:
    """Rinnai control panel Manchester encoded generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, bitrate=1000, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.half_width = int(samplerate / (bitrate * 2))
        builder.set_idle(channel, 0)

    def _manchester_bit(self, bit):
        if bit:
            self.builder.set_level(self.ch, 0, self.half_width)
            self.builder.set_level(self.ch, 1, self.half_width)
        else:
            self.builder.set_level(self.ch, 1, self.half_width)
            self.builder.set_level(self.ch, 0, self.half_width)

    def send_command(self, cmd_byte):
        """Send a command byte with Manchester encoding."""
        for _ in range(4):
            self._manchester_bit(1)
        for i in range(7, -1, -1):
            self._manchester_bit((cmd_byte >> i) & 1)

class CarreraGenerator:
    """Carrera slot car protocol generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        builder.set_idle(channel, 0)

    def _us(self, us):
        return int(us * self.sr / 1000000)

    def send_id(self, car_id):
        """Send car ID pulses."""
        self.builder.set_level(self.ch, 1, self._us(500))
        self.builder.set_level(self.ch, 0, self._us(200))
        for i in range(3, -1, -1):
            bit = (car_id >> i) & 1
            if bit:
                self.builder.set_level(self.ch, 1, self._us(300))
            else:
                self.builder.set_level(self.ch, 1, self._us(150))
            self.builder.set_level(self.ch, 0, self._us(200))

class SDQGenerator:
    """SDQ (Smart Data Quality) 1-wire protocol generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        builder.set_idle(channel, 1)

    def _us(self, us):
        return int(us * self.sr / 1000000)

    def send_reset(self):
        """Send reset pulse."""
        self.builder.set_level(self.ch, 0, self._us(480))
        self.builder.set_level(self.ch, 1, self._us(60))

    def send_byte(self, val):
        """Send a byte (LSB first, 1-wire timing)."""
        for i in range(8):
            bit = (val >> i) & 1
            self.builder.set_level(self.ch, 0, self._us(5))
            if bit:
                self.builder.set_level(self.ch, 1, self._us(60))
            else:
                self.builder.set_level(self.ch, 1, self._us(10))

class SwimGenerator:
    """SWIM (Single Wire Interface Module) generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        builder.set_idle(channel, 1)

    def _us(self, us):
        return int(us * self.sr / 1000000)

    def send_command(self, cmd):
        """Send a SWIM command: start + 8 bits."""
        self.builder.set_level(self.ch, 0, self._us(2))
        self.builder.set_level(self.ch, 1, self._us(1))
        for i in range(7, -1, -1):
            bit = (cmd >> i) & 1
            if bit:
                self.builder.set_level(self.ch, 1, self._us(1))
                self.builder.set_level(self.ch, 0, self._us(0.5))
            else:
                self.builder.set_level(self.ch, 1, self._us(0.5))
                self.builder.set_level(self.ch, 0, self._us(1))

class SWIGenerator:
    """SWI (Single Wire Interface) NXP Manchester encoded generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, bitrate=9600, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.half_width = int(samplerate / (bitrate * 2))
        builder.set_idle(channel, 0)

    def _manchester_bit(self, bit):
        if bit:
            self.builder.set_level(self.ch, 0, self.half_width)
            self.builder.set_level(self.ch, 1, self.half_width)
        else:
            self.builder.set_level(self.ch, 1, self.half_width)
            self.builder.set_level(self.ch, 0, self.half_width)

    def write_byte(self, val):
        """Send a byte with Manchester encoding (MSB first)."""
        for i in range(7, -1, -1):
            self._manchester_bit((val >> i) & 1)

class MVBusGenerator:
    """MVBus Manchester encoded bus protocol generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, bitrate=9600, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.half_width = int(samplerate / (bitrate * 2))
        builder.set_idle(channel, 0)

    def _manchester_bit(self, bit):
        if bit:
            self.builder.set_level(self.ch, 0, self.half_width)
            self.builder.set_level(self.ch, 1, self.half_width)
        else:
            self.builder.set_level(self.ch, 1, self.half_width)
            self.builder.set_level(self.ch, 0, self.half_width)

    def send_frame(self, sync_bits, data_bytes):
        """Send a frame: sync + data."""
        for b in sync_bits:
            self._manchester_bit(b)
        for byte in data_bytes:
            for i in range(7, -1, -1):
                self._manchester_bit((byte >> i) & 1)

class MorseGenerator:
    """Morse code generator.
    1 channel: DATA(ch0)."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        self.unit = int(samplerate * 0.1)
        builder.set_idle(channel, 0)

    def _dit(self):
        self.builder.set_level(self.ch, 1, self.unit)
        self.builder.set_level(self.ch, 0, self.unit)

    def _dah(self):
        self.builder.set_level(self.ch, 1, self.unit * 3)
        self.builder.set_level(self.ch, 0, self.unit)

    def _letter_gap(self):
        self.builder.set_level(self.ch, 0, self.unit * 2)

    def _word_gap(self):
        self.builder.set_level(self.ch, 0, self.unit * 6)

    def send_sos(self):
        """Send SOS: ... --- ..."""
        self._dit(); self._dit(); self._dit()
        self._letter_gap()
        self._dah(); self._dah(); self._dah()
        self._letter_gap()
        self._dit(); self._dit(); self._dit()

class LFASTGenerator:
    """LFAST (Low Frequency Addressable Serial Transceiver) generator.
    1 channel: DATA(ch0).
    NRZ encoding: bit value = signal level (HIGH=1, LOW=0).
    Decoders detect edges and measure bit_len from intervals between edges.
    Sync pattern: 0xA84B transmitted MSB first.
    Protocol: sync + 8-bit header (3 payload_size + 4 channel_type + 1 CTS) +
    variable payload + sleep bit via timeout."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.sr = samplerate
        self.bit_width = max(2, samplerate // 100000)  # 100k bps default
        self.current_level = 0  # Start low
        builder.set_idle(channel, 0)

    def _send_bit(self, bit_val):
        """Send one bit using NRZ encoding: bit value = signal level.
        HIGH = 1, LOW = 0. Hold level for one bit_width."""
        level = bit_val
        self.builder.set_level(self.ch, level, self.bit_width)
        self.current_level = level

    def _send_bits_msb_first(self, value, num_bits):
        """Send bits MSB first using NRZ encoding."""
        for i in range(num_bits - 1, -1, -1):
            self._send_bit((value >> i) & 1)

    def send_frame(self, payload_size_id=0b001, channel_type=0b0100, cts=0,
                   payload_bytes=None, sleep_bit=0):
        """Send a complete LFAST frame.

        payload_size_id: 3-bit index into payload_byte_sizes [1,4,8,12,16,32,64,36]
        channel_type: 4-bit channel type (4-11 = data channels)
        cts: 1-bit clear-to-send
        payload_bytes: list of byte values (must match payload_size_id length)
        sleep_bit: 0=active, 1=sleep (implemented via timeout gap)
        """
        payload_byte_sizes = [1, 4, 8, 12, 16, 32, 64, 36]
        expected_len = payload_byte_sizes[payload_size_id]

        if payload_bytes is None:
            payload_bytes = [0x00] * expected_len
        # Pad or truncate to expected length
        payload_bytes = (payload_bytes + [0x00] * expected_len)[:expected_len]

        # Idle: hold low for a while so the first transition is clearly detected
        self.builder.set_level(self.ch, 0, self.bit_width * 4)
        self.current_level = 0

        # Sync pattern: 0xA84B = 1010100001001011, MSB first
        sync = 0xA84B
        self._send_bits_msb_first(sync, 16)

        # Header: 3 bits payload_size + 4 bits channel_type + 1 bit CTS, MSB first
        header = (payload_size_id << 5) | (channel_type << 1) | cts
        self._send_bits_msb_first(header, 8)

        # Payload: bytes MSB first
        for byte in payload_bytes:
            self._send_bits_msb_first(byte, 8)

        # Sleep bit: hold current level for 1.4*bit_width (no edge = timeout = sleep)
        # For sleep_bit=0 (active): make a transition (bit 0 = LOW) to indicate no sleep
        # For sleep_bit=1 (sleep): hold level for longer (timeout triggers sleep)
        if sleep_bit:
            # Hold level for 1.5 * bit_width (no transition = timeout = sleep)
            self.builder.set_level(self.ch, self.current_level, int(1.5 * self.bit_width))
        else:
            # Send a bit with opposite value to create an edge (no sleep)
            self._send_bit(0 if self.current_level == 1 else 1)

        # Idle gap after frame - hold current level, then return to low (idle)
        self.builder.set_level(self.ch, self.current_level, self.bit_width * 4)
        self.current_level = 0

class CCDGenerator:
    """CCD (Chrysler Collision Detection) Data Bus generator.
    1 channel: bus(ch0). UART at 7812.5 bps, idle high.
    Messages separated by idle periods > 10 bit widths of high."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.bit_width = max(1, int(samplerate / 7812.5))
        builder.set_idle(channel, 1)

    def _send_byte(self, val):
        """Send one UART byte: start(0) + 8 data(LSB first) + stop(1)."""
        # Start bit
        self.builder.set_level(self.ch, 0, self.bit_width)
        # Data bits LSB first
        for i in range(8):
            self.builder.set_level(self.ch, (val >> i) & 1, self.bit_width)
        # Stop bit
        self.builder.set_level(self.ch, 1, self.bit_width)

    def send_message(self, bytes_list):
        """Send a CCD message: idle gap + bytes + idle gap.
        Checksum (sum mod 256) is appended automatically."""
        # Idle gap (>10 bit widths of high to trigger IDLE->BUSY transition)
        self.builder.set_level(self.ch, 1, self.bit_width * 12)
        # Send bytes
        for b in bytes_list:
            self._send_byte(b)
        # Append checksum
        chk = sum(bytes_list) % 256
        self._send_byte(chk)
        # Idle gap after message (>10 bit widths for IDLE detection)
        self.builder.set_level(self.ch, 1, self.bit_width * 12)

class MapleBusGenerator:
    """Maple Bus (SEGA Dreamcast) generator.
    2 channels: SDCKA(ch0), SDCKB(ch1).
    Start: SDCKA low + SDCKB high, then 4 SDCKB falls before SDCKA rise = Start.
    Byte: 4 bit-pairs via SDCKA fall->read SDCKB, SDCKB fall->read SDCKA."""
    def __init__(self, builder, sdcka_ch=0, sdckb_ch=1, samplerate=1000000):
        self.builder = builder
        self.sdcka = sdcka_ch
        self.sdckb = sdckb_ch
        self.bit_width = max(2, samplerate // 2000000)  # ~2MHz bus
        builder.set_idle(sdcka_ch, 1)
        builder.set_idle(sdckb_ch, 1)

    def _set_both(self, a, b, duration):
        """Set both channels simultaneously."""
        self.builder.set_level(self.sdcka, a, duration)
        self.builder.pos -= duration
        self.builder.set_level(self.sdckb, b, duration)

    def _send_start(self):
        """Send start pattern: SDCKA=low, SDCKB=high, then 4 SDCKB falling edges
        before SDCKA rises."""
        # Both idle high initially
        # SDCKA goes low while SDCKB stays high
        self._set_both(0, 1, self.bit_width)
        # 4 SDCKB falling edges while SDCKA stays low
        for i in range(4):
            # SDCKB high, SDCKA low
            self._set_both(0, 1, self.bit_width)
            # SDCKB low, SDCKA low (SDCKB falling edge)
            self._set_both(0, 0, self.bit_width)
        # SDCKA rises (end of start pattern), SDCKB stays high
        self._set_both(1, 1, self.bit_width)

    def _send_byte(self, val):
        """Send one byte: 4 bit-pairs.
        Each bit-pair: SDCKA falls -> read SDCKB (bit_a), SDCKB falls -> read SDCKA (bit_b).
        Both lines must be high before each falling edge.
        Bits are collected MSB first (bit0 first, bit1 second per pair)."""
        for bitpair in range(4):
            # Get two bits for this pair
            bit_a = (val >> (7 - bitpair * 2)) & 1
            bit_b = (val >> (6 - bitpair * 2)) & 1

            # Ensure both lines are high before SDCKA falls
            self._set_both(1, 1, self.bit_width)
            # SDCKA falls -> decoder reads SDCKB = bit_a
            # If bit_a=0, drive SDCKB low BEFORE SDCKA falls so decoder reads 0
            # If bit_a=1, keep SDCKB high so decoder reads 1
            if bit_a == 0:
                # Drive SDCKB low first (while SDCKA is still high)
                self._set_both(1, 0, self.bit_width)
            # Now drive SDCKA low (falling edge on SDCKA)
            self._set_both(0, bit_a, self.bit_width)

            # Ensure SDCKB is high before it falls
            # If bit_a was 0, SDCKB is low, need to release it
            if bit_a == 0:
                self._set_both(0, 1, self.bit_width)
            # If bit_b=1, drive SDCKA high before SDCKB falls
            if bit_b == 1:
                self._set_both(1, 1, self.bit_width)
            # SDCKB falls -> decoder reads SDCKA = bit_b
            self._set_both(bit_b, 0, self.bit_width)
        # Both lines go high to complete byte
        self._set_both(1, 1, self.bit_width)

    def _send_end(self):
        """Send end pattern: SDCKA=1, SDCKB=0, then SDCKA falls while SDCKB=0."""
        # Both high briefly
        self._set_both(1, 1, self.bit_width)
        # SDCKA falls with SDCKB=0 (end condition)
        self._set_both(0, 0, self.bit_width)
        # SDCKA rises
        self._set_both(1, 1, self.bit_width)

    def send_frame(self, size_byte, src_ap, dst_ap, command, data_bytes=None):
        """Send a complete Maple Bus frame: start + size + src + dst + cmd + data + checksum + end."""
        if data_bytes is None:
            data_bytes = []
        all_bytes = [size_byte, src_ap, dst_ap, command] + list(data_bytes)
        # Calculate checksum: XOR of all bytes
        chk = 0
        for b in all_bytes:
            chk ^= b
        all_bytes.append(chk)

        self._send_start()
        for b in all_bytes:
            self._send_byte(b)
        self._send_end()

class RVSWDGenerator:
    """RVSWD (RISC-V Serial Wire Debug) generator.
    2 channels: CLK(ch0), DIO(ch1).
    START: CLK high + DIO falling.
    STOP: CLK high + DIO rising.
    Bits sampled on CLK rising, terminated on CLK falling.
    Short packet: 52 bits. Long packet: 84 bits."""
    def __init__(self, builder, clk_ch=0, dio_ch=1, samplerate=1000000):
        self.builder = builder
        self.clk = clk_ch
        self.dio = dio_ch
        self.half_period = max(2, samplerate // 200000)
        self._last_dio = 0
        builder.set_idle(clk_ch, 0)
        builder.set_idle(dio_ch, 0)

    def _start_condition(self):
        """START: CLK high, DIO falling edge.
        First set both high, then DIO falls while CLK stays high.
        Then bring CLK low so DIO can change safely for the first bit."""
        # Both idle low initially, bring both high first
        self.builder.set_level(self.clk, 1, self.half_period)
        self.builder.pos -= self.half_period
        self.builder.set_level(self.dio, 1, self.half_period)
        # Now DIO falls while CLK stays high (START condition detected by decoder)
        self.builder.set_level(self.clk, 1, self.half_period)
        self.builder.pos -= self.half_period
        self.builder.set_level(self.dio, 0, self.half_period)
        # Bring CLK low so DIO can change safely for the first bit
        self.builder.set_level(self.clk, 0, self.half_period)
        self.builder.pos -= self.half_period
        self.builder.set_level(self.dio, 0, self.half_period)

    def _stop_condition(self):
        """STOP: CLK high, DIO rising edge.
        CLK goes high, then DIO rises while CLK stays high."""
        # CLK is currently low. Bring CLK high first, DIO stays at current value
        self.builder.set_level(self.clk, 1, self.half_period)
        self.builder.pos -= self.half_period
        self.builder.set_level(self.dio, self._last_dio, self.half_period)
        # DIO rises while CLK stays high (STOP condition)
        self.builder.set_level(self.clk, 1, self.half_period)
        self.builder.pos -= self.half_period
        self.builder.set_level(self.dio, 1, self.half_period)

    def _send_bit(self, bit):
        """Send one bit: set DIO while CLK is low, CLK rises (sample), CLK falls (terminate).
        Must ensure DIO doesn't change while CLK is high (would trigger START/STOP)."""
        self._last_dio = bit
        # CLK is currently low. Set DIO while CLK is low (safe to change)
        period = self.half_period * 2
        self.builder.set_level(self.dio, bit, period)
        self.builder.pos -= period
        # CLK low for first half (already low, reinforce)
        self.builder.set_level(self.clk, 0, self.half_period)
        # CLK rises - decoder samples DIO here
        self.builder.set_level(self.clk, 1, self.half_period)
        # CLK falls - decoder terminates bit here
        self.builder.set_level(self.clk, 0, self.half_period)

    def send_short_packet(self, addr_host=0x01, operation=0, data_target=0x00000001):
        """Send a short (52-bit) RVSWD packet.
        Layout: 7 addr_host + 1 op + 1 parity_host + 5 skip + 32 data_target + 1 parity_target + 5 skip = 52 bits."""
        # Build bit list
        bits = []
        # 7-bit address host (MSB first)
        for i in range(6, -1, -1):
            bits.append((addr_host >> i) & 1)
        # 1-bit operation
        bits.append(operation & 1)
        # 1-bit parity (odd parity of addr_host + operation)
        parity_host = 0
        for b in bits:
            parity_host ^= b
        bits.append(parity_host)
        # 5 skip bits (don't care, use 0)
        bits.extend([0] * 5)
        # 32-bit data target (MSB first)
        for i in range(31, -1, -1):
            bits.append((data_target >> i) & 1)
        # 1-bit parity target (odd parity of data_target)
        parity_target = 0
        for i in range(31, -1, -1):
            parity_target ^= (data_target >> i) & 1
        bits.append(parity_target)
        # 5 skip bits
        bits.extend([0] * 5)

        assert len(bits) == 52, f"Short packet must be 52 bits, got {len(bits)}"

        self._start_condition()
        for b in bits:
            self._send_bit(b)
        self._stop_condition()

class SDIOGenerator:
    """SDIO (Secure Digital I/O) generator.
    2 channels: CMD(ch0), CLK(ch1).
    Command token: 48 bits (start=0 + transmission=1 + 6-bit cmd + 32-bit arg + 7-bit CRC + end=1).
    CMD sampled on CLK rising edge (default polarity)."""
    def __init__(self, builder, cmd_ch=0, clk_ch=1, samplerate=1000000):
        self.builder = builder
        self.cmd = cmd_ch
        self.clk = clk_ch
        self.half_period = max(2, samplerate // 400000)
        builder.set_idle(clk_ch, 0)
        builder.set_idle(cmd_ch, 1)

    def _crc7(self, bits_list):
        """Compute CRC7 for SD command."""
        data = 0
        for b in bits_list:
            di = b ^ ((data >> 6) & 1)
            data = (data & 0x07) | ((data & 0x38) << 1) | ((di ^ ((data >> 2) & 1)) << 3)
            data = (data & 0x78) | ((data & 0x03) << 1) | di
        return data & 0x7F

    def _send_bit(self, bit):
        """Send one bit on CMD with CLK rising edge sampling."""
        period = self.half_period * 2
        # Set CLK low for full period, overlay CMD
        self.builder.set_level(self.clk, 0, period)
        self.builder.pos -= period
        self.builder.set_level(self.cmd, bit, period)
        self.builder.pos -= period
        # CLK high then low
        self.builder.set_level(self.clk, 1, self.half_period)
        self.builder.set_level(self.clk, 0, self.half_period)

    def send_command(self, cmd, arg=0x00000000):
        """Send an SDIO command token (48 bits).
        Format: start(0) + transmission(1) + 6-bit cmd + 32-bit arg + 7-bit CRC + end(1)."""
        bits = []
        # Start bit = 0
        bits.append(0)
        # Transmission bit = 1 (host)
        bits.append(1)
        # 6-bit command (MSB first)
        for i in range(5, -1, -1):
            bits.append((cmd >> i) & 1)
        # 32-bit argument (MSB first)
        for i in range(31, -1, -1):
            bits.append((arg >> i) & 1)
        # 7-bit CRC (over start+transmission+cmd+arg = 40 bits)
        crc = self._crc7(bits[:40])
        for i in range(6, -1, -1):
            bits.append((crc >> i) & 1)
        # End bit = 1
        bits.append(1)

        assert len(bits) == 48, f"Command token must be 48 bits, got {len(bits)}"

        # CMD idle high before command
        self.builder.set_level(self.cmd, 1, self.half_period * 4)
        for b in bits:
            self._send_bit(b)
        # CMD idle high after command
        self.builder.set_level(self.cmd, 1, self.half_period * 4)

class DSIGenerator:
    """DSI (Display Serial Interface) generator.
    1 channel: DATA(ch0). SPI-like."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.bit_width = int(samplerate / 100000)
        builder.set_idle(channel, 0)

    def send_packet(self, data_type, data_val):
        """Send a short DSI command packet."""
        for i in range(7, -1, -1):
            bit = (data_type >> i) & 1
            self.builder.set_level(self.ch, bit, self.bit_width)
        for i in range(7, -1, -1):
            bit = (data_val >> i) & 1
            self.builder.set_level(self.ch, bit, self.bit_width)

class EM4305Generator:
    """EM4305 RFID Manchester encoded generator.
    1 channel: DATA(ch0). 64 bits like EM4100."""
    def __init__(self, builder, channel=0, samplerate=1000000):
        self.builder = builder
        self.ch = channel
        self.bit_period = int(samplerate * 64 / 1000000)
        self.half_bit = self.bit_period // 2
        builder.set_idle(channel, 0)

    def _manchester_bit(self, bit):
        if bit:
            self.builder.set_level(self.ch, 0, self.half_bit)
            self.builder.set_level(self.ch, 1, self.half_bit)
        else:
            self.builder.set_level(self.ch, 1, self.half_bit)
            self.builder.set_level(self.ch, 0, self.half_bit)

    def send_card_data(self, card_id):
        """Send EM4305 card data (simplified 64-bit pattern)."""
        for _ in range(9):
            self._manchester_bit(1)
        data_bits = []
        for i in range(39, -1, -1):
            data_bits.append((card_id >> i) & 1)
        for row in range(10):
            row_bits = data_bits[row*4:(row+1)*4]
            for b in row_bits:
                self._manchester_bit(b)
            self._manchester_bit(sum(row_bits) % 2)
        for col in range(4):
            col_p = sum(data_bits[col + row*4] for row in range(10)) % 2
            self._manchester_bit(col_p)
        self._manchester_bit(0)

class TL5620Generator:
    """TL5620 DAC protocol generator (alias for TLC5620).
    2 channels: CLK(ch0), DATA(ch1)."""
    def __init__(self, builder, clk_ch=0, data_ch=1, samplerate=1000000):
        self._impl = TLC5620Generator(builder, clk_ch, data_ch, samplerate)

    def write(self, channel, rng, data_val):
        self._impl.write(channel, rng, data_val)

if __name__ == "__main__":
    # Quick test
    bb = BitstreamBuilder(2, 1000)
    uart = UARTGenerator(bb, 0)
    uart.write_byte(0x55)
    print(f"Generated {bb.pos} samples of UART")
