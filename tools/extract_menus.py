"""Decode Shaped's statically initialized menu and button tables."""

import csv
import json
import re
import struct
from pathlib import Path


binary = Path("original/SHAPED.EXE").read_bytes()
dgroup = 0x2F1A0
symbols = {}
for line in Path("reverse/SHAPED.sym").read_text(encoding="utf-8").splitlines():
    match = re.match(r"(\S+)\s+0x[0-9a-fA-F]+:0x([0-9a-fA-F]+)$", line.strip())
    if match:
        symbols[match.group(1)] = int(match.group(2), 16)

publics = {}
with Path("reverse/publics.csv").open(newline="", encoding="utf-8") as stream:
    for row in csv.DictReader(stream):
        publics.setdefault((int(row["segment"], 16), int(row["offset"], 16)), row["name"])


def cstring(offset):
    start = dgroup + offset
    stop = binary.find(b"\x00", start)
    return binary[start:stop].decode("ascii", "replace")


result = {}
for button_name, button_offset in symbols.items():
    if "MenuButs" not in button_name:
        continue
    menu_name = button_name.replace("MenuButs", "Menu")
    menu_offset = symbols.get(menu_name)
    if menu_offset is None:
        continue
    buttons_far, buttons_segment, count, menu_flags, menu_state = struct.unpack_from("<5H", binary, dgroup + menu_offset)
    if buttons_far != button_offset or count > 64:
        continue
    buttons = []
    for index in range(count):
        key, label_offset, label_segment, fn_offset, fn_segment, flags, submenu_offset, submenu_segment = struct.unpack_from(
            "<8H", binary, dgroup + button_offset + index * 16
        )
        buttons.append(
            {
                "key": chr(key) if 0x20 <= key < 0x7F else key,
                "label": cstring(label_offset) if label_segment else "",
                "callback": publics.get((fn_segment, fn_offset), f"{fn_segment:04x}:{fn_offset:04x}") if fn_segment else None,
                "flags": flags,
                "submenu": f"{submenu_segment:04x}:{submenu_offset:04x}" if submenu_segment else None,
            }
        )
    result[menu_name] = {"offset": f"{menu_offset:04x}", "flags": menu_flags, "state": menu_state, "buttons": buttons}

target = Path("reverse/menus.json")
target.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(f"wrote {len(result)} menus and {sum(len(menu['buttons']) for menu in result.values())} buttons to {target}")
