import time
import board
import digitalio

import usb_host
from adafruit_usb_host.keyboard import Keyboard
from adafruit_usb_host.keycode import Keycode
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS

# -------------------------
# Solenoid setup
# -------------------------
sol0 = digitalio.DigitalInOut(board.GP2)    # Binary 0
sol1 = digitalio.DigitalInOut(board.GP3)    # Binary 1

sol0.direction = digitalio.Direction.OUTPUT
sol1.direction = digitalio.Direction.OUTPUT

sol0.value = False
sol1.value = False

PULSE_MS = 0.035
GAP_MS   = 0.035

def pulse(sol):
    sol.value = True
    time.sleep(PULSE_MS)
    sol.value = False
    time.sleep(GAP_MS)

def send_byte(byte):
    # MSB first (matches your binary keyboard)
    for i in range(7, -1, -1):
        bit = (byte >> i) & 1
        pulse(sol1 if bit else sol0)

# -------------------------
# USB Host init
# -------------------------
host = usb_host.Port(board.GP0, board.GP1)  # D+ D-
kbd = Keyboard(host)
layout = KeyboardLayoutUS(kbd)

print("Waiting for USB keyboard...")

# -------------------------
# Main loop
# -------------------------
while True:
    if not kbd.connected:
        time.sleep(0.1)
        continue

    event = kbd.events.get()
    if not event:
        continue

    if event.pressed:
        keycode = event.key_number

        # Only handle printable keys
        try:
            char = layout.keycodes_to_string([keycode])
        except ValueError:
            char = None

        if char and len(char) == 1:
            print("Sending:", char)
            send_byte(ord(char))
