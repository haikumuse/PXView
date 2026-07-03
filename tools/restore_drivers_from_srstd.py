#!/usr/bin/env python3
"""
restore_drivers_from_srstd.py - Restore migrated drivers to upstream state.

Reverses the changes made by migrate_drivers_to_srstd.py:
  1. Removes '#include "srstd.h"' from protocol.h
  2. Restores 'static' keyword before <driver>_driver_info struct in api.c

This is needed because the dynamic library approach (convert-libsigrokstd-to-shared-library spec)
uses -fvisibility=hidden instead of macro renaming, so drivers no longer need
the srstd.h include or the static removal workaround.

Usage:
    python tools/restore_drivers_from_srstd.py --all
    python tools/restore_drivers_from_srstd.py --driver sipeed-slogic-analyzer
"""

import argparse
import os
import re
import sys
from pathlib import Path


def restore_protocol_h(path: Path) -> bool:
    """Remove '#include "srstd.h"' line from protocol.h. Returns True if changed."""
    if not path.exists():
        return False
    text = path.read_text(encoding='utf-8', errors='replace')
    # Match the include line (with optional surrounding whitespace/newlines)
    new_text = re.sub(
        r'\n#include\s+"srstd\.h"\n',
        '\n',
        text
    )
    if new_text != text:
        path.write_text(new_text, encoding='utf-8')
        return True
    return False


def restore_api_c(path: Path) -> bool:
    """Restore 'static' before <driver>_driver_info struct in api.c.
    The migrate script removed 'static' to allow manual registration.
    In the dynamic library approach, internal static symbols are hidden
    by -fvisibility=hidden, so static can be restored.
    Returns True if changed."""
    if not path.exists():
        return False
    text = path.read_text(encoding='utf-8', errors='replace')
    # The migrate script removed 'static' before 'struct sr_dev_driver <name>_driver_info'
    # Restore it. Match: 'struct sr_dev_driver <name>_driver_info = {'
    # Avoid matching if 'static' is already present.
    pattern = re.compile(
        r'^(?!\s*static\s)struct\s+sr_dev_driver\s+(\w+)_driver_info\s*=',
        re.MULTILINE
    )
    new_text = pattern.sub(r'static struct sr_dev_driver \1_driver_info =', text)
    if new_text != text:
        path.write_text(new_text, encoding='utf-8')
        return True
    return False


def find_driver_dirs(hardware_root: Path):
    """Yield (driver_name, driver_dir) for each directory containing api.c + protocol.h."""
    for entry in sorted(hardware_root.iterdir()):
        if not entry.is_dir():
            continue
        api_c = entry / 'api.c'
        protocol_h = entry / 'protocol.h'
        if api_c.exists() or protocol_h.exists():
            yield entry.name, entry


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--all', action='store_true',
                        help='Restore all drivers in libsigrokstd/src/hardware/')
    parser.add_argument('--driver', metavar='NAME',
                        help='Restore a single driver by directory name')
    parser.add_argument('--hardware-root', default=None,
                        help='Override hardware root path (default: auto-detect)')
    args = parser.parse_args()

    if not args.all and not args.driver:
        parser.error('must specify --all or --driver NAME')

    # Locate hardware root
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    if args.hardware_root:
        hardware_root = Path(args.hardware_root)
    else:
        # Try libsigrokstd/src/hardware first, then libsigrokstd/hardware
        candidates = [
            project_root / 'libsigrokstd' / 'src' / 'hardware',
            project_root / 'libsigrokstd' / 'hardware',
        ]
        hardware_root = None
        for c in candidates:
            if c.exists():
                hardware_root = c
                break
        if hardware_root is None:
            print(f"ERROR: hardware root not found in {candidates}", file=sys.stderr)
            sys.exit(1)

    print(f"Hardware root: {hardware_root}")

    if args.driver:
        driver_dirs = [(args.driver, hardware_root / args.driver)]
        if not driver_dirs[0][1].exists():
            print(f"ERROR: driver directory not found: {driver_dirs[0][1]}", file=sys.stderr)
            sys.exit(1)
    else:
        driver_dirs = list(find_driver_dirs(hardware_root))

    print(f"Found {len(driver_dirs)} driver(s) to process")

    changed_protocol = 0
    changed_api = 0
    for name, d in driver_dirs:
        protocol_h = d / 'protocol.h'
        api_c = d / 'api.c'

        if restore_protocol_h(protocol_h):
            print(f"  {name}: protocol.h - removed #include \"srstd.h\"")
            changed_protocol += 1
        if restore_api_c(api_c):
            print(f"  {name}: api.c - restored static before _driver_info")
            changed_api += 1

    print(f"\nDone. Modified {changed_protocol} protocol.h and {changed_api} api.c files.")


if __name__ == '__main__':
    main()
