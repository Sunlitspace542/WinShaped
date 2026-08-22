"""Capture a native Windows window by HWND, even when it is occluded."""

import ctypes
import sys
from ctypes import wintypes

from PIL import Image


user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
try:
    user32.SetProcessDPIAware()
except AttributeError:
    pass


class BitmapInfoHeader(ctypes.Structure):
    _fields_ = [
        ("biSize", wintypes.DWORD),
        ("biWidth", wintypes.LONG),
        ("biHeight", wintypes.LONG),
        ("biPlanes", wintypes.WORD),
        ("biBitCount", wintypes.WORD),
        ("biCompression", wintypes.DWORD),
        ("biSizeImage", wintypes.DWORD),
        ("biXPelsPerMeter", wintypes.LONG),
        ("biYPelsPerMeter", wintypes.LONG),
        ("biClrUsed", wintypes.DWORD),
        ("biClrImportant", wintypes.DWORD),
    ]


class BitmapInfo(ctypes.Structure):
    _fields_ = [("bmiHeader", BitmapInfoHeader), ("bmiColors", wintypes.DWORD * 3)]

hwnd = int(sys.argv[1], 0)
output = sys.argv[2]

rect = wintypes.RECT()
if not user32.GetClientRect(hwnd, ctypes.byref(rect)):
    raise ctypes.WinError()
width = rect.right - rect.left
height = rect.bottom - rect.top

window_dc = user32.GetDC(hwnd)
memory_dc = gdi32.CreateCompatibleDC(window_dc)
bitmap = gdi32.CreateCompatibleBitmap(window_dc, width, height)
old_bitmap = gdi32.SelectObject(memory_dc, bitmap)

try:
    if not user32.PrintWindow(hwnd, memory_dc, 3):
        raise ctypes.WinError()

    info = BitmapInfo()
    info.bmiHeader.biSize = ctypes.sizeof(BitmapInfoHeader)
    info.bmiHeader.biWidth = width
    info.bmiHeader.biHeight = -height
    info.bmiHeader.biPlanes = 1
    info.bmiHeader.biBitCount = 32
    info.bmiHeader.biCompression = 0
    pixels = ctypes.create_string_buffer(width * height * 4)
    if not gdi32.GetDIBits(memory_dc, bitmap, 0, height, pixels, ctypes.byref(info), 0):
        raise ctypes.WinError()
    Image.frombuffer("RGB", (width, height), pixels, "raw", "BGRX", 0, 1).save(output)
finally:
    gdi32.SelectObject(memory_dc, old_bitmap)
    gdi32.DeleteObject(bitmap)
    gdi32.DeleteDC(memory_dc)
    user32.ReleaseDC(hwnd, window_dc)
