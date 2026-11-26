"""Optional hosted-runner display preparation for the HD own-window fixture only.

No game or application code, registry writes, or implicit user display changes.
The parent wrapper restores the exact original mode even if the capture child fails.
"""
from contextlib import contextmanager
import ctypes as C
import os
import sys
import time

# Native DEVMODEW, explicit fixed-width fields, including the 16-byte union.
class DevMode(C.Structure):
    _fields_ = [
        ('device', C.c_uint16 * 32), ('spec', C.c_uint16), ('driver', C.c_uint16),
        ('size', C.c_uint16), ('extra', C.c_uint16), ('fields', C.c_uint32),
        ('display_union', C.c_uint8 * 16), ('color', C.c_int16), ('duplex', C.c_int16),
        ('y_resolution', C.c_int16), ('tt_option', C.c_int16), ('collate', C.c_int16),
        ('form', C.c_uint16 * 32), ('log_pixels', C.c_uint16),
        ('bpp', C.c_uint32), ('width', C.c_uint32), ('height', C.c_uint32),
        ('flags', C.c_uint32), ('frequency', C.c_uint32), ('icm_method', C.c_uint32),
        ('icm_intent', C.c_uint32), ('media_type', C.c_uint32), ('dither', C.c_uint32),
        ('reserved1', C.c_uint32), ('reserved2', C.c_uint32),
        ('panning_width', C.c_uint32), ('panning_height', C.c_uint32),
    ]


def signature(mode):
    return (mode.width, mode.height, mode.bpp, mode.frequency, mode.flags)


def candidates(original, modes):
    unique = {}
    for mode in modes:
        if (1280 <= mode.width <= 1920 and 720 <= mode.height <= 1080
                and mode.bpp == original.bpp and mode.flags == original.flags):
            unique.setdefault(signature(mode), mode)
    return sorted(unique.values(), key=lambda m: (m.width*m.height,
                  m.frequency != original.frequency, abs(m.frequency-original.frequency),
                  m.width, m.height, m.frequency))


def allowed(hd, environ, platform):
    return (hd and platform == 'win32' and environ.get('SSC_CI_HD_DISPLAY') == '1'
            and environ.get('GITHUB_ACTIONS') == 'true'
            and environ.get('RUNNER_ENVIRONMENT') == 'github-hosted')


class NativeDisplay:
    def __init__(self):
        if C.sizeof(DevMode) != 220 or DevMode.width.offset != 172:
            raise RuntimeError('unexpected DEVMODEW layout')
        self.api = C.WinDLL('user32', use_last_error=True)
        self.api.EnumDisplaySettingsW.argtypes = (C.c_wchar_p, C.c_uint32, C.POINTER(DevMode))
        self.api.EnumDisplaySettingsW.restype = C.c_int
        self.api.ChangeDisplaySettingsW.argtypes = (C.POINTER(DevMode), C.c_uint32)
        self.api.ChangeDisplaySettingsW.restype = C.c_int32
        self.api.GetSystemMetrics.argtypes = (C.c_int,)
        self.api.GetSystemMetrics.restype = C.c_int

    def get(self, index):
        mode = DevMode()
        mode.size = C.sizeof(DevMode)
        if not self.api.EnumDisplaySettingsW(None, index, C.byref(mode)):
            return None
        if mode.extra:
            raise RuntimeError('unexpected private display-driver data')
        return mode

    def current(self):
        mode = self.get(0xFFFFFFFF)
        if mode is None:
            raise RuntimeError('current display mode unavailable')
        return mode

    def modes(self):
        for index in range(256):
            mode = self.get(index)
            if mode is None:
                return
            yield mode

    def single_monitor(self):
        return self.api.GetSystemMetrics(80) == 1  # SM_CMONITORS

    def change(self, mode, test=False):
        mode.fields = 0x007C0000  # bpp, width, height, display flags, frequency
        return self.api.ChangeDisplaySettingsW(C.byref(mode), 2 if test else 0)


@contextmanager
def temporary_mode(api, settle=time.sleep):
    original = api.current()
    print('HDDisplayOriginal', *signature(original), flush=True)
    if original.width >= 1280 and original.height >= 720:
        yield
        return
    if not api.single_monitor():
        print('HDDisplayUnavailable multi-monitor fixture', flush=True)
        yield
        return
    attempted = False
    try:
        for mode in candidates(original, api.modes())[:8]:
            tested = api.change(mode, test=True)
            print('HDDisplayTest', *signature(mode), tested, flush=True)
            if tested != 0:
                continue
            attempted = True
            result = api.change(mode)
            print('HDDisplayChange', *signature(mode), result, flush=True)
            settle(1.0)
            current = api.current()
            print('HDDisplayCurrent', *signature(current), flush=True)
            if result == 0 and signature(current) == signature(mode):
                break
            # A failed dynamic attempt must not leave an unverified mode active.
            if signature(current) != signature(original):
                raise RuntimeError('display changed unexpectedly')
        else:
            print('HDDisplayUnavailable no verified compatible mode; running unchanged diagnostic', flush=True)
        yield
    finally:
        if attempted:
            result = api.change(original)
            settle(1.0)
            current = api.current()
            print('HDDisplayRestore', *signature(current), result, flush=True)
            if result != 0 or signature(current) != signature(original):
                raise RuntimeError('original display mode restoration failed')


@contextmanager
def hosted_hd_mode(hd):
    if not allowed(hd, os.environ, sys.platform):
        yield
        return
    with temporary_mode(NativeDisplay()):
        yield
