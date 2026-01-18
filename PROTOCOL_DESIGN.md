# Binary AutoTyper Protocol Design

## Overview
This document describes the communication protocol between the Teensy 4.0 (keyboard receiver) and RP2040 Zero (keyboard emulator).

## Protocol Bytes

### ASCII Characters (0x20..0x7E)
- Send printable ASCII directly
- RP2040 decodes and presses/releases the character
- Example: 0x41 = 'A'

### Special Keys (0x02..0x1D)
Reserved protocol values for non-ASCII keys:

| Proto | Key |
|-------|-----|
| 0x02  | F1  |
| 0x03  | F2  |
| 0x04  | F3  |
| 0x05  | F4  |
| 0x06  | F5  |
| 0x07  | F6  |
| 0x08  | F7  |
| 0x09  | F8  |
| 0x0A  | F9  |
| 0x0B  | F10 |
| 0x0C  | F11 |
| 0x0D  | F12 |
| 0x10  | Right Arrow |
| 0x11  | Left Arrow |
| 0x12  | Down Arrow |
| 0x13  | Up Arrow |
| 0x14  | Backspace |
| 0x15  | Enter |
| 0x16  | Tab |
| 0x17  | Escape |
| 0x18  | Delete |
| 0x19  | Insert |
| 0x1A  | Home |
| 0x1B  | End |
| 0x1C  | Page Up |
| 0x1D  | Page Down |

### Ctrl+Letter Combo (0x40..0x59)
**Atomic single-byte encoding for Ctrl+key combinations**

- PROTO_CTRL_A = 0x40
- Ctrl+A = 0x40, Ctrl+B = 0x41, ..., Ctrl+Z = 0x59

**Implementation:**
- When Teensy detects Ctrl+letter (raw HID 0x01..0x1A), it sends a single combo proto byte
- RP2040 receives the combo proto and atomically:
  1. Presses LEFT_CONTROL
  2. Presses the letter key
  3. Releases the letter key
  4. Releases LEFT_CONTROL

This avoids any timing/buffering issues by encoding the entire action in a single byte.

### Modifier Toggles (0x80..0x87)
For holding/releasing modifiers separately:

| Proto | Modifier |
|-------|----------|
| 0x80  | Left Ctrl |
| 0x81  | Left Shift |
| 0x82  | Left Alt |
| 0x83  | Left GUI |
| 0x84  | Right Ctrl |
| 0x85  | Right Shift |
| 0x86  | Right Alt |
| 0x87  | Right GUI |

**Implementation:**
- Each toggle proto byte toggles the corresponding modifier on/off
- RP2040 maintains `modifier_state` dict to track active modifiers
- First press = hold modifier, second press = release modifier

## Flow

### Teensy (keyPresserTeensy4.ino)
```
1. USB keyboard connected to Teensy
2. OnKeyPress callback fires
3. Map key to protocol byte:
   - Ctrl+letter (0x01..0x1A) → combo proto (0x40..0x59)
   - ASCII (0x20..0x7E) → ASCII directly
   - Special keys → protocol bytes (0x02..0x1D, 0x10..0x1D)
   - Modifiers (0xE0..0xE7) → mod toggle (0x80..0x87)
4. Push byte to serial buffer
5. Transmit 8 bits serially to RP2040 (LSB first)
```

### RP2040 (BinaryKeyboard/code.py)
```
1. Receive 8 bits serially from Teensy
2. Convert to integer (0..255)
3. Decode:
   - If combo proto (0x40..0x59): atomic Ctrl+key press/release
   - If special key (0x02..0x1D): press/release special key
   - If ASCII (0x20..0x7E): type character
   - If mod toggle (0x80..0x87): toggle modifier state
4. Send HID report to host
```

## Why Single Bytes for Ctrl+Key?

Previously, the system tried to buffer modifier bytes and wait for ASCII:
- **Problem**: Race conditions, timing issues, lost presses
- **Solution**: Encode Ctrl+key as a single atomic byte
- **Benefit**: One byte = one action, no buffering needed

## Key Design Principles

1. **Atomic Actions**: Each protocol byte produces a single, atomic keyboard action
2. **No Buffering**: No waiting for follow-up bytes (except basic serial framing)
3. **Stateful Modifiers**: Toggle mechanism allows holding modifiers across multiple key presses
4. **Protocol Extensibility**: 256 possible values with reserved ranges for future features
