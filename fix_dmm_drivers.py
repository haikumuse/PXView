#!/usr/bin/env python3
"""Flatten sr_analog_init/meaning/encoding/spec sub-structs to PXView's
flat sr_datafeed_analog layout across the 14 broken DMM drivers."""
import re
import os
import sys

BASE = r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware"
FILES = [
    "fluke-45/protocol.c",
    "agilent-dmm/protocol.c",
    "appa-55ii/protocol.c",
    "uni-t-ut32x/protocol.c",
    "gwinstek-psp/protocol.c",
    "mastech-ms6514/protocol.c",
    "testo/protocol.c",
    "lascar-el-usb/protocol.c",
    "center-3xx/protocol.c",
    "mic-985xx/protocol.c",
    "tondaj-sl-814/protocol.c",
    "kern-scale/protocol.c",
    "teleinfo/protocol.c",
    "pce-322a/protocol.c",
]

# Member-access via the analog struct (pointer or struct): .meaning->X / ->meaning->X
# NOTE: replacing .meaning->mq also covers .meaning->mqflags (mq is a prefix).
POINTER_REPLACES = [
    (".meaning->mq", ".mq"),
    (".meaning->unit", ".unit"),
    (".meaning->channels", ".probes"),
    ("->meaning->mq", "->mq"),
    ("->meaning->unit", "->unit"),
    ("->meaning->channels", "->probes"),
]

# Standalone local-var access: meaning.X -> analog.X (channels -> analog.probes)
STANDALONE_REPLACES = [
    ("meaning.mqflags", "analog.mqflags"),
    ("meaning.mq", "analog.mq"),
    ("meaning.unit", "analog.unit"),
    ("meaning.channels", "analog.probes"),
]

# Delete struct declaration lines (incl. array forms like encoding[2])
DECL_PATTERNS = [
    re.compile(r'(?m)^[ \t]*struct sr_analog_encoding\s+\w+\s*(?:\[[^\]]*\])?\s*;[ \t]*\r?\n'),
    re.compile(r'(?m)^[ \t]*struct sr_analog_meaning\s+\w+\s*(?:\[[^\]]*\])?\s*;[ \t]*\r?\n'),
    re.compile(r'(?m)^[ \t]*struct sr_analog_spec\s+\w+\s*(?:\[[^\]]*\])?\s*;[ \t]*\r?\n'),
]

# Delete assignment lines that set digits/spec_digits/is_bigendian/is_float/unitsize
# on the now-removed encoding/spec sub-structs (standalone or via analog).
ASSIGN_PAT = re.compile(
    r'(?m)^[ \t]*[^\n]*(?:encoding\.digits|spec\.spec_digits|'
    r'encoding->digits|spec->spec_digits|encoding->is_bigendian|'
    r'encoding->is_float|encoding->unitsize)\s*=[^\n]*;[ \t]*\r?\n')

# Replace sr_analog_init(&analog, &encoding, &meaning, &spec, <arg>); (maybe multi-line)
INIT_PAT = re.compile(
    r'(?m)^([ \t]*)sr_analog_init\((&[\w.\[\]\->]+),[^;]*?\);[ \t]*\r?\n',
    re.DOTALL)


def repl_init(m):
    indent = m.group(1)
    analog_ref = m.group(2)            # e.g. "&analog" or "&analog[i]"
    lvalue = analog_ref[1:] if analog_ref.startswith('&') else analog_ref
    return (f"{indent}memset({analog_ref}, 0, sizeof({lvalue}));\n"
            f"{indent}{lvalue}.unit_bits = 32; /* float */\n"
            f"{indent}{lvalue}.unit_pitch = 0;")


def process(rel):
    path = os.path.join(BASE, rel)
    with open(path, 'r', encoding='utf-8', newline='') as f:
        text = f.read()
    orig = text

    for old, new in POINTER_REPLACES:
        text = text.replace(old, new)
    for old, new in STANDALONE_REPLACES:
        text = text.replace(old, new)
    for pat in DECL_PATTERNS:
        text = pat.sub('', text)
    text = ASSIGN_PAT.sub('', text)
    text = INIT_PAT.sub(repl_init, text)

    if text != orig:
        with open(path, 'w', encoding='utf-8', newline='') as f:
            f.write(text)
        return True
    return False


def main():
    for rel in FILES:
        changed = process(rel)
        print(("MODIFIED " if changed else "no-change  ") + rel)


if __name__ == "__main__":
    main()
