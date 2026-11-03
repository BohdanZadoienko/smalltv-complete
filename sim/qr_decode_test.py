"""Decode the firmware's 240x240 screenshots with an independent QR reader.

Test-only dependencies: Pillow and zxing-cpp. Run after sim/build.sh.
"""
from array import array
from pathlib import Path
from PIL import Image
import zxingcpp

out = Path(__file__).resolve().parent / 'out'
cases = {}
for lang in range(3):
    for recovery in (False, True):
        mode = 'recovery' if recovery else 'wifi'
        ssid = 'SmallTV-Recuperacao' if recovery else 'SmallTV-Pomodoro'
        for connected in (False, True):
            step = 'portal' if connected else 'connect'
            cases[f'setup_{mode}_{step}_{lang}'] = (
                'http://192.168.4.1/' if connected else f'WIFI:T:WPA;S:{ssid};P:AbCd2345;;')
cases['setup_escaped'] = 'WIFI:T:WPA;S:Small\\;TV\\:1;P:ab\\\\cd\\,\\"12;;'

for name, expected in cases.items():
    raw = array('H', (out / f'{name}.raw').read_bytes())
    rgb = bytes(c for v in raw for c in (
        ((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31))
    result = zxingcpp.read_barcode(Image.frombytes('RGB', (240, 240), rgb))
    assert result and result.text == expected, f'{name}: QR missing or decoded to different credentials'
print(f'QR: {len(cases)} firmware screenshots decoded correctly at native 240x240 resolution')
