import board
import time
import usb_hid
from digitalio import DigitalInOut, Pull
from adafruit_debouncer import Debouncer
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keycode import Keycode

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
CLEAR_TIMEOUT = 2.0  # seconds
# Start symbol is now: key0 then key1, about 40ms apart
# We detect it as: key0 followed by key1 within 50ms
START_SYMBOL_TIMEOUT = 0.050  # 50ms window for start symbol sequence
BIT_TIMEOUT = 0.050  # 50ms - max time between bits (pulse+gap = 40ms)
NUM_KEYS = 2
PINS = (board.GP2, board.GP3)

# Each key maps to a binary digit
KEYMAP = ("0", "1")

# -------------------------
# Initialize keys
# -------------------------
keys = []
for pin in PINS:
    dio = DigitalInOut(pin)
    dio.pull = Pull.UP
    keys.append(Debouncer(dio))

last_key_time = time.monotonic()

# -------------------------
# State machine
# -------------------------
STATE_WAIT_START_0 = 0  # Waiting for key0 (first part of start symbol)
STATE_WAIT_START_1 = 1  # Got key0, waiting for key1 to complete start symbol
STATE_RECEIVING = 2     # Receiving data bits

state = STATE_WAIT_START_0
state_enter_time = 0.0
bit_buffer = ""

# -------------------------
# Mappings
# -------------------------
PROTO_CTRL_A = 0x40
# Build combo map for Ctrl+a..z
COMBO_MAP = {}
for i in range(26):
    COMBO_MAP[PROTO_CTRL_A + i] = Keycode.A + i

# -------------------------
# Modifier mapping
# -------------------------
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

# -------------------------
# Special keys / navigation
# -------------------------
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
    0x02: Keycode.F1,
    0x03: Keycode.F2,
    0x04: Keycode.F3,
    0x05: Keycode.F4,
    0x06: Keycode.F5,
    0x07: Keycode.F6,
    0x08: Keycode.F7,
    0x09: Keycode.F8,
    0x0A: Keycode.F9,
    0x0B: Keycode.F10,
    0x0C: Keycode.F11,
    0x0D: Keycode.F12,
}

debug_press_count = 0

def process_byte(value):
    """Process a complete received byte"""
    print("COMPLETE BYTE: 0x{:02X} ({})".format(value, value))
    
    if value in COMBO_MAP:
        kc = COMBO_MAP[value]
        print("CTRL+{} combo".format(chr(ord('a') + value - PROTO_CTRL_A)))
        kpd.press(Keycode.LEFT_CONTROL)
        kpd.press(kc)
        kpd.release(kc)
        kpd.release(Keycode.LEFT_CONTROL)
        return

    if value in MOD_MAP:
        kc = MOD_MAP[value]
        print("modifier -> {}".format(kc))
        kpd.press(kc)
        kpd.release(kc)
        return

    if value < 128:
        if 0x20 <= value <= 0x7E:
            ch = chr(value)
            print("ASCII: '{}'".format(ch))
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
                if ch == '0':
                    keycode = Keycode.ZERO
                else:
                    keycode = Keycode.ONE + (ord(ch) - ord('1'))
                kpd.press(keycode)
                kpd.release(keycode)
            elif ch == ' ':
                kpd.press(Keycode.SPACE)
                kpd.release(Keycode.SPACE)
            else:
                try:
                    from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
                    layout = KeyboardLayoutUS(kpd)
                    layout.write(ch)
                except:
                    pass
        elif value in PROTO_TO_KEYCODE:
            print("proto 0x{:02X}".format(value))
            mapped = PROTO_TO_KEYCODE[value]
            kpd.press(mapped)
            kpd.release(mapped)
        else:
            print("UNKNOWN: 0x{:02X}".format(value))

# -------------------------
# Main loop
# -------------------------
while True:
    current_time = time.monotonic()

    # Handle timeouts based on state
    if state == STATE_WAIT_START_1:
        # Waiting for key1 to complete start symbol
        if current_time - state_enter_time > START_SYMBOL_TIMEOUT:
            print("START SYMBOL TIMEOUT - back to waiting")
            state = STATE_WAIT_START_0
    
    elif state == STATE_RECEIVING:
        # Receiving bits - timeout means desync
        if current_time - last_key_time > CLEAR_TIMEOUT:
            if bit_buffer:
                print("RECEIVE TIMEOUT: clearing buffer='{}' len={}".format(bit_buffer, len(bit_buffer)))
            bit_buffer = ""
            state = STATE_WAIT_START_0

    # Scan physical keys
    for i in range(NUM_KEYS):
        keys[i].update()
        
        if keys[i].fell:
            debug_press_count += 1
            last_key_time = current_time
            
            print("PRESS key={} state={} time={:.4f} (press#{})".format(
                i, state, current_time, debug_press_count))
            
            if state == STATE_WAIT_START_0:
                # Waiting for first part of start symbol (key0)
                if i == 0:
                    state = STATE_WAIT_START_1
                    state_enter_time = current_time
                    print("  -> Got key0, waiting for key1")
                else:
                    print("  -> Ignoring key1, need key0 first")
            
            elif state == STATE_WAIT_START_1:
                # Waiting for second part of start symbol (key1)
                if i == 1:
                    # START SYMBOL COMPLETE!
                    state = STATE_RECEIVING
                    bit_buffer = ""
                    print(">>> START SYMBOL COMPLETE - ready to receive <<<")
                elif i == 0:
                    # Another key0 - restart
                    state_enter_time = current_time
                    print("  -> Another key0, restarting wait for key1")
            
            elif state == STATE_RECEIVING:
                # Receiving data bits
                bit_buffer += KEYMAP[i]
                print("  -> BIT: {} -> buffer='{}' len={}".format(
                    KEYMAP[i], bit_buffer, len(bit_buffer)))
                
                # Check if we have a complete byte
                if len(bit_buffer) == 8:
                    value = int(bit_buffer, 2)
                    bit_buffer = ""
                    state = STATE_WAIT_START_0
                    process_byte(value)