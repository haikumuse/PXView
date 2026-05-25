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

    # Extract decoder ID from the srd_c_decoder struct
    # Two patterns:
    #   1. Designated initializer: .id = "spi_c",
    #   2. Positional initializer: struct srd_c_decoder foo = { "usb_signalling_c", ...
    # We must search within the srd_c_decoder struct to avoid matching option .id fields.

    # Find the srd_c_decoder struct definition
    struct_pattern = re.compile(
        r'(?:static\s+)?struct\s+srd_c_decoder\s+\w+\s*=\s*\{',
        re.MULTILINE
    )
    struct_match = struct_pattern.search(content)
    if struct_match:
        # Extract the struct body (from opening { to the matching closing })
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

        # Try designated initializer: .id = "spi_c",
        id_match = re.search(r'\.id\s*=\s*"([^"]+)"', struct_body)
        if id_match:
            result["id"] = id_match.group(1)
        else:
            # Try positional initializer: first string literal in the struct
            # e.g., "usb_signalling_c",
            first_str = re.search(r'^\s*"([^"]+)"', struct_body, re.MULTILINE)
            if first_str:
                result["id"] = first_str.group(1)

    # Extract channels
    # Pattern: static struct srd_channel <name>[] = { ... };
    # Each entry: { "id", "name", "desc", order, SRD_CHANNEL_*, ... },
    # or: { "id", "name", "desc", order, SRD_CHANNEL_*, "idn" },
    result["channels"] = _parse_channel_array(content, "channels")

    # Extract optional_channels
    result["optional_channels"] = _parse_channel_array(content, "optional_channels")

    # Extract options
    # Pattern: static struct srd_decoder_option <name>[] = { ... };
    # Each entry: { "id", "idn", "desc", NULL, NULL },
    # Default values are set in srd_c_decoder_entry() via g_variant_new_*
    result["options"] = _parse_options(content)

    # Extract inputs
    # Pattern: static const char* <name>_inputs[] = { "logic", NULL };
    # or: static const char *<name>_inputs[] = {"logic"};
    result["inputs"] = _parse_inputs(content)

    if result["id"] is None:
        # Try to derive ID from filename
        basename = os.path.basename(filepath)
        result["id"] = basename.replace(".c", "")

    return result


def _parse_channel_array(content, field_name):
    """Parse a channels[] or optional_channels[] array from C source.

    Looks for both the static array definition and the struct assignment
    to determine which variable name is used.
    """
    channels = []

    # Strategy 1: Find static array definitions with field_name in the variable name
    # e.g., static struct srd_channel spi_channels[] = { ... };
    # e.g., static struct srd_channel spi_optional_channels[] = { ... };
    # Also handle: static struct srd_channel uart_optional_channels[] = { ... };

    # First, find variable names that contain the field_name
    # Pattern: static struct srd_channel <varname>[] =
    var_pattern = re.compile(
        r'static\s+struct\s+srd_channel\s+(\w+)\s*\[\]\s*=\s*\{',
        re.MULTILINE
    )

    target_vars = []
    for m in var_pattern.finditer(content):
        varname = m.group(1)
        # Check if this variable name contains the field_name
        # e.g., "spi_channels" contains "channels"
        # e.g., "spi_optional_channels" contains "optional_channels"
        # But "spi_channels" should NOT match "optional_channels"
        if field_name == "channels":
            # Must contain "channels" but NOT "optional_channels"
            if "channels" in varname and "optional_channels" not in varname:
                target_vars.append(varname)
        elif field_name == "optional_channels":
            if "optional_channels" in varname:
                target_vars.append(varname)

    # Also check if the decoder struct references a variable for this field
    # Pattern: .channels = <varname>,
    # Pattern: .optional_channels = <varname>,
    struct_ref_pattern = re.compile(
        r'\.' + field_name + r'\s*=\s*(\w+)\s*,'
    )
    for m in struct_ref_pattern.finditer(content):
        varname = m.group(1)
        if varname != "NULL" and varname not in target_vars:
            target_vars.append(varname)

    for varname in target_vars:
        # Find the array definition for this variable
        array_pattern = re.compile(
            r'static\s+struct\s+srd_channel\s+' + re.escape(varname) +
            r'\s*\[\]\s*=\s*\{(.*?)\}\s*;',
            re.DOTALL
        )
        m = array_pattern.search(content)
        if not m:
            continue

        array_body = m.group(1)

        # Parse individual channel entries
        # Pattern: { "id", "name", "desc", order, TYPE, ... },
        # The 6-field variant: { "id", "name", "desc", order, TYPE, "idn" },
        entry_pattern = re.compile(
            r'\{\s*"([^"]+)"\s*,\s*"[^"]*"\s*,\s*"[^"]*"\s*,\s*(\d+)\s*,'
        )
        for em in entry_pattern.finditer(array_body):
            ch_id = em.group(1)
            ch_order = int(em.group(2))
            channels.append({"id": ch_id, "order": ch_order})

    # Sort by order
    channels.sort(key=lambda c: c["order"])
    return channels


def _parse_options(content):
    """Parse decoder options from C source.

    Extracts option IDs from the static array definition and default values
    from the srd_c_decoder_entry() function.
    """
    options = []

    # Find option array variable names
    # Pattern: static struct srd_decoder_option <varname>[] = { ... };
    var_pattern = re.compile(
        r'static\s+struct\s+srd_decoder_option\s+(\w+)\s*\[\]\s*=\s*\{',
        re.MULTILINE
    )

    # Also check struct reference
    struct_ref_pattern = re.compile(
        r'\.options\s*=\s*(\w+)\s*,'
    )

    target_vars = []
    for m in struct_ref_pattern.finditer(content):
        varname = m.group(1)
        if varname != "NULL":
            target_vars.append(varname)

    for m in var_pattern.finditer(content):
        varname = m.group(1)
        if varname not in target_vars:
            target_vars.append(varname)

    # Collect option IDs from all matching arrays
    # (validation pass - just check that at least one array definition exists)
    found_any = False
    for varname in target_vars:
        array_pattern = re.compile(
            r'static\s+struct\s+srd_decoder_option\s+' + re.escape(varname) +
            r'\s*\[\]\s*=\s*\{(.*?)\}\s*;',
            re.DOTALL
        )
        if array_pattern.search(content):
            found_any = True
            break

    if not found_any:
        return []

    # Now extract default values from srd_c_decoder_entry()
    # Pattern: <varname>[<index>].def = g_variant_new_<type>(<value>);
    defaults = {}

    # Find the entry function
    entry_pattern = re.compile(
        r'SRD_C_DECODER_EXPORT\s+struct\s+srd_c_decoder\s*\*?\s*srd_c_decoder_entry\s*\(\s*void\s*\)\s*\{(.*?)\n\}',
        re.DOTALL
    )
    entry_match = entry_pattern.search(content)
    if entry_match:
        entry_body = entry_match.group(1)

        # Match: <varname>[<index>].def = g_variant_new_int64(<value>);
        # Match: <varname>[<index>].def = g_variant_new_double(<value>);
        # Match: <varname>[<index>].def = g_variant_new_string("<value>");
        def_pattern = re.compile(
            r'(\w+)\[(\d+)\]\.def\s*=\s*g_variant_new_(\w+)\(([^)]+)\)'
        )
        for dm in def_pattern.finditer(entry_body):
            var = dm.group(1)
            idx = int(dm.group(2))
            vtype = dm.group(3)
            vval = dm.group(4).strip()

            if var in target_vars:
                # Parse the value
                if vtype == "string":
                    # Extract string value
                    str_match = re.search(r'"([^"]*)"', vval)
                    if str_match:
                        defaults[(var, idx)] = str_match.group(1)
                elif vtype == "int64" or vtype == "int32" or vtype == "uint64":
                    try:
                        defaults[(var, idx)] = int(vval)
                    except ValueError:
                        defaults[(var, idx)] = 0
                elif vtype == "double":
                    try:
                        defaults[(var, idx)] = float(vval)
                    except ValueError:
                        defaults[(var, idx)] = 0.0
                elif vtype == "boolean":
                    defaults[(var, idx)] = vval == "TRUE"

    # Build options list with defaults
    for varname in target_vars:
        # Count options in this variable's array
        array_pattern = re.compile(
            r'static\s+struct\s+srd_decoder_option\s+' + re.escape(varname) +
            r'\s*\[\]\s*=\s*\{(.*?)\}\s*;',
            re.DOTALL
        )
        m = array_pattern.search(content)
        if not m:
            continue

        array_body = m.group(1)

        # Re-count entries for this specific array
        local_ids = []
        local_entry_pattern = re.compile(r'\{\s*"([^"]+)"\s*,')
        for em in local_entry_pattern.finditer(array_body):
            local_ids.append(em.group(1))

        for local_idx, opt_id in enumerate(local_ids):
            default_val = defaults.get((varname, local_idx), None)
            options.append({
                "id": opt_id,
                "default": default_val,
            })

    return options


def _parse_inputs(content):
    """Parse the inputs array from C source.

    Pattern: static const char* <name>_inputs[] = { "logic", NULL };
    Pattern: static const char *<name>_inputs[] = {"logic"};
    """
    inputs = []

    # Find all inputs array definitions
    # Pattern: static const char* <varname>[] = { ... };
    input_pattern = re.compile(
        r'static\s+const\s+char\s*\*\s*(\w+)\s*\[\]\s*=\s*\{([^}]+)\}',
        re.DOTALL
    )

    # Also find the variable name from the struct
    struct_ref_pattern = re.compile(
        r'\.inputs\s*=\s*(\w+)\s*,'
    )
    target_var = None
    for m in struct_ref_pattern.finditer(content):
        v = m.group(1)
        if v != "NULL":
            target_var = v

    # Search for the inputs array
    for m in input_pattern.finditer(content):
        varname = m.group(1)
        if target_var and varname != target_var:
            continue
        if not target_var and "inputs" not in varname:
            continue

        array_body = m.group(2)
        # Extract string values
        str_pattern = re.compile(r'"([^"]*)"')
        for sm in str_pattern.finditer(array_body):
            val = sm.group(1)
            if val:  # Skip empty strings
                inputs.append(val)
        break  # Use the first matching variable

    return inputs


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


def generate_input_bin(num_channels, sample_count):
    """Generate basic signal patterns for input.bin.

    Channel 0: alternating 0/1 pattern (10101010...)
    Channel 1: all zeros
    Channel 2: all ones
    Channel 3+: random pattern

    Returns bytes for the input.bin file.
    """
    random.seed(42)  # Deterministic output

    all_channels = []
    for ch_idx in range(num_channels):
        if ch_idx == 0:
            # Alternating pattern
            data = [i % 2 for i in range(sample_count)]
        elif ch_idx == 1:
            # All zeros
            data = [0] * sample_count
        elif ch_idx == 2:
            # All ones
            data = [1] * sample_count
        else:
            # Random pattern
            data = [random.randint(0, 1) for _ in range(sample_count)]
        all_channels.append(data)

    # Pack and concatenate
    result = bytearray()
    for ch_idx in range(num_channels):
        result.extend(samples_to_bitpacked(all_channels[ch_idx]))
    return bytes(result)


def generate_config(decoder_info):
    """Generate config.json content for a decoder.

    Returns a dict representing the config, or None if the decoder
    has no channels (cannot accept direct logic input).
    """
    all_channels = decoder_info["channels"] + decoder_info["optional_channels"]
    if not all_channels:
        return None

    num_channels = len(all_channels)

    # Build channels map: id -> index
    channels_map = {}
    for i, ch in enumerate(all_channels):
        channels_map[ch["id"]] = i

    # Build options map: id -> default value
    options_map = {}
    for opt in decoder_info["options"]:
        if opt["default"] is not None:
            options_map[opt["id"]] = opt["default"]

    config = {
        "decoder": decoder_info["id"],
        "samplerate": 1000000,
        "num_channels": num_channels,
        "sample_count": 10000,
        "channels": channels_map,
        "options": options_map,
    }

    return config


def generate_non_logic_config(decoder_info):
    """Generate config.json for a non-logic-input decoder.

    These decoders need upstream decoder output and cannot accept
    direct logic signal input.
    """
    all_channels = decoder_info["channels"] + decoder_info["optional_channels"]

    # Build options map
    options_map = {}
    for opt in decoder_info["options"]:
        if opt["default"] is not None:
            options_map[opt["id"]] = opt["default"]

    config = {
        "decoder": decoder_info["id"],
        "samplerate": 1000000,
        "needs_upstream": True,
        "inputs": decoder_info["inputs"],
        "channels": {ch["id"]: ch["order"] for ch in all_channels} if all_channels else {},
        "options": options_map,
    }

    return config


def main():
    parser = argparse.ArgumentParser(
        description="Generate test data for C decoders by parsing source files"
    )
    parser.add_argument(
        "--c-decoders-dir",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "c_decoders"),
        help="Path to c_decoders directory (default: ../c_decoders/)",
    )
    parser.add_argument(
        "--output-dir",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "testdata"),
        help="Path to testdata output directory (default: ./testdata/)",
    )
    parser.add_argument(
        "--skip-existing",
        action="store_true",
        default=True,
        help="Skip decoders that already have test data (default: True)",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing test data (disables --skip-existing)",
    )
    args = parser.parse_args()

    if args.overwrite:
        args.skip_existing = False

    c_decoders_dir = os.path.normpath(args.c_decoders_dir)
    output_dir = os.path.normpath(args.output_dir)

    if not os.path.isdir(c_decoders_dir):
        print(f"ERROR: C decoders directory not found: {c_decoders_dir}")
        sys.exit(1)

    # Find all C decoder source files
    c_files = sorted([
        f for f in os.listdir(c_decoders_dir)
        if f.endswith("_c.c")
    ])

    if not c_files:
        print(f"No C decoder files found in {c_decoders_dir}")
        sys.exit(1)

    print(f"Found {len(c_files)} C decoder source files in {c_decoders_dir}")
    print(f"Output directory: {output_dir}")
    print()

    processed = 0
    generated = 0
    skipped = 0
    errors = 0
    non_logic = 0
    no_channels = 0

    for c_file in c_files:
        filepath = os.path.join(c_decoders_dir, c_file)
        decoder_name = c_file.replace(".c", "")

        # Parse the C source file
        decoder_info = parse_c_decoder_file(filepath)
        if decoder_info is None:
            print(f"  SKIP {decoder_name}: failed to parse")
            errors += 1
            processed += 1
            continue

        if decoder_info["id"] is None:
            print(f"  SKIP {decoder_name}: no decoder ID found")
            errors += 1
            processed += 1
            continue

        decoder_id = decoder_info["id"]
        processed += 1

        # Determine output directory
        decoder_output_dir = os.path.join(output_dir, decoder_id, "default")

        # Check if test data already exists
        config_path = os.path.join(decoder_output_dir, "config.json")
        if args.skip_existing and os.path.exists(config_path):
            print(f"  SKIP {decoder_id}: test data already exists")
            skipped += 1
            continue

        # Determine if this is a logic-input decoder
        is_logic_input = "logic" in decoder_info["inputs"]

        if not is_logic_input:
            # Non-logic decoder: generate config with needs_upstream=true
            config = generate_non_logic_config(decoder_info)
            if config is None:
                print(f"  SKIP {decoder_id}: cannot generate config (no inputs, no channels)")
                no_channels += 1
                continue

            os.makedirs(decoder_output_dir, exist_ok=True)
            with open(config_path, "w", encoding="utf-8") as f:
                json.dump(config, f, indent=2, ensure_ascii=False)
                f.write("\n")

            print(f"  GEN  {decoder_id}: config.json (needs_upstream=true, inputs={decoder_info['inputs']})")
            non_logic += 1
            generated += 1
            continue

        # Logic-input decoder
        all_channels = decoder_info["channels"] + decoder_info["optional_channels"]
        if not all_channels:
            print(f"  SKIP {decoder_id}: logic input but no channels defined")
            no_channels += 1
            continue

        num_channels = len(all_channels)
        sample_count = 10000

        # Generate config.json
        config = generate_config(decoder_info)
        if config is None:
            print(f"  SKIP {decoder_id}: cannot generate config")
            no_channels += 1
            continue

        # Generate input.bin
        input_data = generate_input_bin(num_channels, sample_count)

        # Write files
        os.makedirs(decoder_output_dir, exist_ok=True)

        with open(config_path, "w", encoding="utf-8") as f:
            json.dump(config, f, indent=2, ensure_ascii=False)
            f.write("\n")

        input_path = os.path.join(decoder_output_dir, "input.bin")
        with open(input_path, "wb") as f:
            f.write(input_data)

        ch_names = ", ".join(ch["id"] for ch in all_channels)
        opt_count = len(decoder_info["options"])
        print(f"  GEN  {decoder_id}: config.json + input.bin "
              f"({num_channels}ch [{ch_names}], {opt_count} opts)")
        generated += 1

    # Summary
    print()
    print("=" * 60)
    print(f"Summary:")
    print(f"  Processed:  {processed}")
    print(f"  Generated:  {generated} ({generated - non_logic} logic, {non_logic} upstream)")
    print(f"  Skipped:    {skipped} (existing)")
    print(f"  No channels: {no_channels}")
    print(f"  Errors:     {errors}")
    print("=" * 60)


if __name__ == "__main__":
    main()
