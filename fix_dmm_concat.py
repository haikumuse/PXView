#!/usr/bin/env python3
"""Fix-up pass: (1) split lines where 'unit_pitch = 0;' was concatenated with
the following statement (script consumed the trailing newline); (2) silence
the now-unused 'digits' local in fluke-45."""
import re
import os

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

# Split "unit_pitch = 0;<ws><content>" into two lines (only when no newline
# separates them -- i.e. the concatenation case).
CONCAT_PAT = re.compile(r'(unit_pitch = 0;)([ \t]+)(\S)')


def main():
    for rel in FILES:
        path = os.path.join(BASE, rel)
        with open(path, 'r', encoding='utf-8', newline='') as f:
            text = f.read()
        orig = text
        text = CONCAT_PAT.sub(r'\1\n\2\3', text)

        # fluke-45: 'digits' is set by get_reading_dd() but no longer read
        # after the encoding->digits assignment was removed. Silence it.
        if rel == "fluke-45/protocol.c":
            text = text.replace(
                "digits = get_reading_dd(reading, strlen(reading));",
                "digits = get_reading_dd(reading, strlen(reading));\n"
                "\t\t\t\t(void)digits; /* PXView flat analog has no digits field */")

        if text != orig:
            with open(path, 'w', encoding='utf-8', newline='') as f:
                f.write(text)
            print("FIXED ", rel)
        else:
            print("ok    ", rel)


if __name__ == "__main__":
    main()
