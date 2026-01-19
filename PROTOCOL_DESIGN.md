# Binary AutoTyper Protocol Design

## Overview

This document specifies the communication protocol between the Binary Keyboard (RP2040-Zero) and Auto Presser (Teensy 4.0) components of the BinaryAutoTyper system. The protocol uses a single-byte encoding scheme for all keyboard actions, optimized for reliable transmission over a serial connection.

## Protocol Byte Format

The protocol uses 8-bit values (0x00-0xFF) to represent all possible keyboard actions. The value space is divided into several ranges:

| Range        | Type                | Description                                      |
|--------------|---------------------|--------------------------------------------------|
| 0x00-0x1F   | Control Chars       | ASCII control characters (Ctrl+A=0x01, etc.)     |
| 0x20-0x7F   | Printable ASCII     | Standard ASCII characters (space through ~)      |
| 0x80-0x87   | Modifier Press      | Press modifier keys (Ctrl, Shift, Alt, GUI)      |
| 0x88-0x8F   | Modifier Release    | Release modifier keys                            |
| 0x90-0x9E   | Navigation Keys     | Arrow keys, Enter, Backspace, etc.               |
| 0x9F        | Reserved            | -                                                |
| 0xA0-0xAB   | Function Keys       | F1-F12                                           |
| 0xAC-0xAF   | Reserved            | For future use                                   |
| 0xB0        | Caps Lock           | Toggle Caps Lock                                 |
| 0xB1        | Fn Press            | Fn key is pressed                                |
| 0xB2        | Fn Release          | Fn key is released                               |
| 0xB3-0xFF   | Reserved            | Future expansion                                 |

## Modifier Keys (0x80-0x8F)

### Modifier Press (0x80-0x87)
| Hex  | Keycode            | Description                     |
|------|--------------------|---------------------------------|
| 0x80 | LEFT_CONTROL       | Press Left Control              |
| 0x81 | LEFT_SHIFT         | Press Left Shift               |
| 0x82 | LEFT_ALT           | Press Left Alt                 |
| 0x83 | LEFT_GUI           | Press Left GUI (Win/Command)   |
| 0x84 | RIGHT_CONTROL      | Press Right Control            |
| 0x85 | RIGHT_SHIFT        | Press Right Shift             |
| 0x86 | RIGHT_ALT          | Press Right Alt (AltGr)        |
| 0x87 | RIGHT_GUI          | Press Right GUI (Win/Command)  |

### Modifier Release (0x88-0x8F)
| Hex  | Keycode            | Description                     |
|------|--------------------|---------------------------------|
| 0x88 | LEFT_CONTROL       | Release Left Control            |
| 0x89 | LEFT_SHIFT         | Release Left Shift             |
| 0x8A | LEFT_ALT           | Release Left Alt               |
| 0x8B | LEFT_GUI           | Release Left GUI               |
| 0x8C | RIGHT_CONTROL      | Release Right Control          |
| 0x8D | RIGHT_SHIFT        | Release Right Shift           |
| 0x8E | RIGHT_ALT          | Release Right Alt             |
| 0x8F | RIGHT_GUI          | Release Right GUI             |

## Navigation Keys (0x90-0x9E)

| Hex  | Keycode            | Description                     |
|------|--------------------|---------------------------------|
| 0x90 | RIGHT_ARROW        | Right Arrow                     |
| 0x91 | LEFT_ARROW         | Left Arrow                      |
| 0x92 | DOWN_ARROW         | Down Arrow                      |
| 0x93 | UP_ARROW           | Up Arrow                        |
| 0x94 | BACKSPACE          | Backspace                       |
| 0x95 | ENTER              | Enter/Return                    |
| 0x96 | TAB                | Tab                             |
| 0x97 | ESCAPE             | Escape                          |
| 0x98 | DELETE             | Forward Delete                  |
| 0x99 | INSERT             | Insert                          |
| 0x9A | HOME               | Home                            |
| 0x9B | END                | End                             |
| 0x9C | PAGE_UP            | Page Up                         |
| 0x9D | PAGE_DOWN          | Page Down                       |
| 0x9E | CLEAR_BUFFER       | Emergency clear all keys        |

### Function Keys (0xA0-0xAB)

Function keys F1-F12 are mapped to 0xA0-0xAB respectively.

When the Fn key is pressed (0xB1) and held, pressing a function key will send a modified key combination. The default mapping is:

- Fn+F1: Ctrl+F1
- Fn+F2: Ctrl+F2
- ...and so on for other F-keys

You can customize these mappings in the `fn_mapping` dictionary in the RP2040 code.

| Hex  | Keycode  | Description |
|------|----------|-------------|
| 0xA0 | F1       | Function 1  |
| 0xA1 | F2       | Function 2  |
| 0xA2 | F3       | Function 3  |
| 0xA3 | F4       | Function 4  |
| 0xA4 | F5       | Function 5  |
| 0xA5 | F6       | Function 6  |
| 0xA6 | F7       | Function 7  |
| 0xA7 | F8       | Function 8  |
| 0xA8 | F9       | Function 9  |
| 0xA9 | F10      | Function 10 |
| 0xAA | F11      | Function 11 |
| 0xAB | F12      | Function 12 |

## Special Keys

| Hex  | Keycode    | Description          |
|------|------------|----------------------|
| 0xB0 | CAPS_LOCK  | Toggle Caps Lock     |
| 0xB1 | FN_PRESS   | Fn key pressed       |
| 0xB2 | FN_RELEASE | Fn key released      |
| 0x14 | 20      | Up Arrow      | Up arrow key                          |
| 0x15 | 21      | Backspace     | Backspace key                         |
| 0x16 | 22      | Enter         | Enter/Return key                      |
| 0x17 | 23      | Tab           | Tab key                               |
| 0x18 | 24      | Escape        | Escape key                            |
| 0x19 | 25      | Delete        | Forward Delete key                    |
| 0x1A | 26      | Insert        | Insert key                            |
| 0x1B | 27      | Home          | Home key                              |
| 0x1C | 28      | End           | End key                               |
| 0x1D | 29      | Page Up       | Page Up key                           |
| 0x1E | 30      | Page Down     | Page Down key                         |
| 0x1F | 31      | Reserved      | Reserved for future use               |

## Communication Flow

### Binary Keyboard (RP2040) → Auto Presser (Teensy 4.0)

1. **Binary Input Processing**
   - User enters binary sequence using the two buttons
   - On completion (both buttons held), the binary value is converted to a protocol byte
   - The byte is transmitted serially to the Teensy 4.0

2. **Serial Transmission**
   - 8N1 format (8 data bits, no parity, 1 stop bit)
   - Default baud rate: 115200
   - LSB first
   - Each byte is framed with a start symbol (key0 then key1 within 50ms)

### Auto Presser (Teensy 4.0) → Host Computer

1. **Byte Reception**
   - Receives byte from RP2040 via serial
   - Decodes according to protocol specifications
   - Maps to appropriate HID keycodes
   - Handles modifier states (press/release)

2. **Key Press Simulation**
   - For standard keys: Press and release the key
   - For modifiers: Press or release the modifier
   - For navigation/function keys: Press and release the key

## Error Handling

- **Invalid Bytes**: Bytes outside defined ranges are ignored
- **Communication Errors**: Timeouts and framing errors are handled
- **Emergency Clear**: Sending 0x9E clears all keys and resets state
- **State Recovery**: System resets modifier states on startup or error

## Implementation Notes

### RP2040 (Binary Keyboard)
- Uses CircuitPython's `usb_hid` and `adafruit_hid` libraries
- Implements a state machine for binary input
- Handles debouncing in hardware/software
- Maintains modifier states

### Teensy 4.0 (Auto Presser)
- Uses Teensyduino's USB Host Shield library
- Implements the protocol state machine
- Handles USB HID communication with host
- Manages solenoid control for key pressing

## Performance Characteristics

- **Latency**: <5ms typical end-to-end
- **Throughput**: Up to 1000 keypresses per second (theoretical)
- **Reliability**: 100% reliable with hardware flow control
- **Solenoid Timing**: 25ms pulse width, 15ms gap between bits

## Future Extensions

1. **Wireless Protocol**
   - Add support for BLE or 2.4GHz wireless
   - Implement error correction and retransmission

2. **Extended Keycodes**
   - Support for international keyboards
   - Media keys and system controls

3. **Battery Optimization**
   - Power management commands
   - Sleep/wake functionality

## Security Considerations

- **No Encryption**: The protocol does not include encryption
- **Physical Access Required**: Both devices must be physically connected
- **No Authentication**: Any device can send commands if connected
- **Emergency Clear**: 0x9E can be used to clear all keys if needed

## References

- [USB HID Usage Tables](https://www.usb.org/document-library/hid-usage-tables-112)
- [Teensy USB Keyboard](https://www.pjrc.com/teensy/td_keyboard.html)
- [CircuitPython HID](https://learn.adafruit.com/circuitpython-essentials/circuitpython-hid-keyboard-and-mouse)
