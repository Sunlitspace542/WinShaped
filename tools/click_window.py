"""Send a real left click to client coordinates of a native window."""

import ctypes
import sys
import time
from ctypes import wintypes


user32 = ctypes.windll.user32
try:
    user32.SetProcessDPIAware()
except AttributeError:
    pass

hwnd = int(sys.argv[1], 0)
point = wintypes.POINT(int(sys.argv[2]), int(sys.argv[3]))
if not user32.ClientToScreen(hwnd, ctypes.byref(point)):
    raise ctypes.WinError()
user32.SetForegroundWindow(hwnd)
user32.SetCursorPos(point.x, point.y)
time.sleep(0.1)
user32.mouse_event(0x0002, 0, 0, 0, 0)
time.sleep(0.12)
user32.mouse_event(0x0004, 0, 0, 0, 0)
