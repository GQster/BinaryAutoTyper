import board
import time
import usb_hid
from digitalio import DigitalInOut, Pull
from adafruit_debouncer import Debouncer
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from adafruit_hid.keycode import Keycode

kpd = Keyboard(usb_hid.devices)
keyboard_layout = KeyboardLayoutUS(kpd)

CLEAR_TIMEOUT = 2  # Clear the binary input after 2 seconds of inactivity

# Choose the correct modifier key for Windows or Mac.
# Comment one line and uncomment the other.
MODIFIER = Keycode.CONTROL  # For Windows
# MODIFIER = Keycode.COMMAND  # For Mac

# Define the pins for the keys
NUM_KEYS = 2
PINS = (
    board.GP2,  
    board.GP3,
)

# Define what each key does 
KEYMAP = (
    ("0", [Keycode.KEYPAD_ZERO]),
    ("1", [Keycode.KEYPAD_ONE])
)

keys = []
for pin in PINS:
    dio = DigitalInOut(pin)
    dio.pull = Pull.UP
    keys.append(Debouncer(dio))

binary_input = ""
last_key_time = time.monotonic()

print("\nWelcome to keypad")
print("keymap:")
for k in range(NUM_KEYS):
    print("\t", (KEYMAP[k][0]))

# Function to convert binary to char
def binary_to_char(binary_str):
    try:
        # Ensure the binary string is valid and has a length of 8 bits
        if len(binary_str) != 8 or not all(c in '01' for c in binary_str):
            raise ValueError("Invalid binary string")
        return chr(int(binary_str, 2))
    except ValueError as e:
        print(f"Error converting binary to char: {e}")
        return ''

while True:
    try:
        current_time = time.monotonic()
        if current_time - last_key_time > CLEAR_TIMEOUT:
            if binary_input:
                print(f"Clearing binary input due to {CLEAR_TIMEOUT} second timeout")
                binary_input = ""

        for i in range(NUM_KEYS):
            keys[i].update()
            if keys[i].fell:
                last_key_time = time.monotonic()
                binary_input += KEYMAP[i][0]
                print(f"Current binary input: {binary_input}")
                if len(binary_input) == 8:  # Assuming 8-bit binary input           MSB first
                    char = binary_to_char(binary_input)
                    if char:  # Only send the character if it's valid
                        print(f"Sending character: {char}")
                        keyboard_layout.write(char)
                    binary_input = ""  # Reset the binary input after processing
    except Exception as e:
        print(f"An error occurred: {e}")
        binary_input = ""  # Reset the binary input in case of any error
