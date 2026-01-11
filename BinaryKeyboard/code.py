import board
import time
import usb_hid
from digitalio import DigitalInOut, Pull
from adafruit_debouncer import Debouncer
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keycode import Keycode
# CircuitPython may not include collections.defaultdict; use a plain dict instead


import supervisor
supervisor.runtime.autoreload = False
# disables autoreload to prevent issues with USB HID


# -------------------------
# Setup keyboard
# -------------------------
kpd = Keyboard(usb_hid.devices)

# -------------------------
# Timing / pins
# -------------------------
CLEAR_TIMEOUT = 2  # seconds
NUM_KEYS = 2
PINS = (board.GP2, board.GP3)

# Each key maps to a binary digit
KEYMAP = (
    ("0",),  # binary 0
    ("1",),  # binary 1
)

# -------------------------
# Initialize keys
# -------------------------
keys = []
for pin in PINS:
    dio = DigitalInOut(pin)
    dio.pull = Pull.UP
    keys.append(Debouncer(dio))

binary_input = ""
last_key_time = time.monotonic()
modifier_state = {}

# Mapping for modifier protocol values (128-135) to Keycode constants
MOD_MAP = {
    128: Keycode.LEFT_CONTROL,
    129: Keycode.LEFT_SHIFT,
    130: Keycode.LEFT_ALT,
    131: Keycode.LEFT_GUI,
    132: Keycode.RIGHT_CONTROL,
    133: Keycode.RIGHT_SHIFT,
    134: Keycode.RIGHT_ALT,
    135: Keycode.RIGHT_GUI,
}

# Special protocol values (0x10..0x13) mapped to non-ASCII keys
SPECIAL_MAP = {
    0x10: Keycode.RIGHT_ARROW,
    0x11: Keycode.LEFT_ARROW,
    0x12: Keycode.DOWN_ARROW,
    0x13: Keycode.UP_ARROW,
}
# Extended special map covering more control/navigation keys
PROTO_TO_KEYCODE = {
    0x10: Keycode.RIGHT_ARROW,
    0x11: Keycode.LEFT_ARROW,
    0x12: Keycode.DOWN_ARROW,
    0x13: Keycode.UP_ARROW,
    0x14: Keycode.BACKSPACE,
    0x15: Keycode.ENTER,
    0x16: Keycode.TAB,
    0x17: Keycode.ESCAPE,
    0x18: Keycode.DELETE,
    0x19: Keycode.INSERT,
    0x1A: Keycode.HOME,
    0x1B: Keycode.END,
    0x1C: Keycode.PAGE_UP,
    0x1D: Keycode.PAGE_DOWN,
}

# -------------------------
# Main loop
# -------------------------
while True:
    current_time = time.monotonic()

    # Clear timeout
    if current_time - last_key_time > CLEAR_TIMEOUT and binary_input:
        binary_input = ""

    # Scan physical keys
    for i in range(NUM_KEYS):
        keys[i].update()

        # Only append when key FALLS (pressed)
        if keys[i].fell:
            last_key_time = current_time
            binary_input += KEYMAP[i][0]

        # Optional: clear flag after release to prevent duplicates
        if keys[i].rose:
            pass  # can use this if you need to reset something

        # Only send when we have 8 bits
        if len(binary_input) == 8:
                # Convert 8-bit string to integer
                value = int(binary_input, 2)

                # Protocol: values 0-127 => ASCII char or special; 128-135 => modifier toggle
                if value < 128:
                    # Handle special non-ASCII keys first
                    if value in PROTO_TO_KEYCODE:
                        kpd.press(PROTO_TO_KEYCODE[value])
                        kpd.release(PROTO_TO_KEYCODE[value])
                        binary_input = ""
                        continue
                    try:
                        ch = chr(value)
                        # Handle simple ASCII letters/digits/space by mapping to HID usages
                        # For letters, press shift for uppercase
                        if 'a' <= ch <= 'z':
                            keycode = Keycode.A + (ord(ch) - ord('a'))
                            kpd.press(keycode)
                            kpd.release(keycode)
                        elif 'A' <= ch <= 'Z':
                            keycode = Keycode.A + (ord(ch) - ord('A'))
                            kpd.press(Keycode.LEFT_SHIFT)
                            kpd.press(keycode)
                            kpd.release(keycode)
                            kpd.release(Keycode.LEFT_SHIFT)
                        elif '0' <= ch <= '9':
                            # '1'..'9' -> usages 0x1E..0x26 ; '0' -> 0x27
                            if ch == '0':
                                keycode = 0x27
                            else:
                                keycode = 0x1E + (ord(ch) - ord('1'))
                            kpd.press(keycode)
                            kpd.release(keycode)
                        elif ch == ' ':
                            kpd.press(Keycode.SPACE)
                            kpd.release(Keycode.SPACE)
                        else:
                            # Fallback: try sending character via KeyboardLayout if available
                            try:
                                from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
                                layout = KeyboardLayoutUS(kpd)
                                layout.write(ch)
                            except Exception:
                                pass
                    except Exception:
                        pass
                else:
                    # Modifier toggle
                    if value in MOD_MAP:
                        kc = MOD_MAP[value]
                        if not modifier_state.get(value, False):
                            kpd.press(kc)
                            modifier_state[value] = True
                        else:
                            kpd.release(kc)
                            modifier_state[value] = False

                binary_input = ""  # reset for next byte
