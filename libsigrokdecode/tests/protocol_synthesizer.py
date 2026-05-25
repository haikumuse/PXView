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
        
        for b in bits:
            # Data must be stable before clock falls
            self.builder.set_level(self.data, b, 0)
            self.builder.set_level(self.clk, 1, self.half_period)
            self.builder.set_level(self.clk, 0, self.half_period)
        
        self.builder.set_level(self.clk, 1, self.half_period * 2)

if __name__ == "__main__":
    # Quick test
    bb = BitstreamBuilder(2, 1000)
    uart = UARTGenerator(bb, 0)
    uart.write_byte(0x55)
    print(f"Generated {bb.pos} samples of UART")
