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
# Protocol mappings
# -------------------------

# Modifier PRESS (0x80-0x87)
MOD_PRESS_MAP = {
    0x80: Keycode.LEFT_CONTROL,
    0x81: Keycode.LEFT_SHIFT,
    0x82: Keycode.LEFT_ALT,
    0x83: Keycode.LEFT_GUI,
    0x84: Keycode.RIGHT_CONTROL,
    0x85: Keycode.RIGHT_SHIFT,
    0x86: Keycode.RIGHT_ALT,
    0x87: Keycode.RIGHT_GUI,
}

# Modifier RELEASE (0x88-0x8F)
MOD_RELEASE_MAP = {
    0x88: Keycode.LEFT_CONTROL,
    0x89: Keycode.LEFT_SHIFT,
    0x8A: Keycode.LEFT_ALT,
    0x8B: Keycode.LEFT_GUI,
    0x8C: Keycode.RIGHT_CONTROL,
    0x8D: Keycode.RIGHT_SHIFT,
    0x8E: Keycode.RIGHT_ALT,
    0x8F: Keycode.RIGHT_GUI,
}

# Navigation keys (0x90-0x9D)
NAV_MAP = {
    0x90: Keycode.RIGHT_ARROW,
    0x91: Keycode.LEFT_ARROW,
    0x92: Keycode.DOWN_ARROW,
    0x93: Keycode.UP_ARROW,
    0x94: Keycode.BACKSPACE,
    0x95: Keycode.ENTER,
    0x96: Keycode.TAB,
    0x97: Keycode.ESCAPE,
    0x98: Keycode.DELETE,
    0x99: Keycode.INSERT,
    0x9A: Keycode.HOME,
    0x9B: Keycode.END,
    0x9C: Keycode.PAGE_UP,
    0x9D: Keycode.PAGE_DOWN,
}

PROTO_CLEAR_BUFFER = 0x9E

# Function keys (0xA0-0xAB)
FUNC_MAP = {
    0xA0: Keycode.F1,
    0xA1: Keycode.F2,
    0xA2: Keycode.F3,
    0xA3: Keycode.F4,
    0xA4: Keycode.F5,
    0xA5: Keycode.F6,
    0xA6: Keycode.F7,
    0xA7: Keycode.F8,
    0xA8: Keycode.F9,
    0xA9: Keycode.F10,
    0xAA: Keycode.F11,
    0xAB: Keycode.F12,
}

# Other special keys
PROTO_CAPS_LOCK = 0xB0
PROTO_FN_PRESS = 0xB1
PROTO_FN_RELEASE = 0xB2

def ascii_to_keypress(ch):
    """Convert ASCII char to (keycode, needs_shift)."""
    c = ord(ch)
    
    # Lowercase a-z
    if ord('a') <= c <= ord('z'):
        return (Keycode.A + (c - ord('a')), False)
    
    # Uppercase A-Z
    if ord('A') <= c <= ord('Z'):
        return (Keycode.A + (c - ord('A')), True)
    
    # Numbers 0-9
    if c == ord('0'):
        return (Keycode.ZERO, False)
    if ord('1') <= c <= ord('9'):
        return (Keycode.ONE + (c - ord('1')), False)
    
    # Shift+number symbols
    shift_num = {
        '!': Keycode.ONE, '@': Keycode.TWO, '#': Keycode.THREE,
        '$': Keycode.FOUR, '%': Keycode.FIVE, '^': Keycode.SIX,
        '&': Keycode.SEVEN, '*': Keycode.EIGHT, '(': Keycode.NINE,
        ')': Keycode.ZERO,
    }
    if ch in shift_num:
        return (shift_num[ch], True)
    
    # Other punctuation
    other = {
        ' ': (Keycode.SPACE, False),
        '-': (Keycode.MINUS, False),
        '_': (Keycode.MINUS, True),
        '=': (Keycode.EQUALS, False),
        '+': (Keycode.EQUALS, True),
        '[': (Keycode.LEFT_BRACKET, False),
        '{': (Keycode.LEFT_BRACKET, True),
        ']': (Keycode.RIGHT_BRACKET, False),
        '}': (Keycode.RIGHT_BRACKET, True),
        '\\': (Keycode.BACKSLASH, False),
        '|': (Keycode.BACKSLASH, True),
        ';': (Keycode.SEMICOLON, False),
        ':': (Keycode.SEMICOLON, True),
        "'": (Keycode.QUOTE, False),
        '"': (Keycode.QUOTE, True),
        '`': (Keycode.GRAVE_ACCENT, False),
        '~': (Keycode.GRAVE_ACCENT, True),
        ',': (Keycode.COMMA, False),
        '<': (Keycode.COMMA, True),
        '.': (Keycode.PERIOD, False),
        '>': (Keycode.PERIOD, True),
        '/': (Keycode.FORWARD_SLASH, False),
        '?': (Keycode.FORWARD_SLASH, True),
    }
    if ch in other:
        return other[ch]
    
    return (None, False)

def emergency_clear():
    """Emergency clear - release all keys and reset state."""
    global state, bit_buffer
    print("!!! EMERGENCY CLEAR !!!")
    kpd.release_all()
    state = STATE_WAIT_START_0
    bit_buffer = ""

# Track Fn key state
fn_pressed = False

def process_byte(value):
    """Process a complete received byte"""
    global fn_pressed
    
    print("BYTE: 0x{:02X}".format(value))
    
    # Check for Fn key press/release
    if value == PROTO_FN_PRESS:
        fn_pressed = True
        print("  FN PRESS")
        return
    elif value == PROTO_FN_RELEASE:
        fn_pressed = False
        print("  FN RELEASE")
        return
    
    # Emergency clear command
    if value == PROTO_CLEAR_BUFFER:
        emergency_clear()
        return
    
    # Modifier PRESS
    if value in MOD_PRESS_MAP:
        kc = MOD_PRESS_MAP[value]
        print("  MOD PRESS")
        kpd.press(kc)
        return
    
    # Modifier RELEASE
    if value in MOD_RELEASE_MAP:
        kc = MOD_RELEASE_MAP[value]
        print("  MOD RELEASE")
        kpd.release(kc)
        return
    
    # Navigation keys
    if value in NAV_MAP:
        kc = NAV_MAP[value]
        print("  NAV")
        kpd.press(kc)
        kpd.release(kc)
        return
    
    # Function keys
    if value in FUNC_MAP:
        kc = FUNC_MAP[value]
        print("  FUNC")
        
        # If Fn is pressed, send the Fn+function key combination
        if fn_pressed:
            print("  (with Fn)")
            # On most keyboards, Fn+Function key sends a different HID code
            # You may need to adjust these mappings based on your specific keyboard
            # Define Fn+Function key combinations
            # Format: Keycode.Fn: (modifier, key)
            fn_mapping = {
                Keycode.F1: (Keycode.LEFT_CONTROL, Keycode.F1),  # Fn+F1 = Ctrl+F1
                Keycode.F2: (Keycode.LEFT_CONTROL, Keycode.F2),  # Fn+F2 = Ctrl+F2
                Keycode.F3: (Keycode.LEFT_CONTROL, Keycode.F3),  # Fn+F3 = Ctrl+F3
                Keycode.F4: (Keycode.LEFT_CONTROL, Keycode.F4),  # Fn+F4 = Ctrl+F4
                Keycode.F5: (Keycode.LEFT_CONTROL, Keycode.F5),  # Fn+F5 = Ctrl+F5
                Keycode.F6: (Keycode.LEFT_CONTROL, Keycode.F6),  # Fn+F6 = Ctrl+F6
                Keycode.F7: (Keycode.LEFT_CONTROL, Keycode.F7),  # Fn+F7 = Ctrl+F7
                Keycode.F8: (Keycode.LEFT_CONTROL, Keycode.F8),  # Fn+F8 = Ctrl+F8
                Keycode.F9: (Keycode.LEFT_CONTROL, Keycode.F9),  # Fn+F9 = Ctrl+F9
                Keycode.F10: (Keycode.LEFT_CONTROL, Keycode.F10), # Fn+F10 = Ctrl+F10
                Keycode.F11: (Keycode.LEFT_CONTROL, Keycode.F11), # Fn+F11 = Ctrl+F11
                Keycode.F12: (Keycode.LEFT_CONTROL, Keycode.F12)  # Fn+F12 = Ctrl+F12
            }
            
            if kc in fn_mapping:
                mod, key = fn_mapping[kc]
                kpd.press(mod, key)
                kpd.release_all()
            else:
                # Default behavior if no specific mapping
                kpd.press(Keycode.LEFT_ALT, kc)
                kpd.release_all()
        else:
            # Normal function key press
            kpd.press(kc)
            kpd.release(kc)
        return
    
    # Caps Lock
    if value == PROTO_CAPS_LOCK:
        print("  CAPS LOCK")
        kpd.press(Keycode.CAPS_LOCK)
        kpd.release(Keycode.CAPS_LOCK)
        return
    
    # Control characters (Ctrl+A=0x01 ... Ctrl+Z=0x1A)
    if 0x01 <= value <= 0x1A:
        letter_kc = Keycode.A + (value - 1)
        print("  CTRL+{}".format(chr(ord('a') + value - 1)))
        kpd.press(Keycode.LEFT_CONTROL)
        kpd.press(letter_kc)
        kpd.release(letter_kc)
        kpd.release(Keycode.LEFT_CONTROL)
        return
    
    # Printable ASCII (0x20-0x7E)
    if 0x20 <= value <= 0x7E:
        ch = chr(value)
        keycode, needs_shift = ascii_to_keypress(ch)
        if keycode is not None:
            print("  ASCII '{}'".format(ch))
            if needs_shift:
                kpd.press(Keycode.LEFT_SHIFT)
            kpd.press(keycode)
            kpd.release(keycode)
            if needs_shift:
                kpd.release(Keycode.LEFT_SHIFT)
            return
    
    print("  UNKNOWN")

debug_press_count = 0

print("Receiver started!")

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
                print("TIMEOUT: clearing buffer '{}'".format(bit_buffer))
            bit_buffer = ""
            state = STATE_WAIT_START_0

    # Scan physical keys
    for i in range(NUM_KEYS):
        keys[i].update()
        
        if keys[i].fell:
            debug_press_count += 1
            last_key_time = current_time
            
            print("PRESS key={} state={} (#{})".format(i, state, debug_press_count))
            
            if state == STATE_WAIT_START_0:
                # Waiting for first part of start symbol (key0)
                if i == 0:
                    state = STATE_WAIT_START_1
                    state_enter_time = current_time
                    print("  -> waiting for key1")
                else:
                    print("  -> ignored, need key0 first")
            
            elif state == STATE_WAIT_START_1:
                # Waiting for second part of start symbol (key1)
                if i == 1:
                    # START SYMBOL COMPLETE!
                    state = STATE_RECEIVING
                    bit_buffer = ""
                    print(">>> START SYMBOL DETECTED <<<")
                elif i == 0:
                    # Another key0 - restart
                    state_enter_time = current_time
                    print("  -> restart, another key0")
            
            elif state == STATE_RECEIVING:
                # Receiving data bits
                bit_buffer += KEYMAP[i]
                print("  -> bit={} buffer='{}' len={}".format(KEYMAP[i], bit_buffer, len(bit_buffer)))
                
                # Check if we have a complete byte
                if len(bit_buffer) == 8:
                    value = int(bit_buffer, 2)
                    bit_buffer = ""
                    state = STATE_WAIT_START_0
                    process_byte(value)