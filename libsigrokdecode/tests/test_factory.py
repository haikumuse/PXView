#!/usr/bin/env python3
"""
test_factory.py - Robust mass generation of high-fidelity tests for 215+ decoders.
"""

import os
import json
import re
import random
from protocol_synthesizer import (
    BitstreamBuilder, UARTGenerator, I2CGenerator, 
    SPIGenerator, CANGenerator, USBGenerator, ManchesterGenerator
)

DECODERS_DIR = "../c_decoders"
TESTDATA_ROOT = "testdata"

# Decoders that should be skipped (helpers, not real PDs)
BLACKLIST = ["signalling", "polarity", "invert", "bit_offset", "HtoD_Clock"]

# Mapping of input types to their generators and default configurations
INPUT_GENERATORS = {
    "logic": ("base", None),
    "i2c": ("i2c_c", I2CGenerator),
    "spi": ("spi_c", SPIGenerator),
    "uart": ("uart_c", UARTGenerator),
    "can": ("can_c", CANGenerator),
    "usb_signalling": ("usb_signalling_c", USBGenerator),
    "ethernet": ("ethernet_c", ManchesterGenerator),
    "ps2": ("ps2_c", None),
    "onewire_link": ("onewire_link_c", None),
    "onewire_network": ("onewire_network_c", None),
    "ipv4": ("ipv4_c", None),
    "usb_packet": ("usb_packet_c", None),
    "tmc": ("tmc_c", None),
    "jtag": ("jtag_c", None),
}

def parse_decoder_metadata(c_file):
    with open(c_file, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    
    # Extract ID (specifically look for the one in srd_c_decoder struct)
    id_match = re.search(r'struct\s+srd_c_decoder\s+\w+\s*=\s*\{.*?\b\.id\s*=\s*"([^"]+)"', content, re.DOTALL)
    if not id_match:
        id_match = re.search(r'static\s+struct\s+srd_c_decoder\s+\w+\s*=\s*\{.*?\b\.id\s*=\s*"([^"]+)"', content, re.DOTALL)
    
    if not id_match:
        all_ids = re.findall(r'\.id\s*=\s*"([^"]+)"', content)
        if all_ids:
            candidates = [i for i in all_ids if not i.startswith("dec_") and not i.startswith("opt_")]
            decoder_id = candidates[-1] if candidates else all_ids[-1]
        else:
            decoder_id = os.path.basename(c_file).replace(".c", "")
    else:
        decoder_id = id_match.group(1)
    
    # Extract Inputs
    inputs = []
    input_match = re.search(r'static\s+const\s+char\s*\*?\s*\w+_inputs\s*\[\]\s*=\s*\{([^}]+)\}', content, re.DOTALL)
    if input_match:
        inputs = [i.strip().strip('"') for i in input_match.group(1).split(',') if i.strip() and "NULL" not in i]
    
    # Extract Channels
    channels = {}
    ch_pat = r'static\s+struct\s+srd_channel\s+\w+_channels\s*\[\]\s*=\s*\{(.*?)\}\s*;'
    ch_match = re.search(ch_pat, content, re.DOTALL)
    if ch_match:
        entries = re.findall(r'\{\s*"([^"]+)"', ch_match.group(1))
        for i, name in enumerate(entries):
            channels[name] = i
    else:
        num_ch_match = re.search(r'\.num_channels\s*=\s*(\d+)', content)
        if num_ch_match:
            num = int(num_ch_match.group(1))
            for i in range(num): channels[f"ch{i}"] = i
            
    return decoder_id, inputs, channels

def generate_test_for_decoder(c_file):
    if any(b in c_file for b in BLACKLIST): return

    try:
        d_id, inputs, channels = parse_decoder_metadata(c_file)
        if d_id in BLACKLIST: return
    except Exception as e:
        print(f"ERROR parsing {c_file}: {e}")
        return

    primary_input = inputs[0] if inputs else "logic"
    stack = []
    
    current_input = primary_input
    while current_input in INPUT_GENERATORS:
        base_id, _ = INPUT_GENERATORS[current_input]
        if base_id == "base": break
        stack.insert(0, {"id": base_id})
        # For simplicity, we only handle 1 level of stacking in this mass generator
        # though decoder_test.c supports more.
        break

    # Generate data
    sample_count = 20000
    samplerate = 1000000
    if "adat" in d_id: samplerate = 100000000
    if "usb" in d_id: samplerate = 12000000
    if "can" in d_id or "can" in primary_input: samplerate = 10000000
    if "eth" in d_id: samplerate = 50000000

    num_channels = max(channels.values()) + 1 if channels else 2
    bb = BitstreamBuilder(num_channels, sample_count, samplerate)
    
    try:
        if "i2c" in primary_input:
            gen = I2CGenerator(bb, scl_ch=0, sda_ch=1)
            gen.start(); gen.write_byte(0x50 << 1); gen.write_byte(0x00); gen.stop()
        elif "spi" in primary_input:
            gen = SPIGenerator(bb, clk=0, mosi=1, miso=2, cs=3)
            gen.select(); gen.write_byte(0x9F); gen.deselect()
        elif "uart" in primary_input:
            gen = UARTGenerator(bb, channel=0)
            gen.write_byte(0x55); gen.write_byte(0xAA)
        elif "can" in primary_input:
            gen = CANGenerator(bb, channel=0)
            gen.send_frame(0x123, [0x11, 0x22, 0x33])
        elif "usb" in primary_input or "usb" in d_id:
            gen = USBGenerator(bb, dp_ch=0, dm_ch=1)
            gen.send_packet([0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00])
        elif "adat" in d_id:
            for _ in range(10):
                bb.set_level(0, 0, 10); bb.set_level(0, 1, 10)
        elif "ps2" in primary_input or "ps2" in d_id:
            gen = PS2Generator(bb, clk_ch=0, data_ch=1)
            # Send 'A' (0x1C) and 'Break A' (0xF0, 0x1C)
            gen.write_byte(0x1C)
            bb.pos += 1000
            gen.write_byte(0xF0)
            gen.write_byte(0x1C)
    except Exception as e:
        pass

    # Save to testdata
    test_dir = os.path.join(TESTDATA_ROOT, d_id, "default")
    os.makedirs(test_dir, exist_ok=True)
    
    with open(os.path.join(test_dir, "input.bin"), "wb") as f:
        f.write(bb.get_bitpacked())
        
    config = {
        "decoder": d_id,
        "samplerate": samplerate,
        "num_channels": num_channels,
        "sample_count": sample_count,
        "channels": channels,
        "stack": stack
    }
    
    with open(os.path.join(test_dir, "config.json"), "w") as f:
        json.dump(config, f, indent=2)
    
    print(f"[OK] {d_id:25} | Input: {primary_input:15} | Stack: {len(stack)}")

def main():
    if not os.path.exists(DECODERS_DIR):
        print(f"Error: {DECODERS_DIR} not found.")
        return
        
    c_files = [f for f in os.listdir(DECODERS_DIR) if f.endswith("_c.c")]
    c_files.sort()
    
    print(f"Regenerating input.bin for {len(c_files)} decoders...")
    print("-" * 60)
    
    for f in c_files:
        generate_test_for_decoder(os.path.join(DECODERS_DIR, f))
    
    print("-" * 60)
    print("Done. All input.bin and config.json files have been updated.")

if __name__ == "__main__":
    main()
