import supervisor
supervisor.runtime.autoreload = False
# disables autoreload to prevent issues with USB HID

import board
import time
import usb_hid
from digitalio import DigitalInOut, Pull
from adafruit_debouncer import Debouncer
from adafruit_hid.keyboard import Keyboard

kpd = Keyboard(usb_hid.devices)

CLEAR_TIMEOUT = 2

PINS = (board.GP2, board.GP3)
KEYMAP = ("0", "1")

keys = []
for pin in PINS:
    d = DigitalInOut(pin)
    d.pull = Pull.UP
    keys.append(Debouncer(d))

binary_input = ""
last_key_time = time.monotonic()

# Modifier range
MOD_MIN = 0xE0
MOD_MAX = 0xE7

while True:
    now = time.monotonic()

    if binary_input and now - last_key_time > CLEAR_TIMEOUT:
        binary_input = ""
        kpd.release_all()

    for i in range(2):
        keys[i].update()
        if keys[i].fell:
            last_key_time = now
            binary_input += KEYMAP[i]

            if len(binary_input) == 8:
                value = int(binary_input, 2)

                if MOD_MIN <= value <= MOD_MAX:
                    # Modifier press (latched)
                    kpd.press(value)
                elif value != 0:
                    # Normal key
                    kpd.press(value)
                    kpd.release(value)

                binary_input = ""
