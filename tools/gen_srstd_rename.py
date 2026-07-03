#!/usr/bin/env python3
# DEPRECATED: superseded by dynamic library approach in convert-libsigrokstd-to-shared-library spec
"""
gen_srstd_rename.py — Generate srstd_rename.h for the libsigrokstd library.

Scans the upstream libsigrok public and internal headers, extracts all sr_*
identifiers (functions, struct tags, enum tags, typedefs), and produces a
macro rename header that maps every sr_xxx -> srstd_xxx.

Additionally scans the libsigrokstd/src/ source tree for SR_PRIV function
and variable definitions whose names do NOT start with sr_ (e.g. serial_*,
std_*, scpi_*, modbus_*, input_*, output_*, ezusb_*, soft_trigger_*,
usb_*, sipeed_*, transform_*). These internal symbols are also renamed
srstd_* to prevent linker-level symbol collisions with PXView's compat
layer (compat_serial.c/compat_helpers.c/compat_scpi.c define the same
non-prefixed names). On Windows/MinGW, SR_PRIV expands to nothing (PE/COFF
has no visibility attribute), so without renaming these symbols become
global exports that conflict with PXView's definitions.

This header is force-injected into every translation unit via the compiler
flag  -include srstd_rename.h  so that the upstream source files remain
unmodified while all public + internal symbols are physically renamed at
preprocessing time, achieving linker-level symbol isolation from PXView's
own libsigrok.

Usage:
    python tools/gen_srstd_rename.py

The script is idempotent: repeated runs produce identical output.
"""

import os
import re
import sys

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Root of the upstream libsigrok sources (read-only reference).
UPSTREAM_ROOT = r"c:\Users\admin\Downloads\libsigrok"

# Header files to scan for sr_* symbols.
HEADER_FILES = [
    os.path.join(UPSTREAM_ROOT, "include", "libsigrok", "libsigrok.h"),
    os.path.join(UPSTREAM_ROOT, "include", "libsigrok", "proto.h"),
    os.path.join(UPSTREAM_ROOT, "src", "libsigrok-internal.h"),
    os.path.join(UPSTREAM_ROOT, "src", "scpi.h"),
    os.path.join(UPSTREAM_ROOT, "src", "serial_hid.h"),
]

# Local libsigrokstd source tree (the actual files compiled into the static
# library). Scanned for SR_PRIV non-sr_* symbols to rename.
SRSTD_SOURCE_ROOT = os.path.join(
    r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb",
    "libsigrokstd", "src",
)

# Output file.
OUTPUT_PATH = os.path.join(
    r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb",
    "libsigrokstd", "include", "srstd_rename.h",
)

# ---------------------------------------------------------------------------
# Symbol extraction
# ---------------------------------------------------------------------------

def read_file(path):
    """Read a file and return its content, handling missing files gracefully."""
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        print(f"WARNING: file not found: {path}", file=sys.stderr)
        return ""


def extract_functions(text):
    """
    Extract function names from SR_API / SR_PRIV declarations.

    Matches patterns like:
        SR_API  int  sr_init(...)
        SR_PRIV struct sr_channel *sr_channel_new(...)
        SR_API  const char *sr_dev_inst_vendor_get(...)

    Returns a sorted list of function names (with the sr_ prefix).
    """
    names = set()
    # Match SR_API or SR_PRIV at start of line (with optional whitespace),
    # followed by anything, then capture sr_word(  as the function name.
    # We use a greedy approach: find 'sr_' followed by word chars followed by '('
    # on lines that start with SR_API or SR_PRIV.
    pattern = re.compile(
        r'^\s*(?:SR_API|SR_PRIV)\b.*?\b(sr_\w+)\s*\(',
        re.MULTILINE | re.DOTALL,
    )
    for m in pattern.finditer(text):
        names.add(m.group(1))
    return names


def extract_struct_tags(text):
    """
    Extract struct tag names from:
        struct sr_xxx {       (full definition)
        struct sr_xxx;        (forward declaration)

    Returns a sorted set of tag names (with the sr_ prefix).
    """
    names = set()
    # Match 'struct sr_word' followed by '{' or ';'
    pattern = re.compile(r'\bstruct\s+(sr_\w+)\s*[{;]')
    for m in pattern.finditer(text):
        names.add(m.group(1))
    return names


def extract_enum_tags(text):
    """
    Extract enum tag names from:
        enum sr_xxx {         (full definition)
        enum sr_xxx;          (forward declaration, rare)

    Returns a sorted set of tag names (with the sr_ prefix).
    """
    names = set()
    pattern = re.compile(r'\benum\s+(sr_\w+)\s*[{;]')
    for m in pattern.finditer(text):
        names.add(m.group(1))
    return names


def extract_typedefs(text):
    """
    Extract typedef names that start with sr_.

    All typedefs in libsigrok are function-pointer typedefs of the form:
        typedef int (*sr_callback)(...)
        typedef void (*sr_log_callback)(...)

    Returns a sorted set of typedef names (with the sr_ prefix).
    """
    names = set()
    # Function-pointer typedef:  typedef ... (*sr_name)(...
    # Use a non-greedy match that stays within a reasonable range to avoid
    # crossing multiple unrelated declarations.
    pattern_fp = re.compile(r'typedef\s+[^(]*?\(\*\s*(sr_\w+)\s*\)\s*\(')
    for m in pattern_fp.finditer(text):
        names.add(m.group(1))
    return names


# --- Internal SR_PRIV non-sr_* symbol extraction (Task 8 symbol isolation) ---

# C type keywords that may appear between SR_PRIV and the identifier.
# These are filtered out so we never mistake a type keyword for the
# declared function/variable name.
_TYPE_KEYWORDS = {
    'int', 'char', 'void', 'unsigned', 'static', 'const', 'struct', 'enum',
    'union', 'size_t', 'uint64_t', 'uint32_t', 'uint16_t', 'uint8_t',
    'gboolean', 'gint', 'guint', 'gpointer', 'gconstpointer', 'gsize',
    'gssize', 'gulong', 'glong', 'gint64', 'guint64', 'float', 'double',
    'long', 'short', 'signed', 'volatile', 'auto', 'register', 'extern',
    'GArray', 'GSList', 'GVariant', 'gchar', 'gdouble',
    'gint32', 'guint32', 'gint16', 'guint16', 'gint8', 'guint8',
    'gint64', 'guint64',
}


def _extract_internal_symbols_from_text(text):
    """
    Extract file-scope SR_PRIV function/variable names that do NOT start
    with sr_. These are the internal symbols that conflict with PXView's
    compat layer on Windows (where SR_PRIV is empty, making them global).

    Strategy: scan line by line. When a line starts with SR_PRIV (after
    optional whitespace), collect text from that point forward across
    continuation lines until we find the first identifier immediately
    followed by ( ; = [ or { — that identifier is the declared name.

    Returns a set of symbol names (without the sr_ prefix, since these
    don't start with sr_).
    """
    names = set()
    lines = text.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r'^\s*SR_PRIV\b', line)
        if not m:
            i += 1
            continue

        # Collect text from after 'SR_PRIV' onward, joining continuation
        # lines until we find the declared name.
        chunk = line[m.end():]
        j = i
        found = False
        while j < len(lines) and j - i < 12:
            # Look for: identifier followed by ( ; = [ {
            em = re.search(r'\b(\w+)\s*[\(\;\=\[\{]', chunk)
            if em:
                name = em.group(1)
                if name not in _TYPE_KEYWORDS and not name.startswith('sr_'):
                    names.add(name)
                found = True
                break
            j += 1
            if j < len(lines):
                chunk += '\n' + lines[j]

        i += 1
    return names


def extract_internal_symbols(source_root):
    """
    Walk the libsigrokstd source tree and extract all SR_PRIV non-sr_*
    function/variable names from .c and .h files.

    Returns a sorted set of symbol names.
    """
    names = set()
    for root, dirs, files in os.walk(source_root):
        # Skip subdirectories that are not compiled (e.g. minilzo test files).
        for fname in files:
            if not (fname.endswith('.c') or fname.endswith('.h')):
                continue
            fpath = os.path.join(root, fname)
            text = read_file(fpath)
            if not text:
                continue
            names |= _extract_internal_symbols_from_text(text)
    return names


# ---------------------------------------------------------------------------
# Header generation
# ---------------------------------------------------------------------------

def generate_header(functions, structs, enums, typedefs, internal_symbols):
    """Generate the content of srstd_rename.h."""
    lines = []
    lines.append("#ifndef SRSTD_RENAME_H_")
    lines.append("#define SRSTD_RENAME_H_")
    lines.append("")
    lines.append("/*")
    lines.append(" * srstd_rename.h - Auto-generated symbol rename header.")
    lines.append(" *")
    lines.append(" * Forces all sr_ public symbols from the upstream libsigrok")
    lines.append(" * to be renamed srstd_ at preprocessing time, achieving")
    lines.append(" * physical symbol isolation from PXView's own libsigrok fork.")
    lines.append(" *")
    lines.append(" * Additionally renames non-sr_ SR_PRIV internal symbols")
    lines.append(" * (serial_, std_, scpi_, modbus_, input_, output_, ezusb_,")
    lines.append(" * soft_trigger_, usb_, sipeed_, transform_) to srstd_ so")
    lines.append(" * they don't collide with PXView's compat-layer definitions")
    lines.append(" * (Windows/MinGW has no visibility attribute, so SR_PRIV")
    lines.append(" * symbols are global exports by default).")
    lines.append(" *")
    lines.append(" * DO NOT EDIT - regenerate with: python tools/gen_srstd_rename.py")
    lines.append(" */")
    lines.append("")

    # --- Functions ---
    lines.append("/* ===== SR_API / SR_PRIV functions: sr_xxx -> srstd_xxx ===== */")
    for name in sorted(functions):
        lines.append(f"#define {name} srstd_{name[3:]}")
    lines.append("")

    # --- Struct tags ---
    lines.append("/* ===== struct tags: struct sr_xxx -> struct srstd_xxx ===== */")
    for name in sorted(structs):
        lines.append(f"#define {name} srstd_{name[3:]}")
    lines.append("")

    # --- Enum tags ---
    lines.append("/* ===== enum tags: enum sr_xxx -> enum srstd_xxx ===== */")
    for name in sorted(enums):
        lines.append(f"#define {name} srstd_{name[3:]}")
    lines.append("")

    # --- Typedefs ---
    lines.append("/* ===== typedef names: sr_xxx -> srstd_xxx ===== */")
    for name in sorted(typedefs):
        lines.append(f"#define {name} srstd_{name[3:]}")
    lines.append("")

    # --- Internal SR_PRIV non-sr_* symbols (Task 8 symbol isolation) ---
    lines.append("/* ===== Internal SR_PRIV non-sr_ symbols: xxx -> srstd_xxx ============")
    lines.append(" * These are file-scope SR_PRIV functions/variables in the upstream")
    lines.append(" * sources whose names don't start with sr_. On Windows/MinGW, SR_PRIV")
    lines.append(" * is empty (no visibility hidden), so they'd be global exports that")
    lines.append(" * conflict with PXView's compat layer. Renaming them to srstd_ at")
    lines.append(" * preprocessing time isolates them from PXView's definitions. */")
    for name in sorted(internal_symbols):
        lines.append(f"#define {name} srstd_{name}")
    lines.append("")

    lines.append("#endif /* SRSTD_RENAME_H_ */")
    lines.append("")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    # Read and concatenate all header files.
    all_text = ""
    for path in HEADER_FILES:
        content = read_file(path)
        if content:
            all_text += "\n" + content
            print(f"  scanned: {os.path.basename(path)} ({len(content)} bytes)")

    # Extract symbols.
    functions = extract_functions(all_text)
    structs = extract_struct_tags(all_text)
    enums = extract_enum_tags(all_text)
    typedefs = extract_typedefs(all_text)

    # Extract internal SR_PRIV non-sr_* symbols from the libsigrokstd source tree.
    internal_symbols = extract_internal_symbols(SRSTD_SOURCE_ROOT)
    print(f"  scanned: libsigrokstd/src/ (internal SR_PRIV non-sr_* symbols)")

    # Generate the header.
    header_content = generate_header(functions, structs, enums, typedefs,
                                     internal_symbols)

    # Write the output file.
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as f:
        f.write(header_content)

    # Print statistics.
    print()
    print("=" * 60)
    print("srstd_rename.h generation complete")
    print("=" * 60)
    print(f"  Output:           {OUTPUT_PATH}")
    print(f"  Functions renamed:  {len(functions)}")
    print(f"  Struct tags renamed:{len(structs)}")
    print(f"  Enum tags renamed:  {len(enums)}")
    print(f"  Typedefs renamed:   {len(typedefs)}")
    print(f"  Internal SR_PRIV renamed: {len(internal_symbols)}")
    print(f"  Total macros:       {len(functions) + len(structs) + len(enums) + len(typedefs) + len(internal_symbols)}")
    print()

    # Print a few samples for verification.
    if functions:
        print("  Sample functions:")
        for name in sorted(functions)[:5]:
            print(f"    {name} -> srstd_{name[3:]}")
        if len(functions) > 5:
            print(f"    ... and {len(functions) - 5} more")
    if internal_symbols:
        print("  Sample internal SR_PRIV symbols:")
        for name in sorted(internal_symbols)[:5]:
            print(f"    {name} -> srstd_{name}")
        if len(internal_symbols) > 5:
            print(f"    ... and {len(internal_symbols) - 5} more")


if __name__ == "__main__":
    main()
