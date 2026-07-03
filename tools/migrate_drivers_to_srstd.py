#!/usr/bin/env python3
# DEPRECATED: superseded by dynamic library approach in convert-libsigrokstd-to-shared-library spec
"""
migrate_drivers_to_srstd.py — Batch-migrate compat drivers from upstream
libsigrok to libsigrokstd.

This script performs the mechanical 4-step adaptation described in the
migrate-all-compat-drivers-to-libsigrokstd spec:

  1. Copy upstream source (.c/.h) to libsigrokstd/src/hardware/<driver>/
  2. Insert #include "srstd.h" before #include "libsigrok-internal.h" in .h files
  3. Remove 'static' modifier from <name>_driver_info declarations in api.c
  4. Generate libsigrokstd/src/driver_registry.c with extern declarations +
     srstd_get_all_drivers() function

The sipeed-slogic-analyzer driver (already migrated manually) is used as the
reference pattern. See libsigrokstd/src/hardware/sipeed-slogic-analyzer/.

Usage:
    python tools/migrate_drivers_to_srstd.py [--all] [--exclude d1,d2,...]
                                             [--dry-run] [--verbose]

The script is idempotent: repeated runs produce identical output.
"""

import os
import re
import sys
import argparse

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Upstream libsigrok hardware directory (read-only source).
UPSTREAM_HW = r"c:\Users\admin\Downloads\libsigrok\src\hardware"

# PXView compat-layer hardware directory (used to identify which drivers
# are compat drivers — the migration candidates are the intersection of
# upstream and PXView compat driver directory names).
PXVIEW_COMPAT_HW = os.path.join(
    r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb",
    "libsigrok", "hardware",
)

# Destination: libsigrokstd hardware directory (where migrated sources land).
SRSTD_HW = os.path.join(
    r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb",
    "libsigrokstd", "src", "hardware",
)

# Output: auto-generated driver registry file.
REGISTRY_PATH = os.path.join(
    r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb",
    "libsigrokstd", "src", "driver_registry.c",
)

# Default excludes — these are NOT compat drivers or are already migrated.
#   DSL                      — DreamSourceLab native, PXView fork (not upstream)
#   pxlogic                  — PXLogic native (not upstream)
#   demo                     — PXView has its own demo driver
#   common                   — shared USB/serial utilities, not a driver
#   compat                   — the PXView compat shim layer itself
#   sipeed-slogic-analyzer   — already migrated manually (reference driver)
DEFAULT_EXCLUDES = {
    "DSL",
    "pxlogic",
    "demo",
    "common",
    "compat",
    "sipeed-slogic-analyzer",
}

# ---------------------------------------------------------------------------
# Regex patterns
# ---------------------------------------------------------------------------

# Match: '[static|SR_PRIV] struct sr_dev_driver <name>_driver_info ='  (definition)
#   or: '[static|SR_PRIV] struct sr_dev_driver <name>_driver_info ;'  (forward decl)
#   or without any prefix (already non-static — idempotent re-run).
# Group 1 = optional prefix keyword ('static' or 'SR_PRIV'), Group 2 = symbol name.
# Note: SR_PRIV expands to nothing on Windows/MinGW, so 'SR_PRIV struct ...' is
# already non-static. We detect it for symbol extraction but do NOT remove it.
DRIVER_INFO_RE = re.compile(
    r'^(?:(static|SR_PRIV)[ \t]+)?struct[ \t]+sr_dev_driver[ \t]+(\w+_driver_info)[ \t]*[=;]',
    re.MULTILINE,
)

# Match: #include "libsigrok-internal.h"  or  #include <libsigrok-internal.h>
LIBSIGROK_INTERNAL_RE = re.compile(
    r'^#include[ \t]+[<"]libsigrok-internal\.h[>"][ \t]*$',
    re.MULTILINE,
)

# Check if srstd.h is already included (idempotency guard).
SRSTD_INCLUDE_RE = re.compile(
    r'^#include[ \t]+"srstd\.h"[ \t]*$',
    re.MULTILINE,
)

# Detect SR_REGISTER_DEV_DRIVER_LIST macro usage (multi-driver-list pattern).
# These drivers have anonymous compound-literal driver_info structs and cannot
# be migrated with the simple extern+reference approach.
DRIVER_LIST_MACRO_RE = re.compile(
    r'\bSR_REGISTER_DEV_DRIVER_LIST\s*\(',
)

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

def scan_driver_dirs(path):
    """Return a set of subdirectory names under *path*."""
    result = set()
    if not os.path.isdir(path):
        return result
    for name in os.listdir(path):
        if os.path.isdir(os.path.join(path, name)):
            result.add(name)
    return result


def find_driver_info_symbols(api_c_path):
    """
    Parse api.c and return a list of unique driver_info symbol names found.

    A single api.c may declare MULTIPLE driver_info structs
    (e.g. scpi-pps has scpi_pps_driver_info + hp_ib_pps_driver_info).

    Many drivers have BOTH a forward declaration ('...driver_info;') and a
    definition ('...driver_info = {').  We deduplicate so each symbol appears
    exactly once in the generated registry.
    """
    try:
        with open(api_c_path, "r", encoding="utf-8") as f:
            content = f.read()
    except (IOError, OSError):
        return []
    seen = set()
    symbols = []
    for m in DRIVER_INFO_RE.finditer(content):
        sym = m.group(2)
        if sym not in seen:
            seen.add(sym)
            symbols.append(sym)
    return symbols


def is_driver_list_only(api_c_path):
    """
    Return True if api.c uses SR_REGISTER_DEV_DRIVER_LIST but has NO named
    driver_info struct.  These drivers (serial-dmm, serial-lcr, uni-t-dmm)
    embed anonymous struct sr_dev_driver instances inside compound literals
    and collect them into a static array.  They need special manual handling
    beyond the mechanical 4-step pattern.
    """
    try:
        with open(api_c_path, "r", encoding="utf-8") as f:
            content = f.read()
    except (IOError, OSError):
        return False
    has_list = bool(DRIVER_LIST_MACRO_RE.search(content))
    has_info = bool(DRIVER_INFO_RE.search(content))
    return has_list and not has_info


def insert_srstd_before_internal_h(content):
    """
    Insert '#include "srstd.h"' immediately BEFORE the
    '#include "libsigrok-internal.h"' line, matching the slogic protocol.h
    pattern (lines 28-30).

    Returns (new_content, was_inserted).
    Idempotent: if srstd.h is already present, returns (content, False).
    """
    if SRSTD_INCLUDE_RE.search(content):
        return content, False
    m = LIBSIGROK_INTERNAL_RE.search(content)
    if not m:
        return content, False
    # Insert srstd.h + blank line before the libsigrok-internal.h line.
    insertion = '#include "srstd.h"\n\n'
    new_content = content[:m.start()] + insertion + content[m.start():]
    return new_content, True


def remove_static_from_driver_info(content):
    """
    Remove the 'static' modifier from every driver_info declaration in
    api.c (both forward declarations ending with ';' and definitions
    ending with '=').

    'SR_PRIV' is NOT removed — it expands to nothing on Windows/MinGW, so
    'SR_PRIV struct ...' is already non-static and needs no modification.

    Returns (new_content, was_modified).
    Idempotent: lines already non-static are left unchanged.
    """
    modified = [False]

    def replacer(m):
        if m.group(1) == "static":  # only strip 'static', not 'SR_PRIV'
            modified[0] = True
            # Strip 'static' + trailing whitespace from the start of the match.
            return re.sub(r"^static[ \t]+", "", m.group(0))
        return m.group(0)

    new_content = DRIVER_INFO_RE.sub(replacer, content)
    return new_content, modified[0]


def copy_and_modify_driver(src_dir, dst_dir, driver_name, dry_run, verbose):
    """
    Copy all .c/.h files from src_dir to dst_dir, applying the mechanical
    modifications:
      - .h files: insert #include "srstd.h" before libsigrok-internal.h
      - api.c:    remove 'static' from driver_info declarations

    Returns list of driver_info symbols found.
    """
    api_c_path = os.path.join(src_dir, "api.c")
    symbols = find_driver_info_symbols(api_c_path)

    if not dry_run:
        os.makedirs(dst_dir, exist_ok=True)

    for fname in sorted(os.listdir(src_dir)):
        if not (fname.endswith(".c") or fname.endswith(".h")):
            continue
        src_file = os.path.join(src_dir, fname)
        if not os.path.isfile(src_file):
            continue

        with open(src_file, "r", encoding="utf-8") as f:
            content = f.read()

        tags = []

        # Step 2: insert srstd.h in .h files before libsigrok-internal.h.
        if fname.endswith(".h"):
            content, did_insert = insert_srstd_before_internal_h(content)
            if did_insert:
                tags.append("srstd.h inserted")

        # Step 3: remove static from driver_info in api.c.
        if fname == "api.c":
            content, did_mod = remove_static_from_driver_info(content)
            if did_mod:
                tags.append("static removed")

        if dry_run:
            if verbose and tags:
                print("      {:30s} -> {}".format(fname, ", ".join(tags)))
            elif verbose:
                print("      {:30s} -> (copy only)".format(fname))
        else:
            dst_file = os.path.join(dst_dir, fname)
            with open(dst_file, "w", encoding="utf-8") as f:
                f.write(content)
            if verbose and tags:
                print("      {:30s} -> {}".format(fname, ", ".join(tags)))
            elif verbose:
                print("      {:30s} -> (copy only)".format(fname))

    return symbols


def generate_driver_registry(all_symbols, dry_run, verbose):
    """
    Generate libsigrokstd/src/driver_registry.c with:
      - extern declarations for each driver_info symbol
      - srstd_get_all_drivers() returning a NULL-terminated array of pointers
    """
    lines = []
    lines.append("/* Auto-generated by tools/migrate_drivers_to_srstd.py. Do not edit. */")
    lines.append("#include \"srstd.h\"")
    lines.append("")
    lines.append("/* extern declarations — force linker to pull in each driver's api.o */")
    for sym in all_symbols:
        lines.append("extern struct sr_dev_driver {};".format(sym))
    lines.append("")
    lines.append("/* Returns NULL-terminated array of all migrated driver_info pointers. */")
    lines.append("struct sr_dev_driver **srstd_get_all_drivers(void)")
    lines.append("{")
    lines.append("    static struct sr_dev_driver *drivers[] = {")
    for sym in all_symbols:
        lines.append("        &{},".format(sym))
    lines.append("        NULL")
    lines.append("    };")
    lines.append("    return drivers;")
    lines.append("}")
    lines.append("")

    content = "\n".join(lines)

    if dry_run:
        if verbose:
            print("\n" + "=" * 60)
            print("[DRY-RUN] driver_registry.c would contain {} symbols:".format(
                len(all_symbols)))
            print("=" * 60)
            for line in lines:
                print("  " + line)
        return

    with open(REGISTRY_PATH, "w", encoding="utf-8") as f:
        f.write(content)

    if verbose:
        print("\nGenerated {} ({} symbols)".format(REGISTRY_PATH, len(all_symbols)))


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Batch-migrate compat drivers from upstream libsigrok "
                    "to libsigrokstd.")
    parser.add_argument("--all", action="store_true", default=True,
                        help="Migrate all compat drivers (default action).")
    parser.add_argument("--exclude", type=str, default="",
                        help="Comma-separated list of driver directory names "
                             "to exclude in addition to the built-in defaults.")
    parser.add_argument("--dry-run", action="store_true",
                        help="Preview without writing any files.")
    parser.add_argument("--verbose", action="store_true",
                        help="Detailed logging.")
    args = parser.parse_args()

    # Build exclude set.
    excludes = set(DEFAULT_EXCLUDES)
    if args.exclude:
        for name in args.exclude.split(","):
            name = name.strip()
            if name:
                excludes.add(name)

    # Scan both driver trees.
    upstream_drivers = scan_driver_dirs(UPSTREAM_HW)
    compat_drivers = scan_driver_dirs(PXVIEW_COMPAT_HW)

    # Migration candidates = intersection (drivers in BOTH trees).
    candidates = sorted(upstream_drivers & compat_drivers)

    if args.verbose:
        print("Upstream libsigrok drivers : {}".format(len(upstream_drivers)))
        print("PXView compat drivers      : {}".format(len(compat_drivers)))
        print("Intersection (candidates)  : {}".format(len(candidates)))
        print("Built-in excludes          : {}".format(sorted(DEFAULT_EXCLUDES)))
        print("User excludes (--exclude)  : {}".format(
            sorted(excludes - DEFAULT_EXCLUDES)))

    # Partition: excluded vs to-migrate.
    to_migrate = [d for d in candidates if d not in excludes]
    excluded_list = [d for d in candidates if d in excludes]

    if args.verbose:
        print("\nDrivers to migrate: {}".format(len(to_migrate)))
        print("Drivers excluded  : {}".format(len(excluded_list)))

    # Migrate.
    all_symbols = []
    migrated = []
    skipped = []

    for driver in to_migrate:
        src_dir = os.path.join(UPSTREAM_HW, driver)
        dst_dir = os.path.join(SRSTD_HW, driver)
        api_c_path = os.path.join(src_dir, "api.c")

        # Skip if no api.c (not a standard driver).
        if not os.path.isfile(api_c_path):
            skipped.append((driver, "no api.c"))
            if args.verbose:
                print("\n  SKIP {}: no api.c".format(driver))
            continue

        # Skip if uses SR_REGISTER_DEV_DRIVER_LIST without driver_info structs.
        if is_driver_list_only(api_c_path):
            skipped.append((driver, "SR_REGISTER_DEV_DRIVER_LIST (multi-driver "
                                    "list, needs manual handling)"))
            if args.verbose:
                print("\n  SKIP {}: SR_REGISTER_DEV_DRIVER_LIST pattern "
                      "(no named driver_info)".format(driver))
            continue

        # Find driver_info symbols.
        symbols = find_driver_info_symbols(api_c_path)
        if not symbols:
            skipped.append((driver, "no driver_info symbol found"))
            if args.verbose:
                print("\n  SKIP {}: no driver_info symbol found".format(driver))
            continue

        if args.verbose:
            print("\n  Migrating {}: symbols={}".format(driver, symbols))

        syms = copy_and_modify_driver(src_dir, dst_dir, driver,
                                      args.dry_run, args.verbose)
        all_symbols.extend(syms)
        migrated.append((driver, syms))

    # Generate driver_registry.c.
    generate_driver_registry(all_symbols, args.dry_run, args.verbose)

    # ---- Summary ----
    mode = "DRY-RUN" if args.dry_run else "EXECUTED"
    print("\n" + "=" * 60)
    print("  Migration Summary  ({})".format(mode))
    print("=" * 60)
    print("  Total candidates (intersection) : {}".format(len(candidates)))
    print("  Excluded (built-in + --exclude)  : {}".format(len(excluded_list)))
    print("  Migrated successfully            : {}".format(len(migrated)))
    print("  Skipped (special pattern)        : {}".format(len(skipped)))
    print("  Total driver_info symbols        : {}".format(len(all_symbols)))

    if skipped:
        print("\n  Skipped drivers (need manual handling):")
        for driver, reason in skipped:
            print("    {} — {}".format(driver, reason))

    if excluded_list:
        print("\n  Excluded drivers:")
        for driver in sorted(excluded_list):
            tag = "(built-in)" if driver in DEFAULT_EXCLUDES else "(user)"
            print("    {} {}".format(driver, tag))

    if args.verbose and migrated:
        print("\n  Migrated drivers + symbols:")
        for driver, syms in migrated:
            print("    {} -> {}".format(driver, ", ".join(syms)))

    print("\n" + "=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
