import math
from .base import *

class IRRC6Generator:
    """RC6 IR protocol generator (baseband Manchester).
    Default polarity is active-low (idle HIGH, burst LOW) to match decoder defaults."""
    def __init__(self, builder, channel, samplerate=1000000):
        self.builder = builder
        self.channel = channel
        self.sr = samplerate
        self.half_period = int(222 * samplerate / 1e6)  # 222us half-bit (444us bit period)
        self.builder.set_idle(channel, 1)  # Idle HIGH (active-low)

    def _us(self, us):
        return int(us * self.sr / 1e6)

    def _write_manchester(self, bit):
        # RC6 Manchester (active-low): 1 = HIGH→LOW, 0 = LOW→HIGH
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
        # Leader: 2.667ms HIGH + 889us LOW (active-low: leader starts with idle HIGH)
        self.builder.set_level(self.channel, 1, self._us(2667))
        self.builder.set_level(self.channel, 0, self._us(889))
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

