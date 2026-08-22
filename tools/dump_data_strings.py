"""Dump null-terminated strings from Shaped's recovered DGROUP image."""

import sys
from pathlib import Path


binary = Path(sys.argv[1] if len(sys.argv) > 1 else "original/SHAPED.EXE").read_bytes()
output = Path(sys.argv[2] if len(sys.argv) > 2 else "reverse/data-strings-map.txt")

# Derived by correlating _ReadM3D's DS:1c50 reference with "Is M3D file".
dgroup_file_offset = 0x2F1A0
end = 0x35F8E
lines = []
offset = 0
while dgroup_file_offset + offset < end:
    start = dgroup_file_offset + offset
    stop = binary.find(b"\x00", start, end)
    if stop < 0:
        break
    value = binary[start:stop]
    if len(value) >= 2 and all(byte in b"\r\n\t" or 0x20 <= byte <= 0x7E for byte in value):
        lines.append(f"{offset:04x} {value.decode('ascii')!r}")
    offset = stop - dgroup_file_offset + 1

output.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"wrote {len(lines)} strings to {output}")
