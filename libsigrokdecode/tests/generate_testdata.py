#!/usr/bin/env python3
"""Automated test data generation for all C decoders.

Parses C decoder source files to extract metadata (channels, options, inputs)
and generates config.json + input.bin test data files.

Usage:
    python generate_testdata.py [--c-decoders-dir <path>] [--output-dir <path>] [--skip-existing]
"""

import argparse
import json
import math
import os
import random
import re
import sys

# Import protocol synthesizer for real data generation
try:
    import protocol_synthesizer as ps
except ImportError:
    ps = None

def synthesize_input_bin(decoder_id, num_channels, sample_count, channels_map):
    """Synthesize real protocol data using protocol_synthesizer.
    Returns (bytes data, int samplerate, int sample_count) or None.
    """
    if ps is None:
        return None

    # Defaults
    synth_sr = 1000000 # 1MHz
    sample_count = 10000 # 10ms - keep it small to avoid timeouts

    if decoder_id in ["ac97_c", "qspi_c", "spi_fast_c", "usb_signalling_c", "usb_packet_c"]:
        synth_sr = 24000000 # 24MHz for high speed
        sample_count = 50000
    elif decoder_id == "usb_power_delivery_c":
        synth_sr = 12000000 # 12MHz for USB PD BMC (600kHz datarate)
        sample_count = 20000
    elif decoder_id.startswith("ir_") or decoder_id in ["dcf77_c", "morse_c"]:
        synth_sr = 100000 # 100kHz for very slow
        sample_count = 100000 # 1 second
    elif decoder_id == "qi_c":
        synth_sr = 1000000 # 1MHz
        sample_count = 25000 # 25ms - enough for full Qi packet at 2kHz

    builder = ps.BitstreamBuilder(num_channels, sample_count, synth_sr)
    
    # Start at a small offset to ensure idle state is detected
    builder.pos = 2000

    try:
        if decoder_id == "uart_c":
            ch = channels_map.get("rx", 0)
            gen = ps.UARTGenerator(builder, ch)
            gen.write_byte(0x55)
            gen.write_byte(0xAA)
            gen.write_byte(0x12)
        elif decoder_id == "i2c_c":
            scl = channels_map.get("scl", 0)
            sda = channels_map.get("sda", 1)
            gen = ps.I2CGenerator(builder, scl, sda)
            gen.start()
            gen.write_byte(0x50 << 1) # Addr
            gen.write_byte(0x00)      # Offset
            gen.write_byte(0xBE)      # Data
            gen.stop()
        elif decoder_id == "spi_c":
            clk = channels_map.get("clk", 0)
            mosi = channels_map.get("mosi", 1)
            miso = channels_map.get("miso", 2)
            cs = channels_map.get("cs", 3)
            gen = ps.SPIGenerator(builder, clk, mosi, miso, cs)
            gen.select()
            gen.write_byte(0xDE)
            gen.write_byte(0xAD)
            gen.deselect()
        elif decoder_id == "ac97_c":
            sync = channels_map.get("sync", 0)
            clk = channels_map.get("clk", 1)
            out = channels_map.get("out", 2)
            s_in = channels_map.get("in", 3)
            gen = ps.AC97Generator(builder, sync, clk, out, s_in)
            gen.send_frame(0x9800, [0x1234] * 12)
        elif decoder_id == "gpib_c":
            gen = ps.GPIBGenerator(builder)
            gen.command_sequence()
        elif decoder_id == "lpc_c":
            gen = ps.LPCGenerator(builder)
            gen.io_write(0x80, 0x55)
        elif decoder_id == "z80_c":
            gen = ps.Z80Generator(builder)
            gen.m1_cycle(0x1234, 0x3E) # LD A, n
            gen.mem_read_cycle(0x1235, 0x55)
        elif decoder_id == "tm1637_c":
            clk = channels_map.get("clk", 0)
            dio = channels_map.get("dio", 1)
            gen = ps.TM1637Generator(builder, clk, dio)
            gen.start()
            gen.write_byte(0x40) # Data command
            gen.stop()
        elif decoder_id == "qspi_c":
            clk = channels_map.get("clk", 0)
            cs = channels_map.get("cs", 1)
            d0 = channels_map.get("io0", 2)
            d1 = channels_map.get("io1", 3)
            d2 = channels_map.get("io2", 4)
            d3 = channels_map.get("io3", 5)
            gen = ps.QSPIGenerator(builder, clk, cs, d0, d1, d2, d3)
            gen.select()
            gen.write_byte_quad(0xAB)
            gen.deselect()
        elif decoder_id == "can_c":
            ch = channels_map.get("can_rx", 0)
            gen = ps.CANGenerator(builder, ch)
            gen.send_frame(0x123, [0x11, 0x22, 0x33])
        elif decoder_id == "usb_power_delivery_c":
            ch = channels_map.get("cc1", 0)
            gen = ps.USBPowerDeliveryGenerator(builder, ch)
            gen.send_packet()
        elif decoder_id == "am230x_c":
            ch = channels_map.get("sda", 0)
            gen = ps.AM230xGenerator(builder, ch)
            gen.send_reading(50.5, 25.1)
        elif decoder_id == "avr_pdi_c":
            clk = channels_map.get("reset", 0)
            data = channels_map.get("data", 1)
            gen = ps.AVRPDIGenerator(builder, clk, data)
            gen.send_break()
        elif decoder_id == "bean_c":
            ch = channels_map.get("data", 0)
            gen = ps.BEANGenerator(builder, ch)
            gen.send_frame()
        elif decoder_id == "c2_c":
            clk = channels_map.get("c2ck", 0)
            data = channels_map.get("c2d", 1)
            gen = ps.C2Generator(builder, clk, data)
            gen.send_reset()
        elif decoder_id == "cec_c":
            ch = channels_map.get("cec", 0)
            gen = ps.CECGenerator(builder, ch)
            gen.send_frame()
        elif decoder_id == "dcf77_c":
            ch = channels_map.get("data", 0)
            gen = ps.DCF77Generator(builder, ch)
            gen.send_bit(1); gen.send_bit(0)
        elif decoder_id == "dmx512_c":
            ch = channels_map.get("dmx", 0)
            gen = ps.DMX512Generator(builder, ch)
            gen.send_frame()
        elif decoder_id == "dsi_c":
            ch = channels_map.get("dsi", 0)
            gen = ps.DSIGenerator(builder, ch)
            gen.send_backward_frame()
        elif decoder_id == "fsi_c":
            clk = channels_map.get("clock", 1)
            data = channels_map.get("data", 0)
            gen = ps.FSiGenerator(builder, clk, data)
            gen.send_frame(0xAA, 0x55)
        elif decoder_id == "ieee488_c":
            gen = ps.GPIBGenerator(builder) # GPIB covers ieee488
            gen.command_sequence()
        elif decoder_id == "mcs48_c":
            gen = ps.MCS48Generator(builder)
            gen.opcode_fetch(0x123, 0x55)
        elif decoder_id == "mvb_c":
            ch = channels_map.get("mvb", 0)
            gen = ps.MVBGenerator(builder, ch)
            gen.send_master_frame()
        elif decoder_id == "ps2_c":
            clk = channels_map.get("clk", 0)
            data = channels_map.get("data", 1)
            gen = ps.PS2Generator(builder, clk, data)
            gen.write_byte(0x55)
        elif decoder_id == "qi_c":
            ch = channels_map.get("qi", 0)
            gen = ps.QiGenerator(builder, ch, bitrate=2000, samplerate=synth_sr)
            gen.send_packet(0x02, [0x04])
        elif decoder_id == "pwm_c":
            ch = channels_map.get("data", 0)
            gen = ps.PWMGenerator(builder, ch)
            gen.send_cycles(10)
        elif decoder_id in ["onewire_c", "onewire_link_c", "onewire_network_c", "ds243x_c", "ds2408_c", "ds28ea00_c"]:
            ch = channels_map.get("owr", 0)
            gen = ps.OneWireGenerator(builder, ch)
            gen.read_rom()
        elif decoder_id in ["ethernet_c", "arp_c", "ipv4_c", "udp_c"]:
            ch = channels_map.get("data", 0)
            if ch is None: ch = 0
            gen = ps.UARTGenerator(builder, ch) # Fallback to UART for framing
            gen.write_byte(0x55)
        elif decoder_id in ["st7735_c", "st7789_c", "spiflash_c", "ssd1306_c", "spi_dual_quad_c"]:
            clk = channels_map.get("clk", 0)
            mosi = channels_map.get("mosi", 1)
            cs = channels_map.get("cs", 3)
            gen = ps.SPIGenerator(builder, clk, mosi, -1, cs)
            gen.select()
            gen.write_byte(0x0)
            gen.deselect()
        elif decoder_id in ["tm1638_c", "tmc_c"]:
            clk = channels_map.get("clk", 0)
            dio = channels_map.get("dio", 1)
            gen = ps.TM1637Generator(builder, clk, dio)
            gen.start()
            gen.write_byte(0x40)
            gen.stop()
        elif decoder_id == "one_single_wire_c":
            data = channels_map.get("osw", 0)
            start = channels_map.get("strt", 1)
            gen = ps.OneSingleWireGenerator(builder, data, start)
            gen.send_byte(0x55)
        elif decoder_id == "caliper_c":
            clk = channels_map.get("clk", 0)
            data = channels_map.get("data", 1)
            gen = ps.CaliperGenerator(builder, clk, data)
            gen.send_value(0x123456, 24)
        elif decoder_id == "carrera_c":
            ch = channels_map.get("data", 0)
            gen = ps.CarreraGenerator(builder, ch)
            gen.send_controller_word()
        elif decoder_id == "dali_c":
            ch = channels_map.get("dali", 0)
            gen = ps.DALIGenerator(builder, ch)
            gen.send_forward()
        elif decoder_id == "dcc_c":
            ch = channels_map.get("data", 0)
            gen = ps.DCCGenerator(builder, ch)
            gen.send_packet(0x03, 0x7F)
        elif decoder_id == "em4100_c":
            ch = channels_map.get("data", 0)
            gen = ps.EM4100Generator(builder, ch)
            gen.send_card(0x123456789A)
        elif decoder_id == "em4305_c":
            ch = channels_map.get("data", 0)
            gen = ps.EM4305Generator(builder, ch)
            gen.send_write_word()
        elif decoder_id == "eth_an_c":
            ch = channels_map.get("dp", 0)
            gen = ps.EthANGenerator(builder, ch)
            gen.send_negotiation()
        elif decoder_id == "hdlc_c":
            ch = channels_map.get("data", 1)
            gen = ps.HDLCGenerator(builder, ch)
            gen.send_frame(0x01, 0x03, b"HELLO")
        elif decoder_id == "i2s_c":
            sck = channels_map.get("sck", 0)
            ws = channels_map.get("ws", 1)
            sd = channels_map.get("sd", 2)
            gen = ps.I2SGenerator(builder, sck, ws, sd)
            gen.send_frame(0x1234, 0x5678)
        elif decoder_id == "iebus_c":
            ch = channels_map.get("bus", 0)
            gen = ps.IEBUSGenerator(builder, ch)
            gen.send_frame([1,0,1,0], 0x11, 0x22)
        elif decoder_id == "ir_nec_c":
            ch = channels_map.get("ir", 0)
            gen = ps.IRNECGenerator(builder, ch)
            gen.send_nec(0x04, 0x08)
        elif decoder_id == "ir_rc5_c":
            ch = channels_map.get("ir", 0)
            gen = ps.IRRC5Generator(builder, ch)
            gen.send_rc5(0x00, 0x01)
        elif decoder_id == "ir_rc6_c":
            ch = channels_map.get("ir", 0)
            gen = ps.IRRC6Generator(builder, ch)
            gen.send_rc6(0x00, 0x01)
        elif decoder_id == "ir_sirc_c":
            ch = channels_map.get("ir", 0)
            gen = ps.IRSIRCGenerator(builder, ch)
            gen.send_sirc(0x15, 0x01)
        elif decoder_id == "iso7816_c":
            clk = channels_map.get("clk", 0)
            data = channels_map.get("data", 1)
            gen = ps.ISO7816Generator(builder, clk, data)
            gen.send_atr()
        elif decoder_id == "jtag_c":
            tdi = channels_map.get("tdi", 0)
            tdo = channels_map.get("tdo", 1)
            tck = channels_map.get("tck", 2)
            tms = channels_map.get("tms", 3)
            gen = ps.JTAGGenerator(builder, tdi, tdo, tck, tms)
            gen.reset_tap()
            gen.shift_dr(0x5, 4)
        elif decoder_id == "mdio_c":
            mdc = channels_map.get("mdc", 0)
            mdio = channels_map.get("mdio", 1)
            gen = ps.MDIOGenerator(builder, mdc, mdio)
            gen.read_clause22(0x01, 0x00, 0x1234)
        elif decoder_id == "mipi_rffe_c":
            sclk = channels_map.get("sclk", 0)
            sdata = channels_map.get("sdata", 1)
            gen = ps.MIPIRFFEGenerator(builder, sclk, sdata)
            gen.send_ext_write(0x11, 0x0, 0x34)
        elif decoder_id == "microwire_c":
            sk = channels_map.get("sk", 1)
            si = channels_map.get("si", 2)
            so = channels_map.get("so", 3)
            cs = channels_map.get("cs", 0)
            gen = ps.MicrowireGenerator(builder, sk, si, so, cs)
            gen.read(0b10, 0x00, 0xABCD)
        elif decoder_id == "morse_c":
            ch = channels_map.get("data", 0)
            gen = ps.MorseGenerator(builder, ch)
            gen.send_sos()
        elif decoder_id == "nrzi_c":
            ch = channels_map.get("data", 0)
            gen = ps.NRZIGenerator(builder, ch)
            gen.send_bytes(b"HELLO")
        elif decoder_id == "ook_c":
            ch = channels_map.get("data", 0)
            gen = ps.OOKGenerator(builder, ch)
            gen.send_bits([1,0,1,1,0,1,0,1])
        elif decoder_id == "opentherm_c":
            ch = channels_map.get("ot", 0)
            gen = ps.OpenthermGenerator(builder, ch)
            gen.send_read_request()
        elif decoder_id == "pjdl_c":
            ch = channels_map.get("data", 0)
            gen = ps.PJDLGenerator(builder, ch)
            gen.write_byte(0x55)
        elif decoder_id == "pxx1_c":
            ch = channels_map.get("data", 0)
            gen = ps.PXX1Generator(builder, ch)
            gen.send_bind_frame(0x1234)
        elif decoder_id == "rc_encode_c":
            ch = channels_map.get("data", 0)
            gen = ps.RCEncodeGenerator(builder, ch)
            gen.send_pattern([1,0,1,1])
        elif decoder_id == "sent_c":
            ch = channels_map.get("data", 0)
            gen = ps.SENTGenerator(builder, ch)
            gen.send_message()
        elif decoder_id == "sdio_c":
            clk = channels_map.get("clk", 1)
            cmd = channels_map.get("cmd", 0)
            gen = ps.SDIOGenerator(builder, cmd, clk)
            gen.send_command(52, 0x0)
        elif decoder_id == "swd_c":
            clk = channels_map.get("swclk", 0)
            data = channels_map.get("swdio", 1)
            gen = ps.SWDGenerator(builder, clk, data)
            gen.line_reset()
            gen.read_dp()
        elif decoder_id == "swi_c":
            ch = channels_map.get("swi", 0)
            gen = ps.SWIGenerator(builder, ch)
            gen.write_byte(0x55)
        elif decoder_id == "swim_c":
            ch = channels_map.get("swim", 0)
            gen = ps.SwimGenerator(builder, ch)
            gen.send_command(0x00)
        elif decoder_id == "tdm_audio_c":
            sck = channels_map.get("clock", 0)
            ws = channels_map.get("frame", 1)
            sd = channels_map.get("data", 2)
            gen = ps.TDMAudioGenerator(builder, sck, ws, sd)
            gen.send_frame(8, 16)
        elif decoder_id == "wiegand_c":
            d0 = channels_map.get("d0", 0)
            d1 = channels_map.get("d1", 1)
            gen = ps.WiegandGenerator(builder, d0, d1)
            gen.send_wiegand26()
        elif decoder_id == "sony_md_c":
            ch = channels_map.get("data", 0)
            gen = ps.SonyMDGenerator(builder, ch)
            gen.send_message()
        else:
            return None
    except Exception as e:
        print(f"  WARN: {decoder_id} synthesis failed: {e}")
        return None

    # Pack and concatenate
    result = bytearray()
    for ch_idx in range(num_channels):
        result.extend(samples_to_bitpacked(builder.channels[ch_idx]))
    return (bytes(result), synth_sr, sample_count)

def samples_to_bitpacked(channel_data):
    """Convert a list of 0/1 sample values to bit-packed bytes.

    bit 0 of byte 0 = sample 0, bit 1 of byte 0 = sample 1, etc.
    """
    num_bytes = math.ceil(len(channel_data) / 8)
    result = bytearray(num_bytes)
    for i, val in enumerate(channel_data):
        if val:
            byte_idx = i // 8
            bit_idx = i % 8
            result[byte_idx] |= (1 << bit_idx)
    return bytes(result)


def parse_c_decoder_file(filepath):
    """Parse a C decoder source file and extract metadata.

    Returns a dict with keys: id, channels, optional_channels, options, inputs
    or None if parsing fails.
    """
    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            content = f.read()
    except Exception as e:
        print(f"  ERROR reading {filepath}: {e}")
        return None

    result = {
        "id": None,
        "channels": [],
        "optional_channels": [],
        "options": [],
        "inputs": [],
    }

    # Find the srd_c_decoder struct definition
    struct_pattern = re.compile(
        r'(?:static\s+)?struct\s+srd_c_decoder\s+\w+\s*=\s*\{',
        re.MULTILINE
    )
    struct_match = struct_pattern.search(content)
    if struct_match:
        start = struct_match.end()
        depth = 1
        pos = start
        while pos < len(content) and depth > 0:
            if content[pos] == '{':
                depth += 1
            elif content[pos] == '}':
                depth -= 1
            pos += 1
        struct_body = content[start:pos]

        id_match = re.search(r'\.id\s*=\s*"([^"]+)"', struct_body)
        if id_match:
            result["id"] = id_match.group(1)
        else:
            first_str = re.search(r'^\s*"([^"]+)"', struct_body, re.MULTILINE)
            if first_str:
                result["id"] = first_str.group(1)

    result["channels"] = _parse_channel_array(content, "channels")
    result["optional_channels"] = _parse_channel_array(content, "optional_channels")
    result["options"] = _parse_options(content)
    result["inputs"] = _parse_inputs(content)

    if result["id"] is None:
        basename = os.path.basename(filepath)
        result["id"] = basename.replace(".c", "")

    return result


def _parse_channel_array(content, field_name):
    channels = []
    var_pattern = re.compile(
        r'static\s+struct\s+srd_channel\s+(\w+)\s*\[\]\s*=\s*\{',
        re.MULTILINE
    )
    target_vars = []
    for m in var_pattern.finditer(content):
        varname = m.group(1)
        if field_name == "channels":
            if "channels" in varname and "optional_channels" not in varname:
                target_vars.append(varname)
        elif field_name == "optional_channels":
            if "optional_channels" in varname:
                target_vars.append(varname)

    struct_ref_pattern = re.compile(r'\.' + field_name + r'\s*=\s*(\w+)\s*,')
    for m in struct_ref_pattern.finditer(content):
        varname = m.group(1)
        if varname != "NULL" and varname not in target_vars:
            target_vars.append(varname)

    for varname in target_vars:
        array_pattern = re.compile(
            r'static\s+struct\s+srd_channel\s+' + re.escape(varname) +
            r'\s*\[\]\s*=\s*\{(.*?)\}\s*;',
            re.DOTALL
        )
        m = array_pattern.search(content)
        if not m: continue
        array_body = m.group(1)
        entry_pattern = re.compile(r'\{\s*"([^"]+)"\s*,\s*"[^"]*"\s*,\s*"[^"]*"\s*,\s*(\d+)\s*,')
        for em in entry_pattern.finditer(array_body):
            ch_id = em.group(1)
            ch_order = int(em.group(2))
            channels.append({"id": ch_id, "order": ch_order})

    channels.sort(key=lambda c: c["order"])
    return channels


def _parse_options(content):
    options = []
    var_pattern = re.compile(r'static\s+struct\s+srd_decoder_option\s+(\w+)\s*\[\]\s*=\s*\{', re.MULTILINE)
    struct_ref_pattern = re.compile(r'\.options\s*=\s*(\w+)\s*,')
    target_vars = []
    for m in struct_ref_pattern.finditer(content):
        varname = m.group(1)
        if varname != "NULL": target_vars.append(varname)
    for m in var_pattern.finditer(content):
        varname = m.group(1)
        if varname not in target_vars: target_vars.append(varname)

    found_any = False
    for varname in target_vars:
        array_pattern = re.compile(r'static\s+struct\s+srd_decoder_option\s+' + re.escape(varname) + r'\s*\[\]\s*=\s*\{(.*?)\}\s*;', re.DOTALL)
        if array_pattern.search(content):
            found_any = True
            break
    if not found_any: return []

    defaults = {}
    entry_pattern = re.compile(r'SRD_C_DECODER_EXPORT\s+struct\s+srd_c_decoder\s*\*?\s*srd_c_decoder_entry\s*\(\s*void\s*\)\s*\{(.*?)\n\}', re.DOTALL)
    entry_match = entry_pattern.search(content)
    if entry_match:
        entry_body = entry_match.group(1)
        def_pattern = re.compile(r'(\w+)\[(\d+)\]\.def\s*=\s*g_variant_new_(\w+)\(([^)]+)\)')
        for dm in def_pattern.finditer(entry_body):
            var, idx, vtype, vval = dm.groups()
            idx = int(idx)
            vval = vval.strip()
            if var in target_vars:
                if vtype == "string":
                    sm = re.search(r'"([^"]*)"', vval)
                    if sm: defaults[(var, idx)] = sm.group(1)
                elif vtype in ["int64", "int32", "uint64"]:
                    try: defaults[(var, idx)] = int(vval)
                    except: defaults[(var, idx)] = 0
                elif vtype == "double":
                    try: defaults[(var, idx)] = float(vval)
                    except: defaults[(var, idx)] = 0.0
                elif vtype == "boolean":
                    defaults[(var, idx)] = vval == "TRUE"

    for varname in target_vars:
        array_pattern = re.compile(r'static\s+struct\s+srd_decoder_option\s+' + re.escape(varname) + r'\s*\[\]\s*=\s*\{(.*?)\}\s*;', re.DOTALL)
        m = array_pattern.search(content)
        if not m: continue
        array_body = m.group(1)
        local_ids = []
        local_entry_pattern = re.compile(r'\{\s*"([^"]+)"\s*,')
        for em in local_entry_pattern.finditer(array_body): local_ids.append(em.group(1))
        for local_idx, opt_id in enumerate(local_ids):
            options.append({"id": opt_id, "default": defaults.get((varname, local_idx), None)})
    return options


def _parse_inputs(content):
    inputs = []
    input_pattern = re.compile(r'static\s+const\s+char\s*\*\s*(\w+)\s*\[\]\s*=\s*\{([^}]+)\}', re.DOTALL)
    struct_ref_pattern = re.compile(r'\.inputs\s*=\s*(\w+)\s*,')
    target_var = None
    for m in struct_ref_pattern.finditer(content):
        v = m.group(1)
        if v != "NULL": target_var = v
    for m in input_pattern.finditer(content):
        varname, array_body = m.groups()
        if target_var and varname != target_var: continue
        token_pattern = re.compile(r'"([^"]*)"|SRD_FMT_LOGIC')
        for sm in token_pattern.finditer(array_body):
            val = sm.group(1)
            if sm.group(0) == "SRD_FMT_LOGIC":
                val = "logic"
            if val: inputs.append(val)
        break
    return inputs


def generate_input_bin(num_channels, sample_count):
    random.seed(42)
    all_channels = []
    for ch_idx in range(num_channels):
        if ch_idx == 0: data = [i % 2 for i in range(sample_count)]
        elif ch_idx == 1: data = [0] * sample_count
        elif ch_idx == 2: data = [1] * sample_count
        else: data = [random.randint(0, 1) for _ in range(sample_count)]
        all_channels.append(data)
    result = bytearray()
    for ch_idx in range(num_channels):
        result.extend(samples_to_bitpacked(all_channels[ch_idx]))
    return bytes(result)


def generate_config(decoder_info):
    all_channels = decoder_info["channels"] + decoder_info["optional_channels"]
    if not all_channels: return None
    channels_map = {ch["id"]: i for i, ch in enumerate(all_channels)}
    options_map = {opt["id"]: opt["default"] for opt in decoder_info["options"] if opt["default"] is not None}
    return {
        "decoder": decoder_info["id"],
        "samplerate": 1000000,
        "num_channels": len(all_channels),
        "sample_count": 10000,
        "channels": channels_map,
        "options": options_map,
    }


def generate_non_logic_config(decoder_info):
    all_channels = decoder_info["channels"] + decoder_info["optional_channels"]
    options_map = {opt["id"]: opt["default"] for opt in decoder_info["options"] if opt["default"] is not None}
    return {
        "decoder": decoder_info["id"],
        "samplerate": 1000000,
        "needs_upstream": True,
        "inputs": decoder_info["inputs"],
        "channels": {ch["id"]: ch["order"] for ch in all_channels} if all_channels else {},
        "options": options_map,
    }


def main():
    parser = argparse.ArgumentParser(description="Generate test data for C decoders")
    parser.add_argument("--c-decoders-dir", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "c_decoders"))
    parser.add_argument("--output-dir", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "testdata"))
    parser.add_argument("--skip-existing", action="store_true", default=True)
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args()
    if args.overwrite: args.skip_existing = False

    c_decoders_dir = os.path.normpath(args.c_decoders_dir)
    output_dir = os.path.normpath(args.output_dir)
    if not os.path.isdir(c_decoders_dir): sys.exit(1)

    c_files = sorted([f for f in os.listdir(c_decoders_dir) if f.endswith("_c.c")])
    if not c_files: sys.exit(1)

    processed = generated = skipped = errors = non_logic = no_channels = 0
    for c_file in c_files:
        filepath = os.path.join(c_decoders_dir, c_file)
        decoder_info = parse_c_decoder_file(filepath)
        if not decoder_info or not decoder_info["id"]:
            errors += 1; processed += 1; continue
        decoder_id = decoder_info["id"]; processed += 1
        decoder_output_dir = os.path.join(output_dir, decoder_id, "default")
        config_path = os.path.join(decoder_output_dir, "config.json")
        if args.skip_existing and os.path.exists(config_path):
            skipped += 1; continue

        if "logic" not in decoder_info["inputs"]:
            config = generate_non_logic_config(decoder_info)
            if not config: no_channels += 1; continue
            os.makedirs(decoder_output_dir, exist_ok=True)
            with open(config_path, "w", encoding="utf-8") as f:
                json.dump(config, f, indent=2, ensure_ascii=False); f.write("\n")
            non_logic += 1; generated += 1; continue

        all_channels = decoder_info["channels"] + decoder_info["optional_channels"]
        if not all_channels: no_channels += 1; continue
        config = generate_config(decoder_info)
        if not config: no_channels += 1; continue
        input_data = synthesize_input_bin(decoder_id, config["num_channels"], config["sample_count"], config["channels"])
        if input_data is None:
            input_data = generate_input_bin(config["num_channels"], config["sample_count"])
        else:
            data_bytes, synth_sr, synth_sc = input_data
            input_data = data_bytes
            config["samplerate"] = synth_sr
            config["sample_count"] = synth_sc
            print(f"  SYN  {decoder_id}: Using synthesized protocol data")

        os.makedirs(decoder_output_dir, exist_ok=True)
        with open(config_path, "w", encoding="utf-8") as f:
            json.dump(config, f, indent=2, ensure_ascii=False); f.write("\n")
        with open(os.path.join(decoder_output_dir, "input.bin"), "wb") as f:
            f.write(input_data)
        generated += 1

    print(f"\nSummary:\n  Processed: {processed}\n  Generated: {generated}\n  Skipped: {skipped}\n  Errors: {errors}")

if __name__ == "__main__":
    main()
